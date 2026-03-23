/* Map rendering — stations, traces, sprite double-buffer, viewport apply
 * Extracted from ui_map_manager.cpp — Étape 3 of refactoring
 */

#ifdef USE_LVGL_UI

#include <Arduino.h>
#include <lvgl.h>
#include <LovyanGFX.hpp>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/event_groups.h>
#include <climits>
#include <cmath>
#include <esp_log.h>

#include "map_state.h"
#include "map_render.h"
#include "map_input.h"
#include "map_tiles.h"
#include "map_engine.h"
#include "map_coordinate_math.h"
#include "station_utils.h"
#include "configuration.h"
#include "ui_map_manager.h"  // For MAP_TILE_SIZE, MAP_SPRITE_SIZE, MAP_MARGIN_X/Y, MAP_STATIONS_MAX, SYMBOL_SIZE, mapStationsCount

using namespace MapState;

static const char* TAG = "MapRender";

#define TRACE_TTL_MS (60 * 60 * 1000)  // 60 minutes TTL for station traces
#define SYMBOL_SIZE 24                  // APRS symbol size in pixels (24x24)

// External globals (defined in lvgl_ui.cpp / LoRa_APRS_Tracker.cpp)
extern Configuration Config;
extern uint8_t myBeaconsIndex;

// =============================================================================
// Internal helpers
// =============================================================================

// Helper: parse APRS symbol string and return cached symbol entry
static MapState::CachedSymbol* parseAndGetSymbol(const char* aprsSymbol) {
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
    return MapTiles::getSymbolCacheEntry(table, symbol);
}

// Draw a single station directly on the canvas (symbol + overlay letter + callsign)
static void drawStationOnCanvas(int canvasX, int canvasY,
                                const char* callsign, const char* aprsSymbol,
                                int8_t stationIdx) {
    if (!map_canvas) return;

    int symX = canvasX - SYMBOL_SIZE / 2;
    int symY = canvasY - SYMBOL_SIZE / 2;

    MapState::CachedSymbol* cache = parseAndGetSymbol(aprsSymbol);
    if (cache && cache->valid) {
        lv_draw_img_dsc_t img_dsc;
        lv_draw_img_dsc_init(&img_dsc);
        img_dsc.opa = LV_OPA_COVER;
        lv_canvas_draw_img(map_canvas, symX, symY, &cache->img_dsc, &img_dsc);
    } else {
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

    // Callsign label below symbol
    if (callsign) {
        int textY = canvasY + SYMBOL_SIZE / 2 + 2;
        if (textY >= 0 && textY < MAP_SPRITE_SIZE) {
            lv_point_t text_size;
            lv_txt_get_size(&text_size, callsign, &lv_font_montserrat_12, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);

            int padding_x = 8;
            int padding_y = 4;
            int textW = text_size.x + padding_x;
            int textH = text_size.y + padding_y;
            int textX = canvasX - textW / 2;
            if (textX < 0) textX = 0;

            lv_draw_rect_dsc_t bg_dsc;
            lv_draw_rect_dsc_init(&bg_dsc);
            bg_dsc.bg_color = lv_color_hex(0xDDDDDD);
            bg_dsc.bg_opa = LV_OPA_COVER;
            bg_dsc.radius = 2;
            lv_canvas_draw_rect(map_canvas, textX, textY, textW, textH, &bg_dsc);

            lv_draw_label_dsc_t label_dsc;
            lv_draw_label_dsc_init(&label_dsc);
            label_dsc.color = lv_color_hex(0x332221);
            label_dsc.font = &lv_font_montserrat_12;
            lv_canvas_draw_text(map_canvas, textX + (padding_x / 2), textY + (padding_y / 2), textW, &label_dsc, callsign);
        }
    }

    // Store hit zone for tap detection
    if (stationIdx >= 0 && stationHitZoneCount < MAP_STATIONS_HIT_MAX) {
        stationHitZones[stationHitZoneCount].x = canvasX - MAP_MARGIN_X;
        stationHitZones[stationHitZoneCount].y = canvasY - MAP_MARGIN_Y + 12;
        stationHitZones[stationHitZoneCount].w = 80;
        stationHitZones[stationHitZoneCount].h = 50;
        stationHitZones[stationHitZoneCount].stationIdx = stationIdx;
        stationHitZoneCount++;
    }
}

// =============================================================================
// Public API — namespace MapRender
// =============================================================================

namespace MapRender {

    void copyBackToFront() {
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

    void applyRenderedViewport() {
        if (!backViewportSprite || !frontViewportSprite) return;

        if (MapEngine::renderLock) {
            if (xSemaphoreTake(MapEngine::renderLock, pdMS_TO_TICKS(50)) != pdTRUE) {
                ESP_LOGW(TAG, "applyRenderedViewport: renderLock busy, retry next tick");
                return;
            }
        }

        copyBackToFront();

        if (MapEngine::renderLock) {
            xSemaphoreGive(MapEngine::renderLock);
        }

        if (MapEngine::mapEventGroup) {
            xEventGroupClearBits(MapEngine::mapEventGroup, MAP_EVENT_NAV_DONE);
        }

        navRenderPending = MapEngine::isRenderActive();
        redraw_in_progress = navRenderPending;
        mainThreadLoading = false;

        int16_t oldNavSubX = navSubTileX;
        int16_t oldNavSubY = navSubTileY;
        bool wasResetting = pendingResetPan;

        if (pendingResetPan) {
            offsetX = 0;
            offsetY = 0;
            velocityX = 0.0f;
            velocityY = 0.0f;
            pendingResetPan = false;
        } else {
            if (MapEngine::lastRenderedZoom == (uint8_t)map_current_zoom) {
                offsetX -= (MapEngine::lastRenderedTileX - centerTileX) * MAP_TILE_SIZE;
                offsetY -= (MapEngine::lastRenderedTileY - centerTileY) * MAP_TILE_SIZE;
            }
        }
        centerTileX = MapEngine::lastRenderedTileX;
        centerTileY = MapEngine::lastRenderedTileY;

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

        if (!wasResetting && navModeActive && MapEngine::lastRenderedZoom == (uint8_t)map_current_zoom) {
            offsetX -= (navSubTileX - oldNavSubX);
            offsetY -= (navSubTileY - oldNavSubY);
        }

        if (map_title_label) {
            char title_text[32];
            snprintf(title_text, sizeof(title_text), "MAP (Z%d)", map_current_zoom);
            lv_label_set_text(map_title_label, title_text);
        }
        if (btn_zoomin)   lv_obj_clear_state(btn_zoomin,   LV_STATE_PRESSED);
        if (btn_zoomout)  lv_obj_clear_state(btn_zoomout,  LV_STATE_PRESSED);
        if (btn_recenter) lv_obj_clear_state(btn_recenter, LV_STATE_PRESSED);

        // Consume queued zoom request (button pressed during render)
        if (pendingZoom != 0) {
            int8_t pz = pendingZoom;
            pendingZoom = 0;
            if (pz > 0)
                MapInput::btn_map_zoomin_clicked(nullptr);
            else
                MapInput::btn_map_zoomout_clicked(nullptr);
            return;  // skip rest — zoom callback triggers new render
        }

        cleanup_station_buttons();
        draw_station_traces();
        update_station_objects();

        if (map_info_label) {
            char info_text[64];
            snprintf(info_text, sizeof(info_text), "Lat:%.4f Lon:%.4f Stn:%d d:%.1fm a:%.2f",
                     map_center_lat, map_center_lon, mapStationsCount,
                     gpsFilter.getLastDeltaMeters(), gpsFilter.getLastAlpha());
            lv_label_set_text(map_info_label, info_text);
        }

        lv_obj_invalidate(map_canvas);
        if (navRenderPending) {
            ESP_LOGV(TAG, "Viewport applied (Z%d) sprTile(%d,%d) offset(%d,%d) — Core 0 still active, keeping pending",
                          map_current_zoom, centerTileX, centerTileY, offsetX, offsetY);
        } else {
            ESP_LOGV(TAG, "Viewport applied (Z%d) sprTile(%d,%d) offset(%d,%d)",
                          map_current_zoom, centerTileX, centerTileY, offsetX, offsetY);
        }
    }

    void refreshStationOverlay() {
        if (!map_canvas || !backViewportSprite || !frontViewportSprite) return;

        if (MapEngine::renderLock) {
            if (xSemaphoreTake(MapEngine::renderLock, pdMS_TO_TICKS(50)) != pdTRUE) {
                return;
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
            snprintf(info_text, sizeof(info_text), "Lat:%.4f Lon:%.4f Stn:%d d:%.1fm a:%.2f",
                     map_center_lat, map_center_lon, mapStationsCount,
                     gpsFilter.getLastDeltaMeters(), gpsFilter.getLastAlpha());
            lv_label_set_text(map_info_label, info_text);
        }

        lv_obj_invalidate(map_canvas);
    }

    void cleanup_station_buttons() {
        stationHitZoneCount = 0;
    }

    void update_station_objects() {
        if (!map_canvas || !map_canvas_buf) return;

        stationHitZoneCount = 0;

        // Draw traces FIRST (under icons)
        draw_station_traces();
        draw_own_trace();

        // Own position (on top of traces)
        double uiLat, uiLon;
        if (gpsFilter.getUiPosition(&uiLat, &uiLon)) {
            int myX, myY;
            MapMath::latLonToPixel(uiLat, uiLon,
                          map_center_lat, map_center_lon, map_current_zoom, navModeActive, centerTileX, centerTileY, &myX, &myY);
            if (myX >= 0 && myX < MAP_SPRITE_SIZE && myY >= 0 && myY < MAP_SPRITE_SIZE) {
                Beacon* currentBeacon = &Config.beacons[myBeaconsIndex];
                char fullSymbol[4];
                snprintf(fullSymbol, sizeof(fullSymbol), "%s%s",
                         currentBeacon->overlay.c_str(), currentBeacon->symbol.c_str());
                drawStationOnCanvas(myX, myY, currentBeacon->callsign.c_str(), fullSymbol, -1);
            }
        }

        // Received stations (on top of traces)
        STATION_Utils::cleanOldMapStations();
        for (int i = 0; i < MAP_STATIONS_MAX; i++) {
            MapStation* station = STATION_Utils::getMapStation(i);
            if (station && station->valid && station->latitude != 0.0f && station->longitude != 0.0f) {
                int stX, stY;
                MapMath::latLonToPixel(station->latitude, station->longitude,
                              map_center_lat, map_center_lon, map_current_zoom, navModeActive, centerTileX, centerTileY, &stX, &stY);
                if (stX >= 0 && stX < MAP_SPRITE_SIZE && stY >= 0 && stY < MAP_SPRITE_SIZE) {
                    drawStationOnCanvas(stX, stY, station->callsign.c_str(),
                                        station->symbol.c_str(), i);
                }
            }
        }
    }

    void draw_station_traces() {
        if (!map_canvas) return;

        uint32_t now = millis();
        lv_draw_line_dsc_t line_dsc;
        lv_draw_line_dsc_init(&line_dsc);
        line_dsc.color = lv_color_hex(0x0055FF);
        line_dsc.width = 2;
        line_dsc.opa   = LV_OPA_COVER;

        for (int s = 0; s < MAP_STATIONS_MAX; s++) {
            MapStation* station = STATION_Utils::getMapStation(s);
            if (!station || !station->valid || station->traceCount < 1) continue;

            static lv_point_t pts[TRACE_MAX_POINTS + 1];
            int validPts = 0;

            for (int i = 0; i < station->traceCount; i++) {
                int idx = (station->traceHead - station->traceCount + i + TRACE_MAX_POINTS) % TRACE_MAX_POINTS;
                if ((now - station->trace[idx].time) > TRACE_TTL_MS) continue;
                int px, py;
                MapMath::latLonToPixel(station->trace[idx].lat, station->trace[idx].lon,
                              map_center_lat, map_center_lon, map_current_zoom, navModeActive, centerTileX, centerTileY, &px, &py);
                if (px < -MAP_SPRITE_SIZE || px > 2 * MAP_SPRITE_SIZE ||
                    py < -MAP_SPRITE_SIZE || py > 2 * MAP_SPRITE_SIZE) continue;
                pts[validPts].x = px;
                pts[validPts].y = py;
                validPts++;
            }

            int cx, cy;
            MapMath::latLonToPixel(station->latitude, station->longitude,
                          map_center_lat, map_center_lon, map_current_zoom, navModeActive, centerTileX, centerTileY, &cx, &cy);
            pts[validPts].x = cx;
            pts[validPts].y = cy;
            validPts++;

            if (validPts >= 2)
                lv_canvas_draw_line(map_canvas, pts, validPts, &line_dsc);
        }
    }

    void draw_own_trace() {
        if (!map_canvas || !map_canvas_buf) return;
        if (gpsFilter.getOwnTraceCount() < 2) return;

        const TracePoint* trace = gpsFilter.getOwnTrace();
        int traceHead  = gpsFilter.getOwnTraceHead();
        int traceCount = gpsFilter.getOwnTraceCount();

        int minPixDist2 = 0;
        if (map_current_zoom <= 10)      minPixDist2 = 144;
        else if (map_current_zoom <= 12) minPixDist2 = 36;
        else if (map_current_zoom <= 14) minPixDist2 = 9;

        static lv_point_t trace_points[MapGPSFilter::OWN_TRACE_MAX_POINTS + 1];
        int startIdx = (traceHead - traceCount + MapGPSFilter::OWN_TRACE_MAX_POINTS) % MapGPSFilter::OWN_TRACE_MAX_POINTS;
        int validPts = 0;
        int lastX = INT_MIN, lastY = INT_MIN;

        for (int i = 0; i < traceCount; ++i) {
            int currentIdx = (startIdx + i) % MapGPSFilter::OWN_TRACE_MAX_POINTS;
            int x, y;
            MapMath::latLonToPixel(trace[currentIdx].lat, trace[currentIdx].lon,
                          map_center_lat, map_center_lon, map_current_zoom, navModeActive, centerTileX, centerTileY, &x, &y);
            if (minPixDist2 > 0 && validPts > 0 && i < traceCount - 1) {
                int dx = x - lastX, dy = y - lastY;
                if (dx * dx + dy * dy < minPixDist2) continue;
            }
            trace_points[validPts++] = { (lv_coord_t)x, (lv_coord_t)y };
            lastX = x;
            lastY = y;
        }

        double uiLat, uiLon;
        if (gpsFilter.getUiPosition(&uiLat, &uiLon)) {
            int cx, cy;
            MapMath::latLonToPixel(uiLat, uiLon,
                          map_center_lat, map_center_lon, map_current_zoom, navModeActive, centerTileX, centerTileY, &cx, &cy);
            trace_points[validPts++] = { (lv_coord_t)cx, (lv_coord_t)cy };
        }

        lv_draw_line_dsc_t line_dsc;
        lv_draw_line_dsc_init(&line_dsc);
        line_dsc.color = lv_color_hex(0x9933FF);
        line_dsc.width = 2;
        line_dsc.opa   = LV_OPA_COVER;

        if (validPts >= 2)
            lv_canvas_draw_line(map_canvas, trace_points, validPts, &line_dsc);
    }

} // namespace MapRender

#endif // USE_LVGL_UI
