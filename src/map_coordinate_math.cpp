/* Map Coordinate Math Module Implementation
 * Pure mathematical functions for coordinate conversions
 * No global state, thread-safe, KISS-compliant
 */

#include "map_coordinate_math.h"

// Include constants from ui_map_manager.h to maintain compatibility
#include "ui_map_manager.h"

namespace MapMath {

// Constants are defined as macros in ui_map_manager.h:
// #define MAP_TILE_SIZE 256
// #define MAP_TILES_GRID 3  
// #define MAP_SPRITE_SIZE (MAP_TILES_GRID * MAP_TILE_SIZE)
// We use them directly in the code below

void latLonToTile(float lat, float lon, int zoom, int* tileX, int* tileY) {
    int n = 1 << zoom;
    *tileX = (int)((lon + 180.0f) / 360.0f * n);
    float latRad = lat * M_PI / 180.0f;
    *tileY = (int)((1.0f - log(tan(latRad) + 1.0f / cos(latRad)) / M_PI) / 2.0f * n);
}

void tileToLatLon(int tileX, int tileY, int zoom, float* lat, float* lon) {
    double n = (double)(1 << zoom);
    *lon = (float)((tileX + 0.5) / n * 360.0 - 180.0);
    double n_rad = M_PI * (1.0 - 2.0 * (tileY + 0.5) / n);
    *lat = (float)(atan(sinh(n_rad)) * 180.0 / M_PI);
}

void latLonToPixel(float lat, float lon, float centerLat, float centerLon, 
                   int zoom, bool navModeActive, int centerTileX, int centerTileY,
                   int* pixelX, int* pixelY) {
    double n = pow(2.0, zoom);
    double target_x_world = (lon + 180.0) / 360.0;
    double target_lat_rad = lat * M_PI / 180.0;
    double target_y_world = (1.0 - log(tan(target_lat_rad) + 1.0 / cos(target_lat_rad)) / M_PI) / 2.0;

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
        double center_lat_rad = centerLat * M_PI / 180.0;
        double center_y_world = (1.0 - log(tan(center_lat_rad) + 1.0 / cos(center_lat_rad)) / M_PI) / 2.0;
        double delta_x_px = (target_x_world - center_x_world) * n * MAP_TILE_SIZE;
        double delta_y_px = (target_y_world - center_y_world) * n * MAP_TILE_SIZE;
        *pixelX = (int)(MAP_SPRITE_SIZE / 2.0 + delta_x_px);
        *pixelY = (int)(MAP_SPRITE_SIZE / 2.0 + delta_y_px);
    }
}

void shiftMapCenter(float centerLat, float centerLon, int zoom,
                    int deltaTileX, int deltaTileY,
                    float* newLat, float* newLon) {
    if (deltaTileX == 0 && deltaTileY == 0) {
        *newLat = centerLat;
        *newLon = centerLon;
        return;
    }
    
    double n = pow(2.0, zoom);
    // Convert current center to cartesian tile coordinates
    double cx = (centerLon + 180.0) / 360.0 * n;
    double latRad = centerLat * M_PI / 180.0;
    double cy = (1.0 - log(tan(latRad) + 1.0 / cos(latRad)) / M_PI) / 2.0 * n;
    
    // Apply the exact tile delta
    cx += deltaTileX;
    cy += deltaTileY;
    
    // Convert back to Lat/Lon
    *newLon = (float)(cx / n * 360.0 - 180.0);
    double n_rad = M_PI * (1.0 - 2.0 * cy / n);
    *newLat = (float)(atan(sinh(n_rad)) * 180.0 / M_PI);
}

void pixelToLatLon(int pixelX, int pixelY, int zoom, bool navModeActive,
                   int centerTileX, int centerTileY,
                   float centerLat, float centerLon,
                   float* lat, float* lon) {
    double n = pow(2.0, zoom);

    double x_world, y_world;
    if (navModeActive) {
        const int8_t gridOffset = MAP_TILES_GRID / 2;
        double grid_origin_wx = (double)(centerTileX - gridOffset) * MAP_TILE_SIZE;
        double grid_origin_wy = (double)(centerTileY - gridOffset) * MAP_TILE_SIZE;
        x_world = (pixelX + grid_origin_wx) / (n * MAP_TILE_SIZE);
        y_world = (pixelY + grid_origin_wy) / (n * MAP_TILE_SIZE);
    } else {
        double center_x_world = (centerLon + 180.0) / 360.0;
        double center_lat_rad = centerLat * M_PI / 180.0;
        double center_y_world = (1.0 - log(tan(center_lat_rad) + 1.0 / cos(center_lat_rad)) / M_PI) / 2.0;
        x_world = center_x_world + (pixelX - MAP_SPRITE_SIZE / 2.0) / (n * MAP_TILE_SIZE);
        y_world = center_y_world + (pixelY - MAP_SPRITE_SIZE / 2.0) / (n * MAP_TILE_SIZE);
    }

    *lon = (float)(x_world * 360.0 - 180.0);
    double n_rad = M_PI * (1.0 - 2.0 * y_world);
    *lat = (float)(atan(sinh(n_rad)) * 180.0 / M_PI);
}

} // namespace MapMath