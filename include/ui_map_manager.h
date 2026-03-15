/* Map logic for T-Deck Plus
 * Offline map tiles display with stations using LVGL
 */

#ifndef UI_MAP_MANAGER_H
#define UI_MAP_MANAGER_H

#ifdef USE_LVGL_UI

#include <lvgl.h>
#include <Arduino.h> // Pour String, millis, etc.
#include <TinyGPS++.h> // Pour les données GPS
#include "LGFX_TDeck.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <cstdint>
#include "esp_heap_caps.h"
#include <vector> // For std::vector
#include "nav_types.h"
#include "map_engine.h"

// Forward declarations
class Configuration;

// External data sources from lvgl_ui.cpp and other global variables
extern TinyGPSPlus gps;
extern Configuration Config;
extern LGFX_TDeck tft;
extern uint8_t myBeaconsIndex;
extern int mapStationsCount;
extern SemaphoreHandle_t spiMutex; // Declared extern for SPI bus mutex access

// Map constants
#define MAP_TILE_SIZE 256
#define MAP_TILES_GRID     3
#define MAP_SPRITE_SIZE    (MAP_TILES_GRID * MAP_TILE_SIZE)  // 768 = 3×256
#define MAP_VISIBLE_WIDTH 320  // Visible area on screen
#define MAP_VISIBLE_HEIGHT 200
#define MAP_MARGIN_X  ((MAP_SPRITE_SIZE - SCREEN_WIDTH) / 2)       // 224
#define MAP_MARGIN_Y  ((MAP_SPRITE_SIZE - MAP_VISIBLE_HEIGHT) / 2) // 284

// Dimensions de l'affichage
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

namespace UIMapManager {

    struct MapTile
    {
        char file[255];
        uint32_t tilex;
        uint32_t tiley;
        uint8_t zoom;
        float lat;
        float lon;
    };
    
    struct CachedSymbol {
        char table;              // '/' for primary, '\' for alternate
        char symbol;             // ASCII character
        lv_img_dsc_t img_dsc;   // LVGL image descriptor (RGB565A8 format)
        uint8_t* data;           // Combined RGB565+Alpha buffer in PSRAM
        uint32_t lastAccess;     // For LRU eviction
        bool valid;
    };

    // APRS symbol arrays
    extern const char* const* symbolArray;
    extern const int& symbolArraySize;
    extern const uint8_t* const* symbolsAPRS;

    // UI elements - Map screen
    extern lv_obj_t* screen_map;
    extern lv_obj_t* map_canvas;
    extern lv_color_t* map_canvas_buf;
    extern lv_obj_t* map_title_label;  // Title label to update zoom level
    extern lv_obj_t* map_container;    // Container for canvas and station buttons

    // Map state variables
    extern int map_zoom_index;  // Index in map_available_zooms (starts at zoom 8)
    extern int map_current_zoom;
    extern float map_center_lat;
    extern float map_center_lon;
    extern String map_current_region;
    extern bool map_follow_gps;  // Follow GPS or free panning mode



    // Function declarations
    bool loadPalette(const char* palettePath);
    // void copyTileToCanvas(uint16_t* tileData, lv_color_t* canvasBuffer,
    //                              int offsetX, int offsetY, int canvasWidth, int canvasHeight);
    CachedSymbol* getSymbolCacheEntry(char table, char symbol);
    void btn_map_back_clicked(lv_event_t* e);
    void btn_map_recenter_clicked(lv_event_t* e);
    bool loadTileFromSD(int tileX, int tileY, int zoom, lv_obj_t* canvas, int offsetX, int offsetY);
    void redraw_map_canvas();
    void btn_map_zoomin_clicked(lv_event_t* e);
    void btn_map_zoomout_clicked(lv_event_t* e);
    float getMapPanStep();
    void btn_map_up_clicked(lv_event_t* e);
    void btn_map_down_clicked(lv_event_t* e);
    void btn_map_left_clicked(lv_event_t* e);
    void btn_map_right_clicked(lv_event_t* e);
    void create_map_screen();
    void addOwnTracePoint();

} // namespace UIMapManager

#endif // USE_LVGL_UI
#endif // UI_MAP_MANAGER_H
