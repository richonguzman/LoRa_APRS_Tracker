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
#include <freertos/event_groups.h>
#include <vector>
#include "nav_types.h"

// Tile cache size (for raster tiles)
#define RASTER_TILE_CACHE_SIZE 16

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

    // Render lock: protects the entire render cycle.
    // Prevents clearTileCache / closeAllNpkSlots from running during render.
    extern SemaphoreHandle_t renderLock;

    // True when renderNavViewport is executing or queued (fast check without mutex)
    bool isRenderActive();

    // Async NAV rendering: EventGroup + request queue
    #define MAP_EVENT_NAV_DONE  (1 << 0)
    extern EventGroupHandle_t mapEventGroup;
    extern QueueHandle_t navRenderQueue;

    // Viewport render request (Core 1 → Core 0)
    // Used for both NAV (vector) and raster (PNG/JPG) compositing
    struct NavRenderRequest {
        int   centerTileX;       // IceNav model: integer tile reference
        int   centerTileY;
        float centerLat;         // Derived from centerTileX/Y for the render engine
        float centerLon;
        uint8_t zoom;
        LGFX_Sprite* targetSprite;
        char regions[8][64];
        int regionCount;
        bool isRaster;           // true = raster compositing, false = NAV vector
    };

    // Set by Core 0 before signaling MAP_EVENT_NAV_DONE — tells Core 1 which tile was rendered
    extern volatile int lastRenderedTileX;
    extern volatile int lastRenderedTileY;
    extern volatile uint8_t lastRenderedZoom;

    void enqueueNavRender(const NavRenderRequest& req);

    // Raster viewport compositing (runs on Core 0)
    bool renderRasterViewport(float centerLat, float centerLon, uint8_t zoom,
                              LGFX_Sprite &map, const char* region);

    // Function declarations
    void startRenderTask(lv_obj_t* canvas_to_invalidate);
    void stopRenderTask();
    void initTileCache(LovyanGFX* gfx);
    void clearTileCache();
    void shrinkProjectionBuffers();
    bool loadMapFont();
    bool renderNavViewport(float centerLat, float centerLon, uint8_t zoom,
                           LGFX_Sprite &map, const char** regions, int regionCount);
    bool renderTile(const char* path, int16_t xOffset, int16_t yOffset, LGFX_Sprite &map, uint8_t zoom = 0);

    // --- Static Raster Cache API ---
    CachedTile* getRasterCacheSlot(int zoom, int tileX, int tileY);
    LGFX_Sprite* findCachedRasterTile(int zoom, int tileX, int tileY);

    bool ensurePSRAMAvailable(size_t needed);
    void copySpriteToCanvasWithClip(lv_obj_t* canvas, LGFX_Sprite* sprite, int offsetX, int offsetY);

    // --- NAV POOL API ---
    void initNavPool();
    void destroyNavPool();
    uint8_t* acquireNavSlot(size_t needed);
    void releaseNavSlot(uint8_t* ptr);
    bool isNavPoolActive();
    int getAvailableNavSlots();

    } // namespace MapEngine

#endif // USE_LVGL_UI
#endif // MAP_ENGINE_H
