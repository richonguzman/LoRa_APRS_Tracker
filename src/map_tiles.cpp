/* Map tile loading, symbol caching, region discovery
 * Extracted from ui_map_manager.cpp — Étape 2 of refactoring
 */

#ifdef USE_LVGL_UI

// Include order is critical: LVGL (lv_meter.h) uses 'local' as a parameter name.
// PNGdec (zutil.h) defines 'local' as a macro (#define local static).
// LVGL must be included before PNGdec to avoid the macro expansion clash.
// Same include order as ui_map_manager.cpp.
#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <LovyanGFX.hpp>
#include <lvgl.h>           // Must precede PNGdec (lv_meter.h uses 'local' as param name)
#include <JPEGDEC.h>
// Undefine macros that conflict between PNGdec and JPEGDEC
#undef INTELSHORT
#undef INTELLONG
#undef MOTOSHORT
#undef MOTOLONG
#include <PNGdec.h>         // Defines 'local' as static — LVGL already parsed above
#include <vector>
#include <climits>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_log.h>

#include "map_state.h"
#include "map_tiles.h"
#include "map_engine.h"
#include "map_coordinate_math.h"
#include "storage_utils.h"
#include "nav_types.h"
#include "ui_map_manager.h"  // For spiMutex, MAP_TILE_SIZE, MAP_SPRITE_SIZE

using namespace MapState;

static const char *TAG = "MapTiles";

// =============================================================================
// File-scope statics — private to map_tiles.cpp
// =============================================================================

// Negative tile cache (tiles not found on SD — avoid repeated lookups)
#define NOT_FOUND_CACHE_SIZE 128
static std::vector<uint32_t> notFoundCache;
static int notFoundCacheIndex = 0;

// Symbol cache in PSRAM
#define SYMBOL_CACHE_SIZE 10
#define SYMBOL_SIZE 24
#define SYMBOL_DATA_SIZE (SYMBOL_SIZE * SYMBOL_SIZE * sizeof(uint16_t) \
                          + SYMBOL_SIZE * SYMBOL_SIZE)  // RGB565 + alpha

static CachedSymbol symbolCache[SYMBOL_CACHE_SIZE];
static uint32_t symbolCacheAccessCounter = 0;
static bool symbolCacheInitialized = false;

// PNG decoder state (file-scope, used only by pngSymbolCallback)
static bool pngFileOpened = false;
static uint8_t* symbolCombinedBuffer = nullptr;  // Target buffer for PNG decode
static PNG symbolPNG;                             // PNG decoder instance

// Tile preload task state
struct TileRequest {
    int tileX;
    int tileY;
    int zoom;
};
static QueueHandle_t tilePreloadQueue = nullptr;
static TaskHandle_t tilePreloadTask = nullptr;
static bool preloadTaskRunning = false;
#define TILE_PRELOAD_QUEUE_SIZE 20

// =============================================================================
// PNG file callbacks (private)
// =============================================================================

static void* pngOpenFile(const char* filename, int32_t* size) {
    pngFileOpened = false;
    File* file = new File(SD.open(filename, FILE_READ));
    if (!file || !*file) {
        delete file;
        return nullptr;
    }
    *size = file->size();
    pngFileOpened = true;
    return file;
}

static void pngCloseFile(void* handle) {
    File* file = (File*)handle;
    if (file) {
        file->close();
        delete file;
    }
}

static int32_t pngReadFile(PNGFILE* pFile, uint8_t* pBuf, int32_t iLen) {
    File* file = (File*)pFile->fHandle;
    return file->read(pBuf, iLen);
}

static int32_t pngSeekFile(PNGFILE* pFile, int32_t iPosition) {
    File* file = (File*)pFile->fHandle;
    return file->seek(iPosition);
}

static int pngSymbolCallback(PNGDRAW* pDraw) {
    if (!symbolCombinedBuffer) return 1;
    if (pDraw->y >= SYMBOL_SIZE) return 1;  // Clamp oversized PNGs

    const int w = (pDraw->iWidth < SYMBOL_SIZE) ? pDraw->iWidth : SYMBOL_SIZE;
    const size_t rgb565Offset = pDraw->y * SYMBOL_SIZE;
    const size_t alphaOffset  = SYMBOL_SIZE * SYMBOL_SIZE * sizeof(uint16_t)
                              + pDraw->y * SYMBOL_SIZE;

    uint8_t*  alphaRow  = symbolCombinedBuffer + alphaOffset;
    uint16_t* rgb565Row = (uint16_t*)symbolCombinedBuffer + rgb565Offset;

    if (pDraw->iPixelType == PNG_PIXEL_INDEXED) {
        uint8_t* indices = (uint8_t*)pDraw->pPixels;
        uint8_t* palette = (uint8_t*)pDraw->pPalette;

        for (int x = 0; x < w; x++) {
            uint8_t idx = indices[x];
            uint8_t r = palette[idx * 3 + 0];
            uint8_t g = palette[idx * 3 + 1];
            uint8_t b = palette[idx * 3 + 2];
            uint8_t a = pDraw->iHasAlpha ? palette[768 + idx] : 255;
            a = (a > 50) ? 255 : 0;
            uint16_t rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
#if LV_COLOR_16_SWAP
            rgb565Row[x] = (rgb565 >> 8) | (rgb565 << 8);
#else
            rgb565Row[x] = rgb565;
#endif
            alphaRow[x] = a;
        }
        for (int x = w; x < SYMBOL_SIZE; x++) { rgb565Row[x] = 0; alphaRow[x] = 0; }
    } else {
        if (pDraw->iHasAlpha) {
            for (int x = 0; x < w; x++) {
                uint8_t a = pDraw->pPixels[x * 4 + 3];
                alphaRow[x] = (a > 50) ? 255 : 0;
            }
            for (int x = w; x < SYMBOL_SIZE; x++) alphaRow[x] = 0;
        } else {
            memset(alphaRow, 255, SYMBOL_SIZE);
        }
#if LV_COLOR_16_SWAP
        symbolPNG.getLineAsRGB565(pDraw, rgb565Row, PNG_RGB565_BIG_ENDIAN, 0x00000000);
#else
        symbolPNG.getLineAsRGB565(pDraw, rgb565Row, PNG_RGB565_LITTLE_ENDIAN, 0x00000000);
#endif
    }
    return 1;
}

// =============================================================================
// Tile preload task (private)
// =============================================================================

static void tilePreloadTaskFunc(void* param) {
    ESP_LOGI(TAG, "Tile preload task started on Core 1");
    TileRequest req;

    while (preloadTaskRunning) {
        if (mainThreadLoading || navModeActive) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        if (xQueueReceive(tilePreloadQueue, &req, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (mainThreadLoading) continue;
            if (MapEngine::findCachedRasterTile(req.zoom, req.tileX, req.tileY) == nullptr) {
                ESP_LOGD(TAG, "Preloading tile %d/%d/%d", req.zoom, req.tileX, req.tileY);
                MapTiles::preloadTileToCache(req.tileX, req.tileY, req.zoom);
            }
        }
    }

    ESP_LOGI(TAG, "Tile preload task stopped");
    vTaskDelete(NULL);
}

// =============================================================================
// MapTiles namespace — public API
// =============================================================================

namespace MapTiles {

    // -------------------------------------------------------------------------
    // Symbol cache
    // -------------------------------------------------------------------------

    void initSymbolCache() {
        for (int i = 0; i < SYMBOL_CACHE_SIZE; i++) {
            symbolCache[i].data = nullptr;
            symbolCache[i].valid = false;
            symbolCache[i].table = 0;
            symbolCache[i].symbol = 0;
            symbolCache[i].lastAccess = 0;
        }
        symbolCacheInitialized = true;
        ESP_LOGI(TAG, "Symbol cache initialized");
    }

    uint8_t* loadSymbolFromSD(char table, char symbol) {
        String tableName = (table == '/') ? "primary" : "alternate";
        char hexCode[3];
        snprintf(hexCode, sizeof(hexCode), "%02X", (uint8_t)symbol);
        String path = String("/LoRa_Tracker/Symbols/") + tableName + "/" + hexCode + ".png";

        if (!STORAGE_Utils::isSDAvailable()) {
            ESP_LOGE(TAG, "SD not available for symbol load");
            return nullptr;
        }

        const size_t rgb565Size = SYMBOL_SIZE * SYMBOL_SIZE * sizeof(uint16_t);
        const size_t alphaSize  = SYMBOL_SIZE * SYMBOL_SIZE;
        const size_t totalSize  = rgb565Size + alphaSize;

        uint8_t* combined = (uint8_t*)ps_malloc(totalSize);
        if (!combined) {
            ESP_LOGE(TAG, "PSRAM allocation failed for symbol");
            return nullptr;
        }
        memset(combined, 0, totalSize);

        symbolCombinedBuffer = combined;

        int rc = symbolPNG.open(path.c_str(), pngOpenFile, pngCloseFile,
                                pngReadFile, pngSeekFile, pngSymbolCallback);
        if (rc == PNG_SUCCESS && pngFileOpened) {
            symbolPNG.decode(nullptr, 0);
            symbolPNG.close();
        } else {
            if (rc != PNG_SUCCESS)
                ESP_LOGE(TAG, "PNG open failed rc=%d for %s", rc, path.c_str());
            free(combined);
            symbolCombinedBuffer = nullptr;
            return nullptr;
        }

        symbolCombinedBuffer = nullptr;
        return combined;
    }

    CachedSymbol* getSymbolCacheEntry(char table, char symbol) {
        if (!symbolCacheInitialized) initSymbolCache();

        symbolCacheAccessCounter++;

        // Search existing entry
        for (int i = 0; i < SYMBOL_CACHE_SIZE; i++) {
            if (symbolCache[i].valid &&
                symbolCache[i].table  == table &&
                symbolCache[i].symbol == symbol) {
                symbolCache[i].lastAccess = symbolCacheAccessCounter;
                return &symbolCache[i];
            }
        }

        // Cache miss — find LRU slot
        int lruIdx = 0;
        uint32_t lruTime = symbolCache[0].lastAccess;
        for (int i = 1; i < SYMBOL_CACHE_SIZE; i++) {
            if (!symbolCache[i].valid) { lruIdx = i; break; }
            if (symbolCache[i].lastAccess < lruTime) {
                lruTime = symbolCache[i].lastAccess;
                lruIdx  = i;
            }
        }

        // Evict existing entry if needed
        if (symbolCache[lruIdx].data) {
            free(symbolCache[lruIdx].data);
            symbolCache[lruIdx].data = nullptr;
        }
        symbolCache[lruIdx].valid = false;

        // Load from SD
        uint8_t* data = loadSymbolFromSD(table, symbol);
        if (!data) return nullptr;

        const size_t rgb565Size = SYMBOL_SIZE * SYMBOL_SIZE * sizeof(uint16_t);
        const size_t alphaSize  = SYMBOL_SIZE * SYMBOL_SIZE;

        symbolCache[lruIdx].data       = data;
        symbolCache[lruIdx].valid      = true;
        symbolCache[lruIdx].table      = table;
        symbolCache[lruIdx].symbol     = symbol;
        symbolCache[lruIdx].lastAccess = symbolCacheAccessCounter;

        symbolCache[lruIdx].img_dsc.header.always_zero = 0;
        symbolCache[lruIdx].img_dsc.header.w  = SYMBOL_SIZE;
        symbolCache[lruIdx].img_dsc.header.h  = SYMBOL_SIZE;
        symbolCache[lruIdx].img_dsc.data_size = rgb565Size + alphaSize;
        symbolCache[lruIdx].img_dsc.header.cf = LV_IMG_CF_RGB565A8;
        symbolCache[lruIdx].img_dsc.data      = data;

        return &symbolCache[lruIdx];
    }

    // -------------------------------------------------------------------------
    // Tile loading
    // -------------------------------------------------------------------------

    bool loadTileFromSD(int tileX, int tileY, int zoom,
                        lv_obj_t* canvas, int offsetX, int offsetY) {
        if (spiMutex == NULL) {
            ESP_LOGE(TAG, "spiMutex is NULL, skipping SD access");
            return false;
        }

        // 1. Check cache
        MapEngine::CachedTile* cacheSlot = MapEngine::getRasterCacheSlot(zoom, tileX, tileY);
        if (!cacheSlot) return false;

        if (cacheSlot->isValid) {
            MapEngine::copySpriteToCanvasWithClip(canvas, cacheSlot->sprite, offsetX, offsetY);
            return true;
        }

        // 2. Negative cache
        uint32_t tileHash = cacheSlot->tileHash;
        for (const auto& hash : notFoundCache) {
            if (hash == tileHash) return false;
        }

        // 3. Find tile file on SD
        char path[128], found_path[128] = {0};
        bool found = false;

        if (map_current_region.isEmpty()) {
            ESP_LOGE(TAG, "loadTileFromSD: region empty, cannot load %d/%d/%d", zoom, tileX, tileY);
            return false;
        }

        if (xSemaphoreTake(spiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            if (STORAGE_Utils::isSDAvailable()) {
                const char* region = map_current_region.c_str();
                snprintf(path, sizeof(path), "/LoRa_Tracker/Maps/%s/%d/%d/%d.png",
                         region, zoom, tileX, tileY);
                if (SD.exists(path)) { strcpy(found_path, path); found = true; }
                else {
                    snprintf(path, sizeof(path), "/LoRa_Tracker/Maps/%s/%d/%d/%d.jpg",
                             region, zoom, tileX, tileY);
                    if (SD.exists(path)) { strcpy(found_path, path); found = true; }
                }
            }
            xSemaphoreGive(spiMutex);
        }

        // 4. Not found — add to negative cache
        if (!found) {
            if ((int)notFoundCache.size() < NOT_FOUND_CACHE_SIZE)
                notFoundCache.push_back(tileHash);
            else {
                notFoundCache[notFoundCacheIndex] = tileHash;
                notFoundCacheIndex = (notFoundCacheIndex + 1) % NOT_FOUND_CACHE_SIZE;
            }
            return false;
        }

        // 5. Decode into cache slot sprite
        LGFX_Sprite* tileSprite = cacheSlot->sprite;
        bool decoded = false;
        if (MapEngine::spriteMutex &&
            xSemaphoreTake(MapEngine::spriteMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
            decoded = MapEngine::renderTile(found_path, 0, 0, *tileSprite, (uint8_t)zoom);
            xSemaphoreGive(MapEngine::spriteMutex);
        }

        if (decoded) {
            MapEngine::copySpriteToCanvasWithClip(canvas, tileSprite, offsetX, offsetY);
            strncpy(cacheSlot->filePath, found_path, sizeof(cacheSlot->filePath) - 1);
            cacheSlot->filePath[sizeof(cacheSlot->filePath) - 1] = '\0';
            cacheSlot->isValid = true;
            return true;
        }
        return false;
    }

    bool preloadTileToCache(int tileX, int tileY, int zoom) {
        if (navModeActive) return false;

        MapEngine::CachedTile* cacheSlot = MapEngine::getRasterCacheSlot(zoom, tileX, tileY);
        if (cacheSlot->isValid) return true;

        char path[128], found_path[128] = {0};
        bool found = false;

        if (spiMutex != NULL && xSemaphoreTake(spiMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
            if (STORAGE_Utils::isSDAvailable()) {
                const char* region = map_current_region.c_str();
                snprintf(path, sizeof(path), "/LoRa_Tracker/Maps/%s/%d/%d/%d.png",
                         region, zoom, tileX, tileY);
                if (SD.exists(path)) { strcpy(found_path, path); found = true; }
                else {
                    snprintf(path, sizeof(path), "/LoRa_Tracker/Maps/%s/%d/%d/%d.jpg",
                             region, zoom, tileX, tileY);
                    if (SD.exists(path)) { strcpy(found_path, path); found = true; }
                }
            }
            xSemaphoreGive(spiMutex);
        }

        if (!found) { cacheSlot->isValid = false; return false; }

        LGFX_Sprite* tileSprite = cacheSlot->sprite;
        if (!tileSprite) { cacheSlot->isValid = false; return false; }

        bool success = false;
        if (MapEngine::spriteMutex &&
            xSemaphoreTake(MapEngine::spriteMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
            success = MapEngine::renderTile(found_path, 0, 0, *tileSprite, (uint8_t)zoom);
            xSemaphoreGive(MapEngine::spriteMutex);
        }

        if (success) {
            strncpy(cacheSlot->filePath, found_path, sizeof(cacheSlot->filePath) - 1);
            cacheSlot->filePath[sizeof(cacheSlot->filePath) - 1] = '\0';
            cacheSlot->isValid = true;
        } else {
            cacheSlot->isValid = false;
        }
        return success;
    }

    // -------------------------------------------------------------------------
    // Tile preload task
    // -------------------------------------------------------------------------

    void startTilePreloadTask() {
        if (tilePreloadTask != nullptr) return;
        if (tilePreloadQueue == nullptr)
            tilePreloadQueue = xQueueCreate(TILE_PRELOAD_QUEUE_SIZE, sizeof(TileRequest));
        preloadTaskRunning = true;
        xTaskCreatePinnedToCore(tilePreloadTaskFunc, "TilePreload", 4096,
                                NULL, 1, &tilePreloadTask, 1);
    }

    void stopTilePreloadTask() {
        if (tilePreloadTask == nullptr) return;
        preloadTaskRunning = false;
        vTaskDelay(pdMS_TO_TICKS(200));
        tilePreloadTask = nullptr;
    }

    // -------------------------------------------------------------------------
    // Region discovery
    // -------------------------------------------------------------------------

    void discoverDefaultPosition() {
        if (map_current_region.isEmpty()) return;

        const int zoom = 6;
        char path[128];
        snprintf(path, sizeof(path), "/LoRa_Tracker/Maps/%s/%d",
                 map_current_region.c_str(), zoom);

        int xMin = INT_MAX, xMax = INT_MIN;
        int yMin = INT_MAX, yMax = INT_MIN;

        if (spiMutex != NULL &&
            xSemaphoreTake(spiMutex, pdMS_TO_TICKS(300)) == pdTRUE) {
            if (STORAGE_Utils::isSDAvailable()) {
                File zoomDir = SD.open(path);
                if (zoomDir && zoomDir.isDirectory()) {
                    File xEntry = zoomDir.openNextFile();
                    while (xEntry) {
                        if (xEntry.isDirectory()) {
                            String xName = String(xEntry.name());
                            int tx = xName.substring(xName.lastIndexOf('/') + 1).toInt();
                            if (tx < xMin) xMin = tx;
                            if (tx > xMax) xMax = tx;
                            File yEntry = xEntry.openNextFile();
                            while (yEntry) {
                                String yName = String(yEntry.name());
                                String base = yName.substring(yName.lastIndexOf('/') + 1);
                                int dotIdx = base.lastIndexOf('.');
                                if (dotIdx > 0) base = base.substring(0, dotIdx);
                                int ty = base.toInt();
                                if (ty < yMin) yMin = ty;
                                if (ty > yMax) yMax = ty;
                                yEntry.close();
                                yEntry = xEntry.openNextFile();
                            }
                        }
                        xEntry.close();
                        xEntry = zoomDir.openNextFile();
                    }
                }
                zoomDir.close();
            }
            xSemaphoreGive(spiMutex);
        }

        if (xMin <= xMax && yMin <= yMax) {
            int cx = (xMin + xMax) / 2;
            int cy = (yMin + yMax) / 2;
            MapMath::tileToLatLon(cx, cy, zoom, &defaultLat, &defaultLon);
            ESP_LOGI(TAG, "Default position from Z%d: %.4f,%.4f (X:%d-%d Y:%d-%d)",
                          zoom, defaultLat, defaultLon, xMin, xMax, yMin, yMax);
        } else {
            ESP_LOGW(TAG, "No Z%d raster tiles found, default 0,0", zoom);
        }
    }

    void discoverAndSetMapRegion() {
        if (!map_current_region.isEmpty()) return;

        ESP_LOGI(TAG, "Discovering map region...");
        if (spiMutex != NULL &&
            xSemaphoreTake(spiMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
            if (STORAGE_Utils::isSDAvailable()) {
                File mapsDir = SD.open("/LoRa_Tracker/Maps");
                if (mapsDir && mapsDir.isDirectory()) {
                    File entry = mapsDir.openNextFile();
                    while (entry) {
                        if (entry.isDirectory()) {
                            String dirName = String(entry.name());
                            map_current_region = dirName.substring(
                                dirName.lastIndexOf('/') + 1);
                            ESP_LOGI(TAG, "Region: %s", map_current_region.c_str());
                            entry.close();
                            break;
                        }
                        entry.close();
                        entry = mapsDir.openNextFile();
                    }
                } else {
                    ESP_LOGE(TAG, "Cannot open /LoRa_Tracker/Maps");
                }
                mapsDir.close();
            }
            xSemaphoreGive(spiMutex);
        } else {
            ESP_LOGE(TAG, "Cannot get SPI mutex for region discovery");
        }

        if (map_current_region.isEmpty())
            ESP_LOGW(TAG, "No map region found on SD");
    }

    bool regionContainsTile(const char* region, int zoom, int tileX, int tileY) {
        char path[128];
        snprintf(path, sizeof(path), "/LoRa_Tracker/VectMaps/%s/Z%d.nav", region, zoom);
        File f = SD.open(path, FILE_READ);
        if (!f) return false;

        UIMapManager::Npk2Header hdr;
        if (f.read((uint8_t*)&hdr, sizeof(hdr)) != sizeof(hdr) ||
            memcmp(hdr.magic, "NPK2", 4) != 0) {
            f.close();
            return false;
        }
        f.close();

        uint32_t ty = (uint32_t)tileY;
        return (ty >= hdr.y_min && ty <= hdr.y_max);
    }

    void discoverNavRegions() {
        if (navRegionCount > 0) return;

        ESP_LOGI(TAG, "Discovering NAV regions...");

        const int checkZoom = 10;
        int centerTX, centerTY;
        MapMath::latLonToTile(map_center_lat, map_center_lon, checkZoom, &centerTX, &centerTY);

        int gpsMatchIdx = -1;

        if (spiMutex != NULL &&
            xSemaphoreTake(spiMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
            if (STORAGE_Utils::isSDAvailable()) {
                File vectDir = SD.open("/LoRa_Tracker/VectMaps");
                if (vectDir && vectDir.isDirectory()) {
                    File entry = vectDir.openNextFile();
                    while (entry && navRegionCount < NAV_MAX_REGIONS) {
                        if (entry.isDirectory()) {
                            String dirName = String(entry.name());
                            String regionName = dirName.substring(
                                dirName.lastIndexOf('/') + 1);
                            navRegions[navRegionCount] = regionName;

                            if (gpsMatchIdx < 0 &&
                                regionContainsTile(regionName.c_str(),
                                                   checkZoom, centerTX, centerTY)) {
                                gpsMatchIdx = navRegionCount;
                                ESP_LOGI(TAG, "NAV GPS match: %s (tile %d/%d Z%d)",
                                              regionName.c_str(), centerTX, centerTY, checkZoom);
                            } else {
                                ESP_LOGI(TAG, "NAV region: %s", regionName.c_str());
                            }
                            navRegionCount++;
                        }
                        entry.close();
                        entry = vectDir.openNextFile();
                    }
                }
                vectDir.close();
            }
            xSemaphoreGive(spiMutex);
        }

        if (gpsMatchIdx > 0) {
            String tmp = navRegions[0];
            navRegions[0] = navRegions[gpsMatchIdx];
            navRegions[gpsMatchIdx] = tmp;
        }

        if (navRegionCount == 0)
            ESP_LOGW(TAG, "No NAV region found on SD");
        else
            ESP_LOGI(TAG, "%d NAV region(s), primary: %s",
                          navRegionCount, navRegions[0].c_str());
    }

    // -------------------------------------------------------------------------
    // Zoom table & center tile
    // -------------------------------------------------------------------------

    void switchZoomTable(const int* newTable, int newCount) {
        map_available_zooms = newTable;
        map_zoom_count = newCount;
        int bestIdx = 0;
        int bestDiff = abs(map_current_zoom - newTable[0]);
        for (int i = 1; i < newCount; i++) {
            int diff = abs(map_current_zoom - newTable[i]);
            if (diff < bestDiff) { bestDiff = diff; bestIdx = i; }
        }
        map_zoom_index = bestIdx;
        map_current_zoom = newTable[bestIdx];
    }

    void initCenterTileFromLatLon(float lat, float lon) {
        MapMath::latLonToTile(lat, lon, map_current_zoom, &centerTileX, &centerTileY);
        renderTileX = centerTileX;
        renderTileY = centerTileY;
        map_center_lat = lat;
        map_center_lon = lon;
    }

} // namespace MapTiles

#endif // USE_LVGL_UI
