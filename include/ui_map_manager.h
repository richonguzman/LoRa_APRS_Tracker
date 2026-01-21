/* Map logic for T-Deck Plus
 * Offline map tiles display with stations using LVGL
 */

#ifndef UI_MAP_MANAGER_H
#define UI_MAP_MANAGER_H

#ifdef USE_LVGL_UI

#include <lvgl.h>
#include <Arduino.h> // Pour String, millis, etc.
#include <TinyGPS++.h> // Pour les données GPS
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Dimensions de l'affichage
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

namespace UIMapManager {

    // Sources de données externes nécessaires de lvgl_ui.cpp (ou autres globales)
    extern TinyGPSPlus gps;
    extern Configuration Config;
    extern uint8_t myBeaconsIndex;
    extern int mapStationsCount;
    extern SemaphoreHandle_t spiMutex; // Déclaré extern pour l'accès au mutex SPI

    // Constantes de la carte
    #define MAP_TILE_SIZE 256
    #define MAP_CANVAS_WIDTH 320
    #define MAP_CANVAS_HEIGHT 200  // Écran moins la barre de titre et la barre de boutons

    // Éléments de l'interface utilisateur - Écran de carte
    extern lv_obj_t* screen_map;
    extern lv_obj_t* map_canvas;
    extern lv_color_t* map_canvas_buf;
    extern lv_obj_t* map_title_label;  // Label de titre pour mettre à jour le niveau de zoom
    extern lv_obj_t* map_container;    // Conteneur pour le canevas et les boutons de station

    // Variables d'état de la carte
    extern int map_zoom_index;  // Index dans map_available_zooms (commence au zoom 8)
    extern int map_current_zoom;
    extern float map_center_lat;
    extern float map_center_lon;
    extern String map_current_region;
    extern bool map_follow_gps;  // Suivre le GPS ou mode de panoramique libre

    // Déclarations des fonctions
    void initTileCache();
    int findCachedTile(int zoom, int tileX, int tileY);
    int findCacheSlot();
    void copyTileToCanvas(uint16_t* tileData, lv_color_t* canvasBuffer,
                                 int offsetX, int offsetY, int canvasWidth, int canvasHeight);
    void latLonToTile(float lat, float lon, int zoom, int* tileX, int* tileY);
    void latLonToPixel(float lat, float lon, float centerLat, float centerLon, int zoom, int* pixelX, int* pixelY);
    lv_color_t getAPRSSymbolColor(const char* symbol);
    void drawMapSymbol(lv_obj_t* canvas, int x, int y, const char* symbolChar, lv_color_t color);
    void map_station_clicked(lv_event_t* e);
    void btn_map_back_clicked(lv_event_t* e);
    void btn_map_recenter_clicked(lv_event_t* e);
    bool loadTileFromSD(int tileX, int tileY, int zoom, lv_obj_t* canvas, int offsetX, int offsetY);
    void redraw_map_canvas();
    void map_reload_timer_cb(lv_timer_t* timer);
    void schedule_map_reload();
    void btn_map_zoomin_clicked(lv_event_t* e);
    void btn_map_zoomout_clicked(lv_event_t* e);
    float getMapPanStep();
    void btn_map_up_clicked(lv_event_t* e);
    void btn_map_down_clicked(lv_event_t* e);
    void btn_map_left_clicked(lv_event_t* e);
    void btn_map_right_clicked(lv_event_t* e);
    void create_map_screen();

} // namespace UIMapManager

#endif // USE_LVGL_UI
#endif // UI_MAP_MANAGER_H
