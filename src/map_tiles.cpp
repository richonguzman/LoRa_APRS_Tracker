/* Map tile loading, PNG decoding, symbol caching, region discovery
 * Extracted from ui_map_manager.cpp (Étape 2 of refactoring)
 */

#include <Arduino.h>
#include <esp_log.h>

#include "map_state.h"
#include "map_tiles.h"
#include "storage_utils.h"

using namespace MapState;

static const char *TAG = "MapTiles";

// ===== File-scope statics (private to map_tiles.cpp) =====

// Negative cache for tiles not found on SD
#define NOT_FOUND_CACHE_SIZE 128
static std::vector<uint32_t> notFoundCache;
static int notFoundCacheIndex = 0;

// Symbol cache related (will be shared with ui_map_manager temporarily)
#define SYMBOL_CACHE_SIZE 10
#define SYMBOL_SIZE 24

// PNG file callbacks state
static bool pngFileOpened = false;

namespace MapTiles {

    // =========================================================================
    // Symbol cache management
    // =========================================================================

    void initSymbolCache() {
        // Stub: actual implementation in ui_map_manager.cpp (TODO: move here)
        ESP_LOGI(TAG, "initSymbolCache: stub (actual in ui_map_manager.cpp)");
    }

    uint8_t* loadSymbolFromSD(char table, char symbol) {
        // Stub: actual implementation in ui_map_manager.cpp (TODO: move here)
        ESP_LOGI(TAG, "loadSymbolFromSD: stub (actual in ui_map_manager.cpp)");
        return nullptr;
    }

    // =========================================================================
    // Tile loading
    // =========================================================================

    bool loadTileFromSD(int tileX, int tileY, int zoom, lv_obj_t* canvas, int offsetX, int offsetY) {
        // Stub: actual implementation in ui_map_manager.cpp (TODO: move here)
        ESP_LOGI(TAG, "loadTileFromSD: stub (actual in ui_map_manager.cpp)");
        return false;
    }

    bool preloadTileToCache(int tileX, int tileY, int zoom) {
        // Stub: actual implementation in ui_map_manager.cpp (TODO: move here)
        ESP_LOGI(TAG, "preloadTileToCache: stub (actual in ui_map_manager.cpp)");
        return false;
    }

    // =========================================================================
    // Tile preload task
    // =========================================================================

    void startTilePreloadTask() {
        // Stub: actual implementation in ui_map_manager.cpp (TODO: move here)
        ESP_LOGI(TAG, "startTilePreloadTask: stub (actual in ui_map_manager.cpp)");
    }

    void stopTilePreloadTask() {
        // Stub: actual implementation in ui_map_manager.cpp (TODO: move here)
        ESP_LOGI(TAG, "stopTilePreloadTask: stub (actual in ui_map_manager.cpp)");
    }

    // =========================================================================
    // Region discovery
    // =========================================================================

    void discoverDefaultPosition() {
        // Stub: actual implementation in ui_map_manager.cpp (TODO: move here)
        ESP_LOGI(TAG, "discoverDefaultPosition: stub (actual in ui_map_manager.cpp)");
    }

    void discoverAndSetMapRegion() {
        // Stub: actual implementation in ui_map_manager.cpp (TODO: move here)
        ESP_LOGI(TAG, "discoverAndSetMapRegion: stub (actual in ui_map_manager.cpp)");
    }

    void discoverNavRegions() {
        // Stub: actual implementation in ui_map_manager.cpp (TODO: move here)
        ESP_LOGI(TAG, "discoverNavRegions: stub (actual in ui_map_manager.cpp)");
    }

    bool regionContainsTile(const char* region, int zoom, int tileX, int tileY) {
        // Stub: actual implementation in ui_map_manager.cpp (TODO: move here)
        ESP_LOGI(TAG, "regionContainsTile: stub (actual in ui_map_manager.cpp)");
        return false;
    }

    // =========================================================================
    // Zoom table & center tile
    // =========================================================================

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

    void initCenterTileFromLatLon(float lat, float lon) {
        // Stub: actual implementation in ui_map_manager.cpp (TODO: move here)
        ESP_LOGI(TAG, "initCenterTileFromLatLon: stub (actual in ui_map_manager.cpp)");
    }

} // namespace MapTiles
