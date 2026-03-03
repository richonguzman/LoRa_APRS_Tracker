/* Map logic for T-Deck Plus
 * Offline map tile display with stations using LVGL
 */

#ifdef USE_LVGL_UI

#include <Arduino.h>
#include <FS.h>
#include <lvgl.h>
#include <LovyanGFX.hpp>
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
#include <vector>
#include <algorithm> // For std::min/max
#include <climits>   // For INT_MIN/INT_MAX

#include "map_engine.h"
#include "ui_map_manager.h"
#include "configuration.h"
#include "station_utils.h"
#include "utils.h"
#include "storage_utils.h"
#include "custom_characters.h" // For symbolsAPRS, SYMBOL_WIDTH, SYMBOL_HEIGHT
#include "lvgl_ui.h" // To call LVGL_UI::open_compose_with_callsign
#include "gpx_writer.h"
#include <esp_task_wdt.h> //
#include <esp_log.h>

static const char *TAG = "Map";

namespace UIMapManager {

    // UI elements - Map screen
    lv_obj_t* screen_map = nullptr;
    lv_obj_t* map_canvas = nullptr;
    lv_color_t* map_canvas_buf = nullptr;
    lv_obj_t* map_title_label = nullptr;
    lv_obj_t* map_info_label = nullptr;
    lv_obj_t* map_container = nullptr;

    // Map state variables
    // Vector (NAV) zooms: step 1 (6..18)
    static const int nav_zooms[] = {9, 10, 11, 12, 13, 14, 15, 16, 17};
    static const int nav_zoom_count = sizeof(nav_zooms) / sizeof(nav_zooms[0]);
    // Raster (PNG/JPG) zooms: step 2 (6,7,8,10,12,14)
    static const int raster_zooms[] = {6, 7, 8, 10, 12, 14};
    static const int raster_zoom_count = sizeof(raster_zooms) / sizeof(raster_zooms[0]);
    // Active zoom table (points to nav_zooms or raster_zooms)
    const int* map_available_zooms = raster_zooms;
    int map_zoom_count = raster_zoom_count;
    int map_zoom_index = 0;
    int map_current_zoom = raster_zooms[0];
    float map_center_lat = 0.0f;
    float map_center_lon = 0.0f;
    String map_current_region = "";
    // Multi-region NAV support — all regions found on SD, GPS match first
    #define NAV_MAX_REGIONS 4
    static String navRegions[NAV_MAX_REGIONS];
    static int navRegionCount = 0;
    bool map_follow_gps = true;  // Follow GPS or free panning mode

    // Negative cache for tiles not found on SD to prevent repeated lookups
    #define NOT_FOUND_CACHE_SIZE 128
    static std::vector<uint32_t> notFoundCache;
    static int notFoundCacheIndex = 0;

    // Touch pan state
    static bool touch_dragging = false;
    static lv_coord_t touch_start_x = 0;
    static lv_coord_t touch_start_y = 0;
    static float drag_start_lat = 0.0f;
    static float drag_start_lon = 0.0f;
    static lv_coord_t last_pan_dx = 0;  // Track last pan delta for station sync
    static lv_coord_t last_pan_dy = 0;
    #define PAN_THRESHOLD 5  // Minimum pixels to trigger pan

    // Tile data size for old raster tiles
    #define TILE_DATA_SIZE (MAP_TILE_SIZE * MAP_TILE_SIZE * sizeof(uint16_t))  // 128KB per tile

    // Symbol cache in PSRAM
    #define SYMBOL_CACHE_SIZE 10  // Cache for frequently used symbols
    #define SYMBOL_SIZE 24        // 24x24 pixels
    #define SYMBOL_DATA_SIZE (SYMBOL_SIZE * SYMBOL_SIZE * sizeof(lv_color_t))

    static CachedSymbol symbolCache[SYMBOL_CACHE_SIZE];
    static uint32_t symbolCacheAccessCounter = 0;
    static bool symbolCacheInitialized = false;

    // Own GPS trace (separate from received stations)
    static TracePoint ownTrace[TRACE_MAX_POINTS];
    static uint8_t ownTraceCount = 0;
    static uint8_t ownTraceHead = 0;

    // Forward declarations
    void cleanup_station_buttons();
    void draw_station_traces();
    void update_station_objects();
    void redraw_map_canvas();

    // Station hit zones for click detection (replaces LVGL buttons - no alloc/dealloc)
    struct StationHitZone {
        int16_t x, y;      // Screen position (center)
        int16_t w, h;      // Hit zone size
        int8_t stationIdx; // Index in mapStations array (-1 = unused)
    };
    static StationHitZone stationHitZones[MAP_STATIONS_MAX];
    static int stationHitZoneCount = 0;

    // No LVGL objects for stations — drawn directly on canvas (zero DRAM cost)

    // Periodic refresh timer for stations
    static lv_timer_t* map_refresh_timer = nullptr;
    #define MAP_REFRESH_INTERVAL 10000  // 10 seconds

    // Redraw synchronization (prevent overlapping redraws and timer pileup)
    static lv_timer_t* pending_reload_timer = nullptr;
    static volatile bool redraw_in_progress = false;

    // Persistent viewport sprite for NAV rendering (avoids PSRAM fragmentation)
    static LGFX_Sprite* persistentViewportSprite = nullptr;

    // NAV priority flag: when true, raster cache is disabled
    static volatile bool navModeActive = false;

    // Switch zoom table and recalculate index to nearest available zoom
    void switchZoomTable(const int* newTable, int newCount) {
        map_available_zooms = newTable;
        map_zoom_count = newCount;
        // Find closest zoom in new table
        int bestIdx = 0;
        int bestDiff = abs(map_current_zoom - newTable[0]);
        for (int i = 1; i < newCount; i++) {
            int diff = abs(map_current_zoom - newTable[i]);
            if (diff < bestDiff) { bestDiff = diff; bestIdx = i; }
        }
        map_zoom_index = bestIdx;
        map_current_zoom = newTable[bestIdx];
    }

    // Filtered own position — only updates when movement exceeds HDOP-based threshold
    static float filteredOwnLat = 0.0f;
    static float filteredOwnLon = 0.0f;
    static bool  filteredOwnValid = false;

    // Lightweight station-only refresh: restore NAV background from sprite, redraw stations.
    // Cost: ~10-50ms (memcpy 1.2MB + station icons) vs 500-3000ms for full NAV re-render.
    static void refreshStationOverlay() {
        if (!map_canvas || !map_canvas_buf) return;

        if (navModeActive && persistentViewportSprite) {
            // NAV mode: restore clean background from cached sprite (no station artifacts)
            uint16_t* src = (uint16_t*)persistentViewportSprite->getBuffer();
            if (src) {
                memcpy(map_canvas_buf, src, MAP_CANVAS_WIDTH * MAP_CANVAS_HEIGHT * sizeof(lv_color_t));
            }
        } else {
            // Raster mode: can't restore background without re-decoding PNGs → full redraw
            redraw_map_canvas();
            return;
        }

        cleanup_station_buttons();
        draw_station_traces();
        update_station_objects();

        if (map_info_label) {
            char info_text[64];
            snprintf(info_text, sizeof(info_text), "Lat: %.4f  Lon: %.4f  Stations: %d",
                     map_center_lat, map_center_lon, mapStationsCount);
            lv_label_set_text(map_info_label, info_text);
        }

        lv_obj_set_pos(map_canvas, -MAP_CANVAS_MARGIN, -MAP_CANVAS_MARGIN);
        lv_canvas_set_buffer(map_canvas, map_canvas_buf, MAP_CANVAS_WIDTH, MAP_CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);
        lv_obj_invalidate(map_canvas);
    }

    // Timer callback for periodic station refresh.
    // In NAV mode: skip full re-render if position/zoom unchanged — just refresh station overlay.
    // This eliminates the 500-3000ms UI freeze every 10 seconds when stationary.
    static void map_refresh_timer_cb(lv_timer_t* timer) {
        if (screen_map && lv_scr_act() == screen_map
            && !touch_dragging && !redraw_in_progress) {
            float prevLat = map_center_lat;
            float prevLon = map_center_lon;

            // Follow GPS: update map center with filtered position before redraw
            if (map_follow_gps && filteredOwnValid) {
                map_center_lat = filteredOwnLat;
                map_center_lon = filteredOwnLon;
            }

            bool posChanged = (map_center_lat != prevLat || map_center_lon != prevLon);

            if (posChanged) {
                ESP_LOGD(TAG, "Periodic refresh (GPS moved, full redraw)");
                redraw_map_canvas();
            } else {
                ESP_LOGD(TAG, "Periodic refresh (station overlay only)");
                refreshStationOverlay();
            }
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
        ESP_LOGI(TAG, "Tile preload task started on Core 1");
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
                int cacheIdx = MapEngine::findCachedTile(req.zoom, req.tileX, req.tileY);
                if (cacheIdx < 0) {
                    // Not in cache - preload it
                    ESP_LOGD(TAG, "Preloading tile %d/%d/%d", req.zoom, req.tileX, req.tileY);
                    preloadTileToCache(req.tileX, req.tileY, req.zoom);
                }
            }
        }

        ESP_LOGI(TAG, "Tile preload task stopped");
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
        if (tilePreloadQueue == nullptr || navModeActive) return;

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

    // Clear station hit zones
    void cleanup_station_buttons() {
        stationHitZoneCount = 0;
    }

    // Helper: parse APRS symbol string and get cached symbol image descriptor
    static CachedSymbol* parseAndGetSymbol(const char* aprsSymbol) {
        char table = '/';
        char symbol = ' ';
        if (aprsSymbol && strlen(aprsSymbol) >= 2) {
            if (aprsSymbol[0] == '/' || aprsSymbol[0] == '\\') {
                table = aprsSymbol[0];
                symbol = aprsSymbol[1];
            } else {
                table = '\\';  // Overlay = alternate table
                symbol = aprsSymbol[1];
            }
        } else if (aprsSymbol && strlen(aprsSymbol) >= 1) {
            symbol = aprsSymbol[0];
        }
        return getSymbolCacheEntry(table, symbol);
    }

    // Draw a single station directly on the canvas (symbol + overlay letter + callsign)
    static void drawStationOnCanvas(int canvasX, int canvasY,
                                    const char* callsign, const char* aprsSymbol,
                                    int8_t stationIdx) {
        if (!map_canvas) return;

        // Symbol top-left position (centered on canvasX, canvasY)
        int symX = canvasX - SYMBOL_SIZE / 2;
        int symY = canvasY - SYMBOL_SIZE / 2;

        // Draw APRS symbol PNG via lv_canvas_draw_img (handles alpha + byte order)
        CachedSymbol* cache = parseAndGetSymbol(aprsSymbol);
        if (cache && cache->valid) {
            lv_draw_img_dsc_t img_dsc;
            lv_draw_img_dsc_init(&img_dsc);
            img_dsc.opa = LV_OPA_COVER;
            lv_canvas_draw_img(map_canvas, symX, symY, &cache->img_dsc, &img_dsc);
        } else {
            // Fallback: red circle
            lv_draw_rect_dsc_t rect_dsc;
            lv_draw_rect_dsc_init(&rect_dsc);
            rect_dsc.bg_color = lv_color_hex(0xff0000);
            rect_dsc.bg_opa = LV_OPA_COVER;
            rect_dsc.radius = LV_RADIUS_CIRCLE;
            lv_canvas_draw_rect(map_canvas, canvasX - 8, canvasY - 8, 16, 16, &rect_dsc);
        }

        // Overlay letter (e.g., "L", "1") centered on the icon
        if (aprsSymbol && strlen(aprsSymbol) >= 2) {
            char overlay = aprsSymbol[0];
            if (overlay != '/' && overlay != '\\' && overlay != ' ') {
                char ovStr[2] = { overlay, '\0' };
                lv_draw_label_dsc_t ov_dsc;
                lv_draw_label_dsc_init(&ov_dsc);
                ov_dsc.color = lv_color_hex(0xffffff);
                ov_dsc.font = &lv_font_montserrat_14;
                lv_canvas_draw_text(map_canvas, canvasX - 5, symY + 4, 14, &ov_dsc, ovStr);
            }
        }

       /* // Callsign text with semi-transparent background below symbol
        if (callsign) {
            int textY = canvasY + SYMBOL_SIZE / 2 + 2;
            if (textY >= 0 && textY < MAP_CANVAS_HEIGHT) {
                
                // 1. Calculate the EXACT text size in pixels based on the font
                lv_point_t text_size;
                lv_txt_get_size(&text_size, callsign, &lv_font_montserrat_12, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);

                // 2. Define margins (padding) around the text
                int padding_x = 8; // 4 pixels on the left, 4 pixels on the right
                int padding_y = 4; // 2 pixels on top, 2 pixels on the bottom

                // 3. Calculate background dimensions based on text size + padding
                int textW = text_size.x + padding_x;
                int textH = text_size.y + padding_y; 
                
                // Center the background horizontally relative to the symbol
                int textX = canvasX - textW / 2;
                if (textX < 0) textX = 0;

                // Background rectangle (light gray, fully opaque to avoid accumulation on refresh)
                lv_draw_rect_dsc_t bg_dsc;
                lv_draw_rect_dsc_init(&bg_dsc);
                bg_dsc.bg_color = lv_color_hex(0xDDDDDD);
                bg_dsc.bg_opa = LV_OPA_COVER;
                bg_dsc.radius = 2;
                lv_canvas_draw_rect(map_canvas, textX, textY, textW, textH, &bg_dsc);

                // Text on top
                lv_draw_label_dsc_t label_dsc;
                lv_draw_label_dsc_init(&label_dsc);
                label_dsc.color = lv_color_hex(0x332221);
                label_dsc.font = &lv_font_montserrat_12;

                // 4. Draw the text offset by half the padding to center it inside the background box
                lv_canvas_draw_text(map_canvas, textX + (padding_x / 2), textY + (padding_y / 2), textW, &label_dsc, callsign);
            }
        }*/
       
       // Callsign text with semi-transparent background below symbol
        if (callsign) {
            int textY = canvasY + SYMBOL_SIZE / 2 + 2;
            if (textY >= 0 && textY < MAP_CANVAS_HEIGHT) {
                
                // 1. Calculate the EXACT text size in pixels based on the font
                lv_point_t text_size;
                lv_txt_get_size(&text_size, callsign, &lv_font_montserrat_12, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);

                // 2. Define margins (padding) around the text
                int padding_x = 8; // 4 pixels on the left, 4 pixels on the right
                int padding_y = 4; // 2 pixels on top, 2 pixels on the bottom

                // 3. Calculate background dimensions based on text size + padding
                int textW = text_size.x + padding_x;
                int textH = text_size.y + padding_y; 
                
                // Center the background horizontally relative to the symbol
                int textX = canvasX - textW / 2;
                if (textX < 0) textX = 0;

                // Background rectangle (light gray, fully opaque to avoid accumulation on refresh)
                lv_draw_rect_dsc_t bg_dsc;
                lv_draw_rect_dsc_init(&bg_dsc);
                bg_dsc.bg_color = lv_color_hex(0xDDDDDD);
                bg_dsc.bg_opa = LV_OPA_COVER;
                bg_dsc.radius = 2;
                lv_canvas_draw_rect(map_canvas, textX, textY, textW, textH, &bg_dsc);

                // Text on top
                lv_draw_label_dsc_t label_dsc;
                lv_draw_label_dsc_init(&label_dsc);
                label_dsc.color = lv_color_hex(0x332221);
                label_dsc.font = &lv_font_montserrat_12;

                // 4. Draw the text offset by half the padding to center it inside the background box
                lv_canvas_draw_text(map_canvas, textX + (padding_x / 2), textY + (padding_y / 2), textW, &label_dsc, callsign);
            }
        }

        // Store hit zone for tap detection
        if (stationIdx >= 0 && stationHitZoneCount < MAP_STATIONS_MAX) {
            stationHitZones[stationHitZoneCount].x = canvasX - MAP_CANVAS_MARGIN;
            stationHitZones[stationHitZoneCount].y = canvasY - MAP_CANVAS_MARGIN + 12;
            stationHitZones[stationHitZoneCount].w = 80;
            stationHitZones[stationHitZoneCount].h = 50;
            stationHitZones[stationHitZoneCount].stationIdx = stationIdx;
            stationHitZoneCount++;
        }
    }

    // (filteredOwn* declared above, before map_refresh_timer_cb)

    static void updateFilteredOwnPosition() {
        static float iconCentroidLat = 0.0f;
        static float iconCentroidLon = 0.0f;
        static uint32_t iconCentroidCount = 0;

        if (!gps.location.isValid()) return;
        // Reject unreliable fixes: need ≥6 sats for decent 3D geometry
        if (gps.satellites.value() < 6) return;
        float lat = gps.location.lat();
        float lon = gps.location.lng();

        if (!filteredOwnValid) {
            filteredOwnLat = lat;
            filteredOwnLon = lon;
            filteredOwnValid = true;
            iconCentroidLat = lat;
            iconCentroidLon = lon;
            iconCentroidCount = 1;
            return;
        }

        // Update running centroid with every GPS reading
        float alpha = (iconCentroidCount < 10) ? 1.0f / (iconCentroidCount + 1) : 0.1f;
        iconCentroidLat += alpha * (lat - iconCentroidLat);
        iconCentroidLon += alpha * (lon - iconCentroidLon);
        iconCentroidCount++;

        // Threshold: 15m min, +5m per HDOP unit
        float hdop = gps.hdop.isValid() ? gps.hdop.hdop() : 2.0f;
        float thresholdM = fmax(15.0f, hdop * 5.0f);
        float thresholdLat = thresholdM / 111320.0f;
        float thresholdLon = thresholdM / (111320.0f * cosf(lat * M_PI / 180.0f));

        // Compare to centroid — only update if real movement
        if (fabs(lat - iconCentroidLat) > thresholdLat || fabs(lon - iconCentroidLon) > thresholdLon) {
            filteredOwnLat = lat;
            filteredOwnLon = lon;
            iconCentroidLat = lat;
            iconCentroidLon = lon;
            iconCentroidCount = 1;
        }
    }

    // Draw all stations directly on the canvas (zero LVGL objects, all PSRAM)
    void update_station_objects() {
        if (!map_canvas || !map_canvas_buf) return;

        stationHitZoneCount = 0;

        // Own position — use filtered position to eliminate GPS jitter
        updateFilteredOwnPosition();
        if (filteredOwnValid) {
            int myX, myY;
            latLonToPixel(filteredOwnLat, filteredOwnLon,
                          map_center_lat, map_center_lon, map_current_zoom, &myX, &myY);
            if (myX >= 0 && myX < MAP_CANVAS_WIDTH && myY >= 0 && myY < MAP_CANVAS_HEIGHT) {
                Beacon* currentBeacon = &Config.beacons[myBeaconsIndex];
                char fullSymbol[4];
                snprintf(fullSymbol, sizeof(fullSymbol), "%s%s",
                         currentBeacon->overlay.c_str(), currentBeacon->symbol.c_str());
                drawStationOnCanvas(myX, myY, currentBeacon->callsign.c_str(), fullSymbol, -1);
            }
        }

        // Received stations
        STATION_Utils::cleanOldMapStations();
        for (int i = 0; i < MAP_STATIONS_MAX; i++) {
            MapStation* station = STATION_Utils::getMapStation(i);
            if (station && station->valid && station->latitude != 0.0f && station->longitude != 0.0f) {
                int stX, stY;
                latLonToPixel(station->latitude, station->longitude,
                              map_center_lat, map_center_lon, map_current_zoom, &stX, &stY);
                if (stX >= 0 && stX < MAP_CANVAS_WIDTH && stY >= 0 && stY < MAP_CANVAS_HEIGHT) {
                    drawStationOnCanvas(stX, stY, station->callsign.c_str(),
                                        station->symbol.c_str(), i);
                }
            }
        }
    }

    // Initialize symbol cache
    void initSymbolCache() {
        if (symbolCacheInitialized) return;
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

    // Forward declarations for PNG callbacks
    static void* pngOpenFile(const char* filename, int32_t* size);
    static void pngCloseFile(void* handle);
    static int32_t pngReadFile(PNGFILE* pFile, uint8_t* pBuf, int32_t iLen);
    static int32_t pngSeekFile(PNGFILE* pFile, int32_t iPosition);
    static bool pngFileOpened = false;  // Track if PNG file actually opened

    // PNG file callbacks implementation
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

    // PNG draw callback for symbols - stores alpha channel info
    static uint8_t* symbolCombinedBuffer = nullptr;  // Target combined buffer
    static PNG symbolPNG;  // PNG decoder instance for symbols

    static int pngSymbolCallback(PNGDRAW* pDraw) {
        if (!symbolCombinedBuffer) return 1;
        if (pDraw->y >= SYMBOL_SIZE) return 1;  // Clamp oversized PNGs

        const int w = (pDraw->iWidth < SYMBOL_SIZE) ? pDraw->iWidth : SYMBOL_SIZE;
        const size_t rgb565Offset = pDraw->y * SYMBOL_SIZE;  // In uint16_t units
        const size_t alphaOffset  = SYMBOL_SIZE * SYMBOL_SIZE * sizeof(uint16_t)
                                  + pDraw->y * SYMBOL_SIZE;   // In bytes

        uint8_t* alphaRow = symbolCombinedBuffer + alphaOffset;
        uint16_t* rgb565Row = (uint16_t*)symbolCombinedBuffer + rgb565Offset;

        // Handle palette (type 3) vs truecolor+alpha (type 6) vs others
        if (pDraw->iPixelType == PNG_PIXEL_INDEXED) {
            // Palette mode: convert palette indices to RGBA
            uint8_t* indices = (uint8_t*)pDraw->pPixels;
            uint8_t* palette = (uint8_t*)pDraw->pPalette;

            for (int x = 0; x < w; x++) {
                uint8_t idx = indices[x];
                uint8_t r = palette[idx * 3 + 0];
                uint8_t g = palette[idx * 3 + 1];
                uint8_t b = palette[idx * 3 + 2];
                uint8_t a = pDraw->iHasAlpha ? palette[768 + idx] : 255;

                // Binarize alpha for sharp edges on 16-bit canvas
                a = (a > 50) ? 255 : 0;  // 50/255 ≈ 20%

                // Convert RGB888 to RGB565
                uint16_t rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
#if LV_COLOR_16_SWAP
                rgb565Row[x] = (rgb565 >> 8) | (rgb565 << 8);
#else
                rgb565Row[x] = rgb565;
#endif
                alphaRow[x] = a;
            }

            for (int x = w; x < SYMBOL_SIZE; x++) {
                rgb565Row[x] = 0;
                alphaRow[x] = 0;
            }
        } else {
            // Direct RGBA mode
            if (pDraw->iHasAlpha) {
                for (int x = 0; x < w; x++) {
                    uint8_t a = pDraw->pPixels[x * 4 + 3];
                    // Binarize alpha (same threshold as palette mode)
                    alphaRow[x] = (a > 50) ? 255 : 0;
                }
                for (int x = w; x < SYMBOL_SIZE; x++) {
                    alphaRow[x] = 0;
                }
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

    // Load symbol PNG from SD card into a combined RGB565A8 buffer
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
        const size_t totalSize  = rgb565Size + alphaSize;  // 1728 bytes for 24x24

        // Single allocation: combined RGB565A8 buffer in PSRAM
        uint8_t* combined = (uint8_t*)ps_malloc(totalSize);
        if (!combined) {
            ESP_LOGE(TAG, "PSRAM allocation failed for symbol");
            return nullptr;
        }
        memset(combined, 0, totalSize);  // Zero-init (transparent black)

        // Point callback directly at combined buffer
        symbolCombinedBuffer = combined;

        // Decode PNG
        int rc = symbolPNG.open(path.c_str(), pngOpenFile, pngCloseFile, pngReadFile, pngSeekFile, pngSymbolCallback);
        if (rc == PNG_SUCCESS && pngFileOpened) {
            rc = symbolPNG.decode(nullptr, 0);
            symbolPNG.close();

            if (rc == PNG_SUCCESS) {
                symbolCombinedBuffer = nullptr;
                ESP_LOGI(TAG, "Loaded RGB565A8: %c%c from %s", table, symbol, path.c_str());
                return combined;
            }
        }

        // Failed
        symbolPNG.close();
        symbolCombinedBuffer = nullptr;
        free(combined);
        ESP_LOGE(TAG, "Failed to load symbol: %s", path.c_str());
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

        // Not in cache - load from SD (returns combined RGB565A8 buffer)
        uint8_t* data = loadSymbolFromSD(table, symbol);
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
        }

        const size_t rgb565Size = SYMBOL_SIZE * SYMBOL_SIZE * sizeof(uint16_t);
        const size_t alphaSize = SYMBOL_SIZE * SYMBOL_SIZE;

        // Store in cache
        symbolCache[slotIdx].table = table;
        symbolCache[slotIdx].symbol = symbol;
        symbolCache[slotIdx].data = data;
        symbolCache[slotIdx].lastAccess = symbolCacheAccessCounter++;
        symbolCache[slotIdx].valid = true;

        // Setup LVGL image descriptor for RGB565A8 format
        symbolCache[slotIdx].img_dsc.header.always_zero = 0;
        symbolCache[slotIdx].img_dsc.header.w = SYMBOL_SIZE;
        symbolCache[slotIdx].img_dsc.header.h = SYMBOL_SIZE;
        symbolCache[slotIdx].img_dsc.data_size = rgb565Size + alphaSize;
        symbolCache[slotIdx].img_dsc.header.cf = LV_IMG_CF_RGB565A8;
        symbolCache[slotIdx].img_dsc.data = data;

        return &symbolCache[slotIdx];
    }

    // Get symbol from cache or load from SD (backward compatibility wrapper)
    lv_img_dsc_t* getSymbol(char table, char symbol) {
        CachedSymbol* cache = getSymbolCacheEntry(table, symbol);
        return cache ? &cache->img_dsc : nullptr;
    }


// =========================================================================
    // =                  END OF VECTOR TILE RENDERING ENGINE                  =
    // =========================================================================

    // Preload a tile into cache (no canvas drawing) - called from Core 1 task
    bool preloadTileToCache(int tileX, int tileY, int zoom) {
        // NAV priority: don't cache raster tiles when NAV mode is active
        if (navModeActive) return false;

        // 1. Check if already in cache
        if (MapEngine::findCachedTile(zoom, tileX, tileY) >= 0) {
            return true;
        }

        // 2. Find file path (PNG, then JPG) — under SPI mutex
        // NAV tiles are NOT cached per-tile; they use renderNavViewport() exclusively.
        char path[128];
        char found_path[128] = {0};
        bool found = false;

        if (spiMutex != NULL && xSemaphoreTake(spiMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
            if (STORAGE_Utils::isSDAvailable()) {
                const char* region = map_current_region.c_str();

                snprintf(path, sizeof(path), "/LoRa_Tracker/Maps/%s/%d/%d/%d.png", region, zoom, tileX, tileY);
                if (SD.exists(path)) { strcpy(found_path, path); found = true; }
                else {
                    snprintf(path, sizeof(path), "/LoRa_Tracker/Maps/%s/%d/%d/%d.jpg", region, zoom, tileX, tileY);
                    if (SD.exists(path)) { strcpy(found_path, path); found = true; }
                }
            }
            xSemaphoreGive(spiMutex);
        }

        if (!found) {
            return false;
        }

        // 3. Allocate a new sprite for the tile
        LGFX_Sprite* newSprite = new LGFX_Sprite(&tft);
        newSprite->setPsram(true);
        if (newSprite->createSprite(MAP_TILE_SIZE, MAP_TILE_SIZE) == nullptr) {
            ESP_LOGE(TAG, "Sprite creation failed");
            delete newSprite;
            return false;
        }

        // 4. Render the tile directly (synchronous call). The render function handles its own mutex.
        bool success = MapEngine::renderTile(found_path, 0, 0, *newSprite, (uint8_t)zoom);

        // 5. If rendering is successful, add to cache
        if (success) {
            MapEngine::addToCache(found_path, zoom, tileX, tileY, newSprite);
        } else {
            // Cleanup on failure
            newSprite->deleteSprite();
            delete newSprite;
        }
        
        return success;
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
        double n = pow(2.0, zoom);

        // Calculate world coordinates (0.0 to 1.0) for target and center using Web Mercator projection
        double target_x_world = (lon + 180.0) / 360.0;
        double target_lat_rad = lat * PI / 180.0;
        double target_y_world = (1.0 - log(tan(target_lat_rad) + 1.0 / cos(target_lat_rad)) / PI) / 2.0;

        double center_x_world = (centerLon + 180.0) / 360.0;
        double center_lat_rad = centerLat * PI / 180.0;
        double center_y_world = (1.0 - log(tan(center_lat_rad) + 1.0 / cos(center_lat_rad)) / PI) / 2.0;

        // Calculate delta in world coordinates, scale by total map size in pixels at this zoom
        double delta_x_px = (target_x_world - center_x_world) * n * MAP_TILE_SIZE;
        double delta_y_px = (target_y_world - center_y_world) * n * MAP_TILE_SIZE;

        // Position relative to canvas center
        *pixelX = (int)(MAP_CANVAS_WIDTH / 2.0 + delta_x_px);
        *pixelY = (int)(MAP_CANVAS_HEIGHT / 2.0 + delta_y_px);
    }



/*    // Station click handler - opens compose screen with prefilled callsign
    void map_station_clicked(lv_event_t* e) {
        int stationIndex = (int)(intptr_t)lv_event_get_user_data(e);
        MapStation* station = STATION_Utils::getMapStation(stationIndex);

        if (station && station->valid && station->callsign.length() > 0) {
            ESP_LOGI(TAG, "Station clicked: %s", station->callsign.c_str());
            map_prefill_callsign = station->callsign;
            LVGL_UI::open_compose_with_callsign(station->callsign); // Call public function
        }
    }
*/
    // Map back button handler
    void btn_map_back_clicked(lv_event_t* e) {
        ESP_LOGI(TAG, "MAP BACK button pressed");
        MapEngine::stopRenderTask();
        cleanup_station_buttons();
        map_follow_gps = true;  // Reset to follow GPS when leaving map
        // Stop periodic refresh timer
        if (map_refresh_timer) {
            lv_timer_del(map_refresh_timer);
            map_refresh_timer = nullptr;
        }
        // Stop tile preload task
        stopTilePreloadTask();
        // Keep persistent viewport sprite alive (never free — avoids PSRAM fragmentation)
        // See CLAUDE.md: "Sprites persistants — allouer une seule fois, ne jamais libérer/recréer"
        // Return CPU to 80 MHz for power saving
        setCpuFrequencyMhz(80);
        ESP_LOGI(TAG, "CPU reduced to %d MHz", getCpuFrequencyMhz());
        ESP_LOGI(TAG, "After MAP exit - DRAM: %u  PSRAM: %u  Largest DRAM block: %u",
                      heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                      heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
        // Return to main dashboard screen
        LVGL_UI::return_to_dashboard();
    }

    // GPX recording button and handler
    static lv_obj_t* btn_gpx_rec = nullptr;
    static lv_obj_t* lbl_gpx_rec = nullptr;

    static void updateGpxRecButton() {
        if (!btn_gpx_rec) return;
        bool rec = GPXWriter::isRecording();
        lv_obj_set_style_bg_color(btn_gpx_rec, rec ? lv_color_hex(0xCC0000) : lv_color_hex(0x16213e), 0);
        lv_label_set_text(lbl_gpx_rec, rec ? LV_SYMBOL_STOP : LV_SYMBOL_PLAY);
    }

    static void btn_gpx_rec_clicked(lv_event_t* e) {
        if (GPXWriter::isRecording()) {
            GPXWriter::stopRecording();
        } else {
            GPXWriter::startRecording();
        }
        updateGpxRecButton();
    }

    // Map recenter button handler - return to GPS position
    void btn_map_recenter_clicked(lv_event_t* e) {
        ESP_LOGI(TAG, "Recentering on GPS");
        map_follow_gps = true;
        if (gps.location.isValid()) {
            map_center_lat = gps.location.lat();
            map_center_lon = gps.location.lng();
            ESP_LOGI(TAG, "Recentered on GPS: %.4f, %.4f", map_center_lat, map_center_lon);
        } else {
            // No GPS - return to default Ariège position
            map_center_lat = 42.9667f;
            map_center_lon = 1.6053f;
            ESP_LOGW(TAG, "No GPS, recentered on default position: %.4f, %.4f", map_center_lat, map_center_lon);
        }
        schedule_map_reload();
    }

    // Add a point to own GPS trace (called from sendBeacon when position changes)
    void addOwnTracePoint(float lat, float lon, float hdop) {
        static float centroidLat = 0.0f;  // Running centroid of GPS readings
        static float centroidLon = 0.0f;
        static uint32_t centroidCount = 0; // Number of readings in centroid
        static float lastHdop = 99.0f;

        // Reject bad GPS fix entirely
        if (hdop > 5.0f) return;
        if (gps.satellites.value() < 6) return;

        // Centroid-based jitter filter: compare to average of all recent GPS readings,
        // not just the last accepted point. This prevents drift from accumulated jitter.
        if (ownTraceCount > 0) {
            // Update running centroid with this reading (accepted or not)
            if (centroidCount == 0) {
                centroidLat = lat;
                centroidLon = lon;
                centroidCount = 1;
            } else {
                // Exponential moving average (alpha ~0.1 for smooth convergence)
                float alpha = (centroidCount < 10) ? 1.0f / (centroidCount + 1) : 0.1f;
                centroidLat += alpha * (lat - centroidLat);
                centroidLon += alpha * (lon - centroidLon);
                centroidCount++;
            }

            // Threshold based on HDOP: 15m min, +5m per HDOP unit
            float worstHdop = fmax(lastHdop, hdop);
            float thresholdM = fmax(15.0f, worstHdop * 5.0f);
            float thresholdLat = thresholdM / 111320.0f;
            float thresholdLon = thresholdM / (111320.0f * cosf(lat * M_PI / 180.0f));

            // Compare to centroid, not to last accepted point
            if (fabs(lat - centroidLat) < thresholdLat && fabs(lon - centroidLon) < thresholdLon) {
                return;
            }

            // Real movement — reset centroid to new position
            centroidLat = lat;
            centroidLon = lon;
            centroidCount = 1;
        }

        lastHdop = hdop;

        // Add point to circular buffer
        ownTrace[ownTraceHead].lat = lat;
        ownTrace[ownTraceHead].lon = lon;
        ownTrace[ownTraceHead].time = millis();
        ownTraceHead = (ownTraceHead + 1) % TRACE_MAX_POINTS;
        if (ownTraceCount < TRACE_MAX_POINTS) {
            ownTraceCount++;
        }

        ESP_LOGD(TAG, "Own trace point added: %.6f, %.6f (count=%d)", lat, lon, ownTraceCount);
    }

    // Draw GPS traces for mobile stations on the canvas
    #define TRACE_TTL_MS (60 * 60 * 1000)  // 60 minutes TTL for station traces

    void draw_station_traces() {
        if (!map_canvas) return;

        uint32_t now = millis();
        lv_draw_line_dsc_t line_dsc;
        lv_draw_line_dsc_init(&line_dsc);

        // Draw own GPS trace first (purple/violet) — no TTL for own trace
        if (ownTraceCount > 0 && filteredOwnValid) {
            line_dsc.color = lv_color_hex(0x9933FF);  // Purple/violet
            line_dsc.width = 2;
            line_dsc.opa   = LV_OPA_COVER;

            lv_point_t pts[TRACE_MAX_POINTS + 1];
            int validPts = 0;

            for (int i = 0; i < ownTraceCount; i++) {
                int idx = (ownTraceHead - ownTraceCount + i + TRACE_MAX_POINTS) % TRACE_MAX_POINTS;
                int px, py;
                latLonToPixel(ownTrace[idx].lat, ownTrace[idx].lon,
                              map_center_lat, map_center_lon, map_current_zoom, &px, &py);
                pts[validPts].x = px;
                pts[validPts].y = py;
                validPts++;
            }

            // Filtered own position as last point (consistent with icon)
            int cx, cy;
            latLonToPixel(filteredOwnLat, filteredOwnLon,
                          map_center_lat, map_center_lon, map_current_zoom, &cx, &cy);
            pts[validPts].x = cx;
            pts[validPts].y = cy;
            validPts++;

            if (validPts >= 2)
                lv_canvas_draw_line(map_canvas, pts, validPts, &line_dsc);
        }

        // Draw received stations traces (blue) — TTL filtered
        line_dsc.color = lv_color_hex(0x0055FF);
        line_dsc.width = 2;
        line_dsc.opa   = LV_OPA_COVER;

        for (int s = 0; s < MAP_STATIONS_MAX; s++) {
            MapStation* station = STATION_Utils::getMapStation(s);
            if (!station || !station->valid || station->traceCount < 1) continue;

            lv_point_t pts[TRACE_MAX_POINTS + 1];
            int validPts = 0;

            for (int i = 0; i < station->traceCount; i++) {
                int idx = (station->traceHead - station->traceCount + i + TRACE_MAX_POINTS) % TRACE_MAX_POINTS;
                // Skip points older than TTL
                if ((now - station->trace[idx].time) > TRACE_TTL_MS) continue;
                int px, py;
                latLonToPixel(station->trace[idx].lat, station->trace[idx].lon,
                              map_center_lat, map_center_lon, map_current_zoom, &px, &py);
                pts[validPts].x = px;
                pts[validPts].y = py;
                validPts++;
            }

            // Current position as last point
            int cx, cy;
            latLonToPixel(station->latitude, station->longitude,
                          map_center_lat, map_center_lon, map_current_zoom, &cx, &cy);
            pts[validPts].x = cx;
            pts[validPts].y = cy;
            validPts++;

            if (validPts >= 2)
                lv_canvas_draw_line(map_canvas, pts, validPts, &line_dsc);
        }
    }

    // Redraw only canvas content without recreating screen (for zoom)
    void redraw_map_canvas() {
        if (!map_canvas || !map_canvas_buf || !map_title_label) {
            screen_map = nullptr; // Force recreation
            create_map_screen();
            lv_disp_load_scr(screen_map);
            return;
        }

        if (redraw_in_progress) return;  // Prevent overlapping redraws
        redraw_in_progress = true;

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

        ESP_LOGD(TAG, "Center tile: %d/%d, sub-tile offset: %d,%d", centerTileX, centerTileY, subTileOffsetX, subTileOffsetY);

        // Load tiles — two paths:
        // 1) NAV viewport (IceNav-v3 pattern): loads ALL tiles, renders in single z-ordered pass
        // 2) Raster per-tile (PNG/JPG): loads tiles individually via cache
        bool hasTiles = false;
        if (STORAGE_Utils::isSDAvailable()) {
            // Check if NAV data available: try each region for pack file or legacy tile
            // Z6-Z8: force raster — NAV feature density too high for ESP32
            char navCheckPath[128];
            bool isNavMode = false;
            if (navRegionCount > 0 && map_current_zoom >= 9) {
                if (spiMutex != NULL && xSemaphoreTake(spiMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
                    for (int r = 0; r < navRegionCount && !isNavMode; r++) {
                        // Try NPK2 pack file first
                        snprintf(navCheckPath, sizeof(navCheckPath), "/LoRa_Tracker/VectMaps/%s/Z%d.nav",
                                 navRegions[r].c_str(), map_current_zoom);
                        isNavMode = SD.exists(navCheckPath);
                        if (!isNavMode) {
                            // Try split pack (Z{z}_0.nav)
                            snprintf(navCheckPath, sizeof(navCheckPath), "/LoRa_Tracker/VectMaps/%s/Z%d_0.nav",
                                     navRegions[r].c_str(), map_current_zoom);
                            isNavMode = SD.exists(navCheckPath);
                        }
                        if (!isNavMode) {
                            // Fallback: legacy individual tile
                            snprintf(navCheckPath, sizeof(navCheckPath), "/LoRa_Tracker/VectMaps/%s/%d/%d/%d.nav",
                                     navRegions[r].c_str(), map_current_zoom, centerTileX, centerTileY);
                            isNavMode = SD.exists(navCheckPath);
                        }
                    }
                    xSemaphoreGive(spiMutex);
                } else {
                    ESP_LOGW(TAG, "isNavMode check TIMEOUT (spiMutex busy) at Z%d", map_current_zoom);
                }
            }

            if (isNavMode) {
                // NAV priority: free all raster cache to maximize PSRAM for NAV tiles
                if (!navModeActive) {
                    navModeActive = true;
                    MapEngine::clearTileCache();
                    ESP_LOGI(TAG, "After clearTileCache - PSRAM free: %u KB, largest block: %u KB",
                                  ESP.getFreePsram() / 1024,
                                  heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024);
                    switchZoomTable(nav_zooms, nav_zoom_count);
                }

                // NAV viewport rendering (IceNav-v3 pattern)
                // Temporarily unsubscribe loopTask from WDT — rendering at low zoom
                // can take 10-30s with thousands of features (IceNav doesn't subscribe either)
                esp_task_wdt_delete(xTaskGetCurrentTaskHandle());

                // Build region pointer array for renderNavViewport
                const char* regionPtrs[NAV_MAX_REGIONS];
                for (int r = 0; r < navRegionCount; r++) regionPtrs[r] = navRegions[r].c_str();

                if (persistentViewportSprite) {
                    ESP_LOGI(TAG, "Before renderNavViewport - PSRAM free: %u KB, largest block: %u KB",
                                  ESP.getFreePsram() / 1024,
                                  heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024);
                    hasTiles = MapEngine::renderNavViewport(
                        map_center_lat, map_center_lon, (uint8_t)map_current_zoom,
                        *persistentViewportSprite, regionPtrs, navRegionCount);

                    if (hasTiles) {
                        uint16_t* src = (uint16_t*)persistentViewportSprite->getBuffer();
                        if (src && map_canvas_buf) {
                            // NAV rendering uses LGFX native format (big-endian on ESP32)
                            // No byte-swap needed - direct copy
                            memcpy(map_canvas_buf, src, MAP_CANVAS_WIDTH * MAP_CANVAS_HEIGHT * sizeof(lv_color_t));
                        }
                    }
                } else {
                    ESP_LOGW(TAG, "No viewport sprite available for NAV rendering");
                }

                // Re-subscribe loopTask to WDT
                esp_task_wdt_add(xTaskGetCurrentTaskHandle());
                esp_task_wdt_reset();
            } else {
                if (navModeActive) {
                    navModeActive = false;
                    switchZoomTable(raster_zooms, raster_zoom_count);
                }
                // Raster per-tile rendering (PNG/JPG via cache)
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int tileX = centerTileX + dx;
                        int tileY = centerTileY + dy;
                        int offsetX = MAP_CANVAS_WIDTH / 2 - subTileOffsetX + dx * MAP_TILE_SIZE;
                        int offsetY = MAP_CANVAS_HEIGHT / 2 - subTileOffsetY + dy * MAP_TILE_SIZE;

                        if (loadTileFromSD(tileX, tileY, map_current_zoom, map_canvas, offsetX, offsetY)) {
                            hasTiles = true;
                        }
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

        // Draw GPS traces for mobile stations (on canvas, under station icons)
        draw_station_traces();

        // Update station LVGL objects (own position + received stations)
        update_station_objects();

        // Update info bar with current coordinates and station count
        if (map_info_label) {
            char info_text[64];
            snprintf(info_text, sizeof(info_text), "Lat: %.4f  Lon: %.4f  Stations: %d",
                     map_center_lat, map_center_lon, mapStationsCount);
            lv_label_set_text(map_info_label, info_text);
        }

        // Recenter canvas after drawing new tiles (avoids visual jump)
        lv_obj_set_pos(map_canvas, -MAP_CANVAS_MARGIN, -MAP_CANVAS_MARGIN);

        // Force LVGL to re-read the canvas buffer after direct memcpy writes.
        // Without lv_canvas_set_buffer, LVGL may use a stale cached state and
        // not repaint tiles that were written via memcpy (not lv_canvas_draw_*).
        lv_canvas_set_buffer(map_canvas, map_canvas_buf, MAP_CANVAS_WIDTH, MAP_CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);
        lv_obj_invalidate(map_canvas);

        // Reset periodic refresh timer so it doesn't fire right after this redraw
        // (avoids double-blocking: pan redraw 600ms + immediate periodic refresh 600ms)
        if (map_refresh_timer) {
            lv_timer_reset(map_refresh_timer);
        }

        // Resume async preloading
        mainThreadLoading = false;
        redraw_in_progress = false;

        // Flush pending LVGL events immediately so touches during the blocking
        // render are processed without waiting for the next main loop iteration
        lv_timer_handler();
    }

    // Timer callback to reload map screen (for panning/recentering)
    void map_reload_timer_cb(lv_timer_t* timer) {
        pending_reload_timer = nullptr;
        lv_timer_del(timer);
        if (redraw_in_progress) return;  // Skip if previous redraw still running
        redraw_map_canvas();
    }

    // Helper function to schedule map reload with delay
    // Cancels any pending reload to avoid piling up redraws
    void schedule_map_reload() {
        if (pending_reload_timer) {
            lv_timer_del(pending_reload_timer);
            pending_reload_timer = nullptr;
        }
        pending_reload_timer = lv_timer_create(map_reload_timer_cb, 20, NULL);
        lv_timer_set_repeat_count(pending_reload_timer, 1);
    }

    // Map zoom in handler
    void btn_map_zoomin_clicked(lv_event_t* e) {
        if (!navModeActive && navRegionCount > 0 &&
            map_current_zoom < nav_zooms[0] &&
            (map_zoom_index >= map_zoom_count - 1 ||
             map_available_zooms[map_zoom_index + 1] > nav_zooms[0])) {
            // Next raster zoom would skip over NAV start — switch to NAV
            switchZoomTable(nav_zooms, nav_zoom_count);
            map_zoom_index = 0;
            map_current_zoom = nav_zooms[0];
            ESP_LOGI(TAG, "Zoom in: %d (raster->NAV)", map_current_zoom);
            redraw_map_canvas();
        } else if (map_zoom_index < map_zoom_count - 1) {
            map_zoom_index++;
            map_current_zoom = map_available_zooms[map_zoom_index];
            ESP_LOGI(TAG, "Zoom in: %d", map_current_zoom);
            if (navModeActive) MapEngine::clearTileCache();
            redraw_map_canvas();
        }
    }

    // Map zoom out handler
    void btn_map_zoomout_clicked(lv_event_t* e) {
        if (map_zoom_index > 0) {
            map_zoom_index--;
            map_current_zoom = map_available_zooms[map_zoom_index];
            ESP_LOGI(TAG, "Zoom out: %d", map_current_zoom);
            if (navModeActive) MapEngine::clearTileCache();
            redraw_map_canvas();
        } else if (navModeActive) {
            // At min NAV zoom — switch to raster
            navModeActive = false;
            MapEngine::clearTileCache();
            switchZoomTable(raster_zooms, raster_zoom_count);
            ESP_LOGI(TAG, "Zoom out: %d (NAV->raster)", map_current_zoom);
            redraw_map_canvas();
        }
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
            last_pan_dx = 0;
            last_pan_dy = 0;
            drag_start_lat = map_center_lat;
            drag_start_lon = map_center_lon;
            touch_dragging = false;

            // Cancel pending reload timer to allow immediate pan response
            // Avoids starting a redraw just as user begins a new pan
            if (pending_reload_timer) {
                lv_timer_del(pending_reload_timer);
                pending_reload_timer = nullptr;
            }

            ESP_LOGD(TAG, "Touch PRESSED at %d,%d - start pos: %.4f, %.4f",
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
                ESP_LOGD(TAG, "Touch pan started");

                // Immediate preload: queue tiles in direction of movement (raster only)
                if (tilePreloadQueue != nullptr && !navModeActive) {
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
                    ESP_LOGD(TAG, "Preload queued for direction dx=%d dy=%d", dir_x, dir_y);
                }
            }

            // Live preview: move canvas within margin bounds
            if (touch_dragging && map_canvas) {
                // Canvas starts at (-MARGIN, -MARGIN), so new position is (-MARGIN + dx, -MARGIN + dy)
                lv_coord_t new_x = -MAP_CANVAS_MARGIN + dx;
                lv_coord_t new_y = -MAP_CANVAS_MARGIN + dy;

                if (abs(dx) > MAP_CANVAS_MARGIN - 10 || abs(dy) > MAP_CANVAS_MARGIN - 10) {
                    if (dx > MAP_CANVAS_MARGIN - 10) dx = MAP_CANVAS_MARGIN - 10;
                    if (dx < -(MAP_CANVAS_MARGIN - 10)) dx = -(MAP_CANVAS_MARGIN - 10);
                    if (dy > MAP_CANVAS_MARGIN - 10) dy = MAP_CANVAS_MARGIN - 10;
                    if (dy < -(MAP_CANVAS_MARGIN - 10)) dy = -(MAP_CANVAS_MARGIN - 10);

                    new_x = -MAP_CANVAS_MARGIN + dx;
                    new_y = -MAP_CANVAS_MARGIN + dy;
                }

                {
                    // Normal panning within margin
                    lv_obj_set_pos(map_canvas, new_x, new_y);
                    last_pan_dx = dx;
                    last_pan_dy = dy;
                }
            }
        }
        else if (code == LV_EVENT_RELEASED) {
            if (touch_dragging) {
                // Finger up after pan - finish pan
                touch_dragging = false;

                // Calculate final displacement (clamped to same range as preview)
                lv_coord_t dx = point.x - touch_start_x;
                lv_coord_t dy = point.y - touch_start_y;
                lv_coord_t max_pan = MAP_CANVAS_MARGIN - 10;
                if (dx > max_pan) dx = max_pan;
                if (dx < -max_pan) dx = -max_pan;
                if (dy > max_pan) dy = max_pan;
                if (dy < -max_pan) dy = -max_pan;

                // Convert pixel movement to lat/lon change using Mercator projection
                double n_d = pow(2.0, map_current_zoom);
                double degrees_per_pixel_lon = 360.0 / n_d / MAP_TILE_SIZE;

                // Longitude: linear in Mercator (correct as-is)
                map_center_lon = drag_start_lon - (dx * degrees_per_pixel_lon);

                // Latitude: inverse Mercator for correct N/S displacement
                double start_lat_rad = drag_start_lat * PI / 180.0;
                double center_y_world = (1.0 - log(tan(start_lat_rad) + 1.0 / cos(start_lat_rad)) / PI) / 2.0;
                double new_y_world = center_y_world - (double)dy / (n_d * MAP_TILE_SIZE);
                // Clamp to valid Mercator range
                if (new_y_world < 0.0) new_y_world = 0.0;
                if (new_y_world > 1.0) new_y_world = 1.0;
                map_center_lat = atan(sinh(PI * (1.0 - 2.0 * new_y_world))) * 180.0 / PI;

                ESP_LOGD(TAG, "Touch pan end: %.4f,%.4f -> %.4f,%.4f",
                              drag_start_lat, drag_start_lon, map_center_lat, map_center_lon);

                // Schedule redraw (canvas will be recentered after new tiles are drawn)
                schedule_map_reload();

                // Preload tiles at adjacent zoom levels for fast zoom switch
                int cX, cY;
                latLonToTile(map_center_lat, map_center_lon, map_current_zoom, &cX, &cY);
                queueAdjacentZoomTiles(cX, cY, map_current_zoom);
            } else {
                // Tap (no drag) - check if a station was tapped
                for (int i = 0; i < stationHitZoneCount; i++) {
                    int16_t hx = stationHitZones[i].x;
                    int16_t hy = stationHitZones[i].y;
                    int16_t hw = stationHitZones[i].w;
                    int16_t hh = stationHitZones[i].h;

                    // Check if tap is within hit zone (centered on station)
                    if (point.x >= hx - hw/2 && point.x <= hx + hw/2 &&
                        point.y >= hy - hh/2 && point.y <= hy + hh/2) {

                        int stationIdx = stationHitZones[i].stationIdx;
                        MapStation* station = STATION_Utils::getMapStation(stationIdx);
                        if (station && station->valid && station->callsign.length() > 0) {
                            ESP_LOGI(TAG, "Station tapped: %s", station->callsign.c_str());
                            LVGL_UI::open_compose_with_callsign(station->callsign);
                        }
                        break;
                    }
                }
            }
        }
    }

    // Discover the first available map region from the SD card
    static void discoverAndSetMapRegion() {
        if (!map_current_region.isEmpty()) {
            return; // Region is already set
        }

        ESP_LOGI(TAG, "Map region not set, attempting to discover...");
        if (spiMutex != NULL && xSemaphoreTake(spiMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
            if (STORAGE_Utils::isSDAvailable()) {
                File mapsDir = SD.open("/LoRa_Tracker/Maps");
                if (mapsDir && mapsDir.isDirectory()) {
                    File entry = mapsDir.openNextFile();
                    while(entry) {
                        if (entry.isDirectory()) {
                            String dirName = String(entry.name());
                            // Extract just the directory name from the full path
                            map_current_region = dirName.substring(dirName.lastIndexOf('/') + 1);
                            ESP_LOGI(TAG, "Discovered and set map region: %s", map_current_region.c_str());
                            entry.close();
                            break; // Use the first one we find
                        }
                        entry.close();
                        entry = mapsDir.openNextFile();
                    }
                } else {
                    ESP_LOGE(TAG, "Could not open /LoRa_Tracker/Maps directory");
                }
                mapsDir.close();
            }
            xSemaphoreGive(spiMutex);
        } else {
            ESP_LOGE(TAG, "Could not get SPI Mutex for region discovery");
        }

        if (map_current_region.isEmpty()) {
            ESP_LOGW(TAG, "No map region found on SD card");
        }
    }

    // Check if a region's NPK2 pack file at given zoom contains tile (tileX, tileY)
    // Reads only the 25-byte header and checks Y range against y_min/y_max
    static bool regionContainsTile(const char* region, int zoom, int tileX, int tileY) {
        char path[128];
        snprintf(path, sizeof(path), "/LoRa_Tracker/VectMaps/%s/Z%d.nav", region, zoom);
        File f = SD.open(path, FILE_READ);
        if (!f) return false;

        // Read NPK2 header (25 bytes)
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

    // Discover ALL NAV regions on SD card, GPS-matching region first
    static void discoverNavRegions() {
        if (navRegionCount > 0) {
            return; // Already discovered
        }

        ESP_LOGI(TAG, "Discovering NAV regions...");

        // Compute center tile at a mid-range zoom for bounding box check
        const int checkZoom = 10;
        int centerTX, centerTY;
        latLonToTile(map_center_lat, map_center_lon, checkZoom, &centerTX, &centerTY);

        int gpsMatchIdx = -1;  // Index of GPS-matching region (to move to front)

        if (spiMutex != NULL && xSemaphoreTake(spiMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
            if (STORAGE_Utils::isSDAvailable()) {
                File vectDir = SD.open("/LoRa_Tracker/VectMaps");
                if (vectDir && vectDir.isDirectory()) {
                    File entry = vectDir.openNextFile();
                    while(entry && navRegionCount < NAV_MAX_REGIONS) {
                        if (entry.isDirectory()) {
                            String dirName = String(entry.name());
                            String regionName = dirName.substring(dirName.lastIndexOf('/') + 1);

                            navRegions[navRegionCount] = regionName;

                            // Check if this region covers our GPS position
                            if (gpsMatchIdx < 0 && regionContainsTile(regionName.c_str(), checkZoom, centerTX, centerTY)) {
                                gpsMatchIdx = navRegionCount;
                                ESP_LOGI(TAG, "NAV region GPS match: %s (tile %d/%d at Z%d)",
                                              regionName.c_str(), centerTX, centerTY, checkZoom);
                            } else {
                                ESP_LOGI(TAG, "NAV region found: %s", regionName.c_str());
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

        // Move GPS-matching region to front of array
        if (gpsMatchIdx > 0) {
            String tmp = navRegions[0];
            navRegions[0] = navRegions[gpsMatchIdx];
            navRegions[gpsMatchIdx] = tmp;
        }

        if (navRegionCount == 0) {
            ESP_LOGW(TAG, "No NAV region found on SD card");
        } else {
            ESP_LOGI(TAG, "Discovered %d NAV region(s), primary: %s",
                          navRegionCount, navRegions[0].c_str());
        }
    }


bool loadTileFromSD(int tileX, int tileY, int zoom, lv_obj_t* canvas, int offsetX, int offsetY) {
    if (spiMutex == NULL) {
        ESP_LOGE(TAG, "spiMutex is NULL, skipping SD access");
        return false;
    }

    uint32_t tileHash = (static_cast<uint32_t>(zoom) << 28) | (static_cast<uint32_t>(tileX) << 14) | static_cast<uint32_t>(tileY);

    // --- 1. Check positive cache first ---
    int cacheIdx = MapEngine::findCachedTile(zoom, tileX, tileY);
    if (cacheIdx >= 0) {
        LGFX_Sprite* cachedSprite = MapEngine::getCachedTileSprite(cacheIdx);
        MapEngine::copySpriteToCanvasWithClip(canvas, cachedSprite, offsetX, offsetY);
        return true;
    }

    // --- 2. Check negative cache ---
    for (const auto& hash : notFoundCache) {
        if (hash == tileHash) {
            return false; // Tile is known to be missing, don't scan SD
        }
    }

    // --- 3. Find a valid tile file on SD card ---
    char path[128];
    char found_path[128] = {0};
    bool found = false;
    
    if (map_current_region.isEmpty()) {
        ESP_LOGE(TAG, "loadTileFromSD: map_current_region is empty! Cannot load %d/%d/%d",
                      zoom, tileX, tileY);
        return false;
    }

    // NAV tiles are NOT loaded per-tile; they use renderNavViewport() exclusively.
    // Only search for raster tiles (PNG, JPG).
    if (xSemaphoreTake(spiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (STORAGE_Utils::isSDAvailable()) {
            const char* region = map_current_region.c_str();

            snprintf(path, sizeof(path), "/LoRa_Tracker/Maps/%s/%d/%d/%d.png", region, zoom, tileX, tileY);
            if (SD.exists(path)) { strcpy(found_path, path); found = true; }
            else {
                snprintf(path, sizeof(path), "/LoRa_Tracker/Maps/%s/%d/%d/%d.jpg", region, zoom, tileX, tileY);
                if (SD.exists(path)) { strcpy(found_path, path); found = true; }
            }
        }
        xSemaphoreGive(spiMutex);
    } else {
        ESP_LOGE(TAG, "Could not get SPI Mutex for SD access");
    }

    // --- 4. If file not found, add to negative cache and return ---
    if (!found) {
        if (notFoundCache.size() < NOT_FOUND_CACHE_SIZE) {
            notFoundCache.push_back(tileHash);
        } else {
            // Overwrite oldest entry (circular buffer)
            notFoundCache[notFoundCacheIndex] = tileHash;
            notFoundCacheIndex = (notFoundCacheIndex + 1) % NOT_FOUND_CACHE_SIZE;
        }
        return false;
    }

    // --- 5. SYNCHRONOUS decode + copy to canvas + cache ---
    LGFX_Sprite* newSprite = new LGFX_Sprite(&tft);
    newSprite->setPsram(true);
    if (newSprite->createSprite(MAP_TILE_SIZE, MAP_TILE_SIZE) != nullptr) {
        // renderTile() handles its own SPI mutex internally (recursive)
        if (MapEngine::renderTile(found_path, 0, 0, *newSprite, (uint8_t)zoom)) {
            // Copy to canvas IMMEDIATELY
            MapEngine::copySpriteToCanvasWithClip(canvas, newSprite, offsetX, offsetY);
            // Cache the sprite (addToCache takes ownership)
            MapEngine::addToCache(found_path, zoom, tileX, tileY, newSprite);
            return true;
        }
        // Decode failed
        newSprite->deleteSprite();
        delete newSprite;
    } else {
        ESP_LOGE(TAG, "Sprite creation failed (Out of PSRAM?)");
        delete newSprite;
    }
    return false;
}


    // Create map screen
    void create_map_screen() {
        // Boost CPU to 240 MHz for smooth map rendering
        setCpuFrequencyMhz(240);
        ESP_LOGI(TAG, "CPU boosted to %d MHz", getCpuFrequencyMhz());

        // Set initial position before region discovery (needed for GPS-based region matching)
        if (map_follow_gps && gps.location.isValid()) {
            map_center_lat = gps.location.lat();
            map_center_lon = gps.location.lng();
        } else if (map_center_lat == 0.0f && map_center_lon == 0.0f) {
            map_center_lat = 42.9667f;
            map_center_lon = 1.6053f;
        }

        // Discover and set the map region if it's not already defined
        discoverAndSetMapRegion();
        discoverNavRegions();

        ESP_LOGI(TAG, "Regions discovered - Maps: '%s', VectMaps: %d region(s)",
                      map_current_region.c_str(), navRegionCount);

        // Load Unicode font for map labels (VLW from SD)
        MapEngine::loadMapFont();

        // Clean up old station buttons if screen is being recreated
        cleanup_station_buttons();

        screen_map = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen_map, lv_color_hex(0x1a1a2e), 0);

        // Use current GPS position as center if follow mode is active
        if (map_follow_gps && gps.location.isValid()) {
            map_center_lat = gps.location.lat();
            map_center_lon = gps.location.lng();
            ESP_LOGI(TAG, "Using GPS position: %.4f, %.4f", map_center_lat, map_center_lon);
        } else if (map_center_lat == 0.0f && map_center_lon == 0.0f) {
            // Default to Ariège (Foix) if no GPS - matches OCC tiles
            map_center_lat = 42.9667f;
            map_center_lon = 1.6053f;
            ESP_LOGW(TAG, "No GPS, using default position: %.4f, %.4f", map_center_lat, map_center_lon);
        } else {
            ESP_LOGI(TAG, "Using pan position: %.4f, %.4f", map_center_lat, map_center_lon);
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

        // Recenter button (GPS icon) - leftmost, shows different color when GPS not followed
        lv_obj_t* btn_recenter = lv_btn_create(title_bar);
        lv_obj_set_size(btn_recenter, 30, 25);
        lv_obj_set_style_bg_color(btn_recenter, map_follow_gps ? lv_color_hex(0x16213e) : lv_color_hex(0xff6600), 0);
        lv_obj_align(btn_recenter, LV_ALIGN_RIGHT_MID, -105, 0);
        lv_obj_add_event_cb(btn_recenter, btn_map_recenter_clicked, LV_EVENT_CLICKED, NULL);
        lv_obj_t* lbl_recenter = lv_label_create(btn_recenter);
        lv_label_set_text(lbl_recenter, LV_SYMBOL_GPS);
        lv_obj_center(lbl_recenter);

        // Zoom buttons
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

        // GPX record toggle button
        btn_gpx_rec = lv_btn_create(title_bar);
        lv_obj_set_size(btn_gpx_rec, 30, 25);
        lv_obj_align(btn_gpx_rec, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_add_event_cb(btn_gpx_rec, btn_gpx_rec_clicked, LV_EVENT_CLICKED, NULL);
        lbl_gpx_rec = lv_label_create(btn_gpx_rec);
        lv_obj_center(lbl_gpx_rec);
        updateGpxRecButton();

        // Map canvas area (container clips the larger canvas to visible area)
        map_container = lv_obj_create(screen_map);
        lv_obj_set_size(map_container, SCREEN_WIDTH, MAP_VISIBLE_HEIGHT);
        lv_obj_set_pos(map_container, 0, 35);
        lv_obj_set_style_bg_color(map_container, lv_color_hex(0x2F4F4F), 0);  // Dark slate gray
        lv_obj_set_style_border_width(map_container, 0, 0);
        lv_obj_set_style_radius(map_container, 0, 0);
        lv_obj_set_style_pad_all(map_container, 0, 0);
        lv_obj_clear_flag(map_container, LV_OBJ_FLAG_SCROLLABLE);  // Force clipping of children

        // Enable touch pan on map container
        lv_obj_add_flag(map_container, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(map_container, map_touch_event_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(map_container, map_touch_event_cb, LV_EVENT_PRESSING, NULL);
        lv_obj_add_event_cb(map_container, map_touch_event_cb, LV_EVENT_RELEASED, NULL);

        // Create canvas for map drawing
        // Allocate buffer once and keep it persistent (avoids PSRAM fragmentation)
        if (!map_canvas_buf) {
            map_canvas_buf = (lv_color_t*)heap_caps_malloc(MAP_CANVAS_WIDTH * MAP_CANVAS_HEIGHT * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
        }
        // Allocate persistent viewport sprite EARLY (before raster cache fills PSRAM)
        // to guarantee a contiguous 624KB block is available.
        if (!persistentViewportSprite) {
            persistentViewportSprite = new LGFX_Sprite(&tft);
            persistentViewportSprite->setPsram(true);
            if (persistentViewportSprite->createSprite(MAP_CANVAS_WIDTH, MAP_CANVAS_HEIGHT) == nullptr) {
                ESP_LOGE(TAG, "Failed to create persistent viewport sprite");
                delete persistentViewportSprite;
                persistentViewportSprite = nullptr;
            } else {
                ESP_LOGI(TAG, "Viewport sprite allocated: %dx%d (%u KB PSRAM)",
                              MAP_CANVAS_WIDTH, MAP_CANVAS_HEIGHT,
                              MAP_CANVAS_WIDTH * MAP_CANVAS_HEIGHT * 2 / 1024);
                ESP_LOGI(TAG, "PSRAM free: %u KB, largest block: %u KB",
                              ESP.getFreePsram() / 1024,
                              heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024);
            }
        }
        if (map_canvas_buf) {
            map_canvas = lv_canvas_create(map_container);
            lv_obj_clear_flag(map_canvas, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(map_canvas, LV_OBJ_FLAG_EVENT_BUBBLE);
            lv_canvas_set_buffer(map_canvas, map_canvas_buf, MAP_CANVAS_WIDTH, MAP_CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);

            // Start the background render task now that the canvas exists.
            // This is critical to ensure the queue is ready before loadTileFromSD is called.
            MapEngine::startRenderTask(map_canvas);

            // Position canvas with negative margin so visible area is centered
            lv_obj_set_pos(map_canvas, -MAP_CANVAS_MARGIN, -MAP_CANVAS_MARGIN);

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

            ESP_LOGD(TAG, "Center tile: %d/%d, sub-tile offset: %d,%d", centerTileX, centerTileY, subTileOffsetX, subTileOffsetY);

            // Pause async preloading while we load tiles (avoid SD contention)
            mainThreadLoading = true;

            // Try to load tiles from SD card
            bool hasTiles = false;
            if (STORAGE_Utils::isSDAvailable()) {
                // Check if NAV data available: try each region for pack file or legacy tile
                // Z6-Z8: force raster — NAV feature density too high for ESP32
                char navCheckPath[128];
                bool isNavMode = false;
                if (navRegionCount > 0 && map_current_zoom >= 9) {
                    if (spiMutex != NULL && xSemaphoreTake(spiMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
                        for (int r = 0; r < navRegionCount && !isNavMode; r++) {
                            // Try NPK2 pack file first
                            snprintf(navCheckPath, sizeof(navCheckPath), "/LoRa_Tracker/VectMaps/%s/Z%d.nav",
                                     navRegions[r].c_str(), map_current_zoom);
                            isNavMode = SD.exists(navCheckPath);
                            if (!isNavMode) {
                                // Try split pack (Z{z}_0.nav)
                                snprintf(navCheckPath, sizeof(navCheckPath), "/LoRa_Tracker/VectMaps/%s/Z%d_0.nav",
                                         navRegions[r].c_str(), map_current_zoom);
                                isNavMode = SD.exists(navCheckPath);
                            }
                            if (!isNavMode) {
                                // Fallback: legacy individual tile
                                snprintf(navCheckPath, sizeof(navCheckPath), "/LoRa_Tracker/VectMaps/%s/%d/%d/%d.nav",
                                         navRegions[r].c_str(), map_current_zoom, centerTileX, centerTileY);
                                isNavMode = SD.exists(navCheckPath);
                            }
                        }
                        xSemaphoreGive(spiMutex);
                    } else {
                        ESP_LOGW(TAG, "isNavMode check TIMEOUT (spiMutex busy) at Z%d", map_current_zoom);
                    }
                }

                if (isNavMode) {
                    // NAV priority: free all raster cache to maximize PSRAM for NAV tiles
                    navModeActive = true;
                    MapEngine::clearTileCache();
                    ESP_LOGI(TAG, "After clearTileCache - PSRAM free: %u KB, largest block: %u KB",
                                  ESP.getFreePsram() / 1024,
                                  heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024);
                    switchZoomTable(nav_zooms, nav_zoom_count);

                    // NAV viewport rendering (IceNav-v3 pattern)
                    // Temporarily unsubscribe loopTask from WDT — rendering can take 10-30s
                    esp_task_wdt_delete(xTaskGetCurrentTaskHandle());

                    // Build region pointer array for renderNavViewport
                    const char* regionPtrs[NAV_MAX_REGIONS];
                    for (int r = 0; r < navRegionCount; r++) regionPtrs[r] = navRegions[r].c_str();

                    if (persistentViewportSprite) {
                        ESP_LOGI(TAG, "Before renderNavViewport - PSRAM free: %u KB, largest block: %u KB",
                                      ESP.getFreePsram() / 1024,
                                      heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024);
                        hasTiles = MapEngine::renderNavViewport(
                            map_center_lat, map_center_lon, (uint8_t)map_current_zoom,
                            *persistentViewportSprite, regionPtrs, navRegionCount);
                        if (hasTiles && map_canvas_buf) {
                            uint16_t* src = (uint16_t*)persistentViewportSprite->getBuffer();
                            if (src) {
                                memcpy(map_canvas_buf, src, MAP_CANVAS_WIDTH * MAP_CANVAS_HEIGHT * sizeof(lv_color_t));
                            }
                        }
                    } else {
                        ESP_LOGW(TAG, "No viewport sprite available for NAV rendering");
                    }

                    esp_task_wdt_add(xTaskGetCurrentTaskHandle());
                    esp_task_wdt_reset();
                } else {
                    navModeActive = false;
                    switchZoomTable(raster_zooms, raster_zoom_count);
                    // Raster per-tile rendering (PNG/JPG via cache)
                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            int tileX = centerTileX + dx;
                            int tileY = centerTileY + dy;
                            int offsetX = MAP_CANVAS_WIDTH / 2 - subTileOffsetX + dx * MAP_TILE_SIZE;
                            int offsetY = MAP_CANVAS_HEIGHT / 2 - subTileOffsetY + dy * MAP_TILE_SIZE;

                            if (dx == 0 && dy == 0) {
                                ESP_LOGD(TAG, "Center tile offset: %d,%d", offsetX, offsetY);
                            }

                            if (loadTileFromSD(tileX, tileY, map_current_zoom, map_canvas, offsetX, offsetY)) {
                                hasTiles = true;
                            }
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

            // Draw GPS traces for mobile stations (on canvas, under station icons)
            draw_station_traces();

            // Update station LVGL objects (own position + received stations)
            update_station_objects();

            // Force canvas redraw after direct buffer writes
            lv_canvas_set_buffer(map_canvas, map_canvas_buf, MAP_CANVAS_WIDTH, MAP_CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);
            lv_obj_invalidate(map_canvas);

            // Resume async preloading
            mainThreadLoading = false;
        }

        // Info bar at bottom
        lv_obj_t* info_bar = lv_obj_create(screen_map);
        lv_obj_set_size(info_bar, SCREEN_WIDTH, 25);
        lv_obj_set_pos(info_bar, 0, SCREEN_HEIGHT - 25);
        lv_obj_set_style_bg_color(info_bar, lv_color_hex(0x16213e), 0);
        lv_obj_set_style_border_width(info_bar, 0, 0);
        lv_obj_set_style_radius(info_bar, 0, 0);
        lv_obj_set_style_pad_all(info_bar, 2, 0);

        // Display coordinates and station count (updated in redraw_map_canvas)
        map_info_label = lv_label_create(info_bar);
        char coords_text[64];
        snprintf(coords_text, sizeof(coords_text), "Lat: %.4f  Lon: %.4f  Stations: %d",
                 map_center_lat, map_center_lon, mapStationsCount);
        lv_label_set_text(map_info_label, coords_text);
        lv_obj_set_style_text_color(map_info_label, lv_color_hex(0xaaaaaa), 0);
        lv_obj_set_style_text_font(map_info_label, &lv_font_montserrat_14, 0);
        lv_obj_center(map_info_label);

        // Create periodic refresh timer for stations (10 seconds)
        if (map_refresh_timer) {
            lv_timer_del(map_refresh_timer);
        }
        map_refresh_timer = lv_timer_create(map_refresh_timer_cb, MAP_REFRESH_INTERVAL, NULL);

        // Start tile preload task on Core 1 for directional preloading during touch pan
        startTilePreloadTask();

        ESP_LOGD(TAG, "Map screen created");
    }


} // namespace UIMapManager

#endif // USE_LVGL_UI
