#include "map_state.h"

namespace MapState {

    // -------------------------------------------------------------------------
    // Widgets LVGL
    // -------------------------------------------------------------------------
    lv_obj_t*   screen_map      = nullptr;
    lv_obj_t*   map_canvas      = nullptr;
    lv_color_t* map_canvas_buf  = nullptr;
    lv_obj_t*   map_title_label = nullptr;
    lv_obj_t*   map_info_label  = nullptr;
    lv_obj_t*   map_container   = nullptr;
    lv_obj_t*   btn_zoomin      = nullptr;
    lv_obj_t*   btn_zoomout     = nullptr;
    lv_obj_t*   btn_recenter    = nullptr;
    lv_obj_t*   map_title_bar   = nullptr;
    lv_obj_t*   map_info_bar    = nullptr;

    // -------------------------------------------------------------------------
    // Zoom état
    // -------------------------------------------------------------------------
    const int* map_available_zooms = raster_zooms;
    int        map_zoom_count      = raster_zoom_count;
    int        map_zoom_index      = 0;
    int        map_current_zoom    = raster_zooms[0];

    // -------------------------------------------------------------------------
    // Position & région
    // -------------------------------------------------------------------------
    float  map_center_lat     = 0.0f;
    float  map_center_lon     = 0.0f;
    float  defaultLat         = 0.0f;
    float  defaultLon         = 0.0f;
    String map_current_region = "";
    String navRegions[NAV_MAX_REGIONS];
    int    navRegionCount     = 0;
    bool   map_follow_gps     = true;

    // -------------------------------------------------------------------------
    // Référence tuiles
    // -------------------------------------------------------------------------
    int centerTileX = 0;
    int centerTileY = 0;
    int renderTileX = 0;
    int renderTileY = 0;

    // -------------------------------------------------------------------------
    // Pan / scroll
    // -------------------------------------------------------------------------
    bool    isScrollingMap = false;
    bool    dragStarted    = false;
    int16_t offsetX        = 0;
    int16_t offsetY        = 0;
    int16_t navSubTileX    = 0;
    int16_t navSubTileY    = 0;
    float   velocityX      = 0.0f;
    float   velocityY      = 0.0f;

    // -------------------------------------------------------------------------
    // UI state
    // -------------------------------------------------------------------------
    bool mapFullscreen = false;

    // -------------------------------------------------------------------------
    // Hit zones stations
    // -------------------------------------------------------------------------
    StationHitZone stationHitZones[MAP_STATIONS_HIT_MAX];
    int            stationHitZoneCount = 0;

    // -------------------------------------------------------------------------
    // GPS filter
    // -------------------------------------------------------------------------
    MapGPSFilter gpsFilter;
    bool         pendingResetPan = false;

    // -------------------------------------------------------------------------
    // GPX recording button refs
    // -------------------------------------------------------------------------
    lv_obj_t* btn_gpx_rec = nullptr;
    lv_obj_t* lbl_gpx_rec = nullptr;

    // -------------------------------------------------------------------------
    // Timer refresh
    // -------------------------------------------------------------------------
    lv_timer_t* map_refresh_timer = nullptr;

    // -------------------------------------------------------------------------
    // État rendu
    // -------------------------------------------------------------------------
    volatile bool redraw_in_progress = false;
    volatile bool navModeActive      = false;
    volatile bool navRenderPending   = false;
    volatile bool mainThreadLoading  = false;
    volatile int8_t pendingZoom      = 0;      // +1 = zoom in queued, -1 = zoom out queued

    // -------------------------------------------------------------------------
    // Double-buffer sprites
    // -------------------------------------------------------------------------
    LGFX_Sprite* backViewportSprite  = nullptr;
    LGFX_Sprite* frontViewportSprite = nullptr;

} // namespace MapState
