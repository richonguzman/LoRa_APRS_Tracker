/* Map Coordinate Math Module
 * Pure mathematical functions for coordinate conversions
 * No global state, thread-safe, KISS-compliant
 */

#ifndef MAP_COORDINATE_MATH_H
#define MAP_COORDINATE_MATH_H

#include <cmath>

namespace MapMath {

// Constants
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Use macros from ui_map_manager.h - they will be included by the .cpp file
// MAP_TILE_SIZE, MAP_TILES_GRID, MAP_SPRITE_SIZE are defined as macros in ui_map_manager.h

/**
 * Convert latitude/longitude to tile coordinates (Mercator projection)
 * 
 * @param lat Latitude in degrees
 * @param lon Longitude in degrees  
 * @param zoom Zoom level (0..)
 * @param[out] tileX Tile X coordinate
 * @param[out] tileY Tile Y coordinate
 */
void latLonToTile(float lat, float lon, int zoom, int* tileX, int* tileY);

/**
 * Convert tile coordinates to latitude/longitude of tile center
 * 
 * @param tileX Tile X coordinate
 * @param tileY Tile Y coordinate
 * @param zoom Zoom level (0..)
 * @param[out] lat Latitude in degrees
 * @param[out] lon Longitude in degrees
 */
void tileToLatLon(int tileX, int tileY, int zoom, float* lat, float* lon);

/**
 * Convert latitude/longitude to pixel position in sprite/canvas
 * 
 * @param lat Latitude in degrees
 * @param lon Longitude in degrees
 * @param centerLat Center latitude in degrees
 * @param centerLon Center longitude in degrees
 * @param zoom Zoom level (0..)
 * @param navModeActive Whether NAV (vector) mode is active
 * @param centerTileX Center tile X coordinate (for NAV mode)
 * @param centerTileY Center tile Y coordinate (for NAV mode)
 * @param[out] pixelX Pixel X coordinate
 * @param[out] pixelY Pixel Y coordinate
 */
void latLonToPixel(float lat, float lon, float centerLat, float centerLon, 
                   int zoom, bool navModeActive, int centerTileX, int centerTileY,
                   int* pixelX, int* pixelY);

/**
 * Shift map center by tile deltas
 * 
 * @param centerLat Current center latitude in degrees
 * @param centerLon Current center longitude in degrees
 * @param zoom Zoom level (0..)
 * @param deltaTileX Tile delta in X direction
 * @param deltaTileY Tile delta in Y direction
 * @param[out] newLat New center latitude in degrees
 * @param[out] newLon New center longitude in degrees
 */
void shiftMapCenter(float centerLat, float centerLon, int zoom,
                    int deltaTileX, int deltaTileY,
                    float* newLat, float* newLon);

/**
 * Convert canvas pixel coordinates back to latitude/longitude (inverse Mercator)
 * Works for both NAV and raster modes.
 */
void pixelToLatLon(int pixelX, int pixelY, int zoom, bool navModeActive,
                   int centerTileX, int centerTileY,
                   float centerLat, float centerLon,
                   float* lat, float* lon);

} // namespace MapMath

#endif // MAP_COORDINATE_MATH_H