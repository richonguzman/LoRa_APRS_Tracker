/* Map rendering engine for T-Deck Plus
 * Handles vector tile parsing, rendering, and caching.
 */
#ifdef USE_LVGL_UI

#include "map_engine.h"
#include "ui_map_manager.h"
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

    // VLW Unicode font for map labels
    static lgfx::VLWfont vlwFont;
    static bool vlwFontLoaded = false;

    // Handles for the asynchronous rendering system
    QueueHandle_t mapRenderQueue = nullptr;
    SemaphoreHandle_t spriteMutex = nullptr;
    static TaskHandle_t mapRenderTaskHandle = nullptr;
    static lv_obj_t* canvas_to_invalidate_ = nullptr;

    // Render lock: held for the entire renderNavViewport() duration.
    // clearTileCache/closeAllNpkSlots defer when this is held.
    SemaphoreHandle_t renderLock = nullptr;
    static volatile bool renderActive_ = false;
    static volatile bool renderPending_ = false;   // Request queued, not yet started
    static volatile bool deferredClearRequested = false;

    bool isRenderActive() { return renderActive_ || renderPending_; }

    // Async NAV rendering
    EventGroupHandle_t mapEventGroup = nullptr;
    QueueHandle_t navRenderQueue = nullptr;
    volatile int lastRenderedTileX = 0;
    volatile int lastRenderedTileY = 0;
    volatile uint8_t lastRenderedZoom = 0;

    // Static vectors for AEL polygon filler (PSRAM to preserve DRAM)
    static std::vector<UIMapManager::Edge, PSRAMAllocator<UIMapManager::Edge>> edgePool;
    static std::vector<int, PSRAMAllocator<int>> edgeBuckets;

    // Static vectors for coordinate projection (PSRAM, pre-reserved)
    // Use separate int16_t buffers for lines and int buffers for polygons
    static std::vector<int16_t, PSRAMAllocator<int16_t>> proj16X;
    static std::vector<int16_t, PSRAMAllocator<int16_t>> proj16Y;
    static std::vector<int, PSRAMAllocator<int>> proj32X;
    static std::vector<int, PSRAMAllocator<int>> proj32Y;

    // Decoded coords buffer for Delta+ZigZag+VarInt features (PSRAM, reused per feature)
    static std::vector<int16_t, PSRAMAllocator<int16_t>> decodedCoords;

    // npkRowBuf: PSRAM buffer for full index-row reads (allocated in initTileCache).
    // Declared here (before initTileCache) so the function can see the symbol.
    // Capacity 8192 covers Z16/Z17 dense rows; avoids on-disk binary-search fallback.
    static UIMapManager::Npk2IndexEntry* npkRowBuf    = nullptr;
    static uint32_t                      npkRowBufCap = 0;

    // Waterway label buffers (PSRAM, allocated/freed with render task)
    static constexpr int WLABEL_MAX_PTS = 256;
    static int*   wlScreenX = nullptr;   // [WLABEL_MAX_PTS]
    static int*   wlScreenY = nullptr;   // [WLABEL_MAX_PTS]
    static float* wlArcLen  = nullptr;   // [WLABEL_MAX_PTS]

    // Reusable glyph sprite for curvilinear labels (avoids alloc/free per character)
    static LGFX_Sprite* glyphSprite = nullptr;
    static int glyphSpriteW = 0, glyphSpriteH = 0;

    // --- VarInt decoding (protobuf-style LEB128) ---
    static uint32_t readVarInt(const uint8_t* buf, uint32_t& offset, uint32_t limit) {
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

    static inline int16_t zigzagDecode(uint32_t n) {
        return (int16_t)((n >> 1) ^ -(int32_t)(n & 1));
    }

    // Result of decoding a feature's VarInt payload
    struct DecodedFeature {
        uint32_t coordsIdx;      // Start index in decodedCoords[]
        uint16_t ringCount;
        uint16_t* ringEnds;      // Pointer into tile buffer (navCache), NOT decodedCoords
    };

    // Decode Delta+ZigZag+VarInt coords into decodedCoords[].
    // For polygons: reads ringCount + ringEnds from end of payload.
    // Returns false if data is truncated.
    static bool decodeFeatureCoords(const uint8_t* payload, uint16_t coordCount,
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

    // Geometry type constants
    static constexpr uint8_t GEOM_POINT      = 1;
    static constexpr uint8_t GEOM_LINE       = 2;
    static constexpr uint8_t GEOM_POLYGON    = 3;
    static constexpr uint8_t GEOM_TEXT       = 4;
    static constexpr uint8_t GEOM_TEXT_LINE  = 5;  // Curvilinear waterway label

    // Feature reference for zero-copy rendering (pointer into tile buffer)
    struct FeatureRef {
        uint8_t* ptr;           // Pointer to feature header in data buffer
        uint8_t geomType;       // 1=Point, 2=Line, 3=Polygon, 4=Text, 5=TextLine
        uint16_t payloadSize;   // Total payload size in bytes
        uint16_t coordCount;    // Number of coordinates
        int16_t tileOffsetX;    // Pixel offset of tile top-left in viewport
        int16_t tileOffsetY;    // Pixel offset of tile top-left in viewport
    };
    // 16 priority layers (dispatch by getPriority() low nibble)
    static std::vector<FeatureRef, PSRAMAllocator<FeatureRef>> globalLayers[16];

    // --- RASTER TILE CACHE (STATIC POOL) ---
    static LGFX_Sprite* _tileSpritePool[RASTER_TILE_CACHE_SIZE] = {nullptr};
    static std::vector<CachedTile> _tileCache;
    static bool _isRasterCacheInitialized = false;
    static uint32_t _rasterCacheAccessCounter = 0;

    // --- NPK2 pack file support — multi-region slot system (max 8 open packs) ---
    // 8 slots: supports 2 splits × 3 regions + margin
    #define NPK_MAX_REGIONS 8

    struct NpkSlot {
        File file;
        UIMapManager::Npk2Header header;
        UIMapManager::Npk2YEntry* yTable = nullptr;
        char region[64] = {};
        uint8_t zoom = 255;
        uint8_t splitIdx = 0;   // 0 = single file or split #0
        bool active = false;
        uint32_t lastAccess = 0;
    };

    static NpkSlot npkSlots[NPK_MAX_REGIONS];
    static uint32_t npkAccessCounter = 0;

    // NAV raw data cache — avoids re-reading .nav tiles from SD after pan
    struct NavCacheEntry {
        uint8_t* data;        // Raw NAV data (ps_malloc'd)
        size_t   size;        // Size in bytes
        int      tileX;
        int      tileY;
        uint32_t lastAccess;  // LRU counter
        uint8_t  regionIdx;
        uint8_t  zoom;
    };
    #define NAV_CACHE_SIZE 12  // 3×3 grid + multi-region margin
    static std::vector<NavCacheEntry> navCache;
    static uint32_t navCacheAccessCounter = 0;

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
                    ESP_LOGI(TAG, "Async raster render: Z%d (%.4f, %.4f)",
                                  latest.zoom, latest.centerLat, latest.centerLon);
                    renderRasterViewport(latest.centerLat, latest.centerLon, latest.zoom,
                                         *latest.targetSprite, latest.regions[0]);
                } else {
                    ESP_LOGI(TAG, "Async NAV render: Z%d (%.4f, %.4f)",
                                  latest.zoom, latest.centerLat, latest.centerLon);
                    renderNavViewport(latest.centerLat, latest.centerLon, latest.zoom,
                                      *latest.targetSprite, regionPtrs, latest.regionCount);
                }

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

    // Initialize tile cache and pre-reserve render buffers
    void initTileCache(LovyanGFX* gfx) {
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
        proj16X.reserve(1024);
        proj16Y.reserve(1024);
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
            ESP_LOGI(TAG, "Raster tile cache invalidated.");
        }

        if (renderLock) xSemaphoreGive(renderLock);
    }

    // Shrink projection buffers to baseline capacity to prevent memory bloat
    void shrinkProjectionBuffers() {
        const size_t BASELINE_CAPACITY = 1024;
        if (proj16X.capacity() > BASELINE_CAPACITY * 2) {
            proj16X.clear();
            proj16X.shrink_to_fit();
            proj16X.reserve(BASELINE_CAPACITY);
        }
        if (proj16Y.capacity() > BASELINE_CAPACITY * 2) {
            proj16Y.clear();
            proj16Y.shrink_to_fit();
            proj16Y.reserve(BASELINE_CAPACITY);
        }
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

    static int findNavCache(uint8_t regionIdx, uint8_t zoom, int tileX, int tileY) {
        for (int i = 0; i < (int)navCache.size(); i++) {
            if (navCache[i].regionIdx == regionIdx && navCache[i].zoom == zoom &&
                navCache[i].tileX == tileX && navCache[i].tileY == tileY) return i;
        }
        return -1;
    }

    // Add a tile to the NAV cache — takes ownership of the data pointer
    static void addNavCache(uint8_t regionIdx, uint8_t zoom, int tileX, int tileY,
                            uint8_t* data, size_t size,
                            const std::vector<uint8_t*>* inUse = nullptr) {
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
            free(navCache[lruIdx].data);
            navCache[lruIdx] = entry;
        } else {
            // All entries in use — grow beyond NAV_CACHE_SIZE to avoid data loss
            navCache.push_back(entry);
        }
    }

    // Evict LRU navCache entries NOT referenced by inUse until largest free PSRAM
    // block is >= needed bytes. Returns true if enough PSRAM was freed.
    static bool evictUnusedNavCache(const std::vector<uint8_t*>& inUse, size_t needed) {
        int evicted = 0;
        while (heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) < needed) {
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
            free(navCache[lruIdx].data);
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
        for (auto& e : navCache) free(e.data);
        navCache.clear();
        navCacheAccessCounter = 0;
        ESP_LOGI(TAG, "NAV cache cleared");
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
    static NpkSlot* openNpkRegion(const char* region, uint8_t zoom, uint32_t tileY = 0) {
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
    static bool findNpkTileInSlot(NpkSlot* slot, uint32_t x, uint32_t y,
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

    static bool readNpkTileData(NpkSlot* slot, const UIMapManager::Npk2IndexEntry* entry,
                                 uint8_t** outData, size_t* outSize) {
        if (!slot || !slot->file || !entry) return false;

        *outData = (uint8_t*)ps_malloc(entry->size);
        if (!*outData) return false;

        bool ok = false;
        if (spiMutex != NULL && xSemaphoreTakeRecursive(spiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            slot->file.seek(entry->offset);
            // Use DMA-chunked read to reduce SPI transaction overhead for large tile data
            ok = (STORAGE_Utils::readChunked(slot->file, *outData, entry->size) == entry->size);
            xSemaphoreGiveRecursive(spiMutex);
        }

        if (!ok) {
            free(*outData);
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
                lv_color_t* dest_buf = UIMapManager::map_canvas_buf;

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
        ESP_LOGI(TAG, "Raster viewport: %llu ms, %d tiles loaded, Z%d",
                      (endTime - startTime) / 1000, tilesLoaded, zoom);

        // Release render lock
        renderActive_ = false;
        if (renderLock) xSemaphoreGive(renderLock);

        return tilesLoaded > 0;
    }

    static bool fillPolygons = true;

    uint16_t darkenRGB565(const uint16_t color, const float amount) {
        uint16_t r = (color >> 11) & 0x1F;
        uint16_t g = (color >> 5) & 0x3F;
        uint16_t b = color & 0x1F;
        r = (uint16_t)(r * (1.0f - amount));
        g = (uint16_t)(g * (1.0f - amount));
        b = (uint16_t)(b * (1.0f - amount));
        return (r << 11) | (g << 5) | b;
    }

    // AEL polygon filler with fast-forward optimization.
    // Takes high-precision (HP) coordinates (0-4096), iterates pixel-space scanlines (0-255).
    // Supports multi-ring polygons (exterior + holes).
    void fillPolygonGeneral(LGFX_Sprite &map, const int *px_hp, const int *py_hp, const int numPoints, const uint16_t color, const int xOffset, const int yOffset, uint16_t ringCount, uint16_t* ringEnds)
    {
        if (numPoints < 3) return;

        // 1. Find Y bounds in pixel space
        int minY_px = INT_MAX, maxY_px = INT_MIN;
        for (int i = 0; i < numPoints; i++) {
            int y_px = py_hp[i] >> 4;
            if (y_px < minY_px) minY_px = y_px;
            if (y_px > maxY_px) maxY_px = y_px;
        }

        int spriteH = map.height();
        if (maxY_px < 0 || minY_px >= spriteH) return;

        // 2. Set up pixel-space edge buckets
        edgePool.clear();
        int bucketCount = maxY_px - minY_px + 1;
        if (bucketCount <= 0) return;
        edgeBuckets.assign(bucketCount, -1);

        uint16_t count = (ringCount == 0) ? 1 : ringCount;
        uint16_t defaultEnds[1] = { (uint16_t)numPoints };
        uint16_t* ends = (ringEnds == nullptr) ? defaultEnds : ringEnds;

        int ringStart = 0;
        for (uint16_t r = 0; r < count; r++) {
            int ringEnd = ends[r];
            int ringNumPoints = ringEnd - ringStart;
            if (ringNumPoints < 3) {
                ringStart = ringEnd;
                continue;
            }

            // 3. Populate buckets with edges using HP math for precision
            for (int i = 0; i < ringNumPoints; i++) {
                int next = (i + 1) % ringNumPoints;
                int x1_hp = px_hp[ringStart + i], y1_hp = py_hp[ringStart + i];
                int x2_hp = px_hp[ringStart + next], y2_hp = py_hp[ringStart + next];

                int y1_px = y1_hp >> 4;
                int y2_px = y2_hp >> 4;

                if (y1_px == y2_px) continue;

                UIMapManager::Edge e;
                e.nextActive = -1;

                if (y1_hp < y2_hp) {
                    e.yMax = y2_px;
                    e.slope = ((int64_t)(x2_hp - x1_hp) << 16) / (y2_hp - y1_hp);
                    e.xVal = ((int64_t)x1_hp << 16);
                    if (y1_px - minY_px >= 0 && (size_t)(y1_px - minY_px) < edgeBuckets.size()) {
                        e.nextInBucket = edgeBuckets[y1_px - minY_px];
                        edgePool.push_back(e);
                        edgeBuckets[y1_px - minY_px] = edgePool.size() - 1;
                    }
                } else {
                    e.yMax = y1_px;
                    e.slope = ((int64_t)(x1_hp - x2_hp) << 16) / (y1_hp - y2_hp);
                    e.xVal = ((int64_t)x2_hp << 16);
                    if (y2_px - minY_px >= 0 && (size_t)(y2_px - minY_px) < edgeBuckets.size()) {
                        e.nextInBucket = edgeBuckets[y2_px - minY_px];
                        edgePool.push_back(e);
                        edgeBuckets[y2_px - minY_px] = edgePool.size() - 1;
                    }
                }
            }
            ringStart = ringEnd;
        }

        int activeHead = -1;
        int spriteW = map.width();
        // Clip Y range accounting for offset
        int startY_px = std::max(minY_px, -yOffset);
        int endY_px = std::min(maxY_px, spriteH - 1 - yOffset);

        // 4. Fast-forward: process buckets before visible range, jump edge xVal
        // Directly to startY_px (skip invisible scanlines)
        if (startY_px > minY_px) {
            for (int y = minY_px; y < startY_px; y++) {
                if ((size_t)(y - minY_px) >= edgeBuckets.size()) break;
                int eIdx = edgeBuckets[y - minY_px];
                while (eIdx != -1) {
                    int nextIdx = edgePool[eIdx].nextInBucket;
                    // Jump xVal to startY_px: (startY_px - y) pixel scanlines × 16 HP units each
                    edgePool[eIdx].xVal += (int64_t)edgePool[eIdx].slope * 16 * (startY_px - y);
                    edgePool[eIdx].nextActive = activeHead;
                    activeHead = eIdx;
                    eIdx = nextIdx;
                }
            }
            // Remove edges that finish before reaching the visible range
            int* pCurrIdx = &activeHead;
            while (*pCurrIdx != -1) {
                if (edgePool[*pCurrIdx].yMax <= startY_px)
                    *pCurrIdx = edgePool[*pCurrIdx].nextActive;
                else
                    pCurrIdx = &(edgePool[*pCurrIdx].nextActive);
            }
        }

        // 5. Main scanline loop (visible range only)
        int scanlineCount = 0;
        for (int y_px = startY_px; y_px <= endY_px; y_px++) {
            if (++scanlineCount >= 32) {
                esp_task_wdt_reset();
                scanlineCount = 0;
            }

            // Add new edges from bucket
            if ((size_t)(y_px - minY_px) < edgeBuckets.size()) {
                int eIdx = edgeBuckets[y_px - minY_px];
                while (eIdx != -1) {
                    int nextIdx = edgePool[eIdx].nextInBucket;
                    edgePool[eIdx].nextActive = activeHead;
                    activeHead = eIdx;
                    eIdx = nextIdx;
                }
            }

            // Remove finished edges
            int* pCurrIdx = &activeHead;
            while (*pCurrIdx != -1) {
                if (edgePool[*pCurrIdx].yMax <= y_px)
                    *pCurrIdx = edgePool[*pCurrIdx].nextActive;
                else
                    pCurrIdx = &(edgePool[*pCurrIdx].nextActive);
            }

            if (activeHead == -1) continue;

            // Sort AEL by xVal (insertion sort into linked list)
            int sorted = -1;
            int active = activeHead;
            while (active != -1) {
                int nextActive = edgePool[active].nextActive;
                if (sorted == -1 || edgePool[active].xVal < edgePool[sorted].xVal) {
                    edgePool[active].nextActive = sorted;
                    sorted = active;
                } else {
                    int s = sorted;
                    while (edgePool[s].nextActive != -1 && edgePool[edgePool[s].nextActive].xVal < edgePool[active].xVal)
                        s = edgePool[s].nextActive;
                    edgePool[active].nextActive = edgePool[s].nextActive;
                    edgePool[s].nextActive = active;
                }
                active = nextActive;
            }
            activeHead = sorted;

            // Draw horizontal spans
            int left = activeHead;
            while (left != -1 && edgePool[left].nextActive != -1) {
                int right = edgePool[left].nextActive;
                int xStart = (edgePool[left].xVal >> 16) >> 4;
                int xEnd = (edgePool[right].xVal >> 16) >> 4;
                if (xStart < 0) xStart = 0;
                if (xEnd > spriteW) xEnd = spriteW;
                if (xEnd > xStart)
                    map.drawFastHLine(xStart + xOffset, y_px + yOffset, xEnd - xStart, color);
                left = edgePool[right].nextActive;
            }

            // Update xVal for next pixel scanline (step = slope × 16 HP units)
            for (int a = activeHead; a != -1; a = edgePool[a].nextActive)
                edgePool[a].xVal += (int64_t)edgePool[a].slope * 16;
        }
    }

    // =========================================================================
    // Viewport-based NAV rendering.
    // Loads ALL visible tiles, dispatches features to 16 priority layers,
    // renders in a single pass with per-feature setClipRect to tile boundaries.
    // This ensures correct z-ordering across tile boundaries.
    // =========================================================================
    bool renderNavViewport(float centerLat, float centerLon, uint8_t zoom,
                           LGFX_Sprite &map, const char** regions, int regionCount) {
        // Acquire render lock — prevents clearTileCache/closeAllNpkSlots during render
        if (renderLock) xSemaphoreTake(renderLock, portMAX_DELAY);
        renderActive_ = true;

        esp_task_wdt_reset();
        uint64_t startTime = esp_timer_get_time();

        int viewportW = map.width();
        int viewportH = map.height();

        // Compute center tile from lat/lon (Mercator projection)
        const double latRad = (double)centerLat * M_PI / 180.0;
        const double n = pow(2.0, (double)zoom);
        const int centerTileIdxX = (int)floorf((float)((centerLon + 180.0) / 360.0 * n));
        const int centerTileIdxY = (int)floorf((float)((1.0 - log(tan(latRad) + 1.0 / cos(latRad)) / M_PI) / 2.0 * n));

        // Fixed 3×3 grid: tiles at positions {0, 256, 512} in the sprite
        const int8_t gridOffset = MAP_TILES_GRID / 2;  // 1

        // --- Load all tiles and dispatch features ---
        struct ResolvedTile {
            uint8_t* data;
            size_t   size;
            int16_t  tileOffsetX, tileOffsetY;
        };
        static ResolvedTile resolved[9];  // 3×3 fixed grid
        int resolvedCount = 0;
        static std::vector<uint8_t*> inUseData;  // Protects resolved tiles from eviction
        inUseData.clear();
        int navCacheHits = 0, navCacheMisses = 0;
        for (int i = 0; i < 16; i++) globalLayers[i].clear();
        // One-time PSRAM reserve: reduces reallocations on first render
        static bool layersReserved = false;
        if (!layersReserved) {
            for (int i = 0; i < 16; i++) globalLayers[i].reserve(256);
            layersReserved = true;
        }
        // Text labels collected separately — rendered last, on top of all geometry
        std::vector<FeatureRef, PSRAMAllocator<FeatureRef>> textRefs;
        textRefs.reserve(128);

        // Waterway curvilinear labels (GEOM_TEXT_LINE) — rendered in a dedicated pass
        std::vector<FeatureRef, PSRAMAllocator<FeatureRef>> waterwayRefs;
        waterwayRefs.reserve(64);

        uint16_t bgColor = map.color565(0x2F, 0x4F, 0x4F);  // Dark slate gray — same as raster "no data" bg
        bool bgColorExtracted = false;

        // Fixed 3×3 spiral order: center first, then edges, corners last
        static const int8_t spiralOrder[9][2] = {
            {0,0}, {2,0}, {0,2}, {2,2}, {0,1}, {1,0}, {2,1}, {1,2}, {1,1}
        };
        const int tileCount = 9;

        // Pre-evict navCache entries from a different zoom level (useless after zoom change).
        // Grid-based eviction removed: nearby tiles are kept for cache hits on next pan.
        // Phase 3 eviction (LRU) handles PSRAM pressure when loading new tiles.
        {
            int preEvicted = 0;
            for (int i = (int)navCache.size() - 1; i >= 0; i--) {
                if (navCache[i].zoom != zoom) {
                    free(navCache[i].data);
                    navCache.erase(navCache.begin() + i);
                    preEvicted++;
                }
            }
            if (preEvicted > 0) {
                ESP_LOGD(TAG, "Pre-evicted %d wrong-zoom entries, free: %u KB, largest: %u KB",
                              preEvicted, (unsigned)(ESP.getFreePsram() / 1024),
                              (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024));
            }
        }

        // Skip regions that don't contain the center tile.
        // Preserves original region index for stable navCache keys.
        struct ActiveRegion {
            const char* name;
            uint8_t     origIdx;  // index in regions[] — used as navCache key
        };
        ActiveRegion activeRegions[NPK_MAX_REGIONS];
        int activeRegionCount = 0;

        if (regionCount > 1) {
            // Check if the center tile actually exists in each region
            for (int r = 0; r < regionCount && r < NPK_MAX_REGIONS; r++) {
                NpkSlot* slot = openNpkRegion(regions[r], zoom, (uint32_t)centerTileIdxY);
                if (!slot || !slot->yTable) continue;
                UIMapManager::Npk2IndexEntry dummy;
                if (findNpkTileInSlot(slot, (uint32_t)centerTileIdxX, (uint32_t)centerTileIdxY, &dummy)) {
                    activeRegions[activeRegionCount++] = { regions[r], (uint8_t)r };
                }
            }
            if (activeRegionCount == 0) {
                for (int r = 0; r < regionCount && r < NPK_MAX_REGIONS; r++)
                    activeRegions[activeRegionCount++] = { regions[r], (uint8_t)r };
            }
        } else {
            for (int r = 0; r < regionCount && r < NPK_MAX_REGIONS; r++)
                activeRegions[activeRegionCount++] = { regions[r], (uint8_t)r };
        }

        // --- Phase 1: Resolve navCache hits + collect pending SD reads ---
        // Pending reads are sorted by file offset before reading to minimize
        // SD seek latency (critical for non-A1 cards).
        struct PendingTileRead {
            NpkSlot* slot;
            UIMapManager::Npk2IndexEntry entry;
            int16_t  tileOffsetX, tileOffsetY;
            int      tileX, tileY;
            uint8_t  regionIdx;
        };
        static PendingTileRead pendingReads[18];  // 3×3 grid × 2 regions max
        int pendingCount = 0;

        // ================================================================
        // Phase 1 — LOAD: resolve all tiles before any feature dispatch.
        // Cache hits are recorded; misses are read from SD.
        // No FeatureRef exists yet, so eviction is safe.
        // ================================================================
        for (int ti = 0; ti < tileCount; ti++) {
            int gx = spiralOrder[ti][0];
            int gy = spiralOrder[ti][1];
            int tileX = centerTileIdxX - gridOffset + gx;
            int tileY = centerTileIdxY - gridOffset + gy;
            int16_t tileOffsetX = (int16_t)(gx * MAP_TILE_SIZE);  // 0, 256, 512
            int16_t tileOffsetY = (int16_t)(gy * MAP_TILE_SIZE);

            for (int r = 0; r < activeRegionCount; r++) {
                uint8_t regionIdx = activeRegions[r].origIdx;
                int cacheIdx = findNavCache(regionIdx, zoom, tileX, tileY);
                if (cacheIdx >= 0) {
                    navCache[cacheIdx].lastAccess = ++navCacheAccessCounter;
                    navCacheHits++;
                    if (resolvedCount < 9) {
                        resolved[resolvedCount++] = {
                            navCache[cacheIdx].data, navCache[cacheIdx].size,
                            tileOffsetX, tileOffsetY
                        };
                        inUseData.push_back(navCache[cacheIdx].data);
                    }
                } else {
                    NpkSlot* slot = openNpkRegion(activeRegions[r].name, zoom, (uint32_t)tileY);
                    UIMapManager::Npk2IndexEntry entry;
                    if (slot && findNpkTileInSlot(slot, (uint32_t)tileX, (uint32_t)tileY, &entry)
                        && pendingCount < 18) {
                        pendingReads[pendingCount++] = {
                            slot, entry, tileOffsetX, tileOffsetY,
                            tileX, tileY, regionIdx
                        };
                    }
                }
            }
        }

        // Sort pending reads: by file offset if all fit, center-outward otherwise
        size_t totalPendingSize = 0;
        for (int i = 0; i < pendingCount; i++)
            totalPendingSize += pendingReads[i].entry.size;

        if (totalPendingSize <= heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)) {
            std::sort(pendingReads, pendingReads + pendingCount,
                      [](const PendingTileRead& a, const PendingTileRead& b) {
                          if (a.slot != b.slot) return a.slot < b.slot;
                          return a.entry.offset < b.entry.offset;
                      });
        } else {
            std::sort(pendingReads, pendingReads + pendingCount,
                      [centerTileIdxX, centerTileIdxY](const PendingTileRead& a, const PendingTileRead& b) {
                          int adx = a.tileX - centerTileIdxX, ady = a.tileY - centerTileIdxY;
                          int bdx = b.tileX - centerTileIdxX, bdy = b.tileY - centerTileIdxY;
                          return (adx*adx + ady*ady) < (bdx*bdx + bdy*bdy);
                      });
        }

        // Proactive eviction: free PSRAM before loading new tiles
        // Z9 tiles can be 50-100 KB each; ensure headroom for pending reads
        if (pendingCount > 0) {
            const size_t PSRAM_HEADROOM = 200 * 1024;  // 200 KB minimum free
            size_t freeBlock = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
            if (freeBlock < PSRAM_HEADROOM) {
                evictUnusedNavCache(inUseData, PSRAM_HEADROOM);
            }
        }

        // Read pending tiles from SD into navCache
        for (int i = 0; i < pendingCount; i++) {
            esp_task_wdt_reset();
            auto& pr = pendingReads[i];
            uint8_t* data = nullptr;
            size_t fileSize = 0;

            if (!readNpkTileData(pr.slot, &pr.entry, &data, &fileSize)) {
                if (evictUnusedNavCache(inUseData, pr.entry.size)) {
                    readNpkTileData(pr.slot, &pr.entry, &data, &fileSize);
                }
                if (!data) continue;
            }
            if (!data) continue;

            if (memcmp(data, "NAV1", 4) != 0) {
                ESP_LOGE(TAG, "Tile %d/%d: invalid header", pr.tileX, pr.tileY);
                free(data);
                continue;
            }

            addNavCache(pr.regionIdx, zoom, pr.tileX, pr.tileY, data, fileSize, &inUseData);
            navCacheMisses++;

            if (resolvedCount < 9) {
                resolved[resolvedCount++] = { data, fileSize, pr.tileOffsetX, pr.tileOffsetY };
                inUseData.push_back(data);
            }
        }

        uint64_t loadEnd = esp_timer_get_time();

        // ================================================================
        // Phase 2 — COUNT: scan all resolved tiles to get exact feature
        // counts per vector. Enables single reserve() per vector.
        // ================================================================
        uint32_t layerCounts[16] = {};
        uint32_t textCount = 0, waterwayCount = 0;

        for (int t = 0; t < resolvedCount; t++) {
            uint8_t* data = resolved[t].data;
            size_t fileSize = resolved[t].size;
            if (fileSize < 22 + 13) continue;

            uint16_t feature_count;
            memcpy(&feature_count, data + 4, 2);
            uint8_t* p = data + 22;

            for (uint16_t i = 0; i < feature_count; i++) {
                if (p + 13 > data + fileSize) break;
                uint8_t geomType = p[0];
                uint8_t zoomPriority = p[3];
                uint16_t payloadSize;
                memcpy(&payloadSize, p + 11, 2);
                if (p + 13 + payloadSize > data + fileSize) break;

                uint8_t minZoom = zoomPriority >> 4;
                if (minZoom > zoom) { p += 13 + payloadSize; continue; }

                uint8_t priority = zoomPriority & 0x0F;
                if (priority >= 16) priority = 15;

                if (geomType == GEOM_TEXT) textCount++;
                else if (geomType == GEOM_TEXT_LINE) { if (zoom >= 15) waterwayCount++; }
                else layerCounts[priority]++;

                p += 13 + payloadSize;
            }
        }

        // Pre-reserve vectors — single allocation, no per-feature fragmentation
        ESP_LOGI(TAG, "Phase 2 counts: text=%u ww=%u layers=[%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u] PSRAM free=%u largest=%u",
                 textCount, waterwayCount,
                 layerCounts[0], layerCounts[1], layerCounts[2], layerCounts[3],
                 layerCounts[4], layerCounts[5], layerCounts[6], layerCounts[7],
                 layerCounts[8], layerCounts[9], layerCounts[10], layerCounts[11],
                 layerCounts[12], layerCounts[13], layerCounts[14], layerCounts[15],
                 (unsigned)ESP.getFreePsram(),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
        for (int i = 0; i < 16; i++) {
            if (layerCounts[i] > globalLayers[i].capacity()) {
                try {
                    globalLayers[i].reserve(layerCounts[i]);
                } catch (const std::bad_alloc&) {
                    ESP_LOGW(TAG, "Reserve failed layer %d: need %u × %u B, PSRAM largest=%u",
                             i, layerCounts[i], (unsigned)sizeof(FeatureRef),
                             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
                }
            }
        }
        try {
            if (textCount > textRefs.capacity()) textRefs.reserve(textCount);
            if (waterwayCount > waterwayRefs.capacity()) waterwayRefs.reserve(waterwayCount);
        } catch (const std::bad_alloc&) {
            ESP_LOGW(TAG, "Reserve failed text/waterway, PSRAM largest=%u",
                     (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
        }
        ESP_LOGI(TAG, "Phase 2 reserve done, PSRAM free=%u largest=%u",
                 (unsigned)ESP.getFreePsram(),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));

        // ================================================================
        // Phase 3 — DISPATCH: populate globalLayers/textRefs/waterwayRefs.
        // Vectors are pre-reserved, no reallocation during this loop.
        // ================================================================
        for (int t = 0; t < resolvedCount; t++) {
            uint8_t* data = resolved[t].data;
            size_t fileSize = resolved[t].size;
            int16_t tileOffsetX = resolved[t].tileOffsetX;
            int16_t tileOffsetY = resolved[t].tileOffsetY;

            if (!bgColorExtracted && fileSize >= 22 + 13) {
                uint8_t ft0Type = data[22];
                uint8_t ft0Prio = data[25] & 0x0F;
                if (ft0Type == 3 && ft0Prio == 0) {
                    memcpy(&bgColor, data + 23, 2);
                }
                bgColorExtracted = true;
            }

            uint16_t feature_count;
            memcpy(&feature_count, data + 4, 2);
            uint8_t* p = data + 22;

            bool psramExhausted = false;

            for (uint16_t i = 0; i < feature_count && !psramExhausted; i++) {
                if ((i & 63) == 0) esp_task_wdt_reset();
                if (p + 13 > data + fileSize) break;

                uint8_t geomType = p[0];
                uint8_t zoomPriority = p[3];
                uint16_t coordCount;
                memcpy(&coordCount, p + 9, 2);
                uint16_t payloadSize;
                memcpy(&payloadSize, p + 11, 2);
                if (p + 13 + payloadSize > data + fileSize) break;

                uint8_t minZoom = zoomPriority >> 4;
                if (minZoom > zoom) { p += 13 + payloadSize; continue; }

                // Semantic culling: skip tiny features
                // Z9-Z11: skip if bbox < 3×3px. Other zooms: skip if < 1×1px.
                uint8_t bx1 = p[5], by1 = p[6], bx2 = p[7], by2 = p[8];
                if (geomType == GEOM_POLYGON || geomType == GEOM_LINE) {
                    uint8_t minDim = (zoom >= 9 && zoom <= 11) ? 3 : 1;
                    if ((bx2 - bx1) < minDim && (by2 - by1) < minDim) {
                        p += 13 + payloadSize; continue;
                    }
                }

                uint8_t priority = zoomPriority & 0x0F;
                if (priority >= 16) priority = 15;

                FeatureRef ref;
                ref.ptr = p;
                ref.geomType = geomType;
                ref.payloadSize = payloadSize;
                ref.coordCount = coordCount;
                ref.tileOffsetX = tileOffsetX;
                ref.tileOffsetY = tileOffsetY;

                try {
                    if (geomType == GEOM_TEXT) {
                        textRefs.push_back(ref);
                    } else if (geomType == GEOM_TEXT_LINE) {
                        if (zoom >= 15) waterwayRefs.push_back(ref);
                    } else {
                        globalLayers[priority].push_back(ref);
                    }
                } catch (const std::bad_alloc&) {
                    ESP_LOGW(TAG, "PSRAM exhausted at feature %u/%u, rendering partial",
                             i, feature_count);
                    psramExhausted = true;
                }

                p += 13 + payloadSize;
            }
        }

        int totalFeatures = 0;
        for (int i = 0; i < 16; i++) totalFeatures += globalLayers[i].size();
        totalFeatures += textRefs.size() + waterwayRefs.size();
        ESP_LOGD(TAG, "Load: %llu ms, tiles: %d, features: %d, grid: 3x3 fixed",
                      (loadEnd - startTime) / 1000, resolvedCount, totalFeatures);

        // --- Render all layers ---
        // Fill background with color from NAV background polygon
        map.fillSprite(bgColor);

        map.startWrite();
        try {

        struct LabelRect { int16_t x, y, w, h; };
        std::vector<LabelRect> placedLabels;
        placedLabels.reserve(128);

        int featureCount = 0;
        uint64_t lastYieldUs = esp_timer_get_time();
        for (int pri = 0; pri < 16; pri++) {
            if (globalLayers[pri].empty()) continue;

            for (const auto& ref : globalLayers[pri]) {
                // Yield every 20ms to let WiFi/BLE breathe on Core 0
                if ((++featureCount & 15) == 0) {
                    uint64_t nowUs = esp_timer_get_time();
                    if (nowUs - lastYieldUs > 20000) {
                        map.endWrite();
                        vTaskDelay(1);
                        map.startWrite();
                        lastYieldUs = esp_timer_get_time();
                    }
                }

                uint8_t* fp = ref.ptr;

                // BBox culling against viewport BEFORE setClipRect (avoid unnecessary calls)
                uint8_t bx1 = fp[5], by1 = fp[6], bx2 = fp[7], by2 = fp[8];
                int16_t minX = ref.tileOffsetX + bx1;
                int16_t minY = ref.tileOffsetY + by1;
                int16_t maxX = ref.tileOffsetX + bx2;
                int16_t maxY = ref.tileOffsetY + by2;
                if (maxX < 0 || minX > viewportW || maxY < 0 || minY > viewportH) continue;

                // Per-feature setClipRect to tile boundaries
                map.setClipRect(ref.tileOffsetX, ref.tileOffsetY, MAP_TILE_SIZE, MAP_TILE_SIZE);

                // Read colorRgb565 directly (LE, no byte swap)
                uint16_t colorRgb565;
                memcpy(&colorRgb565, fp + 1, 2);

                // Render feature by geometry type (mixed per layer)
                switch (ref.geomType) {
                    case 3: { // Polygon — decode VarInt coords
                        if (ref.coordCount < 3) break;
                        decodedCoords.clear();
                        DecodedFeature df;
                        if (!decodeFeatureCoords(fp + 13, ref.coordCount, ref.payloadSize, ref.geomType, df)) break;
                        int16_t* coords = decodedCoords.data() + df.coordsIdx;

                        if (proj32X.capacity() < ref.coordCount) proj32X.reserve(ref.coordCount * 3 / 2);
                        if (proj32Y.capacity() < ref.coordCount) proj32Y.reserve(ref.coordCount * 3 / 2);
                        proj32X.resize(ref.coordCount);
                        proj32Y.resize(ref.coordCount);

                        int* px_hp = proj32X.data();
                        int* py_hp = proj32Y.data();
                        if (!px_hp || !py_hp) break;

                        // Vertex decimation: skip redundant vertices at same pixel
                        // Only for simple polygons (no multi-ring).
                        int16_t lastVx = -32768, lastVy = -32768;
                        uint16_t actualPoints = 0;
                        for (uint16_t j = 0; j < ref.coordCount; j++) {
                            int16_t cx = coords[j * 2];
                            int16_t cy = coords[j * 2 + 1];
                            if (df.ringCount == 0 && j > 0 &&
                                abs(cx - lastVx) < 1 && abs(cy - lastVy) < 1 &&
                                j < ref.coordCount - 1)
                                continue;
                            px_hp[actualPoints] = (int)cx;
                            py_hp[actualPoints] = (int)cy;
                            lastVx = cx;
                            lastVy = cy;
                            actualPoints++;
                        }

                        if (fillPolygons) {
                            fillPolygonGeneral(map, px_hp, py_hp, actualPoints,
                                colorRgb565, ref.tileOffsetX, ref.tileOffsetY,
                                df.ringCount, df.ringEnds);
                        }

                        // Building outline (bit 7 of fp[4]), z16+ only
                        if ((fp[4] & 0x80) != 0 && zoom >= 16) {
                            uint16_t outlineColor = darkenRGB565(colorRgb565, 0.35f);
                            uint16_t ringStart = 0;
                            uint16_t numRings = (df.ringCount > 0) ? df.ringCount : 1;
                            for (uint16_t r = 0; r < numRings; r++) {
                                uint16_t ringEnd = (df.ringEnds && r < df.ringCount) ? df.ringEnds[r] : actualPoints;
                                if (ringEnd > actualPoints) ringEnd = actualPoints;
                                for (uint16_t j = ringStart; j < ringEnd; j++) {
                                    uint16_t next = (j + 1 < ringEnd) ? j + 1 : ringStart;
                                    int x0 = (px_hp[j] >> 4) + ref.tileOffsetX;
                                    int y0 = (py_hp[j] >> 4) + ref.tileOffsetY;
                                    int x1 = (px_hp[next] >> 4) + ref.tileOffsetX;
                                    int y1 = (py_hp[next] >> 4) + ref.tileOffsetY;
                                    map.drawLine(x0, y0, x1, y1, outlineColor);
                                }
                                ringStart = ringEnd;
                            }
                        }

                        break;
                    }
                    case 2: { // LineString — decode VarInt coords
                        if (ref.coordCount < 2) break;
                        uint8_t widthRaw = fp[4] & 0x7F;        // bits 6-0 = half-pixels
                        if (widthRaw == 0) widthRaw = 2;         // minimum 1.0px
                        float widthF = widthRaw / 2.0f;          // convert to pixels

                        decodedCoords.clear();
                        DecodedFeature df;
                        if (!decodeFeatureCoords(fp + 13, ref.coordCount, ref.payloadSize, ref.geomType, df)) break;
                        int16_t* coords = decodedCoords.data() + df.coordsIdx;

                        // Pre-project all coords with dedup
                        size_t numCoords = ref.coordCount;
                        if (proj16X.capacity() < numCoords) proj16X.reserve(numCoords * 3 / 2);
                        if (proj16Y.capacity() < numCoords) proj16Y.reserve(numCoords * 3 / 2);
                        proj16X.resize(numCoords);
                        proj16Y.resize(numCoords);

                        int16_t* pxArr = proj16X.data();
                        int16_t* pyArr = proj16Y.data();

                        int16_t minPx = INT16_MAX, maxPx = INT16_MIN;
                        int16_t minPy = INT16_MAX, maxPy = INT16_MIN;
                        size_t validPoints = 0;
                        int16_t lastPx = -32768, lastPy = -32768;

                        // Adaptive LOD: 2px threshold for Z15+, 1px otherwise
                        int16_t lodThreshold = (zoom >= 15) ? 2 : 1;

                        for (size_t j = 0; j < numCoords; j++) {
                            int16_t px = (coords[j * 2] >> 4) + ref.tileOffsetX;
                            int16_t py = (coords[j * 2 + 1] >> 4) + ref.tileOffsetY;

                            if (validPoints > 0) {
                                // Skip vertex if closer than LOD threshold (always keep last)
                                if (abs(px - lastPx) < lodThreshold && abs(py - lastPy) < lodThreshold
                                    && j < numCoords - 1) continue;
                            }

                            pxArr[validPoints] = px;
                            pyArr[validPoints] = py;
                            if (px < minPx) minPx = px;
                            if (px > maxPx) maxPx = px;
                            if (py < minPy) minPy = py;
                            if (py > maxPy) maxPy = py;
                            lastPx = px;
                            lastPy = py;
                            validPoints++;
                        }

                        // Bbox check on projected line
                        if (validPoints < 2 || maxPx < 0 || minPx >= viewportW ||
                            maxPy < 0 || minPy >= viewportH) break;

                        for (size_t j = 1; j < validPoints; j++) {
                            if (widthF <= 1.0f) {
                                map.drawLine(pxArr[j-1], pyArr[j-1], pxArr[j], pyArr[j], colorRgb565);
                            } else {
                                map.drawWideLine(pxArr[j-1], pyArr[j-1], pxArr[j], pyArr[j],
                                                 widthF, colorRgb565);
                                map.setClipRect(ref.tileOffsetX, ref.tileOffsetY, MAP_TILE_SIZE, MAP_TILE_SIZE);
                            }
                        }
                        break;
                    }
                    case 1: { // Point — decode VarInt coords (delta from 0 = zigzag value)
                        if (ref.coordCount < 1) break;
                        decodedCoords.clear();
                        DecodedFeature df;
                        if (!decodeFeatureCoords(fp + 13, ref.coordCount, ref.payloadSize, ref.geomType, df)) break;
                        int16_t* coords = decodedCoords.data() + df.coordsIdx;
                        int px = (coords[0] >> 4) + ref.tileOffsetX;
                        int py = (coords[1] >> 4) + ref.tileOffsetY;
                        // Bounds check
                        if (px >= 0 && px < viewportW && py >= 0 && py < viewportH)
                            map.fillCircle(px, py, 3, colorRgb565);
                        break;
                    }
                    case 4: { // Text label (GEOM_TEXT) — payload NOT VarInt, raw int16
                        uint8_t fontSize = fp[4];
                        int16_t* coords = (int16_t*)(fp + 13);
                        int px = (coords[0] >> 4) + ref.tileOffsetX;
                        int py = (coords[1] >> 4) + ref.tileOffsetY;
                        uint8_t textLen = *(fp + 13 + 4);
                        if (textLen > 0 && textLen < 128) {
                            char textBuf[128];
                            memcpy(textBuf, fp + 13 + 5, textLen);
                            textBuf[textLen] = '\0';

                            // Use VLW Unicode font if loaded, fallback to GFX font
                            if (vlwFontLoaded) {
                                map.setFont(&vlwFont);
                                // Scale VLW font based on fontSize (0=small, 1=medium, 2=large)
                                float scale = (fontSize == 0) ? 0.8f : (fontSize == 1) ? 1.0f : 1.2f;
                                map.setTextSize(scale);
                            } else {
                                map.setFont((lgfx::GFXfont*)&OpenSans_Bold6pt7b);
                                map.setTextSize(1);
                            }

                            // Measure label bbox for collision detection
                            int tw = map.textWidth(textBuf);
                            int th = map.fontHeight();
                            int lx = px - tw / 2;  // center horizontally
                            int ly = py - th;       // above the point
                            const int PAD = 4;

                            // Viewport bounds check (label must be at least partially visible)
                            if (lx + tw < 0 || lx >= viewportW || ly + th < 0 || ly >= viewportH) break;

                            // Check collision with already placed labels
                            bool collision = false;
                            for (const auto& r : placedLabels) {
                                if (lx - PAD < r.x + r.w && lx + tw + PAD > r.x &&
                                    ly - PAD < r.y + r.h && ly + th + PAD > r.y) {
                                    collision = true;
                                    break;
                                }
                            }
                            if (collision) break;

                            // Lift tile clipRect so labels can span tile boundaries
                            map.clearClipRect();

                            // Draw label
                            map.setTextColor(colorRgb565);
                            map.setTextDatum(lgfx::top_center);
                            map.drawString(textBuf, px, ly);
                            map.setTextDatum(lgfx::top_left); // restore default

                            // Restore tile clipRect for subsequent features
                            map.setClipRect(ref.tileOffsetX, ref.tileOffsetY, MAP_TILE_SIZE, MAP_TILE_SIZE);

                            placedLabels.push_back({(int16_t)lx, (int16_t)ly, (int16_t)tw, (int16_t)th});
                        }
                        break;
                    }
                }
            }
            globalLayers[pri].clear();
            esp_task_wdt_reset();
        }

        // Label pass — rendered after all geometry so labels appear on top
        map.clearClipRect();
        map.setFont((lgfx::GFXfont*)&OpenSans_Bold6pt7b);
        map.setTextSize(1);
        // Set VLW Unicode font for text labels (must be before textWidth/drawString)
        if (vlwFontLoaded) {
            map.setFont(&vlwFont);
            map.setTextSize(1.0f);
        } else {
            map.setFont((lgfx::GFXfont*)&OpenSans_Bold6pt7b);
            map.setTextSize(1);
        }

        for (const auto& ref : textRefs) {
            if ((++featureCount & 31) == 0) esp_task_wdt_reset();
            uint8_t* fp = ref.ptr;
            uint16_t colorRgb565;
            memcpy(&colorRgb565, fp + 1, 2);
            int16_t* coords = (int16_t*)(fp + 13);  // Text payload is raw int16, NOT VarInt
            int px = (coords[0] >> 4) + ref.tileOffsetX;
            int py = (coords[1] >> 4) + ref.tileOffsetY;
            uint8_t fontSize = fp[4];
            uint8_t textLen = *(fp + 13 + 4);
            if (textLen == 0 || textLen >= 128) continue;
            char textBuf[128];
            memcpy(textBuf, fp + 13 + 5, textLen);
            textBuf[textLen] = '\0';

            // Scale VLW font based on fontSize (0=small, 1=medium, 2=large)
            if (vlwFontLoaded) {
                float scale = (fontSize == 0) ? 0.8f : (fontSize == 1) ? 1.0f : 1.2f;
                map.setTextSize(scale);
            }

            int tw = map.textWidth(textBuf);
            int th = map.fontHeight();
            int lx = px - tw / 2;
            int ly = py - th;
            const int PAD = 4;
            if (lx + tw < 0 || lx >= viewportW || ly + th < 0 || ly >= viewportH) continue;
            bool collision = false;
            for (const auto& r : placedLabels) {
                if (lx - PAD < r.x + r.w && lx + tw + PAD > r.x &&
                    ly - PAD < r.y + r.h && ly + th + PAD > r.y) {
                    collision = true; break;
                }
            }
            if (collision) continue;
            map.setTextColor(colorRgb565);
            map.setTextDatum(lgfx::top_center);
            map.drawString(textBuf, px, ly);
            map.setTextDatum(lgfx::top_left);
            placedLabels.push_back({(int16_t)lx, (int16_t)ly, (int16_t)tw, (int16_t)th});
        }
        textRefs.clear();

        // ---- Waterway curvilinear label pass (GEOM_TEXT_LINE) ----
        // Text follows the waterway path geometry (text-along-path, OSM style).
        // Payload: uint8_t path_count | [int16_t px, int16_t py] x N | uint8_t text_len | uint8_t[text_len] text
        map.clearClipRect();
        // Waterway labels use bitmap font (no anti-aliasing) for clean
        // color-key transparency with pushRotateZoom
        map.setFont((lgfx::GFXfont*)&OpenSans_Bold6pt7b);
        map.setTextSize(1);

        // Ensure glyph sprite exists (reused across all characters)
        int maxGlyphH = map.fontHeight() + 2;
        int maxGlyphW = maxGlyphH * 2;  // widest glyph won't exceed 2× height
        if (glyphSprite && (glyphSpriteW < maxGlyphW || glyphSpriteH < maxGlyphH)) {
            glyphSprite->deleteSprite(); delete glyphSprite;
            glyphSprite = nullptr;
        }
        if (!glyphSprite) {
            glyphSprite = new LGFX_Sprite(&map);
            glyphSprite->setColorDepth(16);
            glyphSprite->setPsram(true);
            if (glyphSprite->createSprite(maxGlyphW, maxGlyphH)) {
                glyphSpriteW = maxGlyphW;
                glyphSpriteH = maxGlyphH;
            } else {
                delete glyphSprite; glyphSprite = nullptr;
                glyphSpriteW = glyphSpriteH = 0;
            }
        }

        for (const auto& ref : waterwayRefs) {
            if (!wlScreenX || !wlScreenY || !wlArcLen) break;  // PSRAM alloc failed
            if ((++featureCount & 31) == 0) esp_task_wdt_reset();

            uint8_t* fp = ref.ptr;
            uint16_t colorRgb565;
            memcpy(&colorRgb565, fp + 1, 2);
            uint8_t fontSize = fp[4];

            // --- Parse GEOM_TEXT_LINE payload (raw int16, NOT VarInt) ---
            const uint8_t* payload = fp + 13;
            if (ref.payloadSize < 2) continue;

            uint8_t pathCount = payload[0];
            if (pathCount < 2) continue;
            const uint8_t pathCountOrig = pathCount;
            if (pathCount > WLABEL_MAX_PTS - 1) pathCount = WLABEL_MAX_PTS - 1;

            size_t minSize = 1u + (size_t)pathCountOrig * 4u + 1u;
            if (ref.payloadSize < minSize) continue;

            const int16_t* rawPts = (const int16_t*)(payload + 1);

            // Project to screen pixels (buffers in PSRAM)
            for (uint8_t j = 0; j < pathCount; j++) {
                int16_t cx = rawPts[j * 2];
                int16_t cy = rawPts[j * 2 + 1];
                wlScreenX[j] = (int)(cx * MAP_TILE_SIZE / 4096) + ref.tileOffsetX;
                wlScreenY[j] = (int)(cy * MAP_TILE_SIZE / 4096) + ref.tileOffsetY;
            }

            // Read text — use pathCountOrig to skip ALL path points in payload
            const uint8_t* afterPath = payload + 1 + (size_t)pathCountOrig * 4;
            uint8_t textLen = afterPath[0];
            if (textLen == 0 || textLen >= 128) continue;
            if (ref.payloadSize < minSize + textLen) continue;

            char textBuf[128];
            memcpy(textBuf, afterPath + 1, textLen);
            textBuf[textLen] = '\0';

            // Bitmap font — no scaling needed

            // RTL detection: if path goes right-to-left, reverse the path
            // (not the text) so characters face left-to-right
            float firstDx = (float)(wlScreenX[1] - wlScreenX[0]);
            bool reversePath = (firstDx < 0.0f);
            if (reversePath) {
                for (int i = 0, j = pathCount - 1; i < j; i++, j--) {
                    int tx = wlScreenX[i]; wlScreenX[i] = wlScreenX[j]; wlScreenX[j] = tx;
                    int ty = wlScreenY[i]; wlScreenY[i] = wlScreenY[j]; wlScreenY[j] = ty;
                }
            }

            ESP_LOGD(TAG, "WLABEL '%s' pts=%d fontSize=%d reversePath=%d",
                     textBuf, pathCount, fontSize, (int)reversePath);

            // Cumulative arc length (buffers in PSRAM)
            wlArcLen[0] = 0.0f;
            for (uint8_t j = 1; j < pathCount; j++) {
                float dx = (float)(wlScreenX[j] - wlScreenX[j - 1]);
                float dy = (float)(wlScreenY[j] - wlScreenY[j - 1]);
                wlArcLen[j] = wlArcLen[j - 1] + sqrtf(dx * dx + dy * dy);
            }
            float totalLen = wlArcLen[pathCount - 1];
            if (totalLen < 4.0f) continue;

            int textWidth = map.textWidth(textBuf);
            int charH = map.fontHeight();

            // Skip if text wider than path
            if ((float)textWidth > totalLen) continue;

            // Visibility: bounding box of path
            int minSX = wlScreenX[0], maxSX = wlScreenX[0];
            int minSY = wlScreenY[0], maxSY = wlScreenY[0];
            for (uint8_t j = 1; j < pathCount; j++) {
                if (wlScreenX[j] < minSX) minSX = wlScreenX[j];
                if (wlScreenX[j] > maxSX) maxSX = wlScreenX[j];
                if (wlScreenY[j] < minSY) minSY = wlScreenY[j];
                if (wlScreenY[j] > maxSY) maxSY = wlScreenY[j];
            }
            if (maxSX < 0 || minSX >= viewportW || maxSY < 0 || minSY >= viewportH) continue;

            // Collision detection
            const int PAD = 4;
            bool collision = false;
            for (const auto& r : placedLabels) {
                if (minSX - PAD < r.x + r.w && maxSX + PAD > r.x &&
                    minSY - PAD < r.y + r.h && maxSY + PAD > r.y) {
                    collision = true;
                    break;
                }
            }
            if (collision) continue;

            placedLabels.push_back({(int16_t)minSX, (int16_t)minSY,
                                    (int16_t)(maxSX - minSX), (int16_t)(maxSY - minSY)});

            // Center text on path
            float startDist = (totalLen - (float)textWidth) / 2.0f;
            if (startDist < 0.0f) startDist = 0.0f;

            map.setTextColor(colorRgb565);
            int seg = 0;
            float charDist = startDist;

            // Iterate by UTF-8 codepoints
            for (int ci = 0; textBuf[ci] != '\0'; ) {
                // Extract one UTF-8 codepoint
                uint8_t c0 = (uint8_t)textBuf[ci];
                int cpLen;
                if      (c0 < 0x80) cpLen = 1;
                else if (c0 < 0xE0) cpLen = 2;
                else if (c0 < 0xF0) cpLen = 3;
                else                cpLen = 4;

                char chBuf[5];
                memcpy(chBuf, textBuf + ci, cpLen);
                chBuf[cpLen] = '\0';
                ci += cpLen;

                int charW = map.textWidth(chBuf);

                // Advance along path
                while (seg < pathCount - 2 && wlArcLen[seg + 1] < charDist)
                    seg++;

                float segLen = wlArcLen[seg + 1] - wlArcLen[seg];
                float t = (segLen > 0.001f) ? (charDist - wlArcLen[seg]) / segLen : 0.0f;

                float px = wlScreenX[seg] + t * (wlScreenX[seg + 1] - wlScreenX[seg]);
                float py = wlScreenY[seg] + t * (wlScreenY[seg + 1] - wlScreenY[seg]);

                if (px >= -charW && px < viewportW + charW && py >= -charH && py < viewportH + charH) {
                    float dx = (float)(wlScreenX[seg + 1] - wlScreenX[seg]);
                    float dy = (float)(wlScreenY[seg + 1] - wlScreenY[seg]);
                    float angleDeg = atan2f(dy, dx) * (180.0f / (float)M_PI);

                    if (glyphSprite) {
                        // Reuse persistent glyph sprite — draw character centered
                        // so pushRotateZoom pivot (sprite center) aligns with glyph center
                        glyphSprite->fillSprite(0xF81F);
                        glyphSprite->setFont((lgfx::GFXfont*)&OpenSans_Bold6pt7b);
                        glyphSprite->setTextDatum(lgfx::middle_center);
                        glyphSprite->setTextColor(colorRgb565);
                        glyphSprite->drawString(chBuf, glyphSpriteW / 2, glyphSpriteH / 2);
                        glyphSprite->pushRotateZoom(&map,
                            (int)px, (int)py, angleDeg, 1.0f, 1.0f, 0xF81F);
                    } else {
                        // Fallback: draw upright
                        map.setTextDatum(lgfx::top_left);
                        map.drawString(chBuf, (int)px, (int)py - charH / 2);
                    }
                }

                charDist += (float)charW;
            }
        }
        waterwayRefs.clear();

        } catch (const std::bad_alloc&) {
            ESP_LOGW(TAG, "PSRAM exhausted during render, partial output");
        }
        map.endWrite();

        // Tile buffers are now owned by navCache — do NOT free here

        uint64_t endTime = esp_timer_get_time();

        ESP_LOGI(TAG, "Viewport: %llu ms (load %llu ms), %d features, cache: %d hit / %d miss, PSRAM free: %u",
                      (endTime - startTime) / 1000, (loadEnd - startTime) / 1000,
                      totalFeatures, navCacheHits, navCacheMisses, ESP.getFreePsram());

        bool result = (resolvedCount > 0);

        // Release render lock + process deferred operations
        renderActive_ = false;
        if (renderLock) xSemaphoreGive(renderLock);

        return result;
    }

    // Render a NAV1 vector tile:
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
