/* Map logic for T-Deck Plus
 * Offline map tile display with stations using LVGL
 */

#ifdef USE_LVGL_UI

#include <Arduino.h>
#include <FS.h>
#include <lvgl.h>
#include <TFT_eSPI.h> // For TFT_eSPI definitions if needed (e.g. for SCREEN_WIDTH/HEIGHT)
#include <TinyGPS++.h>
#include <JPEGDEC.h>
// Undefine macros that conflict between PNGdec and JPEGDEC
#undef INTELSHORT
#undef INTELLONG
#undef MOTOSHORT
#undef MOTOLONG
#include <PNGdec.h>
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "ui_map_manager.h"
#include "configuration.h"
#include "station_utils.h"
#include "utils.h"
#include "storage_utils.h"
#include "custom_characters.h" // For symbolsAPRS, SYMBOL_WIDTH, SYMBOL_HEIGHT
#include "lvgl_ui.h" // To call LVGL_UI::open_compose_with_callsign

namespace UIMapManager {

    // UI elements - Map screen
    lv_obj_t* screen_map = nullptr;
    lv_obj_t* map_canvas = nullptr;
    lv_color_t* map_canvas_buf = nullptr;
    lv_obj_t* map_title_label = nullptr;
    lv_obj_t* map_container = nullptr;

    // Map state variables
    static const int map_available_zooms[] = {8, 10, 12, 14}; // Available zoom levels (only levels with tiles on SD card)
    const int map_zoom_count = sizeof(map_available_zooms) / sizeof(map_available_zooms[0]);
    int map_zoom_index = 0;  // Index in map_available_zooms (starts at zoom 8)
    int map_current_zoom = map_available_zooms[0]; // Initialize with first available zoom
    float map_center_lat = 0.0f;
    float map_center_lon = 0.0f;
    String map_current_region = "";
    static String cachedMapsPath = "";      // Cached maps path (avoid repeated SD access)
    static bool mapsPathCached = false;     // Flag to check if path is cached
    bool map_follow_gps = true;  // Follow GPS or free panning mode

    // Touch pan state
    static bool touch_dragging = false;
    static lv_coord_t touch_start_x = 0;
    static lv_coord_t touch_start_y = 0;
    static float drag_start_lat = 0.0f;
    static float drag_start_lon = 0.0f;
    #define PAN_THRESHOLD 5  // Minimum pixels to trigger pan

    // Tile cache in PSRAM
    #define TILE_CACHE_SIZE 40  // Number of tiles to cache (~5MB in PSRAM)
    #define TILE_DATA_SIZE (MAP_TILE_SIZE * MAP_TILE_SIZE * sizeof(uint16_t))  // 128KB per tile

    struct CachedTile {
        int zoom;
        int tileX;
        int tileY;
        uint16_t* data;      // Decoded tile pixels in PSRAM
        uint32_t lastAccess; // For LRU eviction
        bool valid;
    };

    static CachedTile tileCache[TILE_CACHE_SIZE];
    static uint32_t tileCacheAccessCounter = 0;
    static bool tileCacheInitialized = false;

    // Symbol cache in PSRAM
    #define SYMBOL_CACHE_SIZE 30  // Cache for frequently used symbols
    #define SYMBOL_SIZE 24        // 24x24 pixels
    #define SYMBOL_DATA_SIZE (SYMBOL_SIZE * SYMBOL_SIZE * sizeof(lv_color_t))

    struct CachedSymbol {
        char table;              // '/' for primary, '\' for alternate
        char symbol;             // ASCII character
        lv_img_dsc_t img_dsc;   // LVGL image descriptor
        lv_color_t* data;        // Symbol pixels in PSRAM
        uint8_t* alpha;          // Alpha channel in PSRAM (nullptr if no alpha)
        uint32_t lastAccess;     // For LRU eviction
        bool valid;
    };

    static CachedSymbol symbolCache[SYMBOL_CACHE_SIZE];
    static uint32_t symbolCacheAccessCounter = 0;
    static bool symbolCacheInitialized = false;

    // Forward declarations
    void drawStationOnMap(lv_obj_t* canvas, int x, int y, const String& ssid, const char* aprsSymbol);

    // Station clickable buttons tracking (MAP_STATIONS_MAX defined in station_utils.h)
    static lv_obj_t* station_buttons[MAP_STATIONS_MAX] = {nullptr};

    // Periodic refresh timer for stations
    static lv_timer_t* map_refresh_timer = nullptr;
    #define MAP_REFRESH_INTERVAL 10000  // 10 seconds

    // Timer callback for periodic map refresh (stations update)
    static void map_refresh_timer_cb(lv_timer_t* timer) {
        if (screen_map && lv_scr_act() == screen_map) {
            Serial.println("[MAP] Periodic refresh (stations)");
            redraw_map_canvas();
        }
    }

    // ============ ASYNC TILE PRELOADING (Core 1) ============
    // Structure for tile preload request
    struct TileRequest {
        int tileX;
        int tileY;
        int zoom;
    };

    static QueueHandle_t tilePreloadQueue = nullptr;
    static TaskHandle_t tilePreloadTask = nullptr;
    static bool preloadTaskRunning = false;
    static volatile bool mainThreadLoading = false;  // Pause preload while main thread loads
    #define TILE_PRELOAD_QUEUE_SIZE 20

    // Forward declaration
    bool preloadTileToCache(int tileX, int tileY, int zoom);

    // Task running on Core 1 to preload tiles in background
    static void tilePreloadTaskFunc(void* param) {
        Serial.println("[MAP] Tile preload task started on Core 1");
        TileRequest req;

        while (preloadTaskRunning) {
            // Wait while main thread is loading tiles (avoid SD contention)
            if (mainThreadLoading) {
                vTaskDelay(pdMS_TO_TICKS(50));
                continue;
            }

            // Wait for tile request (100ms timeout to check if task should stop)
            if (xQueueReceive(tilePreloadQueue, &req, pdMS_TO_TICKS(100)) == pdTRUE) {
                // Re-check flag after queue receive
                if (mainThreadLoading) continue;

                // Check if tile already in cache
                int cacheIdx = findCachedTile(req.zoom, req.tileX, req.tileY);
                if (cacheIdx < 0) {
                    // Not in cache - preload it
                    Serial.printf("[MAP-ASYNC] Preloading tile %d/%d/%d\n", req.zoom, req.tileX, req.tileY);
                    preloadTileToCache(req.tileX, req.tileY, req.zoom);
                }
            }
        }

        Serial.println("[MAP] Tile preload task stopped");
        vTaskDelete(NULL);
    }

    // Start the preload task
    void startTilePreloadTask() {
        if (tilePreloadTask != nullptr) return;  // Already running

        // Create queue
        if (tilePreloadQueue == nullptr) {
            tilePreloadQueue = xQueueCreate(TILE_PRELOAD_QUEUE_SIZE, sizeof(TileRequest));
        }

        preloadTaskRunning = true;
        xTaskCreatePinnedToCore(
            tilePreloadTaskFunc,
            "TilePreload",
            4096,
            NULL,
            1,  // Low priority
            &tilePreloadTask,
            1   // Core 1
        );
    }

    // Stop the preload task
    void stopTilePreloadTask() {
        if (tilePreloadTask == nullptr) return;

        preloadTaskRunning = false;
        vTaskDelay(pdMS_TO_TICKS(200));  // Wait for task to finish
        tilePreloadTask = nullptr;
    }

    // Queue tiles from adjacent zoom levels for preloading
    void queueAdjacentZoomTiles(int centerTileX, int centerTileY, int currentZoom) {
        if (tilePreloadQueue == nullptr) return;

        TileRequest req;

        // Get adjacent zoom levels
        int prevZoom = -1, nextZoom = -1;
        for (int i = 0; i < map_zoom_count; i++) {
            if (map_available_zooms[i] == currentZoom) {
                if (i > 0) prevZoom = map_available_zooms[i - 1];
                if (i < map_zoom_count - 1) nextZoom = map_available_zooms[i + 1];
                break;
            }
        }

        // Queue tiles for previous zoom (zoom out = tiles cover larger area)
        if (prevZoom > 0) {
            int scale = 1 << (currentZoom - prevZoom);  // e.g., zoom 12->10 = scale 4
            int prevTileX = centerTileX / scale;
            int prevTileY = centerTileY / scale;

            // Queue 3x3 grid around the corresponding tile
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    req.tileX = prevTileX + dx;
                    req.tileY = prevTileY + dy;
                    req.zoom = prevZoom;
                    xQueueSend(tilePreloadQueue, &req, 0);  // Don't block
                }
            }
        }

        // Queue tiles for next zoom (zoom in = tiles cover smaller area)
        if (nextZoom > 0) {
            int scale = 1 << (nextZoom - currentZoom);  // e.g., zoom 12->14 = scale 4
            int nextTileX = centerTileX * scale;
            int nextTileY = centerTileY * scale;

            // Queue 3x3 grid around the corresponding tile
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    req.tileX = nextTileX + dx;
                    req.tileY = nextTileY + dy;
                    req.zoom = nextZoom;
                    xQueueSend(tilePreloadQueue, &req, 0);  // Don't block
                }
            }
        }
    }
    // ============ END ASYNC TILE PRELOADING ============

    // Clean up old station buttons
    void cleanup_station_buttons() {
        for (int i = 0; i < MAP_STATIONS_MAX; i++) {
            if (station_buttons[i] && lv_obj_is_valid(station_buttons[i])) {
                lv_obj_del(station_buttons[i]);
            }
            station_buttons[i] = nullptr;
        }
    }

    // Create clickable buttons for map stations
    void create_station_buttons() {
        if (!map_container) return;

        // Draw stations and create clickable buttons
        STATION_Utils::cleanOldMapStations();
        for (int i = 0; i < MAP_STATIONS_MAX; i++) {
            MapStation* station = STATION_Utils::getMapStation(i);
            if (station && station->valid && station->latitude != 0.0f && station->longitude != 0.0f) {
                int stX, stY;
                latLonToPixel(station->latitude, station->longitude,
                              map_center_lat, map_center_lon, map_current_zoom, &stX, &stY);

                if (stX >= 0 && stX < MAP_CANVAS_WIDTH && stY >= 0 && stY < MAP_CANVAS_HEIGHT) {
                    // Draw station with symbol + SSID
                    drawStationOnMap(map_canvas, stX, stY, station->callsign, station->symbol.c_str());

                    // Create clickable transparent button over station
                    // Zone covers symbol (24x24) + SSID (label below)
                    station_buttons[i] = lv_btn_create(map_container);
                    lv_obj_set_size(station_buttons[i], 60, 45);  // Touch zone: symbol + SSID + margin
                    lv_obj_set_pos(station_buttons[i], stX - 30, stY - 12);  // Center on symbol + SSID
                    lv_obj_set_style_bg_opa(station_buttons[i], LV_OPA_TRANSP, 0);  // Transparent
                    lv_obj_set_style_border_width(station_buttons[i], 0, 0);
                    lv_obj_set_style_shadow_width(station_buttons[i], 0, 0);
                    lv_obj_add_event_cb(station_buttons[i], map_station_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)i);
                }
            }
        }
    }

    // Initialize symbol cache
    void initSymbolCache() {
        if (symbolCacheInitialized) return;
        for (int i = 0; i < SYMBOL_CACHE_SIZE; i++) {
            symbolCache[i].data = nullptr;
            symbolCache[i].alpha = nullptr;
            symbolCache[i].valid = false;
            symbolCache[i].table = 0;
            symbolCache[i].symbol = 0;
            symbolCache[i].lastAccess = 0;
        }
        symbolCacheInitialized = true;
        Serial.println("[MAP] Symbol cache initialized");
    }

    // Forward declarations for PNG callbacks
    static void* pngOpenFile(const char* filename, int32_t* size);
    static void pngCloseFile(void* handle);
    static int32_t pngReadFile(PNGFILE* pFile, uint8_t* pBuf, int32_t iLen);
    static int32_t pngSeekFile(PNGFILE* pFile, int32_t iPosition);
    static bool pngFileOpened = false;  // Track if PNG file actually opened

    // PNG draw callback for symbols - stores alpha channel info
    static uint16_t* symbolDecodeBuffer = nullptr;
    static uint8_t* symbolAlphaBuffer = nullptr;  // Alpha channel (0-255)
    static PNG symbolPNG;  // PNG decoder instance for symbols

    static int pngSymbolCallback(PNGDRAW* pDraw) {
        if (!symbolDecodeBuffer) return 1;

        // Decode line as RGB565
        symbolPNG.getLineAsRGB565(pDraw, &symbolDecodeBuffer[pDraw->y * SYMBOL_SIZE],
                                  PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);

        // Extract alpha channel if present
        if (symbolAlphaBuffer && pDraw->iHasAlpha) {
            uint8_t* alpha = &symbolAlphaBuffer[pDraw->y * SYMBOL_SIZE];
            for (int x = 0; x < pDraw->iWidth; x++) {
                alpha[x] = pDraw->pPixels[x * 4 + 3];  // RGBA format, get A
            }
        }

        return 1;
    }

    // Load symbol PNG from SD card
    lv_color_t* loadSymbolFromSD(char table, char symbol) {
        // Build path: /LoRa_Tracker/Symbols/primary/5B.png or /alternate/26.png
        String tableName = (table == '/') ? "primary" : "alternate";
        char hexCode[3];
        snprintf(hexCode, sizeof(hexCode), "%02X", (uint8_t)symbol);
        String path = String("/LoRa_Tracker/Symbols/") + tableName + "/" + hexCode + ".png";

        if (!STORAGE_Utils::isSDAvailable()) {
            Serial.println("[SYMBOL] SD not available");
            return nullptr;
        }

        // Allocate buffer in PSRAM for decoded image
        lv_color_t* imgBuf = (lv_color_t*)ps_malloc(SYMBOL_DATA_SIZE);
        if (!imgBuf) {
            Serial.println("[SYMBOL] PSRAM allocation failed");
            return nullptr;
        }

        // Allocate alpha channel buffer in PSRAM (24x24 = 576 bytes)
        symbolAlphaBuffer = (uint8_t*)ps_malloc(SYMBOL_SIZE * SYMBOL_SIZE);
        if (!symbolAlphaBuffer) {
            Serial.println("[SYMBOL] PSRAM allocation failed for alpha");
            free(imgBuf);
            return nullptr;
        }

        // Setup decode buffer for callback
        symbolDecodeBuffer = (uint16_t*)imgBuf;

        // Decode PNG using PNGdec
        int rc = symbolPNG.open(path.c_str(), pngOpenFile, pngCloseFile, pngReadFile, pngSeekFile, pngSymbolCallback);
        if (rc == PNG_SUCCESS && pngFileOpened) {
            rc = symbolPNG.decode(nullptr, 0);
            symbolPNG.close();

            if (rc == PNG_SUCCESS) {
                symbolDecodeBuffer = nullptr;
                // Keep symbolAlphaBuffer set - will be grabbed by getSymbol()
                Serial.printf("[SYMBOL] Loaded: %c%c from %s\n", table, symbol, path.c_str());
                return imgBuf;
            }
        }

        // Failed to decode
        symbolPNG.close();
        symbolDecodeBuffer = nullptr;
        if (symbolAlphaBuffer) {
            free(symbolAlphaBuffer);
            symbolAlphaBuffer = nullptr;
        }
        free(imgBuf);
        Serial.printf("[SYMBOL] Failed to load: %s\n", path.c_str());
        return nullptr;
    }

    // Get symbol cache entry from cache or load from SD
    CachedSymbol* getSymbolCacheEntry(char table, char symbol) {
        initSymbolCache();

        // Search in cache
        for (int i = 0; i < SYMBOL_CACHE_SIZE; i++) {
            if (symbolCache[i].valid && symbolCache[i].table == table && symbolCache[i].symbol == symbol) {
                symbolCache[i].lastAccess = symbolCacheAccessCounter++;
                return &symbolCache[i];
            }
        }

        // Not in cache - load from SD
        lv_color_t* data = loadSymbolFromSD(table, symbol);
        if (!data) {
            return nullptr;  // Symbol not found
        }

        // Find LRU slot or empty slot
        int slotIdx = -1;
        uint32_t oldestAccess = UINT32_MAX;
        for (int i = 0; i < SYMBOL_CACHE_SIZE; i++) {
            if (!symbolCache[i].valid) {
                slotIdx = i;
                break;
            }
            if (symbolCache[i].lastAccess < oldestAccess) {
                oldestAccess = symbolCache[i].lastAccess;
                slotIdx = i;
            }
        }

        // Evict old entry if needed
        if (symbolCache[slotIdx].valid) {
            if (symbolCache[slotIdx].data) {
                free(symbolCache[slotIdx].data);
            }
            if (symbolCache[slotIdx].alpha) {
                free(symbolCache[slotIdx].alpha);
            }
        }

        // Store in cache (including alpha buffer from loadSymbolFromSD)
        symbolCache[slotIdx].table = table;
        symbolCache[slotIdx].symbol = symbol;
        symbolCache[slotIdx].data = data;
        symbolCache[slotIdx].alpha = symbolAlphaBuffer;  // Grab alpha from global
        symbolAlphaBuffer = nullptr;  // Reset global
        symbolCache[slotIdx].lastAccess = symbolCacheAccessCounter++;
        symbolCache[slotIdx].valid = true;

        // Setup LVGL image descriptor
        symbolCache[slotIdx].img_dsc.header.always_zero = 0;
        symbolCache[slotIdx].img_dsc.header.w = SYMBOL_SIZE;
        symbolCache[slotIdx].img_dsc.header.h = SYMBOL_SIZE;
        symbolCache[slotIdx].img_dsc.data_size = SYMBOL_DATA_SIZE;
        symbolCache[slotIdx].img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
        symbolCache[slotIdx].img_dsc.data = (uint8_t*)data;

        return &symbolCache[slotIdx];
    }

    // Get symbol from cache or load from SD (backward compatibility wrapper)
    lv_img_dsc_t* getSymbol(char table, char symbol) {
        CachedSymbol* cache = getSymbolCacheEntry(table, symbol);
        return cache ? &cache->img_dsc : nullptr;
    }

    // Initialize tile cache
    void initTileCache() {
        if (tileCacheInitialized) return;
        for (int i = 0; i < TILE_CACHE_SIZE; i++) {
            tileCache[i].data = nullptr;
            tileCache[i].valid = false;
            tileCache[i].zoom = -1;
            tileCache[i].tileX = -1;
            tileCache[i].tileY = -1;
            tileCache[i].lastAccess = 0;
        }
        tileCacheInitialized = true;
        Serial.println("[MAP] Tile cache initialized");
    }

    // Find a tile in cache, returns index or -1
    int findCachedTile(int zoom, int tileX, int tileY) {
        for (int i = 0; i < TILE_CACHE_SIZE; i++) {
            if (tileCache[i].valid &&
                tileCache[i].zoom == zoom &&
                tileCache[i].tileX == tileX &&
                tileCache[i].tileY == tileY) {
                tileCache[i].lastAccess = ++tileCacheAccessCounter;
                return i;
            }
        }
        return -1;
    }

    // Find a slot for a new tile (empty or LRU)
    int findCacheSlot() {
        // First look for an empty slot
        for (int i = 0; i < TILE_CACHE_SIZE; i++) {
            if (!tileCache[i].valid || tileCache[i].data == nullptr) {
                return i;
            }
        }
        // Find the LRU (oldest access)
        int lruIndex = 0;
        uint32_t oldestAccess = tileCache[0].lastAccess;
        for (int i = 1; i < TILE_CACHE_SIZE; i++) {
            if (tileCache[i].lastAccess < oldestAccess) {
                oldestAccess = tileCache[i].lastAccess;
                lruIndex = i;
            }
        }
        return lruIndex;
    }

    // JPEG decoder for map tiles
    static JPEGDEC jpeg;

    // Context for JPEG decoding to cache
    struct JPEGCacheContext {
        uint16_t* cacheBuffer;  // Target cache buffer
        int tileWidth;
    };

    static JPEGCacheContext jpegCacheContext;

    // JPEGDEC callback for decoding to cache - called for each MCU block
    static int jpegCacheCallback(JPEGDRAW* pDraw) {
        uint16_t* src = pDraw->pPixels;
        for (int y = 0; y < pDraw->iHeight; y++) {
            int destY = pDraw->y + y;
            if (destY >= MAP_TILE_SIZE) break;
            for (int x = 0; x < pDraw->iWidth; x++) {
                int destX = pDraw->x + x;
                if (destX >= MAP_TILE_SIZE) break;
                jpegCacheContext.cacheBuffer[destY * jpegCacheContext.tileWidth + destX] = src[y * pDraw->iWidth + x];
            }
        }
        return 1;
    }

    // PNG decoder for map tiles
    static PNG png;

    // Context for PNG decoding to cache
    struct PNGCacheContext {
        uint16_t* cacheBuffer;  // Target cache buffer
        int tileWidth;
    };

    static PNGCacheContext pngCacheContext;

    // PNGdec callback for decoding to cache
    static int pngCacheCallback(PNGDRAW* pDraw) {
        png.getLineAsRGB565(pDraw, &pngCacheContext.cacheBuffer[pDraw->y * pngCacheContext.tileWidth],
                            PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);
        return 1;
    }

    // PNG file callbacks
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

    // JPEG file callbacks
    static void* jpegOpenFile(const char* filename, int32_t* size) {
        File* file = new File(SD.open(filename, FILE_READ));
        if (!file || !*file) {
            delete file;
            return nullptr;
        }
        *size = file->size();
        return file;
    }

    static void jpegCloseFile(void* handle) {
        File* file = (File*)handle;
        if (file) {
            file->close();
            delete file;
        }
    }

    static int32_t jpegReadFile(JPEGFILE* pFile, uint8_t* pBuf, int32_t iLen) {
        File* file = (File*)pFile->fHandle;
        return file->read(pBuf, iLen);
    }

    static int32_t jpegSeekFile(JPEGFILE* pFile, int32_t iPosition) {
        File* file = (File*)pFile->fHandle;
        return file->seek(iPosition);
    }

    // Preload a tile into cache (no canvas drawing) - called from Core 1 task
    bool preloadTileToCache(int tileX, int tileY, int zoom) {
        initTileCache();

        // Check if already in cache
        if (findCachedTile(zoom, tileX, tileY) >= 0) {
            return true;  // Already cached
        }

        bool success = false;

        // Protect SPI bus access with mutex
        if (spiMutex != NULL && xSemaphoreTakeRecursive(spiMutex, portMAX_DELAY) == pdTRUE) {

            if (STORAGE_Utils::isSDAvailable() && mapsPathCached && map_current_region.length() > 0) {
                char tilePath[128];
                int slot = findCacheSlot();

                if (tileCache[slot].data == nullptr) {
                    tileCache[slot].data = (uint16_t*)heap_caps_malloc(TILE_DATA_SIZE, MALLOC_CAP_SPIRAM);
                }

                if (tileCache[slot].data) {
                    // Try JPEG first
                    snprintf(tilePath, sizeof(tilePath), "%s/%s/%d/%d/%d.jpg",
                             cachedMapsPath.c_str(), map_current_region.c_str(), zoom, tileX, tileY);

                    jpegCacheContext.cacheBuffer = tileCache[slot].data;
                    jpegCacheContext.tileWidth = MAP_TILE_SIZE;

                    int rc = jpeg.open(tilePath, jpegOpenFile, jpegCloseFile, jpegReadFile, jpegSeekFile, jpegCacheCallback);
                    if (rc) {
                        jpeg.setPixelType(RGB565_LITTLE_ENDIAN);
                        rc = jpeg.decode(0, 0, 0);
                        jpeg.close();

                        if (rc) {
                            tileCache[slot].zoom = zoom;
                            tileCache[slot].tileX = tileX;
                            tileCache[slot].tileY = tileY;
                            tileCache[slot].lastAccess = ++tileCacheAccessCounter;
                            tileCache[slot].valid = true;
                            success = true;
                        }
                    }

                    // Try PNG as fallback
                    if (!success) {
                        snprintf(tilePath, sizeof(tilePath), "%s/%s/%d/%d/%d.png",
                                 cachedMapsPath.c_str(), map_current_region.c_str(), zoom, tileX, tileY);

                        pngCacheContext.cacheBuffer = tileCache[slot].data;
                        pngCacheContext.tileWidth = MAP_TILE_SIZE;

                        rc = png.open(tilePath, pngOpenFile, pngCloseFile, pngReadFile, pngSeekFile, pngCacheCallback);
                        if (rc == PNG_SUCCESS && pngFileOpened) {
                            rc = png.decode(nullptr, 0);
                            png.close();

                            if (rc == PNG_SUCCESS) {
                                tileCache[slot].zoom = zoom;
                                tileCache[slot].tileX = tileX;
                                tileCache[slot].tileY = tileY;
                                tileCache[slot].lastAccess = ++tileCacheAccessCounter;
                                tileCache[slot].valid = true;
                                success = true;
                            }
                        } else {
                            png.close();
                        }
                    }
                }
            }
            xSemaphoreGiveRecursive(spiMutex);
        }
        return success;
    }

    // Copy cached tile to canvas with offset and clipping
    void copyTileToCanvas(uint16_t* tileData, lv_color_t* canvasBuffer,
                                 int offsetX, int offsetY, int canvasWidth, int canvasHeight) {
        for (int ty = 0; ty < MAP_TILE_SIZE; ty++) {
            int cy = offsetY + ty;
            if (cy < 0 || cy >= canvasHeight) continue;

            for (int tx = 0; tx < MAP_TILE_SIZE; tx++) {
                int cx = offsetX + tx;
                if (cx < 0 || cx >= canvasWidth) continue;

                int canvasIdx = cy * canvasWidth + cx;
                int tileIdx = ty * MAP_TILE_SIZE + tx;
                canvasBuffer[canvasIdx].full = tileData[tileIdx];
            }
        }
    }

    // Convert lat/lon to tile coordinates
    void latLonToTile(float lat, float lon, int zoom, int* tileX, int* tileY) {
        int n = 1 << zoom;
        *tileX = (int)((lon + 180.0f) / 360.0f * n);
        float latRad = lat * PI / 180.0f;
        *tileY = (int)((1.0f - log(tan(latRad) + 1.0f / cos(latRad)) / PI) / 2.0f * n);
    }

    // Convert lat/lon to pixel position on screen (relative to center)
    void latLonToPixel(float lat, float lon, float centerLat, float centerLon, int zoom, int* pixelX, int* pixelY) {
        int centerTileX, centerTileY;
        latLonToTile(centerLat, centerLon, zoom, &centerTileX, &centerTileY);

        int targetTileX, targetTileY;
        latLonToTile(lat, lon, zoom, &targetTileX, &targetTileY);

        // Calculate sub-tile position
        int n = 1 << zoom;
        float subX = ((lon + 180.0f) / 360.0f * n) - targetTileX;
        float subY = ((1.0f - log(tan(lat * PI / 180.0f) + 1.0f / cos(lat * PI / 180.0f)) / PI) / 2.0f * n) - targetTileY;

        float centerSubX = ((centerLon + 180.0f) / 360.0f * n) - centerTileX;
        float centerSubY = ((1.0f - log(tan(centerLat * PI / 180.0f) + 1.0f / cos(centerLat * PI / 180.0f)) / PI) / 2.0f * n) - centerTileY;

        *pixelX = MAP_CANVAS_WIDTH / 2 + (int)(((targetTileX - centerTileX) + (subX - centerSubX)) * MAP_TILE_SIZE);
        *pixelY = MAP_CANVAS_HEIGHT / 2 + (int)(((targetTileY - centerTileY) + (subY - centerSubY)) * MAP_TILE_SIZE);
    }

    // Get symbol color based on SSID and APRS symbol
    lv_color_t getSymbolColor(const String& ssid, const char* aprsSymbol) {
        // Extract symbol character (always second char in 2-char format)
        // Format: "/X" or "\X" or "OX" where O is overlay
        char symbolChar = ' ';
        if (aprsSymbol && strlen(aprsSymbol) >= 2) {
            symbolChar = aprsSymbol[1];  // Symbol is always second character
        } else if (aprsSymbol && strlen(aprsSymbol) >= 1) {
            symbolChar = aprsSymbol[0];
        }

        // Check symbol first for special cases
        switch (symbolChar) {
            case '&':  // iGate
                return lv_color_hex(0xff0000);  // Red
            case '#':  // Digipeater
                return lv_color_hex(0x0f9600);  // Green
            case '_':  // Weather station (standard APRS weather symbol)
                return lv_color_hex(0x00007f);  // Dark blue
        }

        // Extract SSID suffix for color by SSID
        int dashPos = ssid.lastIndexOf('-');
        int ssidNum = -1;
        if (dashPos >= 0 && dashPos < ssid.length() - 1) {
            ssidNum = ssid.substring(dashPos + 1).toInt();
        }

        // Color by SSID number
        switch (ssidNum) {
            case 7:   // Mobile
            case 9:   // Handheld
                return lv_color_hex(0xff0000);  // Red
            case 13:  // Weather station (custom SSID)
                return lv_color_hex(0x00007f);  // Dark blue
            default:
                return lv_color_hex(0xffff00);  // Yellow
        }
    }

    // Get standard APRS color for symbol (deprecated, use getColorFromSSID instead)
    lv_color_t getAPRSSymbolColor(const char* symbol) {
        if (!symbol || strlen(symbol) < 1) return lv_color_hex(0xffff00);  // Yellow by default

        // The symbol can be a single character "[" or with a table "/[" or overlay "O["
        // Symbol is always second character in 2-char format
        char symbolChar;
        if (strlen(symbol) >= 2) {
            symbolChar = symbol[1];  // Symbol is always second character
        } else {
            symbolChar = symbol[0];
        }

        switch (symbolChar) {
            case '[':  // Human/Jogger
            case 'b':  // Bicycle
                return lv_color_hex(0xff0000);  // Red
            case '>':  // Car
            case 'U':  // Bus
            case 'j':  // Jeep
            case 'k':  // Camion
            case '<':  // Motorcycle
                return lv_color_hex(0x0000ff);  // Blue
            case 's':  // Ship/boat
            case 'Y':  // Yacht
                return lv_color_hex(0x00ffff);  // Cyan
            case '-':  // Home
            case 'y':  // House with yagi
                return lv_color_hex(0x00ff00);  // Green
            case 'a':  // Ambulance
            case 'f':  // Fire truck
            case 'u':  // Fire station
                return lv_color_hex(0xff6600);  // Orange
            case '^':  // Plane
            case '\'': // Small plane
            case 'X':  // Helicopter
                return lv_color_hex(0x00ffff);  // Cyan
            case '&':  // iGate
                return lv_color_hex(0x800080);  // Purple
            default:
                return lv_color_hex(0xffff00);  // Yellow
        }
    }

    // Forward declaration
    void drawMapSymbol(lv_obj_t* canvas, int x, int y, const char* symbolChar, lv_color_t color);

    // Draw custom symbol for special types (iGate, digipeater, weather)
    void drawCustomSymbol(lv_obj_t* canvas, int x, int y, const String& ssid, const char* aprsSymbol, lv_color_t color) {
        // Extract symbol character
        char symbolChar = ' ';
        if (aprsSymbol && strlen(aprsSymbol) >= 2 && (aprsSymbol[0] == '/' || aprsSymbol[0] == '\\')) {
            symbolChar = aprsSymbol[1];
        } else if (aprsSymbol && strlen(aprsSymbol) >= 1) {
            symbolChar = aprsSymbol[0];
        }

        // Extract SSID suffix
        int dashPos = ssid.lastIndexOf('-');
        int ssidNum = -1;
        if (dashPos >= 0 && dashPos < ssid.length() - 1) {
            ssidNum = ssid.substring(dashPos + 1).toInt();
        }

        // Only Weather needs custom symbol (blue circle with "Wx")
        // iGate and Digipeater use PNG symbols with "L" overlay
        lv_draw_rect_dsc_t rect_dsc;
        lv_draw_rect_dsc_init(&rect_dsc);
        lv_draw_label_dsc_t lbl_dsc;
        lv_draw_label_dsc_init(&lbl_dsc);

        // Weather - Blue circle with white "Wx"
        rect_dsc.bg_color = lv_color_hex(0x00007f);  // Dark blue
        rect_dsc.bg_opa = LV_OPA_COVER;
        rect_dsc.border_width = 0;
        rect_dsc.radius = 6;
        lv_canvas_draw_rect(canvas, x - 6, y - 6, 12, 12, &rect_dsc);

        lbl_dsc.color = lv_color_hex(0xffffff);  // White
        lbl_dsc.font = &lv_font_montserrat_14;
        lv_canvas_draw_text(canvas, x - 6, y - 5, 12, &lbl_dsc, "Wx");
    }

    // Draw station on map: symbol + SSID label
    void drawStationOnMap(lv_obj_t* canvas, int x, int y, const String& ssid, const char* aprsSymbol) {
        // Get symbol color based on SSID and APRS symbol
        lv_color_t symbolColor = getSymbolColor(ssid, aprsSymbol);

        // Extract symbol character to check if we need custom rendering
        // Format: "/X" or "\X" or "OX" - symbol is always second character
        char symbolChar = ' ';
        if (aprsSymbol && strlen(aprsSymbol) >= 2) {
            symbolChar = aprsSymbol[1];  // Symbol is always second character
        } else if (aprsSymbol && strlen(aprsSymbol) >= 1) {
            symbolChar = aprsSymbol[0];
        }

        // Extract SSID suffix
        int dashPos = ssid.lastIndexOf('-');
        int ssidNum = -1;
        if (dashPos >= 0 && dashPos < ssid.length() - 1) {
            ssidNum = ssid.substring(dashPos + 1).toInt();
        }

        // Draw standard APRS symbol PNG (includes red diamond for /&, green star for /#, etc.)
        drawMapSymbol(canvas, x, y, aprsSymbol, symbolColor);

        // Add white "L" overlay for iGate, Digipeater, or SSID 1-7
        bool isIGate = (symbolChar == '&');
        bool isDigipeater = (symbolChar == '#' || (ssidNum >= 1 && ssidNum <= 7));

        if (isIGate || isDigipeater) {
            lv_draw_label_dsc_t lbl_dsc;
            lv_draw_label_dsc_init(&lbl_dsc);
            lbl_dsc.color = lv_color_hex(0xffffff);  // White
            lbl_dsc.font = &lv_font_montserrat_14;
            lv_canvas_draw_text(canvas, x - 3, y - 5, 10, &lbl_dsc, "L");
        }

        // For Weather (SSID-13 or symbol _), use custom blue circle
        if (ssidNum == 13 || symbolChar == '_') {
            drawCustomSymbol(canvas, x, y, ssid, aprsSymbol, symbolColor);
            // Skip SSID label for weather - custom symbol already drawn
            return;
        }

        // Draw SSID label below symbol with semi-transparent background
        // Background: GPS gray color #759a9e at 45% opacity
        lv_draw_rect_dsc_t bg_dsc;
        lv_draw_rect_dsc_init(&bg_dsc);
        bg_dsc.bg_color = lv_color_hex(0x759a9e);  // GPS gray
        bg_dsc.bg_opa = (LV_OPA_COVER * 45) / 100;  // 45% opacity (115)
        bg_dsc.radius = 2;  // Slightly rounded corners
        bg_dsc.border_width = 0;

        // Use Montserrat 12 (smallest available normal font)
        int text_width = strlen(ssid.c_str()) * 6;  // ~6px per char with font 12
        int bg_width = text_width + 14;  // Padding: 2px left + 12px right for good coverage
        int bg_height = 12;  // Height for font 12
        // Align background with text start position (x - 35) with small left padding
        lv_canvas_draw_rect(canvas, x - 37, y + 11, bg_width, bg_height, &bg_dsc);

        // Draw text in dark brown #332221 using Montserrat 12
        lv_draw_label_dsc_t lbl_dsc;
        lv_draw_label_dsc_init(&lbl_dsc);
        lbl_dsc.color = lv_color_hex(0x332221);  // Dark brown
        lbl_dsc.font = &lv_font_montserrat_12;  // Small font size 12
        lv_canvas_draw_text(canvas, x - 35, y + 11, 70, &lbl_dsc, ssid.c_str());
    }

    // Draw APRS symbol on map at specified position (low-level function)
    void drawMapSymbol(lv_obj_t* canvas, int x, int y, const char* symbolChar, lv_color_t color) {
        // Parse table, symbol and overlay from symbolChar
        // APRS symbol format:
        //   "/X" = primary table, symbol X, no overlay
        //   "\X" = alternate table, symbol X, no overlay
        //   "OX" = alternate table, symbol X, with overlay character O (A-Z, 0-9)
        char table = '/';  // default primary table
        char symbol = ' ';
        char overlay = 0;  // overlay character to draw on top (0 = none)

        if (symbolChar && strlen(symbolChar) >= 2) {
            if (symbolChar[0] == '/') {
                // Primary table, no overlay
                table = '/';
                symbol = symbolChar[1];
            } else if (symbolChar[0] == '\\') {
                // Alternate table, no overlay
                table = '\\';
                symbol = symbolChar[1];
            } else {
                // Overlay character + symbol = alternate table with overlay
                table = '\\';  // Alternate table
                overlay = symbolChar[0];  // Overlay character (A-Z, 0-9)
                symbol = symbolChar[1];   // Symbol character
            }
        } else if (symbolChar && strlen(symbolChar) >= 1) {
            symbol = symbolChar[0];
        }

        // Get symbol cache entry (includes both image data and alpha)
        CachedSymbol* cache = getSymbolCacheEntry(table, symbol);

        if (cache && cache->data) {
            // Draw PNG image pixel by pixel with alpha transparency
            lv_color_t* imgData = cache->data;
            uint8_t* alphaData = cache->alpha;
            int startX = x - SYMBOL_SIZE / 2;
            int startY = y - SYMBOL_SIZE / 2;

            for (int sy = 0; sy < SYMBOL_SIZE; sy++) {
                for (int sx = 0; sx < SYMBOL_SIZE; sx++) {
                    int pixelIdx = sy * SYMBOL_SIZE + sx;
                    lv_color_t pixelColor = imgData[pixelIdx];

                    // Get alpha value (255 = opaque, 0 = transparent)
                    uint8_t alpha = 255;  // Default fully opaque
                    if (alphaData) {
                        alpha = alphaData[pixelIdx];
                    }

                    // Only draw if not transparent (alpha > threshold)
                    if (alpha > 128) {  // 50% threshold
                        int px = startX + sx;
                        int py = startY + sy;
                        if (px >= 0 && px < MAP_CANVAS_WIDTH && py >= 0 && py < MAP_CANVAS_HEIGHT) {
                            lv_canvas_set_px_color(canvas, px, py, pixelColor);
                        }
                    }
                }
            }

            // Draw overlay character on top of symbol if present
            if (overlay != 0) {
                char overlayStr[2] = {overlay, '\0'};
                lv_draw_label_dsc_t lbl_dsc;
                lv_draw_label_dsc_init(&lbl_dsc);
                lbl_dsc.color = lv_color_hex(0xffffff);  // White text
                lbl_dsc.font = &lv_font_montserrat_14;
                // Center the overlay character on the symbol
                lv_canvas_draw_text(canvas, x - 4, y - 7, 12, &lbl_dsc, overlayStr);
            }
        } else {
            // Fallback: draw simple circle if symbol not found
            Serial.printf("[SYMBOL] Symbol not found: %c%c\n", table, symbol);
            lv_draw_rect_dsc_t rect_dsc;
            lv_draw_rect_dsc_init(&rect_dsc);
            rect_dsc.bg_color = lv_color_hex(0xff0000);  // Red to indicate missing
            rect_dsc.radius = 6;
            lv_canvas_draw_rect(canvas, x - 6, y - 6, 12, 12, &rect_dsc);
        }
    }

    // Track map station click - stores callsign to prefill compose screen
    static String map_prefill_callsign = "";

    // Station click handler - opens compose screen with prefilled callsign
    void map_station_clicked(lv_event_t* e) {
        int stationIndex = (int)(intptr_t)lv_event_get_user_data(e);
        MapStation* station = STATION_Utils::getMapStation(stationIndex);

        if (station && station->valid && station->callsign.length() > 0) {
            Serial.printf("[MAP] Station clicked : %s\n", station->callsign.c_str());
            map_prefill_callsign = station->callsign;
            LVGL_UI::open_compose_with_callsign(station->callsign); // Call public function
        }
    }

    // Map back button handler
    void btn_map_back_clicked(lv_event_t* e) {
        Serial.println("[LVGL] MAP BACK button pressed");
        cleanup_station_buttons();  // Clean up station buttons when leaving map
        map_follow_gps = true;  // Reset to follow GPS when leaving map
        // Stop periodic refresh timer
        if (map_refresh_timer) {
            lv_timer_del(map_refresh_timer);
            map_refresh_timer = nullptr;
        }
        // Stop tile preload task
        stopTilePreloadTask();
        // Return CPU to 80 MHz for power saving
        setCpuFrequencyMhz(80);
        Serial.printf("[MAP] CPU reduced to %d MHz\n", getCpuFrequencyMhz());
        // Return to main dashboard screen
        LVGL_UI::return_to_dashboard();
    }

    // Map recenter button handler - return to GPS position
    void btn_map_recenter_clicked(lv_event_t* e) {
        Serial.println("[MAP] Recentering on GPS");
        map_follow_gps = true;
        if (gps.location.isValid()) {
            map_center_lat = gps.location.lat();
            map_center_lon = gps.location.lng();
            Serial.printf("[MAP] Recentered on GPS : %.4f, %.4f\n", map_center_lat, map_center_lon);
        } else {
            // No GPS - return to default Ariège position
            map_center_lat = 42.9667f;
            map_center_lon = 1.6053f;
            Serial.printf("[MAP] No GPS, recentered on default position : %.4f, %.4f\n", map_center_lat, map_center_lon);
        }
        schedule_map_reload();
    }

    // Redraw only canvas content without recreating screen (for zoom)
    void redraw_map_canvas() {
        if (!map_canvas || !map_canvas_buf || !map_title_label) {
            screen_map = nullptr; // Force recreation
            create_map_screen();
            lv_disp_load_scr(screen_map);
            return;
        }

        // Pause async preloading while we load tiles (avoid SD contention)
        mainThreadLoading = true;

        // Update title with new zoom level
        char title_text[32];
        snprintf(title_text, sizeof(title_text), "MAP (Z%d)", map_current_zoom);
        lv_label_set_text(map_title_label, title_text);

        // Clean up old station buttons before redrawing
        cleanup_station_buttons();

        // Clear canvas with dark slate gray background
        lv_canvas_fill_bg(map_canvas, lv_color_hex(0x2F4F4F), LV_OPA_COVER);

        // Recalculate tile positions
        int centerTileX, centerTileY;
        latLonToTile(map_center_lat, map_center_lon, map_current_zoom, &centerTileX, &centerTileY);

        int n = 1 << map_current_zoom;
        float tileXf = (map_center_lon + 180.0f) / 360.0f * n;
        float latRad = map_center_lat * PI / 180.0f;
        float tileYf = (1.0f - log(tan(latRad) + 1.0f / cos(latRad)) / PI) / 2.0f * n;

        float fracX = tileXf - centerTileX;
        float fracY = tileYf - centerTileY;
        int subTileOffsetX = (int)(fracX * MAP_TILE_SIZE);
        int subTileOffsetY = (int)(fracY * MAP_TILE_SIZE);

        Serial.printf("[MAP] Center tile: %d/%d, sub-tile offset: %d,%d\n", centerTileX, centerTileY, subTileOffsetX, subTileOffsetY);

        // Load tiles
        bool hasTiles = false;
        if (STORAGE_Utils::isSDAvailable()) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int tileX = centerTileX + dx;
                    int tileY = centerTileY + dy;
                    int offsetX = MAP_CANVAS_WIDTH / 2 - subTileOffsetX + dx * MAP_TILE_SIZE;
                    int offsetY = MAP_CANVAS_HEIGHT / 2 - subTileOffsetY + dy * MAP_TILE_SIZE;

                    if (dx == 0 && dy == 0) {
                        Serial.printf("[MAP] Center tile offset: %d,%d\n", offsetX, offsetY);
                    }

                    if (loadTileFromSD(tileX, tileY, map_current_zoom, map_canvas, offsetX, offsetY)) {
                        hasTiles = true;
                    }
                }
            }
        }

        if (!hasTiles) {
            lv_draw_label_dsc_t label_dsc;
            lv_draw_label_dsc_init(&label_dsc);
            label_dsc.color = lv_color_hex(0xaaaaaa);
            label_dsc.font = &lv_font_montserrat_14;
            lv_canvas_draw_text(map_canvas, 40, MAP_CANVAS_HEIGHT / 2 - 30, 240, &label_dsc,
                "No offline tiles available.");
        }

        // Draw own position (overlay + symbol for correct APRS symbol)
        if (gps.location.isValid()) {
            int myX, myY;
            latLonToPixel(gps.location.lat(), gps.location.lng(),
                          map_center_lat, map_center_lon, map_current_zoom, &myX, &myY);
            if (myX >= 0 && myX < MAP_CANVAS_WIDTH && myY >= 0 && myY < MAP_CANVAS_HEIGHT) {
                Beacon* currentBeacon = &Config.beacons[myBeaconsIndex];
                String fullSymbol = currentBeacon->overlay + currentBeacon->symbol;
                drawStationOnMap(map_canvas, myX, myY, currentBeacon->callsign, fullSymbol.c_str());
            }
        }

        // Draw received stations and create clickable buttons
        create_station_buttons();

        // Force container update (needed for touch pan to work)
        lv_obj_invalidate(map_container);

        // Resume async preloading
        mainThreadLoading = false;
    }

    // Timer callback to reload map screen (for panning/recentering)
    void map_reload_timer_cb(lv_timer_t* timer) {
        lv_timer_del(timer);
        redraw_map_canvas(); // Only canvas is redrawn, no need to recreate screen_map
    }

    // Helper function to schedule map reload with delay
    void schedule_map_reload() {
        lv_timer_t* t = lv_timer_create(map_reload_timer_cb, 20, NULL);
        lv_timer_set_repeat_count(t, 1);
    }

    // Map zoom in handler
    void btn_map_zoomin_clicked(lv_event_t* e) {
        if (map_zoom_index < map_zoom_count - 1) {
            map_zoom_index++;
            map_current_zoom = map_available_zooms[map_zoom_index];
            Serial.printf("[MAP] Zoom in: %d\n", map_current_zoom);
            redraw_map_canvas();
        }
    }

    // Map zoom out handler
    void btn_map_zoomout_clicked(lv_event_t* e) {
        if (map_zoom_index > 0) {
            map_zoom_index--;
            map_current_zoom = map_available_zooms[map_zoom_index];
            Serial.printf("[MAP] Zoom out: %d\n", map_current_zoom);
            redraw_map_canvas();
        }
    }

    // Calculate pan step based on zoom level (pixels to degrees)
    float getMapPanStep() {
        int n = 1 << map_current_zoom;
        // Move approximately 50 pixels at current zoom value
        return 50.0f / MAP_TILE_SIZE / n * 360.0f;
    }

    // Map panning handlers
    void btn_map_up_clicked(lv_event_t* e) {
        map_follow_gps = false;
        float step = getMapPanStep();
        map_center_lat += step;
        Serial.printf("[MAP] Pan up: %.4f, %.4f\n", map_center_lat, map_center_lon);
        schedule_map_reload();
    }

    void btn_map_down_clicked(lv_event_t* e) {
        map_follow_gps = false;
        float step = getMapPanStep();
        map_center_lat -= step;
        Serial.printf("[MAP] Pan down: %.4f, %.4f\n", map_center_lat, map_center_lon);
        schedule_map_reload();
    }

    void btn_map_left_clicked(lv_event_t* e) {
        map_follow_gps = false;
        float step = getMapPanStep();
        map_center_lon -= step;
        Serial.printf("[MAP] Pan left: %.4f, %.4f\n", map_center_lat, map_center_lon);
        schedule_map_reload();
    }

    void btn_map_right_clicked(lv_event_t* e) {
        map_follow_gps = false;
        float step = getMapPanStep();
        map_center_lon += step;
        Serial.printf("[MAP] Pan right: %.4f, %.4f\n", map_center_lat, map_center_lon);
        schedule_map_reload();
    }

    // Touch pan handler for finger drag on map
    void map_touch_event_cb(lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        lv_indev_t* indev = lv_indev_get_act();
        if (!indev) return;

        lv_point_t point;
        lv_indev_get_point(indev, &point);

        if (code == LV_EVENT_PRESSED) {
            // Finger down - start tracking
            touch_start_x = point.x;
            touch_start_y = point.y;
            drag_start_lat = map_center_lat;
            drag_start_lon = map_center_lon;
            touch_dragging = false;
            Serial.printf("[MAP] Touch PRESSED at %d,%d - start pos: %.4f, %.4f\n",
                          point.x, point.y, drag_start_lat, drag_start_lon);
        }
        else if (code == LV_EVENT_PRESSING) {
            // Finger moving - check if we should pan
            lv_coord_t dx = point.x - touch_start_x;
            lv_coord_t dy = point.y - touch_start_y;

            // Only start dragging if moved beyond threshold
            if (!touch_dragging && (abs(dx) > PAN_THRESHOLD || abs(dy) > PAN_THRESHOLD)) {
                touch_dragging = true;
                map_follow_gps = false;
                Serial.println("[MAP] Touch pan started");

                // Immediate preload: queue tiles in direction of movement
                if (tilePreloadQueue != nullptr) {
                    // Get current center tile
                    int centerTileX, centerTileY;
                    latLonToTile(map_center_lat, map_center_lon, map_current_zoom, &centerTileX, &centerTileY);

                    // Determine direction (drag left = need tiles on right, etc.)
                    int dir_x = (dx < 0) ? 1 : (dx > 0) ? -1 : 0;
                    int dir_y = (dy < 0) ? 1 : (dy > 0) ? -1 : 0;

                    TileRequest req;
                    req.zoom = map_current_zoom;

                    // Queue tiles in movement direction
                    for (int i = -1; i <= 1; i++) {
                        if (dir_x != 0) {
                            req.tileX = centerTileX + dir_x * 2;
                            req.tileY = centerTileY + i;
                            xQueueSend(tilePreloadQueue, &req, 0);
                        }
                        if (dir_y != 0) {
                            req.tileX = centerTileX + i;
                            req.tileY = centerTileY + dir_y * 2;
                            xQueueSend(tilePreloadQueue, &req, 0);
                        }
                    }
                    Serial.printf("[MAP] Preload queued for direction dx=%d dy=%d\n", dir_x, dir_y);
                }
            }

            // Live preview: move canvas visually while dragging
            if (touch_dragging && map_canvas) {
                lv_obj_set_pos(map_canvas, dx, dy);
            }
        }
        else if (code == LV_EVENT_RELEASED) {
            // Finger up - finish pan
            if (touch_dragging) {
                touch_dragging = false;

                // Calculate final displacement
                lv_coord_t dx = point.x - touch_start_x;
                lv_coord_t dy = point.y - touch_start_y;

                // Convert pixel movement to lat/lon change
                int n = 1 << map_current_zoom;
                float degrees_per_tile = 360.0f / n;
                float degrees_per_pixel = degrees_per_tile / MAP_TILE_SIZE;

                // Update map center (drag right = view moves right = lon increases)
                map_center_lon = drag_start_lon - (dx * degrees_per_pixel);
                map_center_lat = drag_start_lat + (dy * degrees_per_pixel);

                Serial.printf("[MAP] Touch pan: start=%.4f,%.4f dx=%d dy=%d deg/px=%.6f -> end=%.4f,%.4f\n",
                              drag_start_lat, drag_start_lon, dx, dy, degrees_per_pixel,
                              map_center_lat, map_center_lon);

                // Reset canvas position before redraw
                lv_obj_set_pos(map_canvas, 0, 0);

                // Redraw map at new position
                redraw_map_canvas();
            }
        }
    }

    // Load a tile from SD card (with caching) and copy it to canvas
    bool loadTileFromSD(int tileX, int tileY, int zoom, lv_obj_t* canvas, int offsetX, int offsetY) {
        // Initialize cache on first use
        initTileCache();

        // Get canvas buffer
        lv_img_dsc_t* dsc = lv_canvas_get_img(canvas);
        lv_color_t* canvasBuffer = (lv_color_t*)dsc->data;

        // Check cache first
        int cacheIdx = findCachedTile(zoom, tileX, tileY);
        if (cacheIdx >= 0) {
            // Cache hit! Copy from cache to canvas
            Serial.printf("[MAP] Cache hit: %d/%d/%d\n", zoom, tileX, tileY);
            copyTileToCanvas(tileCache[cacheIdx].data, canvasBuffer, offsetX, offsetY,
                             MAP_CANVAS_WIDTH, MAP_CANVAS_HEIGHT);
            return true;
        }

        // Cache miss - need to load from SD card
        bool success = false;

        // Protect SPI bus access with mutex
        if (spiMutex != NULL && xSemaphoreTakeRecursive(spiMutex, portMAX_DELAY) == pdTRUE) {

            if (STORAGE_Utils::isSDAvailable()) {
                // Cache maps path and region on first access (avoid repeated SD directory listing)
                if (!mapsPathCached) {
                    cachedMapsPath = STORAGE_Utils::getMapsPath();
                    std::vector<String> regions = STORAGE_Utils::listDirs(cachedMapsPath);
                    if (!regions.empty()) {
                        map_current_region = regions[0];  // Use first region found
                    }
                    mapsPathCached = true;
                    Serial.printf("[MAP] Cached maps path: %s, region: %s\n",
                                  cachedMapsPath.c_str(), map_current_region.c_str());
                }

                if (map_current_region.length() > 0) {
                    char tilePath[128];
                    int slot = findCacheSlot();

                    // Allocate cache slot if needed
                    if (tileCache[slot].data == nullptr) {
                        tileCache[slot].data = (uint16_t*)heap_caps_malloc(TILE_DATA_SIZE, MALLOC_CAP_SPIRAM);
                    }

                    if (tileCache[slot].data) {
                        // Try JPEG first (priority - faster decoding)
                        // No fileExists() - just try to open directly
                        snprintf(tilePath, sizeof(tilePath), "%s/%s/%d/%d/%d.jpg",
                                 cachedMapsPath.c_str(), map_current_region.c_str(), zoom, tileX, tileY);

                        jpegCacheContext.cacheBuffer = tileCache[slot].data;
                        jpegCacheContext.tileWidth = MAP_TILE_SIZE;

                        int rc = jpeg.open(tilePath, jpegOpenFile, jpegCloseFile, jpegReadFile, jpegSeekFile, jpegCacheCallback);
                        if (rc) {
                            Serial.printf("[MAP] Loading JPEG: %d/%d/%d\n", zoom, tileX, tileY);
                            jpeg.setPixelType(RGB565_LITTLE_ENDIAN);
                            rc = jpeg.decode(0, 0, 0);
                            jpeg.close();

                            if (rc) {
                                tileCache[slot].zoom = zoom;
                                tileCache[slot].tileX = tileX;
                                tileCache[slot].tileY = tileY;
                                tileCache[slot].lastAccess = ++tileCacheAccessCounter;
                                tileCache[slot].valid = true;
                                copyTileToCanvas(tileCache[slot].data, canvasBuffer, offsetX, offsetY, MAP_CANVAS_WIDTH, MAP_CANVAS_HEIGHT);
                                success = true;
                            }
                        }

                        // Try PNG as fallback if JPEG not found
                        if (!success) {
                            snprintf(tilePath, sizeof(tilePath), "%s/%s/%d/%d/%d.png",
                                     cachedMapsPath.c_str(), map_current_region.c_str(), zoom, tileX, tileY);

                            pngCacheContext.cacheBuffer = tileCache[slot].data;
                            pngCacheContext.tileWidth = MAP_TILE_SIZE;

                            rc = png.open(tilePath, pngOpenFile, pngCloseFile, pngReadFile, pngSeekFile, pngCacheCallback);
                            if (rc == PNG_SUCCESS && pngFileOpened) {
                                Serial.printf("[MAP] Loading PNG: %d/%d/%d\n", zoom, tileX, tileY);
                                rc = png.decode(nullptr, 0);
                                png.close();

                                if (rc == PNG_SUCCESS) {
                                    tileCache[slot].zoom = zoom;
                                    tileCache[slot].tileX = tileX;
                                    tileCache[slot].tileY = tileY;
                                    tileCache[slot].lastAccess = ++tileCacheAccessCounter;
                                    tileCache[slot].valid = true;
                                    copyTileToCanvas(tileCache[slot].data, canvasBuffer, offsetX, offsetY, MAP_CANVAS_WIDTH, MAP_CANVAS_HEIGHT);
                                    success = true;
                                }
                            } else {
                                png.close();
                            }
                        }

                        // If tile not found, mark slot as invalid and clear coordinates
                        if (!success) {
                            Serial.printf("[MAP] Tile not found: %d/%d/%d\n", zoom, tileX, tileY);
                            tileCache[slot].valid = false;
                            tileCache[slot].zoom = -1;
                            tileCache[slot].tileX = -1;
                            tileCache[slot].tileY = -1;
                        }
                    }
                }
            }
            xSemaphoreGiveRecursive(spiMutex); // Release SPI mutex
        }
        return success;
    }

    // Create map screen
    void create_map_screen() {
        // Boost CPU to 240 MHz for smooth map rendering
        setCpuFrequencyMhz(240);
        Serial.printf("[MAP] CPU boosted to %d MHz\n", getCpuFrequencyMhz());

        // Clean up old station buttons if screen is being recreated
        cleanup_station_buttons();

        screen_map = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen_map, lv_color_hex(0x1a1a2e), 0);

        // Use current GPS position as center if follow mode is active
        if (map_follow_gps && gps.location.isValid()) {
            map_center_lat = gps.location.lat();
            map_center_lon = gps.location.lng();
            Serial.printf("[MAP] Using GPS position: %.4f, %.4f\n", map_center_lat, map_center_lon);
        } else if (map_center_lat == 0.0f && map_center_lon == 0.0f) {
            // Default to Ariège (Foix) if no GPS - matches OCC tiles
            map_center_lat = 42.9667f;
            map_center_lon = 1.6053f;
            Serial.printf("[MAP] No GPS, using default Ariège position: %.4f, %.4f\n", map_center_lat, map_center_lon);
        } else {
            Serial.printf("[MAP] Using pan position: %.4f, %.4f\n", map_center_lat, map_center_lon);
        }

        // Title bar (green for map)
        lv_obj_t* title_bar = lv_obj_create(screen_map);
        lv_obj_set_size(title_bar, SCREEN_WIDTH, 35);
        lv_obj_set_pos(title_bar, 0, 0);
        lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x009933), 0);
        lv_obj_set_style_border_width(title_bar, 0, 0);
        lv_obj_set_style_radius(title_bar, 0, 0);
        lv_obj_set_style_pad_all(title_bar, 5, 0);

        // Back button
        lv_obj_t* btn_back = lv_btn_create(title_bar);
        lv_obj_set_size(btn_back, 60, 25);
        lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x16213e), 0);
        lv_obj_add_event_cb(btn_back, btn_map_back_clicked, LV_EVENT_CLICKED, NULL);
        lv_obj_t* lbl_back = lv_label_create(btn_back);
        lv_label_set_text(lbl_back, "< BACK");
        lv_obj_center(lbl_back);

        // Title with zoom level (keep reference for updates)
        map_title_label = lv_label_create(title_bar);
        char title_text[32];
        snprintf(title_text, sizeof(title_text), "MAP (Z%d)", map_current_zoom);
        lv_label_set_text(map_title_label, title_text);
        lv_obj_set_style_text_color(map_title_label, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_font(map_title_label, &lv_font_montserrat_18, 0);
        lv_obj_align(map_title_label, LV_ALIGN_CENTER, -30, 0);

        // Zoom buttons on right
        lv_obj_t* btn_zoomin = lv_btn_create(title_bar);
        lv_obj_set_size(btn_zoomin, 30, 25);
        lv_obj_set_style_bg_color(btn_zoomin, lv_color_hex(0x16213e), 0);
        lv_obj_align(btn_zoomin, LV_ALIGN_RIGHT_MID, -70, 0);
        lv_obj_add_event_cb(btn_zoomin, btn_map_zoomin_clicked, LV_EVENT_RELEASED, NULL);
        lv_obj_t* lbl_zoomin = lv_label_create(btn_zoomin);
        lv_label_set_text(lbl_zoomin, "+");
        lv_obj_center(lbl_zoomin);

        lv_obj_t* btn_zoomout = lv_btn_create(title_bar);
        lv_obj_set_size(btn_zoomout, 30, 25);
        lv_obj_set_style_bg_color(btn_zoomout, lv_color_hex(0x16213e), 0);
        lv_obj_align(btn_zoomout, LV_ALIGN_RIGHT_MID, -35, 0);
        lv_obj_add_event_cb(btn_zoomout, btn_map_zoomout_clicked, LV_EVENT_RELEASED, NULL);
        lv_obj_t* lbl_zoomout = lv_label_create(btn_zoomout);
        lv_label_set_text(lbl_zoomout, "-");
        lv_obj_center(lbl_zoomout);

        // Recenter button (GPS icon) - shows different color when GPS not followed
        lv_obj_t* btn_recenter = lv_btn_create(title_bar);
        lv_obj_set_size(btn_recenter, 30, 25);
        lv_obj_set_style_bg_color(btn_recenter, map_follow_gps ? lv_color_hex(0x16213e) : lv_color_hex(0xff6600), 0);
        lv_obj_align(btn_recenter, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_add_event_cb(btn_recenter, btn_map_recenter_clicked, LV_EVENT_CLICKED, NULL);
        lv_obj_t* lbl_recenter = lv_label_create(btn_recenter);
        lv_label_set_text(lbl_recenter, LV_SYMBOL_GPS);
        lv_obj_center(lbl_recenter);

        // Map canvas area
        map_container = lv_obj_create(screen_map);
        lv_obj_set_size(map_container, SCREEN_WIDTH, MAP_CANVAS_HEIGHT);
        lv_obj_set_pos(map_container, 0, 35);
        lv_obj_set_style_bg_color(map_container, lv_color_hex(0x2F4F4F), 0);  // Dark slate gray
        lv_obj_set_style_border_width(map_container, 0, 0);
        lv_obj_set_style_radius(map_container, 0, 0);
        lv_obj_set_style_pad_all(map_container, 0, 0);

        // Enable touch pan on map container
        lv_obj_add_flag(map_container, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(map_container, map_touch_event_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(map_container, map_touch_event_cb, LV_EVENT_PRESSING, NULL);
        lv_obj_add_event_cb(map_container, map_touch_event_cb, LV_EVENT_RELEASED, NULL);

        // Create canvas for map drawing
        // Free old buffer if it exists (memory leak prevention)
        if (map_canvas_buf) {
            heap_caps_free(map_canvas_buf);
            map_canvas_buf = nullptr;
        }
        map_canvas_buf = (lv_color_t*)heap_caps_malloc(MAP_CANVAS_WIDTH * MAP_CANVAS_HEIGHT * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
        if (map_canvas_buf) {
            map_canvas = lv_canvas_create(map_container);
            lv_canvas_set_buffer(map_canvas, map_canvas_buf, MAP_CANVAS_WIDTH, MAP_CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);
            lv_obj_set_pos(map_canvas, 0, 0);

            // Fill with background color
            lv_canvas_fill_bg(map_canvas, lv_color_hex(0x2F4F4F), LV_OPA_COVER);

            // Calculate center tile and fractional position within tile
            int centerTileX, centerTileY;
            latLonToTile(map_center_lat, map_center_lon, map_current_zoom, &centerTileX, &centerTileY);

            // Calculate sub-tile offset (where our center point is within the tile)
            int n = 1 << map_current_zoom;
            float tileXf = (map_center_lon + 180.0f) / 360.0f * n;
            float latRad = map_center_lat * PI / 180.0f;
            float tileYf = (1.0f - log(tan(latRad) + 1.0f / cos(latRad)) / PI) / 2.0f * n;

            // Fractional part (0.0 to 1.0) represents position within tile
            float fracX = tileXf - centerTileX;
            float fracY = tileYf - centerTileY;

            // Convert to pixel offset (how many pixels to shift tiles)
            int subTileOffsetX = (int)(fracX * MAP_TILE_SIZE);
            int subTileOffsetY = (int)(fracY * MAP_TILE_SIZE);

            Serial.printf("[MAP] Center tile: %d/%d, sub-tile offset: %d,%d\n", centerTileX, centerTileY, subTileOffsetX, subTileOffsetY);

            // Pause async preloading while we load tiles (avoid SD contention)
            mainThreadLoading = true;

            // Try to load tiles from SD card
            bool hasTiles = false;
            if (STORAGE_Utils::isSDAvailable()) {
                // Load center tile and surrounding tiles (3x3 grid, or more if needed)
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int tileX = centerTileX + dx;
                        int tileY = centerTileY + dy;
                        // Apply sub-tile offset so center point is at screen center
                        int offsetX = MAP_CANVAS_WIDTH / 2 - subTileOffsetX + dx * MAP_TILE_SIZE;
                        int offsetY = MAP_CANVAS_HEIGHT / 2 - subTileOffsetY + dy * MAP_TILE_SIZE;

                        if (dx == 0 && dy == 0) {
                            Serial.printf("[MAP] Center tile offset: %d,%d\n", offsetX, offsetY);
                        }

                        if (loadTileFromSD(tileX, tileY, map_current_zoom, map_canvas, offsetX, offsetY)) {
                            hasTiles = true;
                        }
                    }
                }
            }

            if (!hasTiles) {
                // No tiles - display message
                lv_draw_label_dsc_t label_dsc;
                lv_draw_label_dsc_init(&label_dsc);
                label_dsc.color = lv_color_hex(0xaaaaaa);
                label_dsc.font = &lv_font_montserrat_14;
                lv_canvas_draw_text(map_canvas, 40, MAP_CANVAS_HEIGHT / 2 - 30, 240, &label_dsc,
                    "No offline tiles available.\nDownload OSM tiles and copy to:\nSD:/LoRa_Tracker/Maps/REGION/z/x/y.png");
            }

            // Draw own position (if GPS is valid) - overlay + symbol for correct APRS symbol
            if (gps.location.isValid()) {
                int myX, myY;
                latLonToPixel(gps.location.lat(), gps.location.lng(),
                              map_center_lat, map_center_lon, map_current_zoom, &myX, &myY);

                if (myX >= 0 && myX < MAP_CANVAS_WIDTH && myY >= 0 && myY < MAP_CANVAS_HEIGHT) {
                    // Get current beacon symbol (overlay + symbol)
                    Beacon* currentBeacon = &Config.beacons[myBeaconsIndex];
                    String fullSymbol = currentBeacon->overlay + currentBeacon->symbol;
                    drawStationOnMap(map_canvas, myX, myY, currentBeacon->callsign, fullSymbol.c_str());
                }
            }

            // Draw received stations and create clickable buttons
            create_station_buttons();

            // Force canvas redraw after direct buffer writes
            lv_canvas_set_buffer(map_canvas, map_canvas_buf, MAP_CANVAS_WIDTH, MAP_CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);
            lv_obj_invalidate(map_canvas);

            // Resume async preloading
            mainThreadLoading = false;
        }

        // Arrow buttons for panning (bottom left corner, D-pad layout)
        int arrow_size = 28;
        int arrow_x = 5;
        int arrow_y = MAP_CANVAS_HEIGHT - 105;  // Above info bar
        lv_color_t arrow_color = lv_color_hex(0x444444);

        // Up button
        lv_obj_t* btn_up = lv_btn_create(map_container);
        lv_obj_set_size(btn_up, arrow_size, arrow_size);
        lv_obj_set_pos(btn_up, arrow_x + arrow_size, arrow_y);
        lv_obj_set_style_bg_color(btn_up, arrow_color, 0);
        lv_obj_set_style_bg_opa(btn_up, LV_OPA_70, 0);
        lv_obj_add_event_cb(btn_up, btn_map_up_clicked, LV_EVENT_CLICKED, NULL);
        lv_obj_t* lbl_up = lv_label_create(btn_up);
        lv_label_set_text(lbl_up, LV_SYMBOL_UP);
        lv_obj_center(lbl_up);

        // Down button
        lv_obj_t* btn_down = lv_btn_create(map_container);
        lv_obj_set_size(btn_down, arrow_size, arrow_size);
        lv_obj_set_pos(btn_down, arrow_x + arrow_size, arrow_y + arrow_size * 2);
        lv_obj_set_style_bg_color(btn_down, arrow_color, 0);
        lv_obj_set_style_bg_opa(btn_down, LV_OPA_70, 0);
        lv_obj_add_event_cb(btn_down, btn_map_down_clicked, LV_EVENT_CLICKED, NULL);
        lv_obj_t* lbl_down = lv_label_create(btn_down);
        lv_label_set_text(lbl_down, LV_SYMBOL_DOWN);
        lv_obj_center(lbl_down);

        // Left button
        lv_obj_t* btn_left = lv_btn_create(map_container);
        lv_obj_set_size(btn_left, arrow_size, arrow_size);
        lv_obj_set_pos(btn_left, arrow_x, arrow_y + arrow_size);
        lv_obj_set_style_bg_color(btn_left, arrow_color, 0);
        lv_obj_set_style_bg_opa(btn_left, LV_OPA_70, 0);
        lv_obj_add_event_cb(btn_left, btn_map_left_clicked, LV_EVENT_CLICKED, NULL);
        lv_obj_t* lbl_left = lv_label_create(btn_left);
        lv_label_set_text(lbl_left, LV_SYMBOL_LEFT);
        lv_obj_center(lbl_left);

        // Right button
        lv_obj_t* btn_right = lv_btn_create(map_container);
        lv_obj_set_size(btn_right, arrow_size, arrow_size);
        lv_obj_set_pos(btn_right, arrow_x + arrow_size * 2, arrow_y + arrow_size);
        lv_obj_set_style_bg_color(btn_right, arrow_color, 0);
        lv_obj_set_style_bg_opa(btn_right, LV_OPA_70, 0);
        lv_obj_add_event_cb(btn_right, btn_map_right_clicked, LV_EVENT_CLICKED, NULL);
        lv_obj_t* lbl_right = lv_label_create(btn_right);
        lv_label_set_text(lbl_right, LV_SYMBOL_RIGHT);
        lv_obj_center(lbl_right);

        // Info bar at bottom
        lv_obj_t* info_bar = lv_obj_create(screen_map);
        lv_obj_set_size(info_bar, SCREEN_WIDTH, 25);
        lv_obj_set_pos(info_bar, 0, SCREEN_HEIGHT - 25);
        lv_obj_set_style_bg_color(info_bar, lv_color_hex(0x16213e), 0);
        lv_obj_set_style_border_width(info_bar, 0, 0);
        lv_obj_set_style_radius(info_bar, 0, 0);
        lv_obj_set_style_pad_all(info_bar, 2, 0);

        // Display coordinates
        lv_obj_t* lbl_coords = lv_label_create(info_bar);
        char coords_text[64];
        snprintf(coords_text, sizeof(coords_text), "Center: %.4f, %.4f  Stations: %d",
                 map_center_lat, map_center_lon, mapStationsCount);
        lv_label_set_text(lbl_coords, coords_text);
        lv_obj_set_style_text_color(lbl_coords, lv_color_hex(0xaaaaaa), 0);
        lv_obj_set_style_text_font(lbl_coords, &lv_font_montserrat_14, 0);
        lv_obj_center(lbl_coords);

        // Create periodic refresh timer for stations (10 seconds)
        if (map_refresh_timer) {
            lv_timer_del(map_refresh_timer);
        }
        map_refresh_timer = lv_timer_create(map_refresh_timer_cb, MAP_REFRESH_INTERVAL, NULL);

        // Start tile preload task on Core 1 for directional preloading during touch pan
        startTilePreloadTask();

        Serial.println("[LVGL] Map screen created");
    }

} // namespace UIMapManager

#endif // USE_LVGL_UI
