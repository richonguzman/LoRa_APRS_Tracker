#ifndef MAP_STATE_H
#define MAP_STATE_H

#include <lvgl.h>
#include <LovyanGFX.hpp>
#include <Arduino.h>
#include "map_gps_filter.h"

// =============================================================================
// MapState — état partagé cross-modules de la carte
//
// Règle : une variable est ici si et seulement si elle est lue ou écrite
// par au moins deux modules futurs (map_tiles, map_render, map_input,
// ui_map_manager). Les variables privées à un seul module restent locales.
//
// Usage dans chaque .cpp :
//   using namespace MapState;
// =============================================================================

namespace MapState {

    // -------------------------------------------------------------------------
    // Widgets LVGL (map screen)
    // -------------------------------------------------------------------------
    extern lv_obj_t* screen_map;
    extern lv_obj_t* map_canvas;
    extern lv_color_t* map_canvas_buf;
    extern lv_obj_t* map_title_label;
    extern lv_obj_t* map_info_label;
    extern lv_obj_t* map_container;
    extern lv_obj_t* btn_zoomin;
    extern lv_obj_t* btn_zoomout;
    extern lv_obj_t* btn_recenter;
    extern lv_obj_t* map_title_bar;
    extern lv_obj_t* map_info_bar;

    // -------------------------------------------------------------------------
    // Constantes zoom (définies ici, pas dans .cpp)
    // -------------------------------------------------------------------------
    constexpr int nav_zooms[]    = {9, 10, 11, 12, 13, 14, 15, 16, 17};
    constexpr int nav_zoom_count = sizeof(nav_zooms) / sizeof(nav_zooms[0]);
    constexpr int raster_zooms[]    = {6, 7, 8, 10, 12, 14};
    constexpr int raster_zoom_count = sizeof(raster_zooms) / sizeof(raster_zooms[0]);

    // -------------------------------------------------------------------------
    // Zoom état
    // -------------------------------------------------------------------------
    extern const int* map_available_zooms;
    extern int        map_zoom_count;
    extern int        map_zoom_index;
    extern int        map_current_zoom;

    // -------------------------------------------------------------------------
    // Position & région
    // -------------------------------------------------------------------------
    extern float  map_center_lat;
    extern float  map_center_lon;
    extern float  defaultLat;
    extern float  defaultLon;
    extern String map_current_region;

    constexpr int NAV_MAX_REGIONS = 4;
    extern String navRegions[NAV_MAX_REGIONS];
    extern int    navRegionCount;

    extern bool map_follow_gps;

    // -------------------------------------------------------------------------
    // Référence tuiles (entiers, jamais float)
    // -------------------------------------------------------------------------
    extern int centerTileX;
    extern int centerTileY;
    extern int renderTileX;
    extern int renderTileY;

    // -------------------------------------------------------------------------
    // Pan / scroll (input écrit, render lit)
    // -------------------------------------------------------------------------
    extern bool    isScrollingMap;
    extern bool    dragStarted;
    extern int16_t offsetX;
    extern int16_t offsetY;
    extern int16_t navSubTileX;
    extern int16_t navSubTileY;
    extern float   velocityX;
    extern float   velocityY;

    // Constantes pan
    constexpr float PAN_FRICTION       = 0.95f;
    constexpr float PAN_FRICTION_BUSY  = 0.85f;
    constexpr int   START_THRESHOLD    = 12;
    constexpr int   PAN_TILE_THRESHOLD = 128;

    // -------------------------------------------------------------------------
    // UI state (input écrit, glue lit)
    // -------------------------------------------------------------------------
    extern bool mapFullscreen;

    // -------------------------------------------------------------------------
    // APRS symbol cache (créé par map_tiles, lu par map_render)
    // -------------------------------------------------------------------------
    struct CachedSymbol {
        char table;              // '/' for primary, '\' for alternate
        char symbol;             // ASCII character
        lv_img_dsc_t img_dsc;   // LVGL image descriptor (RGB565A8 format)
        uint8_t* data;           // Combined RGB565+Alpha buffer in PSRAM
        uint32_t lastAccess;     // For LRU eviction
        bool valid;
    };

    // -------------------------------------------------------------------------
    // Hit zones stations (render écrit, input lit)
    // -------------------------------------------------------------------------
    struct StationHitZone {
        int16_t x, y;
        int16_t w, h;
        int8_t  stationIdx;  // Index dans mapStations[], -1 = unused
    };
    constexpr int MAP_STATIONS_HIT_MAX = 15;  // == MAP_STATIONS_MAX
    extern StationHitZone stationHitZones[MAP_STATIONS_HIT_MAX];
    extern int            stationHitZoneCount;

    // -------------------------------------------------------------------------
    // GPS filter
    // -------------------------------------------------------------------------
    extern MapGPSFilter gpsFilter;
    extern bool         pendingResetPan;

    // -------------------------------------------------------------------------
    // GPX recording button refs (créés par glue, utilisés par input)
    // -------------------------------------------------------------------------
    extern lv_obj_t* btn_gpx_rec;
    extern lv_obj_t* lbl_gpx_rec;

    // -------------------------------------------------------------------------
    // Timer refresh (créé par glue, arrêté par input)
    // -------------------------------------------------------------------------
    extern lv_timer_t* map_refresh_timer;

    // -------------------------------------------------------------------------
    // État rendu (volatile : accès cross-core)
    // -------------------------------------------------------------------------
    extern volatile bool redraw_in_progress;
    extern volatile bool navModeActive;
    extern volatile bool navRenderPending;
    extern volatile bool mainThreadLoading;
    extern volatile int8_t pendingZoom;      // +1 = zoom in queued, -1 = zoom out queued

    // -------------------------------------------------------------------------
    // Double-buffer sprites (PSRAM)
    // -------------------------------------------------------------------------
    extern LGFX_Sprite* backViewportSprite;
    extern LGFX_Sprite* frontViewportSprite;

} // namespace MapState

#endif // MAP_STATE_H
