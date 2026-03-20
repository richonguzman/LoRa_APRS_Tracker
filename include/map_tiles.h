#ifndef MAP_TILES_H
#define MAP_TILES_H

#include <lvgl.h>
#include <Arduino.h>
#include "map_state.h"  // For CachedSymbol, MapState variables

namespace MapTiles {

    // =========================================================================
    // Symbol cache management
    // =========================================================================

    void initSymbolCache();
    uint8_t* loadSymbolFromSD(char table, char symbol);
    MapState::CachedSymbol* getSymbolCacheEntry(char table, char symbol);

    // =========================================================================
    // Tile loading
    // =========================================================================

    bool loadTileFromSD(int tileX, int tileY, int zoom, lv_obj_t* canvas, int offsetX, int offsetY);
    bool preloadTileToCache(int tileX, int tileY, int zoom);

    // =========================================================================
    // Tile preload task
    // =========================================================================

    void startTilePreloadTask();
    void stopTilePreloadTask();

    // =========================================================================
    // Region discovery
    // =========================================================================

    void discoverDefaultPosition();
    void discoverAndSetMapRegion();
    void discoverNavRegions();
    bool regionContainsTile(const char* region, int zoom, int tileX, int tileY);

    // =========================================================================
    // Zoom table switch (aussi utile depuis map_input/map_render)
    // =========================================================================

    void switchZoomTable(const int* newTable, int newCount);
    void initCenterTileFromLatLon(float lat, float lon);

} // namespace MapTiles

#endif // MAP_TILES_H
