/* Map rendering engine for T-Deck Plus
 * Handles vector tile parsing, rendering, and caching.
 */
#ifdef USE_LVGL_UI

#include "map_engine.h"
#include "map_internal.h"
#include "ui_map_manager.h"
#include "sd_logger.h"
#include <esp_log.h>
#include <JPEGDEC.h>
#undef INTELSHORT
#undef INTELLONG
#undef MOTOSHORT
#undef MOTOLONG
#include <PNGdec.h>
#include "storage_utils.h"
#include "OpenSansBold6pt7b.h"
#include <SD.h>
#include <esp_task_wdt.h>
#include <algorithm>
#include <climits>
#include <cmath>
#include <new>

// Global sprite pointer for raster decoder callbacks
static LGFX_Sprite* targetSprite_ = nullptr;

static const char *TAG = "MapEngine";

namespace MapEngine {

    const char* TAG_ENGINE = TAG;

    // VLW Unicode font for map labels
    lgfx::VLWfont vlwFont;
    bool vlwFontLoaded = false;

    // Handles for the asynchronous rendering system
    QueueHandle_t mapRenderQueue = nullptr;
    SemaphoreHandle_t spriteMutex = nullptr;
    static TaskHandle_t mapRenderTaskHandle = nullptr;
    static lv_obj_t* canvas_to_invalidate_ = nullptr;

    // Render lock: held for the entire renderNavViewport() duration.
    // clearTileCache/closeAllNpkSlots defer when this is held.
    SemaphoreHandle_t renderLock = nullptr;
    volatile bool renderActive_ = false;
    static volatile bool renderPending_ = false;   // Request queued, not yet started
    volatile bool deferredClearRequested = false;

    bool isRenderActive() { return renderActive_ || renderPending_; }

    // Async NAV rendering
    EventGroupHandle_t mapEventGroup = nullptr;
    QueueHandle_t navRenderQueue = nullptr;
    volatile int lastRenderedTileX = 0;
    volatile int lastRenderedTileY = 0;
    volatile uint8_t lastRenderedZoom = 0;

    // Vectors for AEL polygon filler (PSRAM to preserve DRAM)
    std::vector<UIMapManager::Edge, PSRAMAllocator<UIMapManager::Edge>> edgePool;
    std::vector<int, PSRAMAllocator<int>> edgeBuckets;

    // Vectors for coordinate projection (PSRAM, pre-reserved)
    std::vector<int, PSRAMAllocator<int>> proj32X;
    std::vector<int, PSRAMAllocator<int>> proj32Y;

    // Decoded coords buffer for Delta+ZigZag+VarInt features (PSRAM, reused per feature)
    std::vector<int16_t, PSRAMAllocator<int16_t>> decodedCoords;

    // npkRowBuf: PSRAM buffer for full index-row reads (allocated in initTileCache).
    // Declared here (before initTileCache) so the function can see the symbol.
    // Capacity 8192 covers Z16/Z17 dense rows; avoids on-disk binary-search fallback.
    static UIMapManager::Npk2IndexEntry* npkRowBuf    = nullptr;
    static uint32_t                      npkRowBufCap = 0;

    // Waterway label buffers (PSRAM, allocated/freed with render task)
    int*   wlScreenX = nullptr;
    int*   wlScreenY = nullptr;
    float* wlArcLen  = nullptr;

    // Reusable glyph sprite for curvilinear labels (avoids alloc/free per character)
    LGFX_Sprite* glyphSprite = nullptr;
    int glyphSpriteW = 0, glyphSpriteH = 0;

    // --- VarInt decoding (protobuf-style LEB128) ---
    uint32_t readVarInt(const uint8_t* buf, uint32_t& offset, uint32_t limit) {
        uint32_t result = 0;
        uint32_t shift = 0;
        while (offset < limit) {
            uint8_t b = buf[offset++];
            result |= (uint32_t)(b & 0x7F) << shift;
            if ((b & 0x80) == 0) return result;
            shift += 7;
            if (shift >= 35) break;  // overflow protection
        }
        return result;
    }

    int16_t zigzagDecode(uint32_t n) {
        return (int16_t)((n >> 1) ^ -(int32_t)(n & 1));
    }

    // Decode Delta+ZigZag+VarInt coords into decodedCoords[].
    // For polygons: reads ringCount + ringEnds from end of payload.
    // Returns false if data is truncated.
    bool decodeFeatureCoords(const uint8_t* payload, uint16_t coordCount,
                                     uint16_t payloadSize, uint8_t geomType,
                                     DecodedFeature& out) {
        out.coordsIdx = decodedCoords.size();
        out.ringCount = 0;
        out.ringEnds = nullptr;

        // Ring data (polygons) is handled after decoding coords below

        // Decode delta+zigzag+varint coords
        uint32_t offset = 0;
        int16_t prevX = 0, prevY = 0;

        // Reserve space
        size_t needed = out.coordsIdx + coordCount * 2;
        if (decodedCoords.capacity() < needed) {
            decodedCoords.reserve(needed + 256);
        }
        decodedCoords.resize(needed);

        for (uint16_t i = 0; i < coordCount; i++) {
            if (offset >= payloadSize) return false;
            uint32_t rawX = readVarInt(payload, offset, payloadSize);
            if (offset >= payloadSize && i < coordCount - 1) return false;
            uint32_t rawY = readVarInt(payload, offset, payloadSize);

            int16_t dx = zigzagDecode(rawX);
            int16_t dy = zigzagDecode(rawY);
            int16_t x = prevX + dx;
            int16_t y = prevY + dy;

            decodedCoords[out.coordsIdx + i * 2]     = x;
            decodedCoords[out.coordsIdx + i * 2 + 1] = y;

            prevX = x;
            prevY = y;
        }

        // For polygons: read ring data from remaining bytes after varint coords
        if (geomType == 3) {
            uint32_t remaining = payloadSize - offset;
            if (remaining >= 2) {
                uint16_t ringCount;
                memcpy(&ringCount, payload + offset, 2);
                if (remaining >= 2 + ringCount * 2) {
                    out.ringCount = ringCount;
                    out.ringEnds = (uint16_t*)(const_cast<uint8_t*>(payload) + offset + 2);
                }
            }
        }

        return true;
    }

    // 16 priority layers (dispatch by getPriority() low nibble)
    std::vector<FeatureRef, PSRAMAllocator<FeatureRef>> globalLayers[16];

    // --- RASTER TILE CACHE (STATIC POOL) ---
    static LGFX_Sprite* _tileSpritePool[RASTER_TILE_CACHE_SIZE] = {nullptr};
    static std::vector<CachedTile> _tileCache;
    static bool _isRasterCacheInitialized = false;
    static uint32_t _rasterCacheAccessCounter = 0;

    // --- NAV POOL ---
    #define NAV_POOL_SLOT_SIZE 524288 // 512KB — Z9 tiles range 290-500KB
    #define NAV_POOL_MAX_SLOTS 5

    static uint8_t* _navPool[NAV_POOL_MAX_SLOTS] = {nullptr};
    static bool _navPoolInUse[NAV_POOL_MAX_SLOTS] = {false};
    static bool _navPoolActive = false;
    static LovyanGFX* _gfx = nullptr;

    NpkSlot npkSlots[NPK_MAX_REGIONS];
    static uint32_t npkAccessCounter = 0;

    std::vector<NavCacheEntry> navCache;
    uint32_t navCacheAccessCounter = 0;

    // LVGL async call to invalidate the map canvas from another thread
    static void invalidate_map_canvas_cb(void* user_data) {
        lv_obj_t* canvas = (lv_obj_t*)user_data;
        if (canvas) {
            lv_obj_invalidate(canvas);
        }
    }

    // --- RASTER DECODING ENGINE ---
    static PNG png;
    static JPEGDEC jpeg;

    // Generic file callbacks for raster decoders
    static void* rasterOpenFile(const char* filename, int32_t* size) {
        File* file = new File(SD.open(filename, FILE_READ));
        if (!file || !*file) {
            delete file;
            return nullptr;
        }
        *size = file->size();
        return file;
    }

    static void rasterCloseFile(void* handle) {
        File* file = (File*)handle;
        if (file) {
            file->close();
            delete file;
        }
    }

    static int32_t rasterReadFile(void* handle, uint8_t* pBuf, int32_t iLen) {
        File* file = (File*)handle;
        return file->read(pBuf, iLen);
    }
    
    static int32_t rasterReadFileJPEG(JPEGFILE* pFile, uint8_t* pBuf, int32_t iLen) {
        return rasterReadFile(pFile->fHandle, pBuf, iLen);
    }

    static int32_t rasterReadFilePNG(PNGFILE* pFile, uint8_t* pBuf, int32_t iLen) {
        return rasterReadFile(pFile->fHandle, pBuf, iLen);
    }

    static int32_t rasterSeekFile(void* handle, int32_t iPosition) {
        File* file = (File*)handle;
        return file->seek(iPosition);
    }

    static int32_t rasterSeekFileJPEG(JPEGFILE* pFile, int32_t iPosition) {
        return rasterSeekFile(pFile->fHandle, iPosition);
    }

    static int32_t rasterSeekFilePNG(PNGFILE* pFile, int32_t iPosition) {
        return rasterSeekFile(pFile->fHandle, iPosition);
    }

    // JPEG draw callback
    static int jpegDrawCallback(JPEGDRAW* pDraw) {
        if (!targetSprite_) return 0;
        targetSprite_->pushImage(pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight, pDraw->pPixels);
        return 1;
    }

    // PNG draw callback
    static int pngDrawCallback(PNGDRAW* pDraw) {
        if (!targetSprite_) return 0;
        uint16_t* pfb = (uint16_t*)targetSprite_->getBuffer();
        if(pfb) {
            uint16_t* pLine = pfb + (pDraw->y * MAP_TILE_SIZE);
            png.getLineAsRGB565(pDraw, pLine, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);
        }
        return 1;
    }

    // Raster renderers
    static bool renderJPGRaster(const char* path, LGFX_Sprite& map) {
        targetSprite_ = &map;
        bool success = false;
        if (xSemaphoreTakeRecursive(spiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            if (jpeg.open(path, rasterOpenFile, rasterCloseFile, rasterReadFileJPEG, rasterSeekFileJPEG, jpegDrawCallback) == 1) {
                jpeg.setPixelType(RGB565_LITTLE_ENDIAN);
                if (jpeg.decode(0, 0, 0) == 1) success = true;
                jpeg.close();
            }
            xSemaphoreGiveRecursive(spiMutex);
        }
        targetSprite_ = nullptr;
        return success;
    }

    static bool renderPNGRaster(const char* path, LGFX_Sprite& map) {
        targetSprite_ = &map;
        bool success = false;
        if (xSemaphoreTakeRecursive(spiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            if (png.open(path, rasterOpenFile, rasterCloseFile, rasterReadFilePNG, rasterSeekFilePNG, pngDrawCallback) == 1) {
                if (png.decode(nullptr, 0) == 1) success = true;
                png.close();
            }
            xSemaphoreGiveRecursive(spiMutex);
        }
        targetSprite_ = nullptr;
        return success;
    }

    // Background task on Core 0: handles raster tile decoding + async NAV viewport rendering
    static void mapRenderTask(void* param) {
        RenderRequest request;
        NavRenderRequest navReq;
        ESP_LOGI(TAG, "Render task started on Core 0");

        while (true) {
            // Priority: NAV viewport requests (latest-wins — drain queue)
            if (navRenderQueue && xQueueReceive(navRenderQueue, &navReq, 0) == pdTRUE) {
                NavRenderRequest latest = navReq;
                while (xQueueReceive(navRenderQueue, &latest, 0) == pdTRUE) {}

                renderPending_ = false;

                // Process deferred clear BEFORE rendering (not after)
                if (deferredClearRequested) {
                    deferredClearRequested = false;
                    ESP_LOGI(TAG, "Processing deferred clearTileCache (pre-render)");
                    clearTileCache();
                }

                xEventGroupClearBits(mapEventGroup, MAP_EVENT_NAV_DONE);

                const char* regionPtrs[8];
                for (int r = 0; r < latest.regionCount && r < 8; r++)
                    regionPtrs[r] = latest.regions[r];

                if (latest.isRaster) {
                    ESP_LOGD(TAG, "Async raster render: Z%d (%.4f, %.4f)",
                                  latest.zoom, latest.centerLat, latest.centerLon);
                    SD_Logger::updateCrashContext("MAP_RASTER", latest.centerLat, latest.centerLon);
                    renderRasterViewport(latest.centerLat, latest.centerLon, latest.zoom,
                                         *latest.targetSprite, latest.regions[0]);
                } else {
                    ESP_LOGD(TAG, "Async NAV render: Z%d (%.4f, %.4f)",
                                  latest.zoom, latest.centerLat, latest.centerLon);
                    SD_Logger::updateCrashContext("MAP_NAV", latest.centerLat, latest.centerLon);
                    renderNavViewport(latest.centerLat, latest.centerLon, latest.zoom,
                                      *latest.targetSprite, regionPtrs, latest.regionCount);
                }
                SD_Logger::updateCrashContext("MAP_IDLE", latest.centerLat, latest.centerLon);

                lastRenderedTileX = latest.centerTileX;
                lastRenderedTileY = latest.centerTileY;
                lastRenderedZoom = latest.zoom;
                xEventGroupSetBits(mapEventGroup, MAP_EVENT_NAV_DONE);
                continue;
            }

            // Raster tile requests (50ms timeout to re-check NAV queue promptly)
            if (xQueueReceive(mapRenderQueue, &request, pdMS_TO_TICKS(50)) == pdTRUE) {
                // Background loading for individual tiles (legacy path)
                CachedTile* slot = getRasterCacheSlot(request.zoom, request.tileX, request.tileY);
                if (slot && !slot->isValid) {
                    bool success = false;
                    if (xSemaphoreTake(spriteMutex, portMAX_DELAY) == pdTRUE) {
                        success = renderTile(request.path, 0, 0, *slot->sprite, (uint8_t)request.zoom);
                        xSemaphoreGive(spriteMutex);
                    }

                    if (success) {
                        strncpy(slot->filePath, request.path, sizeof(slot->filePath) - 1);
                        slot->filePath[sizeof(slot->filePath) - 1] = '\0';
                        slot->isValid = true;
                    }
                    lv_async_call(invalidate_map_canvas_cb, canvas_to_invalidate_);
                }
            }
        }
    }

    void stopRenderTask() {
        // Wait for any active render to finish before killing the task
        if (renderLock) {
            xSemaphoreTake(renderLock, pdMS_TO_TICKS(15000));
            xSemaphoreGive(renderLock);
        }
        renderPending_ = false;
        renderActive_ = false;

        if (mapRenderTaskHandle) {
            vTaskDelete(mapRenderTaskHandle);
            mapRenderTaskHandle = nullptr;
        }
        if (mapRenderQueue) {
            vQueueDelete(mapRenderQueue);
            mapRenderQueue = nullptr;
        }
        if (navRenderQueue) {
            vQueueDelete(navRenderQueue);
            navRenderQueue = nullptr;
        }
        if (mapEventGroup) {
            vEventGroupDelete(mapEventGroup);
            mapEventGroup = nullptr;
        }
        if (spriteMutex) {
            vSemaphoreDelete(spriteMutex);
            spriteMutex = nullptr;
        }
        // Free waterway label buffers
        if (glyphSprite) { glyphSprite->deleteSprite(); delete glyphSprite; glyphSprite = nullptr; }
        glyphSpriteW = glyphSpriteH = 0;
        heap_caps_free(wlScreenX); wlScreenX = nullptr;
        heap_caps_free(wlScreenY); wlScreenY = nullptr;
        heap_caps_free(wlArcLen);  wlArcLen  = nullptr;
        canvas_to_invalidate_ = nullptr;
        ESP_LOGI(TAG, "Render task stopped.");
    }

    void enqueueNavRender(const NavRenderRequest& req) {
        if (!navRenderQueue) return;
        renderPending_ = true;
        xQueueOverwrite(navRenderQueue, &req);
    }

    void startRenderTask(lv_obj_t* canvas_to_invalidate) {
        if (mapRenderTaskHandle) return;

        canvas_to_invalidate_ = canvas_to_invalidate;
        if (!renderLock) renderLock = xSemaphoreCreateMutex();
        spriteMutex = xSemaphoreCreateMutex();
        mapRenderQueue = xQueueCreate(10, sizeof(RenderRequest));
        navRenderQueue = xQueueCreate(1, sizeof(NavRenderRequest));
        mapEventGroup = xEventGroupCreate();

        // Allocate waterway label buffers in PSRAM (freed in stopRenderTask)
        if (!wlScreenX) wlScreenX = (int*)heap_caps_malloc(WLABEL_MAX_PTS * sizeof(int), MALLOC_CAP_SPIRAM);
        if (!wlScreenY) wlScreenY = (int*)heap_caps_malloc(WLABEL_MAX_PTS * sizeof(int), MALLOC_CAP_SPIRAM);
        if (!wlArcLen)  wlArcLen  = (float*)heap_caps_malloc(WLABEL_MAX_PTS * sizeof(float), MALLOC_CAP_SPIRAM);

        xTaskCreatePinnedToCore(
            mapRenderTask,
            "MapRender",
            16384,  // Increased for feature index + sort + AEL
            NULL,
            1, // Low priority
            &mapRenderTaskHandle,
            0  // Core 0
        );
    }

    // Forward declarations (defined after shrinkProjectionBuffers / clearNavCache)
    static void clearNavCache();
    static void closeNpkSlot(int idx);
    static void closeAllNpkSlots();
    static void invalidateIdxRowCacheForSlot(int slotIdx);
    static void invalidateAllIdxRowCache();

    bool isNavPoolActive() {
        return _navPoolActive;
    }

    void initNavPool() {
        if (_navPoolActive) return;

        if (renderLock) xSemaphoreTake(renderLock, portMAX_DELAY);

        // 1. Free raster sprites
        ESP_LOGI(TAG, "Freeing raster pool to allocate NAV pool");
        for (int i = 0; i < RASTER_TILE_CACHE_SIZE; ++i) {
            if (_tileSpritePool[i]) {
                _tileSpritePool[i]->deleteSprite();
                delete _tileSpritePool[i];
                _tileSpritePool[i] = nullptr;
            }
            if (i < (int)_tileCache.size()) {
                _tileCache[i].sprite = nullptr;
                _tileCache[i].isValid = false;
            }
        }
        _isRasterCacheInitialized = false;

        // 2. Allocate NAV slots
        int allocated = 0;
        for (int i = 0; i < NAV_POOL_MAX_SLOTS; ++i) {
            if (!_navPool[i]) {
                _navPool[i] = (uint8_t*)heap_caps_malloc(NAV_POOL_SLOT_SIZE, MALLOC_CAP_SPIRAM);
                if (_navPool[i]) {
                    allocated++;
                } else {
                    ESP_LOGW(TAG, "Failed to allocate NAV pool slot %d", i);
                }
            } else {
                allocated++;
            }
            _navPoolInUse[i] = false;
        }

        _navPoolActive = true;
        ESP_LOGI(TAG, "NAV pool initialized with %d slots (%dKB each)", allocated, NAV_POOL_SLOT_SIZE / 1024);

        if (renderLock) xSemaphoreGive(renderLock);
    }

    void destroyNavPool() {
        if (!_navPoolActive) return;

        if (renderLock) xSemaphoreTake(renderLock, portMAX_DELAY);

        ESP_LOGI(TAG, "Freeing NAV pool to restore raster pool");
        // 1. Clear NAV cache completely to ensure all slots are marked free
        navCache.clear();

        // 2. Free NAV slots
        for (int i = 0; i < NAV_POOL_MAX_SLOTS; ++i) {
            if (_navPool[i]) {
                heap_caps_free(_navPool[i]);
                _navPool[i] = nullptr;
            }
            _navPoolInUse[i] = false;
        }
        _navPoolActive = false;

        // 3. Restore raster pool
        if (_gfx) {
            // Need to do this manually since initTileCache takes the lock again if we called it
            ESP_LOGI(TAG, "Initializing static raster tile pool (%d sprites)...", RASTER_TILE_CACHE_SIZE);
            for (int i = 0; i < RASTER_TILE_CACHE_SIZE; ++i) {
                if (_tileSpritePool[i] == nullptr) {
                    _tileSpritePool[i] = new LGFX_Sprite(_gfx);
                    if (!_tileSpritePool[i]) {
                        ESP_LOGE(TAG, "Failed to allocate sprite %d in pool", i);
                        continue;
                    }
                    _tileSpritePool[i]->setPsram(true);
                    _tileSpritePool[i]->setColorDepth(16);
                    if (!_tileSpritePool[i]->createSprite(MAP_TILE_SIZE, MAP_TILE_SIZE)) {
                        ESP_LOGE(TAG, "Failed to create sprite %d in pool (PSRAM)", i);
                        delete _tileSpritePool[i];
                        _tileSpritePool[i] = nullptr;
                    }
                }
            }

            _tileCache.resize(RASTER_TILE_CACHE_SIZE);
            for (int i = 0; i < RASTER_TILE_CACHE_SIZE; ++i) {
                _tileCache[i].sprite = _tileSpritePool[i];
                _tileCache[i].isValid = false;
            }
            _isRasterCacheInitialized = true;
            ESP_LOGI(TAG, "Raster tile pool initialized.");
        } else {
            ESP_LOGE(TAG, "Cannot restore raster pool: gfx pointer is null");
        }

        if (renderLock) xSemaphoreGive(renderLock);
    }

    uint8_t* acquireNavSlot(size_t needed) {
        if (!_navPoolActive) return nullptr;
        if (needed > NAV_POOL_SLOT_SIZE) {
            ESP_LOGE(TAG, "Requested NAV tile size %d exceeds slot size %d", (int)needed, NAV_POOL_SLOT_SIZE);
            return nullptr;
        }

        uint8_t* slot = nullptr;
        for (int i = 0; i < NAV_POOL_MAX_SLOTS; ++i) {
            if (_navPool[i] && !_navPoolInUse[i]) {
                _navPoolInUse[i] = true;
                slot = _navPool[i];
                break;
            }
        }
        return slot;
    }

    void releaseNavSlot(uint8_t* ptr) {
        if (!ptr) return;

        // Check if pointer belongs to the pool
        for (int i = 0; i < NAV_POOL_MAX_SLOTS; ++i) {
            if (_navPool[i] == ptr) {
                _navPoolInUse[i] = false;
                return;
            }
        }
        // Not a pool pointer — was allocated via ps_malloc fallback
        free(ptr);
    }

    // Initialize tile cache and pre-reserve render buffers
    void initTileCache(LovyanGFX* gfx) {
        if (!_gfx) _gfx = gfx;

        // --- Raster Cache Pool Allocation (once) ---
        if (!_isRasterCacheInitialized) {
            ESP_LOGI(TAG, "Initializing static raster tile pool (%d sprites)...", RASTER_TILE_CACHE_SIZE);
            for (int i = 0; i < RASTER_TILE_CACHE_SIZE; ++i) {
                if (_tileSpritePool[i] == nullptr) {
                    _tileSpritePool[i] = new LGFX_Sprite(gfx);
                    if (!_tileSpritePool[i]) {
                        ESP_LOGE(TAG, "Failed to allocate sprite %d in pool", i);
                        return; // Abort
                    }
                    _tileSpritePool[i]->setPsram(true);
                    _tileSpritePool[i]->setColorDepth(16);
                    if (!_tileSpritePool[i]->createSprite(MAP_TILE_SIZE, MAP_TILE_SIZE)) {
                        ESP_LOGE(TAG, "Failed to create sprite %d in pool (PSRAM)", i);
                        delete _tileSpritePool[i];
                        _tileSpritePool[i] = nullptr;
                        return; // Abort
                    }
                }
            }

            _tileCache.resize(RASTER_TILE_CACHE_SIZE);
            for (int i = 0; i < RASTER_TILE_CACHE_SIZE; ++i) {
                _tileCache[i].sprite = _tileSpritePool[i];
                _tileCache[i].isValid = false;
            }
            _isRasterCacheInitialized = true;
            ESP_LOGI(TAG, "Raster tile pool initialized.");
        }

        // Create render lock once (persists for the entire session)
        if (!renderLock) {
            renderLock = xSemaphoreCreateMutex();
        }

        // Invalidate all raster tiles
        for (auto& cachedTile : _tileCache) {
            cachedTile.isValid = false;
        }
        _rasterCacheAccessCounter = 0;

        // Pre-reserve AEL, projection, and feature index buffers in PSRAM
        edgePool.reserve(1024);
        edgeBuckets.reserve(768);
        proj32X.reserve(1024);
        proj32Y.reserve(1024);
        decodedCoords.reserve(4096);
        for (int i = 0; i < 16; i++) globalLayers[i].reserve(256);
        navCache.reserve(NAV_CACHE_SIZE);

        ESP_LOGI(TAG, "Cache %d raster + %d NAV tiles, render buffers pre-reserved (PSRAM)",
                      RASTER_TILE_CACHE_SIZE, NAV_CACHE_SIZE);

        // Allocate npkRowBuf in PSRAM once — persists for the whole map session.
        // 8192 entries covers Z16/Z17 dense index rows; frees ~24-32 KB DRAM
        // that the old static array consumed.
        if (!npkRowBuf) {
            npkRowBuf    = (UIMapManager::Npk2IndexEntry*)
                            heap_caps_malloc(8192 * sizeof(UIMapManager::Npk2IndexEntry),
                                             MALLOC_CAP_SPIRAM);
            npkRowBufCap = npkRowBuf ? 8192 : 0;
            ESP_LOGI(TAG, "npkRowBuf: %u entries in %s",
                          npkRowBufCap, npkRowBuf ? "PSRAM" : "FAILED (fallback to on-disk search)");
        }
    }

    void clearTileCache() {
        // Defer if render is active or queued on Core 0
        if (isRenderActive()) {
            deferredClearRequested = true;
            ESP_LOGW(TAG, "clearTileCache deferred (render active/pending)");
            return;
        }
        if (renderLock && xSemaphoreTake(renderLock, 0) != pdTRUE) {
            deferredClearRequested = true;
            ESP_LOGW(TAG, "clearTileCache deferred (render lock held)");
            return;
        }
        clearNavCache();
        
        // Invalidate all raster tiles instead of deleting them
        if (_isRasterCacheInitialized) {
            for (auto& cachedTile : _tileCache) {
                cachedTile.isValid = false;
            }
            _rasterCacheAccessCounter = 0;
            ESP_LOGD(TAG, "Raster tile cache invalidated.");
        }

        if (renderLock) xSemaphoreGive(renderLock);
    }

    // Shrink projection buffers to baseline capacity to prevent memory bloat
    void shrinkProjectionBuffers() {
        const size_t BASELINE_CAPACITY = 1024;
        if (proj32X.capacity() > BASELINE_CAPACITY * 2) {
            proj32X.clear();
            proj32X.shrink_to_fit();
            proj32X.reserve(BASELINE_CAPACITY);
        }
        if (proj32Y.capacity() > BASELINE_CAPACITY * 2) {
            proj32Y.clear();
            proj32Y.shrink_to_fit();
            proj32Y.reserve(BASELINE_CAPACITY);
        }
        const size_t DECODED_BASELINE = 4096;
        if (decodedCoords.capacity() > DECODED_BASELINE * 2) {
            decodedCoords.clear();
            decodedCoords.shrink_to_fit();
            decodedCoords.reserve(DECODED_BASELINE);
        }
    }

    // --- NAV raw data cache functions ---

    int findNavCache(uint8_t regionIdx, uint8_t zoom, int tileX, int tileY) {
        for (int i = 0; i < (int)navCache.size(); i++) {
            if (navCache[i].regionIdx == regionIdx && navCache[i].zoom == zoom &&
                navCache[i].tileX == tileX && navCache[i].tileY == tileY) return i;
        }
        return -1;
    }

    // Add a tile to the NAV cache — takes ownership of the data pointer
    void addNavCache(uint8_t regionIdx, uint8_t zoom, int tileX, int tileY,
                            uint8_t* data, size_t size,
                            const std::vector<uint8_t*>* inUse) {
        NavCacheEntry entry;
        entry.data = data;
        entry.size = size;
        entry.tileX = tileX;
        entry.tileY = tileY;
        entry.regionIdx = regionIdx;
        entry.zoom = zoom;
        entry.lastAccess = ++navCacheAccessCounter;

        if ((int)navCache.size() < NAV_CACHE_SIZE) {
            navCache.push_back(entry);
            return;
        }
        // Evict LRU entry, skipping any that are currently in use
        int lruIdx = -1;
        uint32_t lruMin = UINT32_MAX;
        for (int i = 0; i < (int)navCache.size(); i++) {
            if (navCache[i].lastAccess < lruMin) {
                if (inUse) {
                    bool used = false;
                    for (const auto* p : *inUse) {
                        if (p == navCache[i].data) { used = true; break; }
                    }
                    if (used) continue;
                }
                lruMin = navCache[i].lastAccess;
                lruIdx = i;
            }
        }
        if (lruIdx >= 0) {
            if (isNavPoolActive()) releaseNavSlot(navCache[lruIdx].data);
            else free(navCache[lruIdx].data);
            navCache[lruIdx] = entry;
        } else {
            // All entries in use — grow beyond NAV_CACHE_SIZE to avoid data loss
            navCache.push_back(entry);
        }
    }

    // Returns the number of available NAV pool slots (or 0 if not active)
    int getAvailableNavSlots() {
        if (!_navPoolActive) return 0;
        int count = 0;
        for (int i = 0; i < NAV_POOL_MAX_SLOTS; ++i) {
            if (_navPool[i] && !_navPoolInUse[i]) count++;
        }
        return count;
    }

    // Evict LRU navCache entries NOT referenced by inUse until largest free PSRAM
    // block is >= needed bytes (or a NAV slot is available). Returns true if enough PSRAM/slots were freed.
    bool evictUnusedNavCache(const std::vector<uint8_t*>& inUse, size_t needed) {
        int evicted = 0;
        while ((isNavPoolActive() ? (getAvailableNavSlots() == 0) : (heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) < needed))) {
            int lruIdx = -1;
            uint32_t lruMin = UINT32_MAX;
            for (int i = 0; i < (int)navCache.size(); i++) {
                if (navCache[i].lastAccess < lruMin) {
                    bool used = false;
                    for (const auto* p : inUse) {
                        if (p == navCache[i].data) { used = true; break; }
                    }
                    if (!used) {
                        lruMin = navCache[i].lastAccess;
                        lruIdx = i;
                    }
                }
            }
            if (lruIdx < 0) break;  // Nothing left to evict
            if (isNavPoolActive()) releaseNavSlot(navCache[lruIdx].data);
            else free(navCache[lruIdx].data);
            navCache.erase(navCache.begin() + lruIdx);
            evicted++;
        }
        if (evicted > 0) {
            ESP_LOGD(TAG, "Evicted %d unused entries, largest block: %u KB",
                          evicted, (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024));
        }
        return heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) >= needed;
    }

    static void clearNavCache() {
        invalidateAllIdxRowCache();
        closeAllNpkSlots();
        for (auto& e : navCache) {
            if (isNavPoolActive()) releaseNavSlot(e.data);
            else free(e.data);
        }
        navCache.clear();
        navCacheAccessCounter = 0;
        ESP_LOGD(TAG, "NAV cache cleared");
    }

    // --- NPK2 pack file functions (multi-region slot system) ---

    static void closeNpkSlot(int idx) {
        invalidateIdxRowCacheForSlot(idx);
        NpkSlot& s = npkSlots[idx];
        if (s.yTable) {
            heap_caps_free(s.yTable);
            s.yTable = nullptr;
        }
        if (s.file) {
            s.file.close();
        }
        memset(&s.header, 0, sizeof(s.header));
        s.region[0] = '\0';
        s.zoom = 255;
        s.splitIdx = 0;
        s.active = false;
        s.lastAccess = 0;
    }

    static void closeAllNpkSlots() {
        for (int i = 0; i < NPK_MAX_REGIONS; i++) {
            closeNpkSlot(i);
        }
        npkAccessCounter = 0;
    }

    // Helper: allocate a slot (free or LRU-evicted), returns slot index
    static int allocNpkSlot(const char* region, uint8_t zoom) {
        // Find a free slot
        int slotIdx = -1;
        for (int i = 0; i < NPK_MAX_REGIONS; i++) {
            if (!npkSlots[i].active) {
                slotIdx = i;
                break;
            }
        }
        // No free slot → evict LRU
        if (slotIdx < 0) {
            slotIdx = 0;
            uint32_t oldest = npkSlots[0].lastAccess;
            for (int i = 1; i < NPK_MAX_REGIONS; i++) {
                if (npkSlots[i].lastAccess < oldest) {
                    oldest = npkSlots[i].lastAccess;
                    slotIdx = i;
                }
            }
            ESP_LOGD(TAG, "Evicting slot %d (%s/Z%d split%d) for %s/Z%d",
                          slotIdx, npkSlots[slotIdx].region, npkSlots[slotIdx].zoom,
                          npkSlots[slotIdx].splitIdx, region, zoom);
            closeNpkSlot(slotIdx);
        }
        return slotIdx;
    }

    // Helper: open an NPK2 file into a slot, read header + Y-table. Returns slot pointer.
    // Holds spiMutex for the entire open+read sequence to prevent SPI bus contention
    // with display flush on Core 1 during async rendering.
    static NpkSlot* openNpkFile(int slotIdx, const char* packPath, const char* region, uint8_t zoom, uint8_t splitIdx) {
        NpkSlot& s = npkSlots[slotIdx];

        // Mark slot as active (even on failure) to prevent repeated open attempts
        auto markSlot = [&]() {
            strncpy(s.region, region, sizeof(s.region) - 1);
            s.region[sizeof(s.region) - 1] = '\0';
            s.zoom = zoom;
            s.splitIdx = splitIdx;
            s.active = true;
            s.lastAccess = ++npkAccessCounter;
        };

        if (spiMutex == NULL || xSemaphoreTakeRecursive(spiMutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
            ESP_LOGE(TAG, "openNpkFile: failed to acquire spiMutex for %s", packPath);
            markSlot();
            return &s;
        }

        // All SD I/O within a single spiMutex hold
        s.file = SD.open(packPath, FILE_READ);
        if (!s.file) {
            xSemaphoreGiveRecursive(spiMutex);
            markSlot();
            return &s;
        }

        // Read NPK2 header (25 bytes)
        if (s.file.read((uint8_t*)&s.header, sizeof(s.header)) != sizeof(s.header) ||
            memcmp(s.header.magic, "NPK2", 4) != 0) {
            ESP_LOGE(TAG, "Invalid magic in %s (expected NPK2)", packPath);
            s.file.close();
            xSemaphoreGiveRecursive(spiMutex);
            markSlot();
            return &s;
        }

        if (s.header.tile_count == 0 || s.header.y_max < s.header.y_min) {
            ESP_LOGE(TAG, "Invalid header in %s (tiles=%u, y_min=%u, y_max=%u)",
                          packPath, s.header.tile_count, s.header.y_min, s.header.y_max);
            s.file.close();
            xSemaphoreGiveRecursive(spiMutex);
            markSlot();
            return &s;
        }

        // Load Y-table into PSRAM
        uint32_t ySpan = s.header.y_max - s.header.y_min + 1;
        size_t ytableSize = ySpan * sizeof(UIMapManager::Npk2YEntry);
        s.yTable = (UIMapManager::Npk2YEntry*)heap_caps_malloc(ytableSize, MALLOC_CAP_SPIRAM);
        if (!s.yTable) {
            ESP_LOGE(TAG, "Failed to alloc Y-table (%u bytes)", (unsigned)ytableSize);
            s.file.close();
            xSemaphoreGiveRecursive(spiMutex);
            markSlot();
            return &s;
        }

        s.file.seek(s.header.ytable_offset);
        if (s.file.read((uint8_t*)s.yTable, ytableSize) != ytableSize) {
            ESP_LOGE(TAG, "Failed to read Y-table from %s", packPath);
            heap_caps_free(s.yTable);
            s.yTable = nullptr;
            s.file.close();
            xSemaphoreGiveRecursive(spiMutex);
            markSlot();
            return &s;
        }

        xSemaphoreGiveRecursive(spiMutex);
        markSlot();
        ESP_LOGI(TAG, "Opened pack: %s (%u tiles, Y %u-%u, Y-table %u bytes)",
                      packPath, s.header.tile_count, s.header.y_min, s.header.y_max, (unsigned)ytableSize);
        return &s;
    }

    // Open (or reuse) an NPK pack for a given region+zoom+tileY. Returns slot pointer or nullptr.
    // tileY is used to select the correct split when the pack is split into multiple files.
    NpkSlot* openNpkRegion(const char* region, uint8_t zoom, uint32_t tileY) {
        // 1. Check for existing slot with same region+zoom that covers tileY
        for (int i = 0; i < NPK_MAX_REGIONS; i++) {
            if (npkSlots[i].active && npkSlots[i].zoom == zoom &&
                strcmp(npkSlots[i].region, region) == 0) {
                // Only return slots with valid yTable — failed opens must be retried
                if (npkSlots[i].yTable &&
                    tileY >= npkSlots[i].header.y_min && tileY <= npkSlots[i].header.y_max) {
                    npkSlots[i].lastAccess = ++npkAccessCounter;
                    return &npkSlots[i];
                }
            }
        }

        // 2. Allocate a slot
        int slotIdx = allocNpkSlot(region, zoom);

        // 3. Try single file first: Z{z}.nav
        char packPath[128];
        snprintf(packPath, sizeof(packPath), "/LoRa_Tracker/VectMaps/%s/Z%d.nav", region, zoom);

        bool singleExists = false;
        if (spiMutex != NULL && xSemaphoreTakeRecursive(spiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            singleExists = SD.exists(packPath);
            xSemaphoreGiveRecursive(spiMutex);
        }

        if (singleExists) {
            return openNpkFile(slotIdx, packPath, region, zoom, 0);
        }

        // 4. Single file absent → scan splits: Z{z}_0.nav, Z{z}_1.nav, ...
        for (uint8_t si = 0; si < 16; si++) {
            snprintf(packPath, sizeof(packPath), "/LoRa_Tracker/VectMaps/%s/Z%d_%d.nav", region, zoom, si);

            bool splitExists = false;
            if (spiMutex != NULL && xSemaphoreTakeRecursive(spiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                splitExists = SD.exists(packPath);
                xSemaphoreGiveRecursive(spiMutex);
            }

            if (!splitExists) break;  // No more splits

            // Read header to check Y-range without fully opening
            UIMapManager::Npk2Header splitHeader;
            bool headerOk = false;
            if (spiMutex != NULL && xSemaphoreTakeRecursive(spiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                File f = SD.open(packPath, FILE_READ);
                if (f) {
                    headerOk = (f.read((uint8_t*)&splitHeader, sizeof(splitHeader)) == sizeof(splitHeader) &&
                                memcmp(splitHeader.magic, "NPK2", 4) == 0);
                    f.close();
                }
                xSemaphoreGiveRecursive(spiMutex);
            }

            if (!headerOk) continue;

            if (tileY >= splitHeader.y_min && tileY <= splitHeader.y_max) {
                ESP_LOGD(TAG, "Split Z%d_%d.nav covers tileY=%u (Y %u-%u)",
                              zoom, si, tileY, splitHeader.y_min, splitHeader.y_max);
                return openNpkFile(slotIdx, packPath, region, zoom, si);
            }
        }

        // 5. No file/split covers tileY — mark slot but leave yTable null
        //    Step 1 will skip this slot (yTable check), allowing retry next render
        ESP_LOGW(TAG, "No NAV pack for %s Z%d tileY=%u", region, zoom, tileY);
        NpkSlot& s = npkSlots[slotIdx];
        strncpy(s.region, region, sizeof(s.region) - 1);
        s.region[sizeof(s.region) - 1] = '\0';
        s.zoom = zoom;
        s.splitIdx = 0;
        s.active = true;
        s.lastAccess = ++npkAccessCounter;
        return &s;
    }


    // --- Index row cache in PSRAM (avoids re-reading the same Y-row from SD) ---
    struct IndexRowCache {
        UIMapManager::Npk2IndexEntry* entries;  // ps_malloc'd
        uint32_t count;
        uint8_t  slotIdx;     // which NpkSlot (0..NPK_MAX_REGIONS-1)
        uint32_t rowY;
        bool     valid;
    };
    #define IDX_ROW_CACHE_SIZE 12  // 6 Y values × 2 regions max
    static IndexRowCache idxRowCache[IDX_ROW_CACHE_SIZE];
    static int idxRowCacheNext = 0;  // round-robin insertion

    static void invalidateIdxRowCacheForSlot(int slotIdx) {
        for (int i = 0; i < IDX_ROW_CACHE_SIZE; i++) {
            if (idxRowCache[i].valid && idxRowCache[i].slotIdx == (uint8_t)slotIdx) {
                heap_caps_free(idxRowCache[i].entries);
                idxRowCache[i].entries = nullptr;
                idxRowCache[i].valid = false;
            }
        }
    }

    static void invalidateAllIdxRowCache() {
        for (int i = 0; i < IDX_ROW_CACHE_SIZE; i++) {
            if (idxRowCache[i].valid) {
                heap_caps_free(idxRowCache[i].entries);
                idxRowCache[i].entries = nullptr;
                idxRowCache[i].valid = false;
            }
        }
        idxRowCacheNext = 0;
    }

    // Find a tile in a single NPK slot (binary search in Y-table + index)
    // Uses index row cache to avoid redundant SD reads for the same Y-row.
    bool findNpkTileInSlot(NpkSlot* slot, uint32_t x, uint32_t y,
                                   UIMapManager::Npk2IndexEntry* result) {
        if (!slot || !slot->yTable || !slot->file) return false;

        // Check Y range
        if (y < slot->header.y_min || y > slot->header.y_max) return false;

        const UIMapManager::Npk2YEntry& row = slot->yTable[y - slot->header.y_min];
        if (row.idx_count == 0) return false;

        // Determine slot index for cache key
        int slotIdx = (int)(slot - npkSlots);

        // --- Check index row cache ---
        for (int i = 0; i < IDX_ROW_CACHE_SIZE; i++) {
            if (idxRowCache[i].valid && idxRowCache[i].slotIdx == (uint8_t)slotIdx &&
                idxRowCache[i].rowY == y) {
                // Cache hit — binary search in cached entries
                UIMapManager::Npk2IndexEntry* entries = idxRowCache[i].entries;
                int lo = 0, hi = (int)idxRowCache[i].count - 1;
                while (lo <= hi) {
                    int mid = (lo + hi) / 2;
                    if (entries[mid].x < x) lo = mid + 1;
                    else if (entries[mid].x > x) hi = mid - 1;
                    else { *result = entries[mid]; return true; }
                }
                return false;
            }
        }

        // --- Cache miss — read from SD ---
        uint32_t baseOff = slot->header.index_offset + row.idx_start * sizeof(UIMapManager::Npk2IndexEntry);
        bool found = false;

        if (npkRowBuf && row.idx_count <= npkRowBufCap) {
            // Fast path: read entire row in one SD read, binary search in RAM
            size_t readSize = row.idx_count * sizeof(UIMapManager::Npk2IndexEntry);
            bool readOk = false;
            if (spiMutex != NULL && xSemaphoreTakeRecursive(spiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                slot->file.seek(baseOff);
                // Use DMA-chunked read to reduce SPI transaction overhead
                readOk = (STORAGE_Utils::readChunked(slot->file, (uint8_t*)npkRowBuf, readSize) == readSize);
                xSemaphoreGiveRecursive(spiMutex);
            }
            if (!readOk) return false;

            // Try to cache this row in PSRAM for reuse
            size_t allocSize = row.idx_count * sizeof(UIMapManager::Npk2IndexEntry);
            UIMapManager::Npk2IndexEntry* cached =
                (UIMapManager::Npk2IndexEntry*)heap_caps_malloc(allocSize, MALLOC_CAP_SPIRAM);
            if (cached) {
                memcpy(cached, npkRowBuf, allocSize);
                // Evict existing entry at round-robin position
                IndexRowCache& slot_c = idxRowCache[idxRowCacheNext];
                if (slot_c.valid) {
                    heap_caps_free(slot_c.entries);
                }
                slot_c.entries = cached;
                slot_c.count = row.idx_count;
                slot_c.slotIdx = (uint8_t)slotIdx;
                slot_c.rowY = y;
                slot_c.valid = true;
                idxRowCacheNext = (idxRowCacheNext + 1) % IDX_ROW_CACHE_SIZE;
            }

            // Binary search in npkRowBuf
            int lo = 0, hi = (int)row.idx_count - 1;
            while (lo <= hi) {
                int mid = (lo + hi) / 2;
                if (npkRowBuf[mid].x < x) lo = mid + 1;
                else if (npkRowBuf[mid].x > x) hi = mid - 1;
                else { *result = npkRowBuf[mid]; return true; }
            }
        } else {
            // Fallback: binary search on disk for very large rows
            if (spiMutex != NULL && xSemaphoreTakeRecursive(spiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                int lo = 0, hi = (int)row.idx_count - 1;
                UIMapManager::Npk2IndexEntry e;
                while (lo <= hi) {
                    int mid = (lo + hi) / 2;
                    slot->file.seek(baseOff + mid * sizeof(UIMapManager::Npk2IndexEntry));
                    if (slot->file.read((uint8_t*)&e, sizeof(e)) != sizeof(e)) break;
                    if (e.x < x) lo = mid + 1;
                    else if (e.x > x) hi = mid - 1;
                    else { *result = e; found = true; break; }
                }
                xSemaphoreGiveRecursive(spiMutex);
            }
        }
        return found;
    }

    bool readNpkTileData(NpkSlot* slot, const UIMapManager::Npk2IndexEntry* entry,
                                 uint8_t** outData, size_t* outSize) {
        if (!slot || !slot->file || !entry) return false;

        if (isNavPoolActive()) {
            *outData = acquireNavSlot(entry->size);
            if (!*outData) {
                // Pool exhausted or tile too large — fallback to ps_malloc
                *outData = (uint8_t*)ps_malloc(entry->size);
                if (*outData) {
                    ESP_LOGW(TAG, "NAV pool fallback: ps_malloc %d bytes (slots full or tile > %dKB)",
                             (int)entry->size, NAV_POOL_SLOT_SIZE / 1024);
                }
            }
        } else {
            *outData = (uint8_t*)ps_malloc(entry->size);
        }
        if (!*outData) return false;

        bool ok = false;
        if (spiMutex != NULL && xSemaphoreTakeRecursive(spiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            slot->file.seek(entry->offset);
            // Use DMA-chunked read to reduce SPI transaction overhead for large tile data
            ok = (STORAGE_Utils::readChunked(slot->file, *outData, entry->size) == entry->size);
            xSemaphoreGiveRecursive(spiMutex);
        }

        if (!ok) {
            releaseNavSlot(*outData);  // Handles both pool slots and ps_malloc fallback
            *outData = nullptr;
            return false;
        }
        *outSize = entry->size;
        return true;
    }


    // Load VLW Unicode font for map labels from SD card
    static uint8_t* vlwFontData = nullptr;
    static lgfx::PointerWrapper vlwFontWrapper;  // Must outlive vlwFont (VLWfont keeps _fontData pointer)

    bool loadMapFont() {
        if (vlwFontLoaded) return true;

        // Try 16pt first (better readability on 480×320), fallback to 12pt
        const char* fontPath = "/LoRa_Tracker/fonts/OpenSans-Bold-14.vlw";
        if (!SD.exists(fontPath)) {
            fontPath = "/LoRa_Tracker/fonts/OpenSans-Bold-12.vlw";
        }
        if (!SD.exists(fontPath)) {
            ESP_LOGW(TAG, "VLW font not found: %s (will use fallback GFX font)", fontPath);
            return false;
        }

        File file = SD.open(fontPath, FILE_READ);
        if (!file) {
            ESP_LOGE(TAG, "Failed to open VLW font: %s", fontPath);
            return false;
        }

        size_t fileSize = file.size();
        vlwFontData = (uint8_t*)heap_caps_malloc(fileSize, MALLOC_CAP_SPIRAM);
        if (!vlwFontData) {
            ESP_LOGE(TAG, "Failed to allocate %d bytes in PSRAM for VLW font", fileSize);
            file.close();
            return false;
        }

        size_t bytesRead = file.read(vlwFontData, fileSize);
        file.close();

        if (bytesRead != fileSize) {
            ESP_LOGE(TAG, "Failed to read VLW font: read %d/%d bytes", bytesRead, fileSize);
            heap_caps_free(vlwFontData);
            vlwFontData = nullptr;
            return false;
        }

        vlwFontWrapper.set(vlwFontData, fileSize);
        if (vlwFont.loadFont(&vlwFontWrapper)) {
            vlwFontLoaded = true;
            ESP_LOGI(TAG, "Loaded VLW font: %s (%d bytes in PSRAM)", fontPath, fileSize);
            return true;
        }

        ESP_LOGE(TAG, "VLW font validation failed");
        heap_caps_free(vlwFontData);
        vlwFontData = nullptr;
        return false;
    }

    CachedTile* getRasterCacheSlot(int zoom, int tileX, int tileY) {
        if (!_isRasterCacheInitialized) {
            ESP_LOGE(TAG, "getRasterCacheSlot called before cache is initialized!");
            return nullptr;
        }
    
        uint32_t tileHash = (static_cast<uint32_t>(zoom) << 28) | (static_cast<uint32_t>(tileX) << 14) | static_cast<uint32_t>(tileY);

        // 1. Check for cache hit
        for (auto& tile : _tileCache) {
            if (tile.isValid && tile.tileHash == tileHash) {
                tile.lastAccess = ++_rasterCacheAccessCounter;
                return &tile;
            }
        }

        // 2. Cache miss, find a free slot
        for (auto& tile : _tileCache) {
            if (!tile.isValid) {
                tile.tileHash = tileHash;
                tile.lastAccess = ++_rasterCacheAccessCounter;
                tile.filePath[0] = '\0';
                // isValid is still false, caller must set it to true after loading
                return &tile;
            }
        }

        // 3. Cache full, evict LRU
        auto lruIt = std::min_element(_tileCache.begin(), _tileCache.end(),
            [](const CachedTile& a, const CachedTile& b) {
                return a.lastAccess < b.lastAccess;
        });

        ESP_LOGD(TAG, "Evicting raster tile: %s", lruIt->filePath);
        lruIt->isValid = false; // Mark as invalid for the caller to fill
        lruIt->tileHash = tileHash;
        lruIt->lastAccess = ++_rasterCacheAccessCounter;
        lruIt->filePath[0] = '\0';
        
        return &(*lruIt);
    }

    // Proactively evict LRU items to ensure PSRAM is available.
    // With static raster cache, this can only act on the NAV cache now.
    bool ensurePSRAMAvailable(size_t needed) {
        if (heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) >= needed) {
            return true;
        }
        
        // This function used to evict raster tiles. With a static pool, we can't free that memory.
        // We log a warning that the raster cache is static and cannot be shrunk.
        // The success of a subsequent allocation will depend on what else is in PSRAM (like the NAV cache).
        ESP_LOGW(TAG, "ensurePSRAMAvailable: Raster cache is static. Cannot free %d bytes from it.", needed);
        return heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) >= needed;
    }

    LGFX_Sprite* findCachedRasterTile(int zoom, int tileX, int tileY) {
        if (!_isRasterCacheInitialized) return nullptr;

        uint32_t tileHash = (static_cast<uint32_t>(zoom) << 28) | (static_cast<uint32_t>(tileX) << 14) | static_cast<uint32_t>(tileY);

        for (auto& tile : _tileCache) {
            if (tile.isValid && tile.tileHash == tileHash) {
                tile.lastAccess = ++_rasterCacheAccessCounter;
                return tile.sprite;
            }
        }
        return nullptr;
    }
    
    // Helper function to safely copy a sprite to the canvas with clipping
    void copySpriteToCanvasWithClip(lv_obj_t* canvas, LGFX_Sprite* sprite, int offsetX, int offsetY) {
        if (!canvas || !sprite || spriteMutex == NULL) return;

        if (xSemaphoreTake(spriteMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (sprite->getBuffer() == nullptr) {
                xSemaphoreGive(spriteMutex);
                return;
            }

            int src_x = 0;
            int src_y = 0;
            int dest_x = offsetX;
            int dest_y = offsetY;
            int copy_w = MAP_TILE_SIZE;
            int copy_h = MAP_TILE_SIZE;

            if (dest_x < 0) {
                src_x = -dest_x;
                copy_w += dest_x;
                dest_x = 0;
            }
            if (dest_y < 0) {
                src_y = -dest_y;
                copy_h += dest_y;
                dest_y = 0;
            }
            if (dest_x + copy_w > MAP_SPRITE_SIZE) {
                copy_w = MAP_SPRITE_SIZE - dest_x;
            }
            if (dest_y + copy_h > MAP_SPRITE_SIZE) {
                copy_h = MAP_SPRITE_SIZE - dest_y;
            }

            if (copy_w > 0 && copy_h > 0) {
                uint16_t* src_buf = (uint16_t*)sprite->getBuffer();
                lv_color_t* dest_buf = MapState::map_canvas_buf;

                for (int y = 0; y < copy_h; y++) {
                    uint16_t* src_ptr = src_buf + ((src_y + y) * MAP_TILE_SIZE) + src_x;
                    lv_color_t* dest_ptr = dest_buf + ((dest_y + y) * MAP_SPRITE_SIZE) + dest_x;
#if LV_COLOR_16_SWAP
                    // LGFX sprites are little-endian RGB565, LVGL canvas is big-endian
                    uint16_t* dp = (uint16_t*)dest_ptr;
                    for (int x = 0; x < copy_w; x++) {
                        uint16_t px = src_ptr[x];
                        dp[x] = (px >> 8) | (px << 8);
                    }
#else
                    memcpy(dest_ptr, src_ptr, copy_w * sizeof(lv_color_t));
#endif
                }
            }
            xSemaphoreGive(spriteMutex);
        }
    }

    // =========================================================================
    // Raster viewport compositing (runs on Core 0)
    // Loads 3×3 tile grid from cache/SD, composites into target sprite.
    // Same pattern as renderNavViewport but for PNG/JPG tiles.
    // =========================================================================
    bool renderRasterViewport(float centerLat, float centerLon, uint8_t zoom,
                              LGFX_Sprite &map, const char* region) {
        if (!region || !region[0]) return false;

        // Acquire render lock (same as NAV) — prevents clearTileCache during render
        if (renderLock) xSemaphoreTake(renderLock, portMAX_DELAY);
        renderActive_ = true;

        uint64_t startTime = esp_timer_get_time();
        int viewportW = map.width();
        int viewportH = map.height();

        // Calculate center tile and sub-tile offset (same as redraw_map_canvas)
        int n = 1 << zoom;
        float tileXf = (centerLon + 180.0f) / 360.0f * n;
        float latRad = centerLat * PI / 180.0f;
        float tileYf = (1.0f - log(tan(latRad) + 1.0f / cos(latRad)) / PI) / 2.0f * n;

        int centerTileX = (int)tileXf;
        int centerTileY = (int)tileYf;
        int subTileOffsetX = (int)((tileXf - centerTileX) * MAP_TILE_SIZE);
        int subTileOffsetY = (int)((tileYf - centerTileY) * MAP_TILE_SIZE);

        // Fill background (dark teal — same as raster canvas clear)
        map.fillSprite(map.color565(0x2F, 0x4F, 0x4F));

        int tilesLoaded = 0;

        // Direct sprite-to-sprite buffer copy with clipping (bypasses LGFX pipeline)
        auto blitTileToViewport = [&](LGFX_Sprite* src, int dstX, int dstY) {
            int sx = 0, sy = 0;
            int w = MAP_TILE_SIZE, h = MAP_TILE_SIZE;
            if (dstX < 0) { sx = -dstX; w += dstX; dstX = 0; }
            if (dstY < 0) { sy = -dstY; h += dstY; dstY = 0; }
            if (dstX + w > viewportW) w = viewportW - dstX;
            if (dstY + h > viewportH) h = viewportH - dstY;
            if (w <= 0 || h <= 0) return;
            uint16_t* srcBuf = (uint16_t*)src->getBuffer();
            uint16_t* dstBuf = (uint16_t*)map.getBuffer();
            for (int y = 0; y < h; y++) {
                memcpy(dstBuf + (dstY + y) * viewportW + dstX,
                       srcBuf + (sy + y) * MAP_TILE_SIZE + sx,
                       w * sizeof(uint16_t));
            }
        };

        // Load 3×3 grid around center tile
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                esp_task_wdt_reset();
                vTaskDelay(1);  // Yield Core 0 for WiFi/BLE between tiles

                int tileX = centerTileX + dx;
                int tileY = centerTileY + dy;
                int offsetX = viewportW / 2 - subTileOffsetX + dx * MAP_TILE_SIZE;
                int offsetY = viewportH / 2 - subTileOffsetY + dy * MAP_TILE_SIZE;

                // Skip tiles completely outside viewport
                if (offsetX + MAP_TILE_SIZE <= 0 || offsetX >= viewportW) continue;
                if (offsetY + MAP_TILE_SIZE <= 0 || offsetY >= viewportH) continue;

                MapEngine::CachedTile* cacheSlot = getRasterCacheSlot(zoom, tileX, tileY);
                if (!cacheSlot) continue;

                // If slot is valid, it's a cache hit.
                if (cacheSlot->isValid) {
                    blitTileToViewport(cacheSlot->sprite, offsetX, offsetY);
                    tilesLoaded++;
                    continue;
                }

                // --- Cache miss, load from SD ---
                char path[128];
                bool found = false;

                if (spiMutex && xSemaphoreTake(spiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                    if (STORAGE_Utils::isSDAvailable()) {
                        snprintf(path, sizeof(path), "/LoRa_Tracker/Maps/%s/%d/%d/%d.png",
                                 region, zoom, tileX, tileY);
                        if (SD.exists(path)) { found = true; }
                        else {
                            snprintf(path, sizeof(path), "/LoRa_Tracker/Maps/%s/%d/%d/%d.jpg",
                                     region, zoom, tileX, tileY);
                            if (SD.exists(path)) { found = true; }
                        }
                    }
                    xSemaphoreGive(spiMutex);
                }

                if (!found) continue;

                // Decode tile into the sprite provided by the cache slot
                LGFX_Sprite* tileSprite = cacheSlot->sprite;
                bool decoded = false;
                if (xSemaphoreTake(spriteMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
                    decoded = renderTile(path, 0, 0, *tileSprite, (uint8_t)zoom);
                    xSemaphoreGive(spriteMutex);
                }

                if (decoded) {
                    blitTileToViewport(tileSprite, offsetX, offsetY);
                    // Validate the slot
                    strncpy(cacheSlot->filePath, path, sizeof(cacheSlot->filePath) - 1);
                    cacheSlot->filePath[sizeof(cacheSlot->filePath) - 1] = '\0';
                    cacheSlot->isValid = true;
                    tilesLoaded++;
                } else {
                    cacheSlot->isValid = false; // Ensure it's marked as invalid on failure
                }
            }
        }

        uint64_t endTime = esp_timer_get_time();
        ESP_LOGD(TAG, "Raster viewport: %llu ms, %d tiles loaded, Z%d",
                      (endTime - startTime) / 1000, tilesLoaded, zoom);

        // Release render lock
        renderActive_ = false;
        if (renderLock) xSemaphoreGive(renderLock);

        return tilesLoaded > 0;
    }

    // renderNavViewport + fillPolygonGeneral → map_nav_render.cpp

    // Public render dispatcher (raster only — NAV uses renderNavViewport)
    bool renderTile(const char* path, int16_t xOffset, int16_t yOffset, LGFX_Sprite &map, uint8_t zoom) {
        String pathStr(path);
        if (pathStr.endsWith(".png")) {
            return renderPNGRaster(path, map);
        } else if (pathStr.endsWith(".jpg")) {
            return renderJPGRaster(path, map);
        }
        return false;
    }

}
#endif
