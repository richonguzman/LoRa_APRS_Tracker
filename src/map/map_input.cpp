/* Map input handlers — touch pan, zoom, recenter, back, GPX recording
 * Extracted from ui_map_manager.cpp — Étape 4 of refactoring
 */

#ifdef USE_LVGL_UI

#include <Arduino.h>
#include <lvgl.h>
#include <freertos/FreeRTOS.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <cmath>

#include "map_state.h"
#include "map_input.h"
#include "map_render.h"
#include "map_tiles.h"
#include "map_engine.h"
#include "map_coordinate_math.h"
#include "station_utils.h"
#include "gpx_writer.h"
#include "lvgl_ui.h"
#include "ui_map_manager.h"  // For SCREEN_WIDTH, SCREEN_HEIGHT, MAP_VISIBLE_HEIGHT, MAP_MARGIN_X/Y, MAP_TILE_SIZE, redraw_map_canvas
#include "ui_dashboard.h"

using namespace MapState;

static const char* TAG = "MapInput";

// File-scope statics — private to map_input.cpp
static int   last_x    = 0;
static int   last_y    = 0;
static uint32_t last_time = 0;

// =============================================================================
// Internal helpers
// =============================================================================

static inline void resetPanOffset() {
    pendingResetPan = true;
}

static inline void commitVisualCenter() {
    int visualCenterPx = MAP_SPRITE_SIZE / 2 + offsetX + navSubTileX;
    int visualCenterPy = MAP_SPRITE_SIZE / 2 + offsetY + navSubTileY;
    MapMath::pixelToLatLon(visualCenterPx, visualCenterPy,
                           map_current_zoom, navModeActive,
                           centerTileX, centerTileY,
                           map_center_lat, map_center_lon,
                           &map_center_lat, &map_center_lon);
}

static inline void resetZoom() {
    MapTiles::initCenterTileFromLatLon(map_center_lat, map_center_lon);
    resetPanOffset();
}

static void shiftMapCenter(int deltaTileX, int deltaTileY) {
    if (deltaTileX == 0 && deltaTileY == 0) return;
    float newLat, newLon;
    MapMath::shiftMapCenter(map_center_lat, map_center_lon, map_current_zoom,
                            deltaTileX, deltaTileY, &newLat, &newLon);
    map_center_lat = newLat;
    map_center_lon = newLon;
}

static void toggleMapFullscreen() {
    mapFullscreen = !mapFullscreen;
    if (mapFullscreen) {
        if (map_title_bar) lv_obj_add_flag(map_title_bar, LV_OBJ_FLAG_HIDDEN);
        if (map_info_bar)  lv_obj_add_flag(map_info_bar,  LV_OBJ_FLAG_HIDDEN);
        if (map_container) {
            lv_obj_set_pos(map_container, 0, 0);
            lv_obj_set_size(map_container, SCREEN_WIDTH, SCREEN_HEIGHT);
        }
    } else {
        if (map_title_bar) lv_obj_clear_flag(map_title_bar, LV_OBJ_FLAG_HIDDEN);
        if (map_info_bar)  lv_obj_clear_flag(map_info_bar,  LV_OBJ_FLAG_HIDDEN);
        if (map_container) {
            lv_obj_set_pos(map_container, 0, 35);
            lv_obj_set_size(map_container, SCREEN_WIDTH, MAP_VISIBLE_HEIGHT);
        }
    }
}

// =============================================================================
// Public API — namespace MapInput
// =============================================================================

namespace MapInput {

    void updateGpxRecButton() {
        if (!btn_gpx_rec) return;
        bool rec = GPXWriter::isRecording();
        lv_obj_set_style_bg_color(btn_gpx_rec, rec ? lv_color_hex(0xCC0000) : lv_color_hex(0x16213e), 0);
        lv_label_set_text(lbl_gpx_rec, rec ? LV_SYMBOL_STOP : LV_SYMBOL_PLAY);
    }

    void btn_gpx_rec_clicked(lv_event_t* e) {
        if (GPXWriter::isRecording()) {
            GPXWriter::stopRecording();
        } else {
            GPXWriter::startRecording();
        }
        updateGpxRecButton();
    }

    void btn_map_back_clicked(lv_event_t* e) {
        ESP_LOGI(TAG, "MAP BACK button pressed");

        MapEngine::stopRenderTask();

        // Do not destroy NAV pool or clear cache on exit to prevent PSRAM fragmentation.
        // Keep NAV mode active if it was, so returning to map keeps the same zoom/context.
        // MapEngine::clearTileCache();
        // MapEngine::destroyNavPool();
        // MapTiles::switchZoomTable(raster_zooms, raster_zoom_count);

        MapRender::cleanup_station_buttons();
        // Commented out to preserve user panning/follow state between map sessions
        // map_follow_gps = true;
        if (map_refresh_timer) {
            lv_timer_del(map_refresh_timer);
            map_refresh_timer = nullptr;
        }
        MapTiles::stopTilePreloadTask();
        setCpuFrequencyMhz(80);
        ESP_LOGI(TAG, "CPU reduced to %d MHz", getCpuFrequencyMhz());
        ESP_LOGI(TAG, "After MAP exit - DRAM: %u  PSRAM: %u  Largest DRAM block: %u",
                      heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                      heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));

        // Retour explicite au dashboard avec del=true pour détruire la map et libérer la DRAM
        lv_scr_load_anim(UIDashboard::getMainScreen(), LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true);
        MapState::screen_map = nullptr;
        }

    void btn_map_recenter_clicked(lv_event_t* e) {
        ESP_LOGI(TAG, "Recentering on GPS");
        map_follow_gps = true;
        // Apply PRESSED state (orange) FIRST, then silently change the base color to blue underneath.
        // This prevents the brief blue flicker when clicking the orange button.
        if (btn_recenter) lv_obj_add_state(btn_recenter, LV_STATE_PRESSED);
        if (btn_recenter) lv_obj_set_style_bg_color(btn_recenter, lv_color_hex(0x16213e), 0);

        double initLat, initLon;
        if (gpsFilter.getUiPosition(&initLat, &initLon)) {
            ESP_LOGI(TAG, "Recentered on GPS: %.4f, %.4f", initLat, initLon);
        } else {
            initLat = (defaultLat != 0.0f) ? defaultLat : 42.9667f;
            initLon = (defaultLon != 0.0f) ? defaultLon : 1.6053f;
            ESP_LOGW(TAG, "No GPS, recentered on default position: %.4f, %.4f", initLat, initLon);
        }
        MapTiles::initCenterTileFromLatLon(initLat, initLon);
        resetPanOffset();
        if (btn_recenter) lv_obj_add_state(btn_recenter, LV_STATE_PRESSED);
        UIMapManager::redraw_map_canvas();
    }

    void btn_map_zoomin_clicked(lv_event_t* e) {
        if (redraw_in_progress || navRenderPending) {
            pendingZoom = 1;
            if (btn_zoomin) lv_obj_add_state(btn_zoomin, LV_STATE_PRESSED);
            return;
        }

        if (!navModeActive && navRegionCount > 0 &&
            map_current_zoom < nav_zooms[0] &&
            (map_zoom_index >= map_zoom_count - 1 ||
             map_available_zooms[map_zoom_index + 1] > nav_zooms[0])) {
            commitVisualCenter();
            MapTiles::switchZoomTable(nav_zooms, nav_zoom_count);
            MapEngine::initNavPool();
            map_zoom_index = 0;
            map_current_zoom = nav_zooms[0];
            ESP_LOGI(TAG, "Zoom in: %d (raster->NAV)", map_current_zoom);
            if (btn_zoomin) lv_obj_add_state(btn_zoomin, LV_STATE_PRESSED);
            resetZoom();
            UIMapManager::redraw_map_canvas();
        } else if (map_zoom_index < map_zoom_count - 1) {
            commitVisualCenter();
            map_zoom_index++;
            map_current_zoom = map_available_zooms[map_zoom_index];
            ESP_LOGI(TAG, "Zoom in: %d", map_current_zoom);
            if (btn_zoomin) lv_obj_add_state(btn_zoomin, LV_STATE_PRESSED);
            if (navModeActive) MapEngine::clearTileCache();
            resetZoom();
            UIMapManager::redraw_map_canvas();
        }
    }

    void btn_map_zoomout_clicked(lv_event_t* e) {
        if (redraw_in_progress || navRenderPending) {
            pendingZoom = -1;
            if (btn_zoomout) lv_obj_add_state(btn_zoomout, LV_STATE_PRESSED);
            return;
        }

        if (map_zoom_index > 0) {
            commitVisualCenter();
            map_zoom_index--;
            map_current_zoom = map_available_zooms[map_zoom_index];
            ESP_LOGI(TAG, "Zoom out: %d", map_current_zoom);
            if (btn_zoomout) lv_obj_add_state(btn_zoomout, LV_STATE_PRESSED);
            if (navModeActive) MapEngine::clearTileCache();
            resetZoom();
            UIMapManager::redraw_map_canvas();
        } else if (navModeActive) {
            commitVisualCenter();
            navModeActive = false;
            MapEngine::clearTileCache();
            MapEngine::destroyNavPool();
            MapTiles::switchZoomTable(raster_zooms, raster_zoom_count);
            ESP_LOGI(TAG, "Zoom out: %d (NAV->raster)", map_current_zoom);
            if (btn_zoomout) lv_obj_add_state(btn_zoomout, LV_STATE_PRESSED);
            resetZoom();
            UIMapManager::redraw_map_canvas();
        }
    }

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
                    if (map_follow_gps) {
                        map_follow_gps = false;
                        ESP_LOGD(TAG, "map_follow_gps DISABLED due to confirmed drag.");
                        if (btn_recenter) {
                            lv_obj_set_style_bg_color(btn_recenter, lv_color_hex(0xff6600), 0); // Passe à l'orange
                        }
                    }
                    pendingResetPan = false;
                }
            }

            if (dragStarted && dt > 0) {
                scrollMap(-dx, -dy);

                if (map_canvas) {
                    int16_t canvasX = -MAP_MARGIN_X - offsetX - navSubTileX;
                    int16_t canvasY = -MAP_MARGIN_Y - offsetY - navSubTileY;
                    lv_obj_set_pos(map_canvas, canvasX, canvasY);
                }

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

            if (fabsf(velocityX) < 0.1f) velocityX = 0.0f;
            if (fabsf(velocityY) < 0.1f) velocityY = 0.0f;

            if (!wasDragging) {
                static uint32_t firstTapTime = 0;
                static uint8_t tapCount = 0;
                uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);

                if (now - firstTapTime > 300) {
                    tapCount = 1;
                    firstTapTime = now;
                } else {
                    tapCount++;
                    if (tapCount >= 2) {
                        toggleMapFullscreen();
                        tapCount = 0;
                        firstTapTime = 0;
                        break;
                    }
                }
            }

            if (!wasDragging) {
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

    void scrollMap(int16_t dx, int16_t dy) {
        if (dx == 0 && dy == 0) return;

        const int16_t softLimit = PAN_TILE_THRESHOLD;
        if (abs(offsetX) > softLimit && ((dx > 0 && offsetX > 0) || (dx < 0 && offsetX < 0)))
            dx /= 2;
        if (abs(offsetY) > softLimit && ((dy > 0 && offsetY > 0) || (dy < 0 && offsetY < 0)))
            dy /= 2;

        offsetX += dx;
        offsetY += dy;

        if (!pendingResetPan) {
            map_follow_gps = false;
        }

        int16_t maxOffX = MAP_MARGIN_X - 10;
        int16_t maxOffY = MAP_MARGIN_Y - 10;
        offsetX = (int16_t)constrain(offsetX, -maxOffX, maxOffX);
        offsetY = (int16_t)constrain(offsetY, -maxOffY, maxOffY);

        int targetX = centerTileX;
        int targetY = centerTileY;
        int16_t tempX = offsetX, tempY = offsetY;
        if (tempX >= PAN_TILE_THRESHOLD)       { targetX++; tempX -= MAP_TILE_SIZE; }
        else if (tempX <= -PAN_TILE_THRESHOLD) { targetX--; tempX += MAP_TILE_SIZE; }
        if (tempY >= PAN_TILE_THRESHOLD)       { targetY++; tempY -= MAP_TILE_SIZE; }
        else if (tempY <= -PAN_TILE_THRESHOLD) { targetY--; tempY += MAP_TILE_SIZE; }

        if (targetX != renderTileX || targetY != renderTileY) {
            int dX = targetX - renderTileX;
            int dY = targetY - renderTileY;
            renderTileX = targetX;
            renderTileY = targetY;
            shiftMapCenter(dX, dY);
            ESP_LOGD(TAG, "scrollMap → render tile(%d,%d) offset(%d,%d) lat/lon %.4f,%.4f",
                          renderTileX, renderTileY, offsetX, offsetY, map_center_lat, map_center_lon);
            UIMapManager::redraw_map_canvas();
        }
    }

} // namespace MapInput

#endif // USE_LVGL_UI
