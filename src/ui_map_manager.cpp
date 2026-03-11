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

    // Tile reference — integer tile center (never a float)
    static int centerTileX = 0;       // Tile the DISPLAYED sprite is centered on
    static int centerTileY = 0;
    static int renderTileX = 0;       // Target tile for the next/current render request
    static int renderTileY = 0;

    // Touch pan state model
    static bool isScrollingMap = false;   // True while finger is on screen
    static bool dragStarted = false;      // True after START_THRESHOLD crossed
    static int16_t offsetX = 0;           // Sub-tile pixel offset
    static int16_t offsetY = 0;           // Sub-tile pixel offset
    static int16_t navSubTileX = 0;       // GPS sub-tile offset for NAV canvas centering
    static int16_t navSubTileY = 0;
    static float velocityX = 0.0f;        // Inertial momentum px/ms
    static float velocityY = 0.0f;
    static constexpr float PAN_FRICTION = 0.95f;       // Decay factor
    static constexpr float PAN_FRICTION_BUSY = 0.85f;  // Heavier friction during render
    static int last_x = 0, last_y = 0;
    static uint32_t last_time = 0;
    #define START_THRESHOLD 12        // Minimum pixels to start drag
    #define PAN_TILE_THRESHOLD 128    // Pixel threshold to trigger re-render

    // Tile data size for old raster tiles
    #define TILE_DATA_SIZE (MAP_TILE_SIZE * MAP_TILE_SIZE * sizeof(uint16_t))  // 128KB per tile

    // Symbol cache in PSRAM
    #define SYMBOL_CACHE_SIZE 10  // Cache for frequently used symbols
    #define SYMBOL_SIZE 24        // 24x24 pixels
    #define SYMBOL_DATA_SIZE (SYMBOL_SIZE * SYMBOL_SIZE * sizeof(lv_color_t))

    static CachedSymbol symbolCache[SYMBOL_CACHE_SIZE];
    static uint32_t symbolCacheAccessCounter = 0;
    static bool symbolCacheInitialized = false;

    // Map action buttons (persistent refs for press-state feedback during render)
    static lv_obj_t* btn_zoomin = nullptr;
    static lv_obj_t* btn_zoomout = nullptr;
    static lv_obj_t* btn_recenter = nullptr;
    static lv_obj_t* map_title_bar = nullptr;
    static lv_obj_t* map_info_bar = nullptr;
    static bool mapFullscreen = false;

    // Own GPS trace (separate from received stations)
    static TracePoint ownTrace[TRACE_MAX_POINTS];
    static uint8_t ownTraceCount = 0;
    static uint8_t ownTraceHead = 0;
    static bool pendingResetPan = false;

    // Forward declarations
    static void updateFilteredOwnPosition();
    static bool getUiPosition(float* lat, float* lon);
    void cleanup_station_buttons();
    void draw_station_traces();
    void update_station_objects();
    void redraw_map_canvas();
    static void scrollMap(int16_t dx, int16_t dy);
    static inline void resetPanOffset();
    static inline void resetZoom();
    static void tileToLatLon(int tileX, int tileY, int zoom, float* lat, float* lon);
    static void initCenterTileFromLatLon(float lat, float lon);
    static void toggleMapFullscreen();

    // Station hit zones for click detection (replaces LVGL buttons - no alloc/dealloc)
    struct StationHitZone {
        int16_t x, y;      // Screen position (center)
        int16_t w, h;      // Hit zone size
        int8_t stationIdx; // Index in mapStations array (-1 = unused)
    };
    static StationHitZone stationHitZones[MAP_STATIONS_MAX];
    static int stationHitZoneCount = 0;

    // No LVGL objects for stations — drawn directly on canvas (zero DRAM cost)

    // Periodic refresh timer — also polls for async render completion (50ms)
    static lv_timer_t* map_refresh_timer = nullptr;
    #define MAP_REFRESH_INTERVAL 50  // 50ms (polls NAV_DONE + periodic station refresh)

    // Redraw synchronization (prevent overlapping redraws)
    static volatile bool redraw_in_progress = false;

    // Double-buffer: back (Core 0 renders into) + front (Core 1 reads from)
    static LGFX_Sprite* backViewportSprite = nullptr;   // Render target (Core 0)
    static LGFX_Sprite* frontViewportSprite = nullptr;  // Display source (Core 1)

    // NAV priority flag: when true, raster cache is disabled
    static volatile bool navModeActive = false;

    // Async NAV render state
    static volatile bool navRenderPending = false;
    static volatile bool mainThreadLoading = false;  // Pause preload while main thread loads tiles on SD

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
    // Two levels: iconGps (≥3 sats, for display) and filteredOwn (≥6 sats, for trace/recentrage)
    static float iconGpsLat = 0.0f;
    static float iconGpsLon = 0.0f;
    static bool  iconGpsValid = false;       // Loose: ≥3 sats (2D fix minimum)
    static float filteredOwnLat = 0.0f;
    static float filteredOwnLon = 0.0f;
    static bool  filteredOwnValid = false;   // Strict: ≥6 sats (good 3D geometry)

    // Copy back→front sprite (byte-swap for raster, memcpy for NAV)
    // Caller MUST hold renderLock.
    static void copyBackToFront() {
        if (!backViewportSprite || !frontViewportSprite) return;
        uint16_t* src = (uint16_t*)backViewportSprite->getBuffer();
        uint16_t* dst = (uint16_t*)frontViewportSprite->getBuffer();
        if (!src || !dst) return;

        const int totalPixels = MAP_SPRITE_SIZE * MAP_SPRITE_SIZE;
#if LV_COLOR_16_SWAP
        if (!navModeActive) {
            for (int i = 0; i < totalPixels; i++) {
                uint16_t px = src[i];
                dst[i] = (px >> 8) | (px << 8);
            }
        } else {
            memcpy(dst, src, totalPixels * sizeof(uint16_t));
        }
#else
        memcpy(dst, src, totalPixels * sizeof(uint16_t));
#endif
    }

    // Apply rendered viewport.
    // Just copies sprite + updates UI. Does NOT touch offsetX/Y.
    static void applyRenderedViewport() {
        if (!backViewportSprite || !frontViewportSprite) return;

        // Try renderLock 50ms — skip frame if Core 0 is still rendering
        if (MapEngine::renderLock) {
            if (xSemaphoreTake(MapEngine::renderLock, pdMS_TO_TICKS(50)) != pdTRUE) {
                return;  // Core 0 busy — retry next 50ms tick
            }
        }

        copyBackToFront();

        if (MapEngine::renderLock) {
            xSemaphoreGive(MapEngine::renderLock);
        }

        navRenderPending = false;
        redraw_in_progress = false;
        mainThreadLoading = false;

        // Save old navSubTile offsets before updating them
        int16_t oldNavSubX = navSubTileX;
        int16_t oldNavSubY = navSubTileY;
        bool wasResetting = pendingResetPan;

        if (pendingResetPan) {
            offsetX = 0;
            offsetY = 0;
            velocityX = 0.0f;
            velocityY = 0.0f;
            renderTileX = centerTileX;
            renderTileY = centerTileY;
            pendingResetPan = false;
        } else {
            // Rebase offset to match new sprite center (async gap compensation)
            if (MapEngine::lastRenderedZoom == (uint8_t)map_current_zoom) {
                offsetX -= (MapEngine::lastRenderedTileX - centerTileX) * MAP_TILE_SIZE;
                offsetY -= (MapEngine::lastRenderedTileY - centerTileY) * MAP_TILE_SIZE;
            }
        }
        centerTileX = MapEngine::lastRenderedTileX;
        centerTileY = MapEngine::lastRenderedTileY;

        // Recalculate NAV sub-tile offset now that the new sprite is ready
        if (navModeActive) {
            uint32_t scale = 1 << map_current_zoom;
            navSubTileX = (int16_t)(((uint32_t)((map_center_lon + 180.0f) / 360.0f * scale * MAP_TILE_SIZE)) % MAP_TILE_SIZE) - MAP_TILE_SIZE / 2;
            float latRad = map_center_lat * (float)M_PI / 180.0f;
            float merc = logf(tanf(latRad) + 1.0f / cosf(latRad));
            navSubTileY = (int16_t)(((uint32_t)((1.0f - merc / (float)M_PI) / 2.0f * scale * MAP_TILE_SIZE)) % MAP_TILE_SIZE) - MAP_TILE_SIZE / 2;
        } else {
            navSubTileX = 0;
            navSubTileY = 0;
        }

        // Compensate the sub-tile grid jump if we are actively panning (not resetting).
        // The user dragged the canvas (offsetX/Y), but the map_center changed (jumping navSubTileX/Y).
        if (!wasResetting && navModeActive && MapEngine::lastRenderedZoom == (uint8_t)map_current_zoom) {
            offsetX -= (navSubTileX - oldNavSubX);
            offsetY -= (navSubTileY - oldNavSubY);
        }

        // UI updates
        if (map_title_label) {
            char title_text[32];
            snprintf(title_text, sizeof(title_text), "MAP (Z%d)", map_current_zoom);
            lv_label_set_text(map_title_label, title_text);
        }
        if (btn_zoomin) lv_obj_clear_state(btn_zoomin, LV_STATE_PRESSED);
        if (btn_zoomout) lv_obj_clear_state(btn_zoomout, LV_STATE_PRESSED);
        if (btn_recenter) lv_obj_clear_state(btn_recenter, LV_STATE_PRESSED);

        cleanup_station_buttons();
        draw_station_traces();
        update_station_objects();

        if (map_info_label) {
            char info_text[64];
            snprintf(info_text, sizeof(info_text), "Lat: %.4f  Lon: %.4f  Stations: %d",
                     map_center_lat, map_center_lon, mapStationsCount);
            lv_label_set_text(map_info_label, info_text);
        }

        lv_obj_invalidate(map_canvas);
        ESP_LOGI(TAG, "Viewport applied (Z%d) sprTile(%d,%d) offset(%d,%d)",
                      map_current_zoom, centerTileX, centerTileY, offsetX, offsetY);
    }

    // Lightweight station-only refresh: restore clean front from back, redraw stations.
    // Cost: ~10-50ms (memcpy 1.2MB + station icons) vs 500-3000ms for full NAV re-render.
    static void refreshStationOverlay() {
        if (!map_canvas || !backViewportSprite || !frontViewportSprite) return;

        // Restore clean background from back sprite (front has station artifacts)
        if (MapEngine::renderLock) {
            if (xSemaphoreTake(MapEngine::renderLock, pdMS_TO_TICKS(50)) != pdTRUE) {
                return;  // Core 0 busy — skip this refresh cycle
            }
        }
        copyBackToFront();
        if (MapEngine::renderLock) {
            xSemaphoreGive(MapEngine::renderLock);
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

        lv_obj_invalidate(map_canvas);
    }

    // Timer callback: polls async render completion + periodic station refresh.
    // Runs every 50ms. Replaces the old nav_render_poll_cb + 10s station refresh.
    static void map_refresh_timer_cb(lv_timer_t* timer) {
        if (!screen_map || lv_scr_act() != screen_map) return;

        // Check async render completion
        if (navRenderPending && MapEngine::mapEventGroup) {
            EventBits_t bits = xEventGroupGetBits(MapEngine::mapEventGroup);
            if (bits & MAP_EVENT_NAV_DONE) {
                applyRenderedViewport();
                // Clear bits only after successful apply (navRenderPending == false)
                if (!navRenderPending) {
                    xEventGroupClearBits(MapEngine::mapEventGroup, MAP_EVENT_NAV_DONE);
                }
            }
        }

        // Inertia handling
        // Apply momentum when finger is not on screen
        if (!isScrollingMap && (velocityX != 0.0f || velocityY != 0.0f)) {
            uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
            uint32_t dt = (last_time > 0) ? (now - last_time) : 0;
            last_time = now;
            if (dt > 0 && dt < 100) {  // Avoid jumps on long pauses
                int16_t dx = (int16_t)(velocityX * dt);
                int16_t dy = (int16_t)(velocityY * dt);
                if (dx != 0 || dy != 0)
                    scrollMap(dx, dy);

                float friction = redraw_in_progress ? PAN_FRICTION_BUSY : PAN_FRICTION;
                velocityX *= friction;
                velocityY *= friction;

                if (fabsf(velocityX) < 0.01f) velocityX = 0.0f;
                if (fabsf(velocityY) < 0.01f) velocityY = 0.0f;
            }
        }

        // Update canvas position every frame
        if (map_canvas) {
            int16_t canvasX = -MAP_MARGIN_X - offsetX;
            int16_t canvasY = -MAP_MARGIN_Y - offsetY;

            // NAV fixed grid: shift canvas by GPS sub-tile offset so GPS is centered
            if (navModeActive) {
                canvasX -= navSubTileX;
                canvasY -= navSubTileY;
            }

            lv_obj_set_pos(map_canvas, canvasX, canvasY);
        }

        // 1. Update smoothed own position and trace every second (20 x 50ms = 1s)
        static uint16_t gpsUpdateCounter = 0;
        bool positionChanged = false;
        
        if (++gpsUpdateCounter >= 20) {
            gpsUpdateCounter = 0;
            
            float oldLat = filteredOwnLat;
            float oldLon = filteredOwnLon;
            
            updateFilteredOwnPosition();
            addOwnTracePoint();
            
            // Trigger UI refresh if filtered position actually moved
            if (filteredOwnLat != oldLat || filteredOwnLon != oldLon) {
                positionChanged = true;
            }
        }

        // 2. Periodic station refresh (received stations every ~10s OR own station moved)
        static uint16_t refreshCounter = 0;
        refreshCounter++;
        
        if (refreshCounter >= 200 || positionChanged) {  // 200 × 50ms = 10s
            if (refreshCounter >= 200) refreshCounter = 0;
            
            if (!isScrollingMap) {
                // Follow GPS: update centerTile from stable filtered position (prevents jitter)
                float uiLat, uiLon;
                if (map_follow_gps && getUiPosition(&uiLat, &uiLon)) {
                    int prevRenderTileX = renderTileX;
                    int prevRenderTileY = renderTileY;
                    initCenterTileFromLatLon(uiLat, uiLon);

                    if (renderTileX != prevRenderTileX || renderTileY != prevRenderTileY) {
                        ESP_LOGD(TAG, "Refresh (GPS moved tile, full redraw)");
                        redraw_map_canvas();
                    } else if (!redraw_in_progress && !navRenderPending) {
                        // Apply precise pixel offset based on new map_center_lat/lon without re-rendering the whole tile
                        applyRenderedViewport();
                        ESP_LOGD(TAG, "Refresh (GPS moved inside tile, pan viewport)");
                    }
                } else if (!redraw_in_progress && !navRenderPending) {
                    ESP_LOGD(TAG, "Refresh (station overlay only)");
                    refreshStationOverlay();
                }
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
    #define TILE_PRELOAD_QUEUE_SIZE 20

    // Forward declaration
    bool preloadTileToCache(int tileX, int tileY, int zoom);

    // Task running on Core 1 to preload tiles in background
    static void tilePreloadTaskFunc(void* param) {
        ESP_LOGI(TAG, "Tile preload task started on Core 1");
        TileRequest req;

        while (preloadTaskRunning) {
            // Skip during main thread SD access or NAV mode (no raster cache needed)
            if (mainThreadLoading || navModeActive) {
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
            if (textY >= 0 && textY < MAP_SPRITE_SIZE) {
                
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
            if (textY >= 0 && textY < MAP_SPRITE_SIZE) {
                
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
            stationHitZones[stationHitZoneCount].x = canvasX - MAP_MARGIN_X;
            stationHitZones[stationHitZoneCount].y = canvasY - MAP_MARGIN_Y + 12;
            stationHitZones[stationHitZoneCount].w = 80;
            stationHitZones[stationHitZoneCount].h = 50;
            stationHitZones[stationHitZoneCount].stationIdx = stationIdx;
            stationHitZoneCount++;
        }
    }

    // (filteredOwn* declared above, before map_refresh_timer_cb)

    static bool getUiPosition(float* lat, float* lon) {
        if (filteredOwnValid) {
            *lat = filteredOwnLat;
            *lon = filteredOwnLon;
            return true;
        } else if (iconGpsValid) {
            *lat = iconGpsLat;
            *lon = iconGpsLon;
            return true;
        }
        return false;
    }

    static void updateFilteredOwnPosition() {
        static float iconCentroidLat = 0.0f;
        static float iconCentroidLon = 0.0f;
        static uint32_t iconCentroidCount = 0;

        if (!gps.location.isValid()) return;
        float lat = gps.location.lat();
        float lon = gps.location.lng();
        int sats = gps.satellites.value();

        // Level 1: icon display (≥3 sats = 2D fix minimum)
        if (sats >= 3) {
            iconGpsLat = lat;
            iconGpsLon = lon;
            iconGpsValid = true;
        }

        // Level 2: filtered position for trace + recentrage (≥6 sats)
        if (sats < 6) return;

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

    // Add a point to the own-trace circular buffer, using the UI-smoothed position
    void addOwnTracePoint() {
        if (!filteredOwnValid) return; // No valid smoothed position yet

        float lat = filteredOwnLat;
        float lon = filteredOwnLon;

        // Ensure we only add a new point if we moved enough from the last trace point.
        // This prevents the buffer from filling up with identical points during standing updates.
        if (ownTraceCount > 0) {
            int lastIdx = (ownTraceHead - 1 + TRACE_MAX_POINTS) % TRACE_MAX_POINTS;
            float lastLat = ownTrace[lastIdx].lat;
            float lastLon = ownTrace[lastIdx].lon;
            
            // Threshold: 0.0001 degrees is roughly 11 meters
            if (fabs(lat - lastLat) < 0.0001f && fabs(lon - lastLon) < 0.0001f) {
                return; // Hasn't moved enough from the last recorded trace point
            }
        }

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

    // Draw all stations directly on the canvas (zero LVGL objects, all PSRAM)
    void update_station_objects() {
        if (!map_canvas || !map_canvas_buf) return;

        stationHitZoneCount = 0;

        // Own position — now updated every second by map_refresh_timer_cb
        if (iconGpsValid) {
            int myX, myY;
            latLonToPixel(iconGpsLat, iconGpsLon,
                          map_center_lat, map_center_lon, map_current_zoom, &myX, &myY);
            if (myX >= 0 && myX < MAP_SPRITE_SIZE && myY >= 0 && myY < MAP_SPRITE_SIZE) {
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
                if (stX >= 0 && stX < MAP_SPRITE_SIZE && stY >= 0 && stY < MAP_SPRITE_SIZE) {
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

        // 3. Ensure enough PSRAM before allocating sprite
        const size_t spriteSize = MAP_TILE_SIZE * MAP_TILE_SIZE * 2; // RGB565
        if (!MapEngine::ensurePSRAMAvailable(spriteSize)) {
            ESP_LOGW(TAG, "Not enough PSRAM for preload after eviction");
            return false;
        }

        // 4. Allocate a new sprite for the tile
        LGFX_Sprite* newSprite = new LGFX_Sprite(&tft);
        newSprite->setPsram(true);
        if (newSprite->createSprite(MAP_TILE_SIZE, MAP_TILE_SIZE) == nullptr) {
            ESP_LOGE(TAG, "Sprite creation failed");
            delete newSprite;
            return false;
        }

        // 5. Render tile — serialize with spriteMutex (static decoders not thread-safe)
        bool success = false;
        if (MapEngine::spriteMutex && xSemaphoreTake(MapEngine::spriteMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
            success = MapEngine::renderTile(found_path, 0, 0, *newSprite, (uint8_t)zoom);
            xSemaphoreGive(MapEngine::spriteMutex);
        }

        // 6. If rendering is successful, add to cache
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

    // Convert tile index to lat/lon of tile center (tileX+0.5, tileY+0.5)
    static void tileToLatLon(int tileX, int tileY, int zoom, float* lat, float* lon) {
        double n = (double)(1 << zoom);
        *lon = (float)((tileX + 0.5) / n * 360.0 - 180.0);
        double n_rad = PI * (1.0 - 2.0 * (tileY + 0.5) / n);
        *lat = (float)(atan(sinh(n_rad)) * 180.0 / PI);
    }

    // Initialize centerTileX/Y from lat/lon (recenter, GPS init, zoom change).
    // Sets centerTileX/Y for pan tracking. Keeps map_center_lat/lon = actual position
    // (NOT tile center) so the render engine centers on the real GPS position.
    static void initCenterTileFromLatLon(float lat, float lon) {
        latLonToTile(lat, lon, map_current_zoom, &centerTileX, &centerTileY);
        renderTileX = centerTileX;
        renderTileY = centerTileY;
        map_center_lat = lat;
        map_center_lon = lon;
    }

    // Convert lat/lon to pixel position in sprite.
    // NAV (fixed grid): tile-relative — grid origin = (centerTileX-1, centerTileY-1)
    // Raster (variable grid): delta from center position, offset by sprite center
    void latLonToPixel(float lat, float lon, float centerLat, float centerLon, int zoom, int* pixelX, int* pixelY) {
        double n = pow(2.0, zoom);
        double target_x_world = (lon + 180.0) / 360.0;
        double target_lat_rad = lat * PI / 180.0;
        double target_y_world = (1.0 - log(tan(target_lat_rad) + 1.0 / cos(target_lat_rad)) / PI) / 2.0;

        if (navModeActive) {
            // Fixed grid: world pixel relative to grid origin
            const int8_t gridOffset = MAP_TILES_GRID / 2;
            double grid_origin_wx = (double)(centerTileX - gridOffset) * MAP_TILE_SIZE;
            double grid_origin_wy = (double)(centerTileY - gridOffset) * MAP_TILE_SIZE;
            *pixelX = (int)(target_x_world * n * MAP_TILE_SIZE - grid_origin_wx);
            *pixelY = (int)(target_y_world * n * MAP_TILE_SIZE - grid_origin_wy);
        } else {
            // Variable grid: delta from map center, offset by sprite center
            double center_x_world = (centerLon + 180.0) / 360.0;
            double center_lat_rad = centerLat * PI / 180.0;
            double center_y_world = (1.0 - log(tan(center_lat_rad) + 1.0 / cos(center_lat_rad)) / PI) / 2.0;
            double delta_x_px = (target_x_world - center_x_world) * n * MAP_TILE_SIZE;
            double delta_y_px = (target_y_world - center_y_world) * n * MAP_TILE_SIZE;
            *pixelX = (int)(MAP_SPRITE_SIZE / 2.0 + delta_x_px);
            *pixelY = (int)(MAP_SPRITE_SIZE / 2.0 + delta_y_px);
        }
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
        // See CLAUDE.md: "Persistent sprites - allocate once, never free/recreate"
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
        float initLat, initLon;
        if (gps.location.isValid()) {
            initLat = gps.location.lat();
            initLon = gps.location.lng();
            ESP_LOGI(TAG, "Recentered on GPS: %.4f, %.4f", initLat, initLon);
        } else {
            initLat = 42.9667f;
            initLon = 1.6053f;
            ESP_LOGW(TAG, "No GPS, recentered on default position: %.4f, %.4f", initLat, initLon);
        }
        initCenterTileFromLatLon(initLat, initLon);
        resetPanOffset();
        if (btn_recenter) lv_obj_add_state(btn_recenter, LV_STATE_PRESSED);
        redraw_map_canvas();
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

            // Immediate own position as last point (consistent with actual icon)
            int cx, cy;
            latLonToPixel(iconGpsLat, iconGpsLon,
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

    void redraw_map_canvas() {
        if (!map_canvas || !map_canvas_buf || !map_title_label) {
            screen_map = nullptr; // Force recreation
            create_map_screen();
            lv_disp_load_scr(screen_map);
            return;
        }

        // Always allow re-enqueue — scrollMap() calls generateMap()
        // freely, xQueueOverwrite keeps latest request. No blocking.
        redraw_in_progress = true;

        // Pause async preloading while we load tiles (avoid SD contention)
        mainThreadLoading = true;

        // Title update deferred to applyRenderedViewport — zoom stays visible
        // until tiles are actually rendered and displayed.

        // Clean up old station buttons before redrawing
        cleanup_station_buttons();

        // Recalculate tile positions
        ESP_LOGD(TAG, "Render tile: %d/%d, sprite tile: %d/%d, offset: %d,%d",
                      renderTileX, renderTileY, centerTileX, centerTileY, offsetX, offsetY);

        if (STORAGE_Utils::isSDAvailable()) {
            // Check if NAV data available: try each region for pack file or legacy tile
            // Z6-Z8: force raster — NAV feature density too high for ESP32
            char navCheckPath[128];
            bool isNavMode = false;
            if (navRegionCount > 0 && map_current_zoom >= 9) {
                // Skip SD.exists() probe when already in NAV mode — avoids SPI bus
                // contention with renderNavViewport (Phase 2: runs on another core).
                // NAV→raster transition only happens on zoom-out below Z9 (handled above).
                if (navModeActive) {
                    isNavMode = true;
                } else if (spiMutex != NULL && xSemaphoreTake(spiMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
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
                                     navRegions[r].c_str(), map_current_zoom, renderTileX, renderTileY);
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

                // Enqueue async NAV render on Core 0 (non-blocking)
                if (backViewportSprite) {
                    MapEngine::NavRenderRequest req = {};
                    req.centerTileX = renderTileX;
                    req.centerTileY = renderTileY;
                    req.centerLat = map_center_lat;
                    req.centerLon = map_center_lon;
                    req.zoom = (uint8_t)map_current_zoom;
                    req.targetSprite = backViewportSprite;
                    req.isRaster = false;
                    req.regionCount = navRegionCount;
                    for (int r = 0; r < navRegionCount && r < 8; r++) {
                        strncpy(req.regions[r], navRegions[r].c_str(), 63);
                        req.regions[r][63] = '\0';
                    }

                    MapEngine::enqueueNavRender(req);
                    navRenderPending = true;

                    ESP_LOGI(TAG, "NAV render enqueued - PSRAM free: %u KB, largest block: %u KB",
                                  ESP.getFreePsram() / 1024,
                                  heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024);
                }

                // Return early — map_refresh_timer_cb handles memcpy + stations when done
                // Keep redraw_in_progress = true until applyRenderedViewport clears it
                return;
            } else {
                if (navModeActive) {
                    navModeActive = false;
                    switchZoomTable(raster_zooms, raster_zoom_count);
                }

                // Enqueue async raster compositing on Core 0 (same pattern as NAV)
                if (backViewportSprite) {
                    MapEngine::NavRenderRequest req = {};
                    req.centerTileX = renderTileX;
                    req.centerTileY = renderTileY;
                    req.centerLat = map_center_lat;
                    req.centerLon = map_center_lon;
                    req.zoom = (uint8_t)map_current_zoom;
                    req.targetSprite = backViewportSprite;
                    req.isRaster = true;
                    req.regionCount = 1;
                    strncpy(req.regions[0], map_current_region.c_str(), 63);
                    req.regions[0][63] = '\0';

                    MapEngine::enqueueNavRender(req);
                    navRenderPending = true;

                    ESP_LOGI(TAG, "Raster render enqueued Z%d - PSRAM free: %u KB",
                                  map_current_zoom, ESP.getFreePsram() / 1024);
                }

                // Return early — map_refresh_timer_cb handles memcpy + stations when done
                return;
            }
        }
    }


    // Helper: reset pan offset, velocity, and canvas position before re-render
    // Does NOT touch centerTileX/Y — caller must update those first if zoom changed.
    static inline void resetPanOffset() {
        // Schedule a pan reset that will only be visually applied 
        // once the newly centered tile has finished rendering.
        // Doing this synchronously jumps the screen on the OLD background tile.
        pendingResetPan = true;
    }

    // Helper: resync centerTile to current map_center at new zoom, then reset offset.
    // Call after changing map_current_zoom.
    static inline void resetZoom() {
        initCenterTileFromLatLon(map_center_lat, map_center_lon);
        resetPanOffset();
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
            if (btn_zoomin) lv_obj_add_state(btn_zoomin, LV_STATE_PRESSED);
            resetZoom();
            redraw_map_canvas();
        } else if (map_zoom_index < map_zoom_count - 1) {
            map_zoom_index++;
            map_current_zoom = map_available_zooms[map_zoom_index];
            ESP_LOGI(TAG, "Zoom in: %d", map_current_zoom);
            if (btn_zoomin) lv_obj_add_state(btn_zoomin, LV_STATE_PRESSED);
            if (navModeActive) MapEngine::clearTileCache();
            resetZoom();
            redraw_map_canvas();
        }
    }

    // Map zoom out handler
    void btn_map_zoomout_clicked(lv_event_t* e) {
        if (map_zoom_index > 0) {
            map_zoom_index--;
            map_current_zoom = map_available_zooms[map_zoom_index];
            ESP_LOGI(TAG, "Zoom out: %d", map_current_zoom);
            if (btn_zoomout) lv_obj_add_state(btn_zoomout, LV_STATE_PRESSED);
            if (navModeActive) MapEngine::clearTileCache();
            resetZoom();
            redraw_map_canvas();
        } else if (navModeActive) {
            // At min NAV zoom — switch to raster
            navModeActive = false;
            MapEngine::clearTileCache();
            switchZoomTable(raster_zooms, raster_zoom_count);
            ESP_LOGI(TAG, "Zoom out: %d (NAV->raster)", map_current_zoom);
            if (btn_zoomout) lv_obj_add_state(btn_zoomout, LV_STATE_PRESSED);
            resetZoom();
            redraw_map_canvas();
        }
    }

    // Toggle fullscreen: hide/show title bar + info bar, resize map container
    static void toggleMapFullscreen() {
        mapFullscreen = !mapFullscreen;
        if (mapFullscreen) {
            if (map_title_bar) lv_obj_add_flag(map_title_bar, LV_OBJ_FLAG_HIDDEN);
            if (map_info_bar)  lv_obj_add_flag(map_info_bar, LV_OBJ_FLAG_HIDDEN);
            if (map_container) {
                lv_obj_set_pos(map_container, 0, 0);
                lv_obj_set_size(map_container, SCREEN_WIDTH, SCREEN_HEIGHT);
            }
        } else {
            if (map_title_bar) lv_obj_clear_flag(map_title_bar, LV_OBJ_FLAG_HIDDEN);
            if (map_info_bar)  lv_obj_clear_flag(map_info_bar, LV_OBJ_FLAG_HIDDEN);
            if (map_container) {
                lv_obj_set_pos(map_container, 0, 35);
                lv_obj_set_size(map_container, SCREEN_WIDTH, MAP_VISIBLE_HEIGHT);
            }
        }
    }

    // Async adaptation of scrollMap()
    // offsetX/Y grows freely (clamped by margin). No wrap — avoids 256px visual snap
    // while async render hasn't delivered the new sprite yet.
    // renderTileX/Y tracks the target tile for the render request.
    // centerTileX/Y only changes in applyRenderedViewport() when the new sprite arrives.
    static void scrollMap(int16_t dx, int16_t dy) {
        if (dx == 0 && dy == 0) return;

        // Dampen excessive offsets in same direction
        const int16_t softLimit = PAN_TILE_THRESHOLD;
        if (abs(offsetX) > softLimit && ((dx > 0 && offsetX > 0) || (dx < 0 && offsetX < 0)))
            dx /= 2;
        if (abs(offsetY) > softLimit && ((dy > 0 && offsetY > 0) || (dy < 0 && offsetY < 0)))
            dy /= 2;

        offsetX += dx;
        offsetY += dy;
        map_follow_gps = false;

        // Clamp to canvas margin — hard limit of pre-rendered area
        int16_t maxOffX = MAP_MARGIN_X - 10;  // 214
        int16_t maxOffY = MAP_MARGIN_Y - 10;  // 274
        offsetX = (int16_t)constrain(offsetX, -maxOffX, maxOffX);
        offsetY = (int16_t)constrain(offsetY, -maxOffY, maxOffY);

        // Compute target tile from accumulated offset (without modifying offset)
        int targetX = centerTileX;
        int targetY = centerTileY;
        int16_t tempX = offsetX, tempY = offsetY;
        if (tempX >= PAN_TILE_THRESHOLD) { targetX++; tempX -= MAP_TILE_SIZE; }
        else if (tempX <= -PAN_TILE_THRESHOLD) { targetX--; tempX += MAP_TILE_SIZE; }
        if (tempY >= PAN_TILE_THRESHOLD) { targetY++; tempY -= MAP_TILE_SIZE; }
        else if (tempY <= -PAN_TILE_THRESHOLD) { targetY--; tempY += MAP_TILE_SIZE; }

        if (targetX != renderTileX || targetY != renderTileY) {
            renderTileX = targetX;
            renderTileY = targetY;
            tileToLatLon(renderTileX, renderTileY, map_current_zoom, &map_center_lat, &map_center_lon);
            ESP_LOGD(TAG, "scrollMap → render tile(%d,%d) offset(%d,%d) lat/lon %.4f,%.4f",
                          renderTileX, renderTileY, offsetX, offsetY, map_center_lat, map_center_lon);
            redraw_map_canvas();
        }
    }

    // Touch pan handler
    void map_touch_event_cb(lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        lv_indev_t* indev = lv_indev_get_act();
        if (!indev) return;

        lv_point_t p;
        lv_indev_get_point(indev, &p);

        switch (code) {
        case LV_EVENT_PRESSED:
            last_x = p.x;
            last_y = p.y;
            last_time = (uint32_t)(esp_timer_get_time() / 1000);
            dragStarted = false;
            isScrollingMap = true;
            // Stop any ongoing inertia when touching the screen
            velocityX = 0.0f;
            velocityY = 0.0f;
            break;

        case LV_EVENT_PRESSING: {
            uint32_t current_time = (uint32_t)(esp_timer_get_time() / 1000);
            int dx = p.x - last_x;
            int dy = p.y - last_y;
            uint32_t dt = current_time - last_time;

            if (!dragStarted) {
                if (abs(dx) > START_THRESHOLD || abs(dy) > START_THRESHOLD) {
                    dragStarted = true;
                    // User took manual control via touch drag. Cancel any pending
                    // automatic recenter/zoom offset reset to prevent jumping.
                    pendingResetPan = false;
                }
            }

            if (dragStarted && dt > 0) {
                // Move map
                scrollMap(-dx, -dy);

                // Immediate visual feedback
                if (map_canvas) {
                    int16_t canvasX = -MAP_MARGIN_X - offsetX;
                    int16_t canvasY = -MAP_MARGIN_Y - offsetY;
                    if (navModeActive) {
                        canvasX -= navSubTileX;
                        canvasY -= navSubTileY;
                    }
                    lv_obj_set_pos(map_canvas, canvasX, canvasY);
                }

                // Sample velocity px/ms with exponential filter (weight 0.7)
                float weight = 0.7f;
                velocityX = velocityX * (1.0f - weight) + (-(float)dx / (float)dt) * weight;
                velocityY = velocityY * (1.0f - weight) + (-(float)dy / (float)dt) * weight;

                last_x = p.x;
                last_y = p.y;
                last_time = current_time;
            }
            break;
        }

        case LV_EVENT_RELEASED:
        case LV_EVENT_PRESS_LOST: {
            bool wasDragging = dragStarted;
            isScrollingMap = false;
            dragStarted = false;

            // Kill very low velocity
            if (fabsf(velocityX) < 0.1f) velocityX = 0.0f;
            if (fabsf(velocityY) < 0.1f) velocityY = 0.0f;

            // Double-tap detection (200ms window)
            if (!wasDragging) {
                static uint32_t firstTapTime = 0;
                static uint8_t tapCount = 0;
                uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);

                if (now - firstTapTime > 300) {
                    // First tap or timeout — start new sequence
                    tapCount = 1;
                    firstTapTime = now;
                } else {
                    tapCount++;
                    if (tapCount >= 2) {
                        toggleMapFullscreen();
                        tapCount = 0;
                        firstTapTime = 0;
                        break;  // Don't process as station tap
                    }
                }
            }

            if (!wasDragging) {
                // Single tap — check if a station was tapped
                for (int i = 0; i < stationHitZoneCount; i++) {
                    int16_t hx = stationHitZones[i].x;
                    int16_t hy = stationHitZones[i].y;
                    int16_t hw = stationHitZones[i].w;
                    int16_t hh = stationHitZones[i].h;

                    if (p.x >= hx - hw/2 && p.x <= hx + hw/2 &&
                        p.y >= hy - hh/2 && p.y <= hy + hh/2) {
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
            break;
        }

        default: break;
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

    // --- 5. Ensure PSRAM available, then decode + copy to canvas + cache ---
    const size_t spriteSize = MAP_TILE_SIZE * MAP_TILE_SIZE * 2; // RGB565
    MapEngine::ensurePSRAMAvailable(spriteSize);

    LGFX_Sprite* newSprite = new LGFX_Sprite(&tft);
    newSprite->setPsram(true);
    if (newSprite->createSprite(MAP_TILE_SIZE, MAP_TILE_SIZE) != nullptr) {
        // Serialize with spriteMutex (static decoders not thread-safe across cores)
        bool decoded = false;
        if (MapEngine::spriteMutex && xSemaphoreTake(MapEngine::spriteMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
            decoded = MapEngine::renderTile(found_path, 0, 0, *newSprite, (uint8_t)zoom);
            xSemaphoreGive(MapEngine::spriteMutex);
        }
        if (decoded) {
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
        mapFullscreen = false;
        map_title_bar = nullptr;
        map_info_bar = nullptr;

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
            initCenterTileFromLatLon(gps.location.lat(), gps.location.lng());
            ESP_LOGI(TAG, "Using GPS position: %.4f, %.4f", map_center_lat, map_center_lon);
        } else if (map_center_lat == 0.0f && map_center_lon == 0.0f) {
            initCenterTileFromLatLon(42.9667f, 1.6053f);
            ESP_LOGW(TAG, "No GPS, using default position: %.4f, %.4f", map_center_lat, map_center_lon);
        } else if (centerTileX == 0 && centerTileY == 0) {
            // Screen recreated but lat/lon preserved — re-sync tile from lat/lon
            initCenterTileFromLatLon(map_center_lat, map_center_lon);
            ESP_LOGI(TAG, "Using pan position: %.4f, %.4f (tile %d/%d)", map_center_lat, map_center_lon, centerTileX, centerTileY);
        }

        // Title bar (green for map)
        map_title_bar = lv_obj_create(screen_map);
        lv_obj_t* title_bar = map_title_bar;
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
        btn_recenter = lv_btn_create(title_bar);
        lv_obj_set_size(btn_recenter, 30, 25);
        lv_obj_set_style_bg_color(btn_recenter, map_follow_gps ? lv_color_hex(0x16213e) : lv_color_hex(0xff6600), 0);
        lv_obj_align(btn_recenter, LV_ALIGN_RIGHT_MID, -105, 0);
        lv_obj_add_event_cb(btn_recenter, btn_map_recenter_clicked, LV_EVENT_CLICKED, NULL);
        lv_obj_t* lbl_recenter = lv_label_create(btn_recenter);
        lv_label_set_text(lbl_recenter, LV_SYMBOL_GPS);
        lv_obj_center(lbl_recenter);

        // Zoom buttons — LV_STATE_PRESSED held until tiles are rendered
        btn_zoomin = lv_btn_create(title_bar);
        lv_obj_set_size(btn_zoomin, 30, 25);
        lv_obj_set_style_bg_color(btn_zoomin, lv_color_hex(0x16213e), 0);
        lv_obj_align(btn_zoomin, LV_ALIGN_RIGHT_MID, -70, 0);
        lv_obj_add_event_cb(btn_zoomin, btn_map_zoomin_clicked, LV_EVENT_RELEASED, NULL);
        lv_obj_t* lbl_zoomin = lv_label_create(btn_zoomin);
        lv_label_set_text(lbl_zoomin, "+");
        lv_obj_center(lbl_zoomin);

        btn_zoomout = lv_btn_create(title_bar);
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

        // Allocate double-buffer sprites EARLY (before raster cache fills PSRAM)
        // Canvas buffer = front sprite buffer (zero-copy)
        const size_t spriteBytes = MAP_SPRITE_SIZE * MAP_SPRITE_SIZE * 2;
        if (!backViewportSprite) {
            backViewportSprite = new LGFX_Sprite(&tft);
            backViewportSprite->setPsram(true);
            if (backViewportSprite->createSprite(MAP_SPRITE_SIZE, MAP_SPRITE_SIZE) == nullptr) {
                ESP_LOGE(TAG, "Failed to create back viewport sprite");
                delete backViewportSprite;
                backViewportSprite = nullptr;
            }
        }
        if (!frontViewportSprite) {
            frontViewportSprite = new LGFX_Sprite(&tft);
            frontViewportSprite->setPsram(true);
            if (frontViewportSprite->createSprite(MAP_SPRITE_SIZE, MAP_SPRITE_SIZE) == nullptr) {
                ESP_LOGE(TAG, "Failed to create front viewport sprite");
                delete frontViewportSprite;
                frontViewportSprite = nullptr;
            }
        }
        // Point canvas buffer directly at front sprite (no separate allocation)
        if (frontViewportSprite) {
            map_canvas_buf = (lv_color_t*)frontViewportSprite->getBuffer();
        }
        if (backViewportSprite && frontViewportSprite) {
            ESP_LOGI(TAG, "Double-buffer sprites: %dx%d (%u KB each, %u KB total PSRAM)",
                          MAP_SPRITE_SIZE, MAP_SPRITE_SIZE,
                          spriteBytes / 1024, spriteBytes * 2 / 1024);
        }
        ESP_LOGI(TAG, "PSRAM free: %u KB, largest block: %u KB",
                      ESP.getFreePsram() / 1024,
                      heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024);
        if (map_canvas_buf) {
            map_canvas = lv_canvas_create(map_container);
            lv_obj_clear_flag(map_canvas, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(map_canvas, LV_OBJ_FLAG_EVENT_BUBBLE);
            lv_canvas_set_buffer(map_canvas, map_canvas_buf, MAP_SPRITE_SIZE, MAP_SPRITE_SIZE, LV_IMG_CF_TRUE_COLOR);

            // Start the background render task now that the canvas exists.
            // This is critical to ensure the queue is ready before loadTileFromSD is called.
            MapEngine::startRenderTask(map_canvas);

            // Position canvas with negative margin so visible area is centered
            lv_obj_set_pos(map_canvas, -MAP_MARGIN_X, -MAP_MARGIN_Y);

            // Fill with background color
            lv_canvas_fill_bg(map_canvas, lv_color_hex(0x2F4F4F), LV_OPA_COVER);

            ESP_LOGD(TAG, "Center tile: %d/%d, offset: %d,%d", centerTileX, centerTileY, offsetX, offsetY);

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
                    if (navModeActive) {
                        isNavMode = true;
                    } else if (spiMutex != NULL && xSemaphoreTake(spiMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
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

                    // NAV viewport rendering
                    // Temporarily unsubscribe loopTask from WDT — rendering can take 10-30s
                    esp_task_wdt_delete(xTaskGetCurrentTaskHandle());

                    // Build region pointer array for renderNavViewport
                    const char* regionPtrs[NAV_MAX_REGIONS];
                    for (int r = 0; r < navRegionCount; r++) regionPtrs[r] = navRegions[r].c_str();

                    if (backViewportSprite && frontViewportSprite) {
                        ESP_LOGI(TAG, "Before renderNavViewport - PSRAM free: %u KB, largest block: %u KB",
                                      ESP.getFreePsram() / 1024,
                                      heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024);
                        hasTiles = MapEngine::renderNavViewport(
                            map_center_lat, map_center_lon, (uint8_t)map_current_zoom,
                            *backViewportSprite, regionPtrs, navRegionCount);
                        if (hasTiles) {
                            copyBackToFront();
                        }
                    } else {
                        ESP_LOGW(TAG, "No viewport sprites available for NAV rendering");
                    }

                    esp_task_wdt_add(xTaskGetCurrentTaskHandle());
                    esp_task_wdt_reset();
                } else {
                    navModeActive = false;
                    switchZoomTable(raster_zooms, raster_zoom_count);
                    // Raster viewport compositing into back sprite
                    if (backViewportSprite && frontViewportSprite) {
                        hasTiles = MapEngine::renderRasterViewport(
                            map_center_lat, map_center_lon, (uint8_t)map_current_zoom,
                            *backViewportSprite, map_current_region.c_str());
                        if (hasTiles) {
                            copyBackToFront();
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
                lv_canvas_draw_text(map_canvas, 40, MAP_SPRITE_SIZE / 2 - 30, 240, &label_dsc,
                    "No offline tiles available.\nDownload OSM tiles and copy to:\nSD:/LoRa_Tracker/Maps/REGION/z/x/y.png");
            }

            // Draw GPS traces for mobile stations (on canvas, under station icons)
            draw_station_traces();

            // Update station LVGL objects (own position + received stations)
            update_station_objects();

            // Force canvas redraw after direct buffer writes
            lv_canvas_set_buffer(map_canvas, map_canvas_buf, MAP_SPRITE_SIZE, MAP_SPRITE_SIZE, LV_IMG_CF_TRUE_COLOR);
            lv_obj_invalidate(map_canvas);

            // Resume async preloading
            mainThreadLoading = false;
        }

        // Info bar at bottom
        map_info_bar = lv_obj_create(screen_map);
        lv_obj_t* info_bar = map_info_bar;
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
