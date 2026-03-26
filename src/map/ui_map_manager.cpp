/* Map logic for T-Deck Plus — GLUE module
 * Offline map tile display with stations using LVGL
 *
 * After refactoring (Steps 1-5), this file only contains:
 *   - addOwnTracePoint()     — public wrapper
 *   - map_refresh_timer_cb() — 50ms poll timer
 *   - redraw_map_canvas()    — enqueue async render
 *   - create_map_screen()    — build LVGL screen + initial sync render
 *
 * Implementation lives in:
 *   map_state.h/cpp   — shared state
 *   map_tiles.cpp     — tile loading, symbol cache, region discovery
 *   map_render.cpp    — viewport copy, station overlay, traces
 *   map_input.cpp     — touch pan, zoom, buttons
 */

#ifdef USE_LVGL_UI

#include <Arduino.h>
#include <FS.h>
#include <lvgl.h>
#include <LovyanGFX.hpp>
#include <NMEAGPS.h>
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "map_engine.h"
#include "ui_map_manager.h"
#include "map_state.h"
#include "map_tiles.h"
#include "map_render.h"
#include "map_input.h"
#include "configuration.h"
#include "station_utils.h"
#include "utils.h"
#include "storage_utils.h"
#include "lvgl_ui.h"
#include "map_gps_filter.h"
#include <esp_task_wdt.h>
#include <esp_log.h>

static const char *TAG = "Map";

// Screen dimmed state — defined in lvgl_ui.cpp, set true when eco mode is active.
// Used to pause map rendering while screen is off.
extern bool screenDimmed;

namespace UIMapManager {

using namespace MapState;

// Forward declaration (timer_cb is defined before redraw_map_canvas)
void redraw_map_canvas();

#define MAP_REFRESH_INTERVAL 50  // 50ms (polls NAV_DONE + periodic station refresh)

// =============================================================================
// Public wrapper — add own trace point from external modules (e.g., station_utils)
// =============================================================================

void addOwnTracePoint() {
    gpsFilter.addOwnTracePoint();
}

// =============================================================================
// Timer callback: polls async render completion + periodic station refresh.
// Runs every 50ms.
// =============================================================================

static void map_refresh_timer_cb(lv_timer_t* timer) {
    if (!screen_map || lv_scr_act() != screen_map) return;

    // Check async render completion (finalize pending render — no SD access)
    if (navRenderPending && MapEngine::mapEventGroup) {
        EventBits_t bits = xEventGroupGetBits(MapEngine::mapEventGroup);
        if (bits & MAP_EVENT_NAV_DONE) {
            MapRender::applyRenderedViewport();
        }
    }

    // 1. Update smoothed own position and trace every 500ms (10 × 50ms)
    // Runs even in eco mode — GPS trace collection must continue with screen off.
    static uint16_t gpsUpdateCounter = 0;
    bool positionChanged = false;

    if (++gpsUpdateCounter >= 10) {
        gpsUpdateCounter = 0;

        double oldLat = gpsFilter.getOwnLat();
        double oldLon = gpsFilter.getOwnLon();

        gpsFilter.updateFilteredOwnPosition(gpsFix);
        gpsFilter.addOwnTracePoint();

        // Trigger UI refresh if filtered position moved by at least ~1m (avoid float noise spam)
        double dLat = gpsFilter.getOwnLat() - oldLat;
        double dLon = gpsFilter.getOwnLon() - oldLon;
        if (dLat * dLat + dLon * dLon > 1e-10) {  // ~1m threshold
            positionChanged = true;
        }
        // When following GPS and moving (1.5–150 km/h), force 500ms recenter cadence.
        // Below 1.5: stationary jitter filter applies. Above 150: GPS spike rejected.
        if (map_follow_gps && gpsFix.valid.speed
                && gpsFix.speed_kph() > 1.5 && gpsFix.speed_kph() < 150.0) {
            positionChanged = true;
        }
    }

    // Screen off (eco mode) — GPS collected above, skip all rendering and SD access.
    // setCpuFrequencyMhz(80) needs a quiet SPI bus to avoid corrupting the SD card.
    if (screenDimmed) return;

    // Inertia handling — apply momentum when finger is not on screen
    if (!isScrollingMap && (velocityX != 0.0f || velocityY != 0.0f)) {
        static uint32_t inertia_time = 0;
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        uint32_t dt = (inertia_time > 0) ? (now - inertia_time) : 0;
        inertia_time = now;
        if (dt > 0 && dt < 100) {  // Avoid jumps on long pauses
            int16_t dx = (int16_t)(velocityX * dt);
            int16_t dy = (int16_t)(velocityY * dt);
            if (dx != 0 || dy != 0)
                MapInput::scrollMap(dx, dy);

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

        canvasX -= navSubTileX;
        canvasY -= navSubTileY;

        lv_obj_set_pos(map_canvas, canvasX, canvasY);
    }

    // 2. Periodic station refresh (received stations every ~10s OR own station moved)
    static uint16_t refreshCounter = 0;
    refreshCounter++;

    if (refreshCounter >= 200 || positionChanged) {  // 200 × 50ms = 10s
        if (refreshCounter >= 200) refreshCounter = 0;

        if (!isScrollingMap) {
            // Follow GPS: update centerTile from stable filtered position (prevents jitter)
            double uiLat, uiLon;
            if (map_follow_gps && gpsFilter.getUiPosition(&uiLat, &uiLon)) {
                // When following GPS, always flag for a pan reset.
                // This ensures that any user-induced pan offsets (offsetX/Y) are cleared,
                // effectively recentering the map precisely on the filtered GPS position.
                // This flag will be acted upon in applyRenderedViewport().
                pendingResetPan = true;

                int prevRenderTileX = renderTileX;
                int prevRenderTileY = renderTileY;
                MapTiles::initCenterTileFromLatLon(uiLat, uiLon);

                if (renderTileX != prevRenderTileX || renderTileY != prevRenderTileY) {
                    ESP_LOGV(TAG, "Refresh (GPS moved tile, full redraw) - pendingResetPan set");
                    redraw_map_canvas();
                } else if (!redraw_in_progress && !navRenderPending &&
                           (offsetX != 0 || offsetY != 0 || positionChanged)) {
                    // Apply precise pixel offset based on new map_center_lat/lon without re-rendering the whole tile.
                    // pendingResetPan will ensure offsetX/Y are zeroed in applyRenderedViewport.
                    MapRender::applyRenderedViewport();
                    ESP_LOGV(TAG, "Refresh (GPS moved inside tile, pan viewport) - pendingResetPan set");
                }
            } else if (!redraw_in_progress && !navRenderPending) {
                ESP_LOGD(TAG, "Refresh (station overlay only)");
                MapRender::refreshStationOverlay();
            }
        }
    }
}

// =============================================================================
// redraw_map_canvas — enqueue async render (NAV or raster) on Core 0
// =============================================================================

void redraw_map_canvas() {
    if (!map_canvas || !map_canvas_buf || !map_title_label) {
        screen_map = nullptr; // Force recreation
        create_map_screen();
        lv_disp_load_scr(screen_map);
        return;
    }

    // Always allow re-enqueue — scrollMap() calls redraw_map_canvas()
    // freely, xQueueOverwrite keeps latest request. No blocking.
    redraw_in_progress = true;

    // Pause async preloading while we load tiles (avoid SD contention)
    mainThreadLoading = true;

    // Title update deferred to applyRenderedViewport — zoom stays visible
    // until tiles are actually rendered and displayed.

    // Clean up old station buttons before redrawing
    MapRender::cleanup_station_buttons();

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
                MapEngine::initNavPool();
                MapEngine::clearTileCache();
                ESP_LOGD(TAG, "After initNavPool & clearTileCache - PSRAM free: %u KB, largest block: %u KB",
                              ESP.getFreePsram() / 1024,
                              heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024);
                MapTiles::switchZoomTable(nav_zooms, nav_zoom_count);
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

                ESP_LOGD(TAG, "NAV render enqueued - PSRAM free: %u KB, largest block: %u KB",
                              ESP.getFreePsram() / 1024,
                              heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024);
            }

            // Return early — map_refresh_timer_cb handles memcpy + stations when done
            // Keep redraw_in_progress = true until applyRenderedViewport clears it
            return;
        } else {
            if (navModeActive) {
                navModeActive = false;
                MapEngine::clearTileCache();
                MapEngine::destroyNavPool();
                MapTiles::switchZoomTable(raster_zooms, raster_zoom_count);
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

                ESP_LOGD(TAG, "Raster render enqueued Z%d - PSRAM free: %u KB",
                              map_current_zoom, ESP.getFreePsram() / 1024);
            }

            // Return early — map_refresh_timer_cb handles memcpy + stations when done
            return;
        }
    }
}

// =============================================================================
// create_map_screen — build LVGL screen + synchronous initial render
// =============================================================================

void create_map_screen() {
    mapFullscreen = false;
    map_title_bar = nullptr;
    map_info_bar = nullptr;

    // Boost CPU to 240 MHz for smooth map rendering
    setCpuFrequencyMhz(240);
    ESP_LOGI(TAG, "CPU boosted to %d MHz", getCpuFrequencyMhz());

    // Set initial position from GPS if available (needed for GPS-based region matching)
    if (map_follow_gps && gpsFix.valid.location) {
        map_center_lat = gpsFix.latitude();
        map_center_lon = gpsFix.longitude();
    }

    // Discover and set the map region if it's not already defined
    MapTiles::discoverAndSetMapRegion();

    // Scan raster Z6 to determine default position from tile coverage
    MapTiles::discoverDefaultPosition();

    // Use tile-derived default if no GPS and no prior position
    if (map_center_lat == 0.0f && map_center_lon == 0.0f &&
        defaultLat != 0.0f && defaultLon != 0.0f) {
        map_center_lat = defaultLat;
        map_center_lon = defaultLon;
    }

    MapTiles::discoverNavRegions();

    ESP_LOGI(TAG, "Regions discovered - Maps: '%s', VectMaps: %d region(s)",
                  map_current_region.c_str(), navRegionCount);

    // Load Unicode font for map labels (VLW from SD)
    MapEngine::loadMapFont();

    // Clean up old station buttons if screen is being recreated
    MapRender::cleanup_station_buttons();

    screen_map = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_map, lv_color_hex(0x1a1a2e), 0);

    // Use current GPS position as center if follow mode is active
    if (map_follow_gps && gpsFix.valid.location) {
        MapTiles::initCenterTileFromLatLon(gpsFix.latitude(), gpsFix.longitude());
        ESP_LOGI(TAG, "Using GPS position: %.4f, %.4f", map_center_lat, map_center_lon);
    } else if (centerTileX == 0 && centerTileY == 0) {
        // Screen recreated but lat/lon preserved — re-sync tile from lat/lon
        MapTiles::initCenterTileFromLatLon(map_center_lat, map_center_lon);
        ESP_LOGI(TAG, "Using pan/default position: %.4f, %.4f (tile %d/%d)", map_center_lat, map_center_lon, centerTileX, centerTileY);
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
    lv_obj_add_event_cb(btn_back, MapInput::btn_map_back_clicked, LV_EVENT_CLICKED, NULL);
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
    lv_obj_set_style_bg_color(btn_recenter, lv_color_hex(0xff6600), LV_STATE_PRESSED);
    lv_obj_align(btn_recenter, LV_ALIGN_RIGHT_MID, -105, 0);
    lv_obj_add_event_cb(btn_recenter, MapInput::btn_map_recenter_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_recenter = lv_label_create(btn_recenter);
    lv_label_set_text(lbl_recenter, LV_SYMBOL_GPS);
    lv_obj_center(lbl_recenter);

    // Zoom buttons — LV_STATE_PRESSED held until tiles are rendered
    btn_zoomin = lv_btn_create(title_bar);
    lv_obj_set_size(btn_zoomin, 30, 25);
    lv_obj_set_style_bg_color(btn_zoomin, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_bg_color(btn_zoomin, lv_color_hex(0xff6600), LV_STATE_PRESSED);
    lv_obj_align(btn_zoomin, LV_ALIGN_RIGHT_MID, -70, 0);
    lv_obj_add_event_cb(btn_zoomin, MapInput::btn_map_zoomin_clicked, LV_EVENT_RELEASED, NULL);
    lv_obj_t* lbl_zoomin = lv_label_create(btn_zoomin);
    lv_label_set_text(lbl_zoomin, "+");
    lv_obj_center(lbl_zoomin);

    btn_zoomout = lv_btn_create(title_bar);
    lv_obj_set_size(btn_zoomout, 30, 25);
    lv_obj_set_style_bg_color(btn_zoomout, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_bg_color(btn_zoomout, lv_color_hex(0xff6600), LV_STATE_PRESSED);
    lv_obj_align(btn_zoomout, LV_ALIGN_RIGHT_MID, -35, 0);
    lv_obj_add_event_cb(btn_zoomout, MapInput::btn_map_zoomout_clicked, LV_EVENT_RELEASED, NULL);
    lv_obj_t* lbl_zoomout = lv_label_create(btn_zoomout);
    lv_label_set_text(lbl_zoomout, "-");
    lv_obj_center(lbl_zoomout);

    // GPX record toggle button
    btn_gpx_rec = lv_btn_create(title_bar);
    lv_obj_set_size(btn_gpx_rec, 30, 25);
    lv_obj_align(btn_gpx_rec, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(btn_gpx_rec, MapInput::btn_gpx_rec_clicked, LV_EVENT_CLICKED, NULL);
    lbl_gpx_rec = lv_label_create(btn_gpx_rec);
    lv_obj_center(lbl_gpx_rec);
    MapInput::updateGpxRecButton();

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
    lv_obj_add_event_cb(map_container, MapInput::map_touch_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(map_container, MapInput::map_touch_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(map_container, MapInput::map_touch_event_cb, LV_EVENT_RELEASED, NULL);

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

    // Initialize the static tile cache pool now that critical sprites are allocated
    // Only if we are not in persistent NAV mode (to avoid reallocating 2MB of raster tiles)
    if (!navModeActive) {
        MapEngine::initTileCache(&tft);
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
                MapEngine::initNavPool();
                MapEngine::clearTileCache();
                ESP_LOGD(TAG, "After initNavPool & clearTileCache - PSRAM free: %u KB, largest block: %u KB",
                              ESP.getFreePsram() / 1024,
                              heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024);
                MapTiles::switchZoomTable(nav_zooms, nav_zoom_count);

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
                        MapRender::copyBackToFront();
                    }
                } else {
                    ESP_LOGW(TAG, "No viewport sprites available for NAV rendering");
                }

                esp_task_wdt_add(xTaskGetCurrentTaskHandle());
                esp_task_wdt_reset();
            } else {
                if (navModeActive) {
                    MapEngine::clearTileCache();
                    MapEngine::destroyNavPool();
                }
                navModeActive = false;
                MapTiles::switchZoomTable(raster_zooms, raster_zoom_count);
                // Raster viewport compositing into back sprite
                if (backViewportSprite && frontViewportSprite) {
                    hasTiles = MapEngine::renderRasterViewport(
                        map_center_lat, map_center_lon, (uint8_t)map_current_zoom,
                        *backViewportSprite, map_current_region.c_str());
                    if (hasTiles) {
                        MapRender::copyBackToFront();
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
        MapRender::draw_station_traces();

        // Update station LVGL objects (own position + received stations)
        MapRender::update_station_objects();

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
    lv_obj_set_style_text_font(map_info_label, &lv_font_montserrat_12, 0);
    lv_obj_center(map_info_label);

    // Create periodic refresh timer for stations (10 seconds)
    if (map_refresh_timer) {
        lv_timer_del(map_refresh_timer);
    }
    map_refresh_timer = lv_timer_create(map_refresh_timer_cb, MAP_REFRESH_INTERVAL, NULL);

    // Start tile preload task on Core 1 for directional preloading during touch pan
    MapTiles::startTilePreloadTask();

    ESP_LOGD(TAG, "Map screen created");
}

} // namespace UIMapManager

#endif // USE_LVGL_UI
