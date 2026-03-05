/* Map rendering engine for T-Deck Plus
 * Handles vector tile parsing, rendering, and caching.
 */
#ifndef MAP_ENGINE_H
#define MAP_ENGINE_H

#ifdef USE_LVGL_UI

#include <lvgl.h>
#include "LGFX_TDeck.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <vector>
#include "nav_types.h"

namespace MapEngine {
    
    struct CachedTile
    {
        lgfx::LGFX_Sprite* sprite;
        uint32_t tileHash;
        uint32_t lastAccess;
        bool isValid;
        char filePath[64];
    };
    
    // Request for the background rendering task
    struct RenderRequest {
        char path[128];
        int16_t xOffset;
        int16_t yOffset;
        lgfx::LGFX_Sprite* targetSprite;
        int zoom;   // For caching
        int tileX;  // For caching
        int tileY;  // For caching
    };

    // Handles for the asynchronous rendering system
    extern QueueHandle_t mapRenderQueue;
    extern SemaphoreHandle_t spriteMutex;

    // Function declarations
    void startRenderTask(lv_obj_t* canvas_to_invalidate);
    void stopRenderTask();
    void initTileCache();
    void clearTileCache();
    void shrinkProjectionBuffers();
    bool loadMapFont();
    bool renderNavViewport(float centerLat, float centerLon, uint8_t zoom,
                           LGFX_Sprite &map, const char** regions, int regionCount);
    bool renderTile(const char* path, int16_t xOffset, int16_t yOffset, LGFX_Sprite &map, uint8_t zoom = 0);
    int findCachedTile(int zoom, int tileX, int tileY);
    void addToCache(const char* filePath, int zoom, int tileX, int tileY, LGFX_Sprite* sourceSprite);
    void copySpriteToCanvasWithClip(lv_obj_t* canvas, LGFX_Sprite* sprite, int offsetX, int offsetY);
    LGFX_Sprite* getCachedTileSprite(int index);

} // namespace MapEngine

#endif // USE_LVGL_UI
#endif // MAP_ENGINE_H
