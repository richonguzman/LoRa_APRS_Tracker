/* NAV vector tile rendering engine for T-Deck Plus
 * fillPolygonGeneral + renderNavViewport
 */
#ifdef USE_LVGL_UI

#include "map_internal.h"
#include "storage_utils.h"
#include "OpenSansBold6pt7b.h"
#include <SD.h>
#include <algorithm>
#include <new>

static const char* TAG = "MapNavRender";

namespace MapEngine {

    bool fillPolygons = true;

    uint16_t darkenRGB565(const uint16_t color, const float amount) {
        uint16_t r = (color >> 11) & 0x1F;
        uint16_t g = (color >> 5) & 0x3F;
        uint16_t b = color & 0x1F;
        r = (uint16_t)(r * (1.0f - amount));
        g = (uint16_t)(g * (1.0f - amount));
        b = (uint16_t)(b * (1.0f - amount));
        return (r << 11) | (g << 5) | b;
    }

    // AEL polygon filler with fast-forward optimization.
    // Takes high-precision (HP) coordinates (0-4096), iterates pixel-space scanlines (0-255).
    // Supports multi-ring polygons (exterior + holes).
    void fillPolygonGeneral(LGFX_Sprite &map, const int *px_hp, const int *py_hp, const int numPoints, const uint16_t color, const int xOffset, const int yOffset, uint16_t ringCount, uint16_t* ringEnds)
    {
        if (numPoints < 3) return;

        // 1. Find Y bounds in pixel space
        int minY_px = INT_MAX, maxY_px = INT_MIN;
        for (int i = 0; i < numPoints; i++) {
            int y_px = py_hp[i] >> 4;
            if (y_px < minY_px) minY_px = y_px;
            if (y_px > maxY_px) maxY_px = y_px;
        }

        int spriteH = map.height();
        if (maxY_px < 0 || minY_px >= spriteH) return;

        // 2. Set up pixel-space edge buckets
        edgePool.clear();
        int bucketCount = maxY_px - minY_px + 1;
        if (bucketCount <= 0) return;
        edgeBuckets.assign(bucketCount, -1);

        uint16_t count = (ringCount == 0) ? 1 : ringCount;
        uint16_t defaultEnds[1] = { (uint16_t)numPoints };
        uint16_t* ends = (ringEnds == nullptr) ? defaultEnds : ringEnds;

        int ringStart = 0;
        for (uint16_t r = 0; r < count; r++) {
            int ringEnd = ends[r];
            int ringNumPoints = ringEnd - ringStart;
            if (ringNumPoints < 3) {
                ringStart = ringEnd;
                continue;
            }

            // 3. Populate buckets with edges using HP math for precision
            for (int i = 0; i < ringNumPoints; i++) {
                int next = (i + 1) % ringNumPoints;
                int x1_hp = px_hp[ringStart + i], y1_hp = py_hp[ringStart + i];
                int x2_hp = px_hp[ringStart + next], y2_hp = py_hp[ringStart + next];

                int y1_px = y1_hp >> 4;
                int y2_px = y2_hp >> 4;

                if (y1_px == y2_px) continue;

                UIMapManager::Edge e;
                e.nextActive = -1;

                if (y1_hp < y2_hp) {
                    e.yMax = y2_px;
                    e.slope = ((int64_t)(x2_hp - x1_hp) << 16) / (y2_hp - y1_hp);
                    e.xVal = ((int64_t)x1_hp << 16);
                    if (y1_px - minY_px >= 0 && (size_t)(y1_px - minY_px) < edgeBuckets.size()) {
                        e.nextInBucket = edgeBuckets[y1_px - minY_px];
                        edgePool.push_back(e);
                        edgeBuckets[y1_px - minY_px] = edgePool.size() - 1;
                    }
                } else {
                    e.yMax = y1_px;
                    e.slope = ((int64_t)(x1_hp - x2_hp) << 16) / (y1_hp - y2_hp);
                    e.xVal = ((int64_t)x2_hp << 16);
                    if (y2_px - minY_px >= 0 && (size_t)(y2_px - minY_px) < edgeBuckets.size()) {
                        e.nextInBucket = edgeBuckets[y2_px - minY_px];
                        edgePool.push_back(e);
                        edgeBuckets[y2_px - minY_px] = edgePool.size() - 1;
                    }
                }
            }
            ringStart = ringEnd;
        }

        int activeHead = -1;
        int spriteW = map.width();
        // Clip Y range accounting for offset
        int startY_px = std::max(minY_px, -yOffset);
        int endY_px = std::min(maxY_px, spriteH - 1 - yOffset);

        // 4. Fast-forward: process buckets before visible range, jump edge xVal
        // Directly to startY_px (skip invisible scanlines)
        if (startY_px > minY_px) {
            for (int y = minY_px; y < startY_px; y++) {
                if ((size_t)(y - minY_px) >= edgeBuckets.size()) break;
                int eIdx = edgeBuckets[y - minY_px];
                while (eIdx != -1) {
                    int nextIdx = edgePool[eIdx].nextInBucket;
                    // Jump xVal to startY_px: (startY_px - y) pixel scanlines × 16 HP units each
                    edgePool[eIdx].xVal += (int64_t)edgePool[eIdx].slope * 16 * (startY_px - y);
                    edgePool[eIdx].nextActive = activeHead;
                    activeHead = eIdx;
                    eIdx = nextIdx;
                }
            }
            // Remove edges that finish before reaching the visible range
            int* pCurrIdx = &activeHead;
            while (*pCurrIdx != -1) {
                if (edgePool[*pCurrIdx].yMax <= startY_px)
                    *pCurrIdx = edgePool[*pCurrIdx].nextActive;
                else
                    pCurrIdx = &(edgePool[*pCurrIdx].nextActive);
            }
        }

        // 5. Main scanline loop (visible range only)
        int scanlineCount = 0;
        for (int y_px = startY_px; y_px <= endY_px; y_px++) {
            if (++scanlineCount >= 32) {
                esp_task_wdt_reset();
                scanlineCount = 0;
            }

            // Add new edges from bucket
            if ((size_t)(y_px - minY_px) < edgeBuckets.size()) {
                int eIdx = edgeBuckets[y_px - minY_px];
                while (eIdx != -1) {
                    int nextIdx = edgePool[eIdx].nextInBucket;
                    edgePool[eIdx].nextActive = activeHead;
                    activeHead = eIdx;
                    eIdx = nextIdx;
                }
            }

            // Remove finished edges
            int* pCurrIdx = &activeHead;
            while (*pCurrIdx != -1) {
                if (edgePool[*pCurrIdx].yMax <= y_px)
                    *pCurrIdx = edgePool[*pCurrIdx].nextActive;
                else
                    pCurrIdx = &(edgePool[*pCurrIdx].nextActive);
            }

            if (activeHead == -1) continue;

            // Sort AEL by xVal (insertion sort into linked list)
            int sorted = -1;
            int active = activeHead;
            while (active != -1) {
                int nextActive = edgePool[active].nextActive;
                if (sorted == -1 || edgePool[active].xVal < edgePool[sorted].xVal) {
                    edgePool[active].nextActive = sorted;
                    sorted = active;
                } else {
                    int s = sorted;
                    while (edgePool[s].nextActive != -1 && edgePool[edgePool[s].nextActive].xVal < edgePool[active].xVal)
                        s = edgePool[s].nextActive;
                    edgePool[active].nextActive = edgePool[s].nextActive;
                    edgePool[s].nextActive = active;
                }
                active = nextActive;
            }
            activeHead = sorted;

            // Draw horizontal spans
            int left = activeHead;
            while (left != -1 && edgePool[left].nextActive != -1) {
                int right = edgePool[left].nextActive;
                int xStart = (edgePool[left].xVal >> 16) >> 4;
                int xEnd = (edgePool[right].xVal >> 16) >> 4;
                if (xStart < 0) xStart = 0;
                if (xEnd > spriteW) xEnd = spriteW;
                if (xEnd > xStart)
                    map.drawFastHLine(xStart + xOffset, y_px + yOffset, xEnd - xStart, color);
                left = edgePool[right].nextActive;
            }

            // Update xVal for next pixel scanline (step = slope × 16 HP units)
            for (int a = activeHead; a != -1; a = edgePool[a].nextActive)
                edgePool[a].xVal += (int64_t)edgePool[a].slope * 16;
        }
    }

    // =========================================================================
    // Render a single geometry feature (polygon, line, point) on the sprite.
    // Shared by both batch (Z10+) and streaming (Z9) pipelines.
    // Does NOT handle text (GEOM_TEXT / GEOM_TEXT_LINE) — caller collects those.
    //
    // Transposed from IceNav-v3 maps.cpp L1007-1197 (renderNavLineString,
    // renderNavPolygon, renderNavPoint, renderNavFeature).
    // =========================================================================

    // --- Line rendering (IceNav renderNavLineString L1007-1062) ---
    // Single-loop: LOD + per-segment culling + draw inline.
    static void renderNavLine(LGFX_Sprite &map, const FeatureRef &ref,
                              int viewportW, int viewportH, uint8_t zoom) {
        if (ref.coordCount < 2) return;
        const uint8_t* fp = ref.ptr;

        decodedCoords.clear();
        DecodedFeature df;
        if (!decodeFeatureCoords(fp + 13, ref.coordCount, ref.payloadSize, ref.geomType, df)) {
            ESP_LOGW(TAG, "Line decode FAILED: coords=%u payload=%u tile(%d,%d)",
                     ref.coordCount, ref.payloadSize, ref.tileOffsetX, ref.tileOffsetY);
            return;
        }
        int16_t* coords = decodedCoords.data() + df.coordsIdx;

        // Width — IceNav L1026-1027
        uint8_t widthRaw = fp[4] & 0x7F;
        if (widthRaw == 0) widthRaw = 2;
        uint16_t colorRgb565;
        memcpy(&colorRgb565, fp + 1, 2);
        float widthF = widthRaw / 2.0f;

        // LOD — IceNav L1036-1040
        int16_t lodThreshold = (zoom >= 15) ? 2 : 1;

        // Single-loop render — IceNav L1042-1061
        int16_t lastPx = -32768;
        int16_t lastPy = -32768;
        for (uint16_t i = 0; i < ref.coordCount; i++) {
            int16_t px = (coords[i * 2] >> 4) + ref.tileOffsetX;
            int16_t py = (coords[i * 2 + 1] >> 4) + ref.tileOffsetY;
            if (i > 0) {
                if (abs(px - lastPx) < lodThreshold && abs(py - lastPy) < lodThreshold)
                    continue;
                // Per-segment culling — IceNav L1051
                if (!((px < 0 && lastPx < 0) ||
                      (px >= viewportW && lastPx >= viewportW) ||
                      (py < 0 && lastPy < 0) ||
                      (py >= viewportH && lastPy >= viewportH))) {
                    if (widthF <= 1.0f)
                        map.drawLine(lastPx, lastPy, px, py, colorRgb565);
                    else
                        // drawWideLine param is radius; LovyanGFX adds +0.5 internally
                        // Compensate: subtract 0.5 to cancel the internal +0.5
                        map.drawWideLine(lastPx, lastPy, px, py,
                                         widthF - 0.5f, colorRgb565);
                }
            }
            lastPx = px;
            lastPy = py;
        }
    }

    // --- Polygon rendering (IceNav renderNavPolygon L1067-1158) ---
    // Coords stay in high-precision (raw), fillPolygonGeneral handles >> 4 + offset.
    static void renderNavPolygon(LGFX_Sprite &map, const FeatureRef &ref,
                                 int viewportW, int viewportH, uint8_t zoom) {
        if (ref.coordCount < 3) return;
        const uint8_t* fp = ref.ptr;

        decodedCoords.clear();
        DecodedFeature df;
        if (!decodeFeatureCoords(fp + 13, ref.coordCount, ref.payloadSize, ref.geomType, df)) {
            ESP_LOGW(TAG, "Polygon decode FAILED: coords=%u payload=%u tile(%d,%d)",
                     ref.coordCount, ref.payloadSize, ref.tileOffsetX, ref.tileOffsetY);
            return;
        }
        int16_t* coords = decodedCoords.data() + df.coordsIdx;

        uint16_t colorRgb565;
        memcpy(&colorRgb565, fp + 1, 2);

        // Project to HP buffers with LOD — IceNav L1105-1133
        if (proj32X.capacity() < ref.coordCount) proj32X.reserve(ref.coordCount * 3 / 2);
        if (proj32Y.capacity() < ref.coordCount) proj32Y.reserve(ref.coordCount * 3 / 2);
        proj32X.resize(ref.coordCount);
        proj32Y.resize(ref.coordCount);
        int* px_hp = proj32X.data();
        int* py_hp = proj32Y.data();
        if (!px_hp || !py_hp) return;

        int minPx = INT_MAX, maxPx = INT_MIN;
        int minPy = INT_MAX, maxPy = INT_MIN;
        int16_t lastX = -32768, lastY = -32768;
        uint16_t actualPoints = 0;

        for (uint16_t i = 0; i < ref.coordCount; i++) {
            int16_t cx = coords[i * 2];
            int16_t cy = coords[i * 2 + 1];
            // LOD: skip duplicate pixels for simple polygons — IceNav L1118
            if (df.ringCount == 0 && i > 0 &&
                abs(cx - lastX) < 1 && abs(cy - lastY) < 1 &&
                i < ref.coordCount - 1)
                continue;
            px_hp[actualPoints] = (int)cx;
            py_hp[actualPoints] = (int)cy;
            int px_screen = (cx >> 4) + ref.tileOffsetX;
            int py_screen = (cy >> 4) + ref.tileOffsetY;
            if (px_screen < minPx) minPx = px_screen;
            if (px_screen > maxPx) maxPx = px_screen;
            if (py_screen < minPy) minPy = py_screen;
            if (py_screen > maxPy) maxPy = py_screen;
            lastX = cx;
            lastY = cy;
            actualPoints++;
        }

        // BBox culling — IceNav L1134
        if (maxPx < 0 || minPx >= viewportW || maxPy < 0 || minPy >= viewportH)
            return;

        // Fill — IceNav L1138-1139
        if (fillPolygons)
            fillPolygonGeneral(map, px_hp, py_hp, actualPoints,
                               colorRgb565, ref.tileOffsetX, ref.tileOffsetY,
                               df.ringCount, df.ringEnds);

        // Building outline — IceNav L1140-1157
        if ((fp[4] & 0x80) != 0 && zoom >= 16) {
            uint16_t outlineColor = darkenRGB565(colorRgb565, 0.35f);
            uint16_t ringStart = 0;
            uint16_t numRings = (df.ringCount > 0) ? df.ringCount : 1;
            for (uint16_t r = 0; r < numRings; r++) {
                uint16_t ringEnd = (df.ringEnds && r < df.ringCount) ? df.ringEnds[r] : actualPoints;
                if (ringEnd > actualPoints) ringEnd = actualPoints;
                for (uint16_t j = ringStart; j < ringEnd; j++) {
                    uint16_t next = (j + 1 < ringEnd) ? j + 1 : ringStart;
                    int x0 = (px_hp[j] >> 4) + ref.tileOffsetX;
                    int y0 = (py_hp[j] >> 4) + ref.tileOffsetY;
                    int x1 = (px_hp[next] >> 4) + ref.tileOffsetX;
                    int y1 = (py_hp[next] >> 4) + ref.tileOffsetY;
                    map.drawLine(x0, y0, x1, y1, outlineColor);
                }
                ringStart = ringEnd;
            }
        }
    }

    // --- Point rendering (IceNav renderNavPoint L1163-1174) ---
    static void renderNavPoint(LGFX_Sprite &map, const FeatureRef &ref,
                               int viewportW, int viewportH) {
        if (ref.coordCount < 1) return;
        const uint8_t* fp = ref.ptr;

        decodedCoords.clear();
        DecodedFeature df;
        if (!decodeFeatureCoords(fp + 13, ref.coordCount, ref.payloadSize, ref.geomType, df)) {
            ESP_LOGW(TAG, "Point decode FAILED: coords=%u payload=%u tile(%d,%d)",
                     ref.coordCount, ref.payloadSize, ref.tileOffsetX, ref.tileOffsetY);
            return;
        }
        int16_t* coords = decodedCoords.data() + df.coordsIdx;

        uint16_t colorRgb565;
        memcpy(&colorRgb565, fp + 1, 2);

        int16_t px = (coords[0] >> 4) + ref.tileOffsetX;
        int16_t py = (coords[1] >> 4) + ref.tileOffsetY;
        if (px >= 0 && px < viewportW && py >= 0 && py < viewportH)
            map.fillCircle(px, py, 3, colorRgb565);
    }

    // --- Feature dispatch (IceNav renderNavFeature L1179-1197) ---
    void renderSingleFeature(LGFX_Sprite &map, const FeatureRef &ref,
                             int viewportW, int viewportH, uint8_t zoom) {
        switch (ref.geomType) {
            case GEOM_POLYGON:
                renderNavPolygon(map, ref, viewportW, viewportH, zoom);
                break;
            case GEOM_LINE:
                renderNavLine(map, ref, viewportW, viewportH, zoom);
                break;
            case GEOM_POINT:
                renderNavPoint(map, ref, viewportW, viewportH);
                break;
        }
    }

    // =========================================================================
    // Streaming NAV rendering for Z9 (and optionally Z10).
    // Processes one tile at a time: load → render geometry → collect labels → free.
    // Peak PSRAM: ~500 KB instead of ~3.6 MB for batch at Z9.
    // Trade-off: loses cross-tile z-ordering (acceptable at Z9, 50 km/tile).
    // Intra-tile z-ordering is preserved (tile generator sorts by priority).
    // =========================================================================
    bool renderNavViewportStreaming(float centerLat, float centerLon, uint8_t zoom,
                                    LGFX_Sprite &map, const char** regions, int regionCount) {
        if (renderLock) xSemaphoreTake(renderLock, portMAX_DELAY);
        renderActive_ = true;

        esp_task_wdt_reset();
        uint64_t startTime = esp_timer_get_time();

        int viewportW = map.width();
        int viewportH = map.height();

        // Compute center tile from lat/lon (Mercator projection)
        const double latRad = (double)centerLat * M_PI / 180.0;
        const double n = pow(2.0, (double)zoom);
        const int centerTileIdxX = (int)floorf((float)((centerLon + 180.0) / 360.0 * n));
        const int centerTileIdxY = (int)floorf((float)((1.0 - log(tan(latRad) + 1.0 / cos(latRad)) / M_PI) / 2.0 * n));

        const int8_t gridOffset = MAP_TILES_GRID / 2;

        // Spiral order: center first, then edges, corners last
        static const int8_t spiralOrder[9][2] = {
            {0,0}, {2,0}, {0,2}, {2,2}, {0,1}, {1,0}, {2,1}, {1,2}, {1,1}
        };

        // Skip regions that don't contain the center tile
        struct ActiveRegion { const char* name; uint8_t origIdx; };
        ActiveRegion activeRegions[NPK_MAX_REGIONS];
        int activeRegionCount = 0;
        if (regionCount > 1) {
            for (int r = 0; r < regionCount && r < NPK_MAX_REGIONS; r++) {
                NpkSlot* slot = openNpkRegion(regions[r], zoom, (uint32_t)centerTileIdxY);
                if (!slot || !slot->yTable) continue;
                UIMapManager::Npk2IndexEntry dummy;
                if (findNpkTileInSlot(slot, (uint32_t)centerTileIdxX, (uint32_t)centerTileIdxY, &dummy))
                    activeRegions[activeRegionCount++] = { regions[r], (uint8_t)r };
            }
            if (activeRegionCount == 0)
                for (int r = 0; r < regionCount && r < NPK_MAX_REGIONS; r++)
                    activeRegions[activeRegionCount++] = { regions[r], (uint8_t)r };
        } else {
            for (int r = 0; r < regionCount && r < NPK_MAX_REGIONS; r++)
                activeRegions[activeRegionCount++] = { regions[r], (uint8_t)r };
        }

        // Pre-evict wrong-zoom and out-of-grid entries
        static constexpr int Z9_CACHE_MAX = 5;
        for (int i = (int)navCache.size() - 1; i >= 0; i--) {
            bool bad = navCache[i].zoom != zoom ||
                       navCache[i].tileX < centerTileIdxX - gridOffset ||
                       navCache[i].tileX > centerTileIdxX + gridOffset ||
                       navCache[i].tileY < centerTileIdxY - gridOffset ||
                       navCache[i].tileY > centerTileIdxY + gridOffset;
            if (bad) {
                if (isNavPoolActive()) releaseNavSlot(navCache[i].data);
                else free(navCache[i].data);
                navCache.erase(navCache.begin() + i);
            }
        }

        // Track tile data for post-render cache decisions
        struct TileSlot { uint8_t* data; size_t size; int tileX, tileY; bool fromCache; };
        TileSlot tileSlots[9] = {};
        int tileSlotCount = 0;

        // Self-contained text labels — data is COPIED because tile buffers are freed
        // between tiles in streaming mode. ~30 bytes per label, ~128 labels max = ~4 KB.
        struct StreamingLabel {
            uint16_t color;
            uint8_t  fontSize;
            int16_t  px, py;          // screen coords (already projected)
            uint8_t  textLen;
            char     text[128];
        };
        std::vector<StreamingLabel, PSRAMAllocator<StreamingLabel>> textLabels;
        textLabels.reserve(128);

        const uint16_t bgColor = map.color565(0x2F, 0x4F, 0x4F);
        map.fillSprite(bgColor);
        map.startWrite();

        struct LabelRect { int16_t x, y, w, h; };
        std::vector<LabelRect> placedLabels;
        placedLabels.reserve(128);

        int totalFeatures = 0;
        int tilesRendered = 0;
        int navCacheHits = 0, navCacheMisses = 0;
        uint64_t lastYieldUs = esp_timer_get_time();

        try {

        // --- Stream tiles one by one ---
        for (int ti = 0; ti < 9; ti++) {
            esp_task_wdt_reset();

            int gx = spiralOrder[ti][0];
            int gy = spiralOrder[ti][1];
            int tileX = centerTileIdxX - gridOffset + gx;
            int tileY = centerTileIdxY - gridOffset + gy;
            int16_t tileOffsetX = (int16_t)(gx * MAP_TILE_SIZE);
            int16_t tileOffsetY = (int16_t)(gy * MAP_TILE_SIZE);

            // Try each region for this tile position
            uint8_t* tileData = nullptr;
            size_t tileSize = 0;
            bool fromCache = false;

            for (int r = 0; r < activeRegionCount && !tileData; r++) {
                uint8_t regionIdx = activeRegions[r].origIdx;
                int cacheIdx = findNavCache(regionIdx, zoom, tileX, tileY);
                if (cacheIdx >= 0) {
                    navCache[cacheIdx].lastAccess = ++navCacheAccessCounter;
                    tileData = navCache[cacheIdx].data;
                    tileSize = navCache[cacheIdx].size;
                    fromCache = true;
                    navCacheHits++;
                } else {
                    NpkSlot* slot = openNpkRegion(activeRegions[r].name, zoom, (uint32_t)tileY);
                    UIMapManager::Npk2IndexEntry entry;
                    if (slot && findNpkTileInSlot(slot, (uint32_t)tileX, (uint32_t)tileY, &entry)) {
                        // Free PSRAM if needed: LRU evict from navCache, then from tileSlots
                        size_t freeBlock = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
                        while ((isNavPoolActive() ? (getAvailableNavSlots() == 0) : (freeBlock < entry.size + 64 * 1024)) && !navCache.empty()) {
                            int lruIdx = 0;
                            uint32_t oldest = UINT32_MAX;
                            for (int ci = 0; ci < (int)navCache.size(); ci++) {
                                if (navCache[ci].lastAccess < oldest) {
                                    oldest = navCache[ci].lastAccess; lruIdx = ci;
                                }
                            }
                            if (isNavPoolActive()) releaseNavSlot(navCache[lruIdx].data);
                            else free(navCache[lruIdx].data);
                            navCache.erase(navCache.begin() + lruIdx);
                            freeBlock = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
                        }
                        for (int si = 0; si < tileSlotCount && (isNavPoolActive() ? (getAvailableNavSlots() == 0) : (freeBlock < entry.size + 64 * 1024)); si++) {
                            if (tileSlots[si].data && !tileSlots[si].fromCache) {
                                if (isNavPoolActive()) releaseNavSlot(tileSlots[si].data);
                                else free(tileSlots[si].data);
                                tileSlots[si].data = nullptr;
                                freeBlock = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
                            }
                        }
                        if (readNpkTileData(slot, &entry, &tileData, &tileSize)) {
                            if (memcmp(tileData, "NAV1", 4) != 0) {
                                ESP_LOGE(TAG, "Z9 tile (%d,%d): invalid NAV1 header", tileX, tileY);
                                if (isNavPoolActive()) releaseNavSlot(tileData);
                                else free(tileData);
                                tileData = nullptr;
                                continue;
                            }
                            navCacheMisses++;
                        }
                    }
                }
            }

            if (!tileData || tileSize < 22 + 13) continue;

            // --- Parse and render features immediately ---
            uint16_t feature_count;
            memcpy(&feature_count, tileData + 4, 2);
            uint8_t* p = tileData + 22;

            for (uint16_t i = 0; i < feature_count; i++) {
                if ((i & 63) == 0) esp_task_wdt_reset();

                // Yield every 20ms
                if ((++totalFeatures & 15) == 0) {
                    uint64_t nowUs = esp_timer_get_time();
                    if (nowUs - lastYieldUs > 20000) {
                        map.endWrite();
                        vTaskDelay(1);
                        map.startWrite();
                        lastYieldUs = esp_timer_get_time();
                    }
                }

                if (p + 13 > tileData + tileSize) break;

                uint8_t geomType = p[0];
                uint8_t zoomPriority = p[3];
                uint16_t coordCount;
                memcpy(&coordCount, p + 9, 2);
                uint16_t payloadSize;
                memcpy(&payloadSize, p + 11, 2);
                if (p + 13 + payloadSize > tileData + tileSize) break;

                uint8_t minZoom = zoomPriority >> 4;
                if (minZoom > zoom) { p += 13 + payloadSize; continue; }

                // Semantic culling Z9: polygons < 3×3px, lines < 2×2px
                uint8_t bx1 = p[5], by1 = p[6], bx2 = p[7], by2 = p[8];
                if (geomType == GEOM_POLYGON) {
                    if ((bx2 - bx1) < 3 && (by2 - by1) < 3) {
                        p += 13 + payloadSize; continue;
                    }
                } else if (geomType == GEOM_LINE) {
                    if ((bx2 - bx1) < 2 && (by2 - by1) < 2) {
                        p += 13 + payloadSize; continue;
                    }
                }

                FeatureRef ref;
                ref.ptr = p;
                ref.geomType = geomType;
                ref.payloadSize = payloadSize;
                ref.coordCount = coordCount;
                ref.tileOffsetX = tileOffsetX;
                ref.tileOffsetY = tileOffsetY;

                if (geomType == GEOM_TEXT) {
                    // Copy label data — tile buffer will be freed after this tile
                    int16_t* coords = (int16_t*)(p + 13);
                    uint8_t textLen = *(p + 13 + 4);
                    if (textLen > 0 && textLen < 128) {
                        StreamingLabel lbl;
                        memcpy(&lbl.color, p + 1, 2);
                        lbl.fontSize = p[4];
                        lbl.px = (coords[0] >> 4) + tileOffsetX;
                        lbl.py = (coords[1] >> 4) + tileOffsetY;
                        lbl.textLen = textLen;
                        memcpy(lbl.text, p + 13 + 5, textLen);
                        lbl.text[textLen] = '\0';
                        textLabels.push_back(lbl);
                    }
                } else if (geomType == GEOM_TEXT_LINE) {
                    // No waterway labels at Z9 (zoom < 15)
                } else {
                    // Render geometry immediately — no globalLayers needed
                    renderSingleFeature(map, ref, viewportW, viewportH, zoom);
                }

                p += 13 + payloadSize;
            }

            tilesRendered++;

            // Store tile pointer for post-render cache decision
            if (tileSlotCount < 9) {
                tileSlots[tileSlotCount++] = { tileData, tileSize, tileX, tileY, fromCache };
            } else if (!fromCache) {
                if (isNavPoolActive()) releaseNavSlot(tileData);
                else free(tileData);
            }
        }

        } catch (const std::bad_alloc&) {
            ESP_LOGW(TAG, "Z9 streaming: PSRAM exhausted during render, partial output");
        }
        map.endWrite();

        // --- Label pass: dedup + sort by priority ---
        // Labels appear in multiple tiles — deduplicate by text+position (±2px)
        // then sort by fontSize descending so important labels win collision.
        std::sort(textLabels.begin(), textLabels.end(),
                  [](const StreamingLabel& a, const StreamingLabel& b) {
                      if (a.fontSize != b.fontSize) return a.fontSize > b.fontSize;
                      return strcmp(a.text, b.text) < 0;
                  });
        {
            int out = 0;
            for (int i = 0; i < (int)textLabels.size(); i++) {
                bool dup = false;
                for (int j = 0; j < out; j++) {
                    if (strcmp(textLabels[i].text, textLabels[j].text) == 0 &&
                        abs(textLabels[i].px - textLabels[j].px) <= 2 &&
                        abs(textLabels[i].py - textLabels[j].py) <= 2) {
                        dup = true; break;
                    }
                }
                if (!dup) {
                    if (out != i) textLabels[out] = textLabels[i];
                    out++;
                }
            }
            textLabels.resize(out);
        }

        map.startWrite();
        map.clearClipRect();
        if (vlwFontLoaded) {
            map.setFont(&vlwFont);
            map.setTextSize(1.0f);
        } else {
            map.setFont((lgfx::GFXfont*)&OpenSans_Bold6pt7b);
            map.setTextSize(1);
        }

        for (const auto& lbl : textLabels) {
            if ((++totalFeatures & 31) == 0) esp_task_wdt_reset();

            if (vlwFontLoaded) {
                float scale = (lbl.fontSize == 0) ? 0.65f : (lbl.fontSize == 1) ? 0.8f : 1.0f;
                map.setTextSize(scale);
            }

            int tw = map.textWidth(lbl.text);
            int th = map.fontHeight();
            int lx = lbl.px - tw / 2;
            int ly = lbl.py - th;
            const int PAD = (zoom <= 9) ? 2 : 4;
            if (lx + tw < 0 || lx >= viewportW || ly + th < 0 || ly >= viewportH) continue;
            bool collision = false;
            for (const auto& r : placedLabels) {
                if (lx - PAD < r.x + r.w && lx + tw + PAD > r.x &&
                    ly - PAD < r.y + r.h && ly + th + PAD > r.y) {
                    collision = true; break;
                }
            }
            if (collision) continue;
            map.setTextColor(lbl.color);
            map.setTextDatum(lgfx::top_center);
            map.drawString(lbl.text, lbl.px, ly);
            map.setTextDatum(lgfx::top_left);
            placedLabels.push_back({(int16_t)lx, (int16_t)ly, (int16_t)tw, (int16_t)th});
        }
        map.endWrite();

        // --- Post-render cache: keep the Z9_CACHE_MAX tiles closest to center ---
        {
            int cached = 0;
            for (int i = 0; i < tileSlotCount; i++)
                if (tileSlots[i].fromCache) cached++;

            for (int dist = 0; dist <= 4 && cached < Z9_CACHE_MAX; dist++) {
                for (int i = 0; i < tileSlotCount && cached < Z9_CACHE_MAX; i++) {
                    if (tileSlots[i].fromCache || !tileSlots[i].data) continue;
                    int d = abs(tileSlots[i].tileX - centerTileIdxX) +
                            abs(tileSlots[i].tileY - centerTileIdxY);
                    if (d != dist) continue;

                    while ((int)navCache.size() >= Z9_CACHE_MAX) {
                        int lru = 0; uint32_t oldest = UINT32_MAX;
                        for (int ci = 0; ci < (int)navCache.size(); ci++)
                            if (navCache[ci].lastAccess < oldest) { oldest = navCache[ci].lastAccess; lru = ci; }
                        if (isNavPoolActive()) releaseNavSlot(navCache[lru].data);
                        else free(navCache[lru].data);
                        navCache.erase(navCache.begin() + lru);
                    }
                    addNavCache(activeRegions[0].origIdx, zoom,
                                tileSlots[i].tileX, tileSlots[i].tileY,
                                tileSlots[i].data, tileSlots[i].size);
                    tileSlots[i].data = nullptr;
                    cached++;
                }
            }
            for (int i = 0; i < tileSlotCount; i++)
                if (tileSlots[i].data && !tileSlots[i].fromCache) {
                    if (isNavPoolActive()) releaseNavSlot(tileSlots[i].data);
                    else free(tileSlots[i].data);
                    tileSlots[i].data = nullptr;
                }
        }

        uint64_t endTime = esp_timer_get_time();
        ESP_LOGI(TAG, "Z9 streaming: %llu ms, %d tiles, %d features, cache: %d hit / %d miss, kept: %d, PSRAM free: %u",
                      (endTime - startTime) / 1000, tilesRendered, totalFeatures,
                      navCacheHits, navCacheMisses, (int)navCache.size(), ESP.getFreePsram());

        bool result = (tilesRendered > 0);
        renderActive_ = false;
        if (renderLock) xSemaphoreGive(renderLock);
        return result;
    }

    // =========================================================================
    // Batch NAV rendering for Z10+.
    // Loads ALL visible tiles, dispatches features to 16 priority layers,
    // renders in a single pass with per-feature setClipRect to tile boundaries.
    // This ensures correct z-ordering across tile boundaries.
    // =========================================================================
    bool renderNavViewport(float centerLat, float centerLon, uint8_t zoom,
                           LGFX_Sprite &map, const char** regions, int regionCount) {
        // Z9: use streaming pipeline (one tile at a time) to stay within PSRAM budget
        if (zoom <= 9)
            return renderNavViewportStreaming(centerLat, centerLon, zoom, map, regions, regionCount);

        // Acquire render lock — prevents clearTileCache/closeAllNpkSlots during render
        if (renderLock) xSemaphoreTake(renderLock, portMAX_DELAY);
        renderActive_ = true;

        esp_task_wdt_reset();
        uint64_t startTime = esp_timer_get_time();

        int viewportW = map.width();
        int viewportH = map.height();

        // Compute center tile from lat/lon (Mercator projection)
        const double latRad = (double)centerLat * M_PI / 180.0;
        const double n = pow(2.0, (double)zoom);
        const int centerTileIdxX = (int)floorf((float)((centerLon + 180.0) / 360.0 * n));
        const int centerTileIdxY = (int)floorf((float)((1.0 - log(tan(latRad) + 1.0 / cos(latRad)) / M_PI) / 2.0 * n));

        // Fixed 3×3 grid: tiles at positions {0, 256, 512} in the sprite
        const int8_t gridOffset = MAP_TILES_GRID / 2;  // 1

        // --- Load all tiles and dispatch features ---
        struct ResolvedTile {
            uint8_t* data;
            size_t   size;
            int16_t  tileOffsetX, tileOffsetY;
        };
        static ResolvedTile resolved[9];  // 3×3 fixed grid
        int resolvedCount = 0;
        static std::vector<uint8_t*> inUseData;  // Protects resolved tiles from eviction
        inUseData.clear();
        int navCacheHits = 0, navCacheMisses = 0;
        for (int i = 0; i < 16; i++) globalLayers[i].clear();
        // One-time PSRAM reserve (IceNav L57): 1024 per layer, no per-render counting
        static bool layersReserved = false;
        if (!layersReserved) {
            for (int i = 0; i < 16; i++) globalLayers[i].reserve(1024);
            layersReserved = true;
        }
        // Text labels collected separately — rendered last, on top of all geometry
        std::vector<FeatureRef, PSRAMAllocator<FeatureRef>> textRefs;
        textRefs.reserve(128);

        // Waterway curvilinear labels (GEOM_TEXT_LINE) — rendered in a dedicated pass
        std::vector<FeatureRef, PSRAMAllocator<FeatureRef>> waterwayRefs;
        waterwayRefs.reserve(64);

        const uint16_t bgColor = map.color565(0x2F, 0x4F, 0x4F);  // Fixed neutral bg for areas without tile data

        // Fixed 3×3 spiral order: center first, then edges, corners last
        static const int8_t spiralOrder[9][2] = {
            {0,0}, {2,0}, {0,2}, {2,2}, {0,1}, {1,0}, {2,1}, {1,2}, {1,1}
        };
        const int tileCount = 9;

        // Pre-evict navCache entries from a different zoom level (useless after zoom change).
        // Grid-based eviction removed: nearby tiles are kept for cache hits on next pan.
        // Phase 3 eviction (LRU) handles PSRAM pressure when loading new tiles.
        {
            int preEvicted = 0;
            for (int i = (int)navCache.size() - 1; i >= 0; i--) {
                if (navCache[i].zoom != zoom) {
                    if (isNavPoolActive()) releaseNavSlot(navCache[i].data);
                    else free(navCache[i].data);
                    navCache.erase(navCache.begin() + i);
                    preEvicted++;
                }
            }
            if (preEvicted > 0) {
                ESP_LOGD(TAG, "Pre-evicted %d wrong-zoom entries, free: %u KB, largest: %u KB",
                              preEvicted, (unsigned)(ESP.getFreePsram() / 1024),
                              (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024));
            }
        }

        // Skip regions that don't contain the center tile.
        // Preserves original region index for stable navCache keys.
        struct ActiveRegion {
            const char* name;
            uint8_t     origIdx;  // index in regions[] — used as navCache key
        };
        ActiveRegion activeRegions[NPK_MAX_REGIONS];
        int activeRegionCount = 0;

        if (regionCount > 1) {
            // Check if the center tile actually exists in each region
            for (int r = 0; r < regionCount && r < NPK_MAX_REGIONS; r++) {
                NpkSlot* slot = openNpkRegion(regions[r], zoom, (uint32_t)centerTileIdxY);
                if (!slot || !slot->yTable) continue;
                UIMapManager::Npk2IndexEntry dummy;
                if (findNpkTileInSlot(slot, (uint32_t)centerTileIdxX, (uint32_t)centerTileIdxY, &dummy)) {
                    activeRegions[activeRegionCount++] = { regions[r], (uint8_t)r };
                }
            }
            if (activeRegionCount == 0) {
                for (int r = 0; r < regionCount && r < NPK_MAX_REGIONS; r++)
                    activeRegions[activeRegionCount++] = { regions[r], (uint8_t)r };
            }
        } else {
            for (int r = 0; r < regionCount && r < NPK_MAX_REGIONS; r++)
                activeRegions[activeRegionCount++] = { regions[r], (uint8_t)r };
        }

        // --- Phase 1: Resolve navCache hits + collect pending SD reads ---
        // Pending reads are sorted by file offset before reading to minimize
        // SD seek latency (critical for non-A1 cards).
        struct PendingTileRead {
            NpkSlot* slot;
            UIMapManager::Npk2IndexEntry entry;
            int16_t  tileOffsetX, tileOffsetY;
            int      tileX, tileY;
            uint8_t  regionIdx;
        };
        static PendingTileRead pendingReads[18];  // 3×3 grid × 2 regions max
        int pendingCount = 0;

        // ================================================================
        // Phase 1 — LOAD: resolve all tiles before any feature dispatch.
        // Cache hits are recorded; misses are read from SD.
        // No FeatureRef exists yet, so eviction is safe.
        // ================================================================
        for (int ti = 0; ti < tileCount; ti++) {
            int gx = spiralOrder[ti][0];
            int gy = spiralOrder[ti][1];
            int tileX = centerTileIdxX - gridOffset + gx;
            int tileY = centerTileIdxY - gridOffset + gy;
            int16_t tileOffsetX = (int16_t)(gx * MAP_TILE_SIZE);  // 0, 256, 512
            int16_t tileOffsetY = (int16_t)(gy * MAP_TILE_SIZE);

            for (int r = 0; r < activeRegionCount; r++) {
                uint8_t regionIdx = activeRegions[r].origIdx;
                int cacheIdx = findNavCache(regionIdx, zoom, tileX, tileY);
                if (cacheIdx >= 0) {
                    navCache[cacheIdx].lastAccess = ++navCacheAccessCounter;
                    navCacheHits++;
                    if (resolvedCount < 9) {
                        resolved[resolvedCount++] = {
                            navCache[cacheIdx].data, navCache[cacheIdx].size,
                            tileOffsetX, tileOffsetY
                        };
                        inUseData.push_back(navCache[cacheIdx].data);
                    }
                } else {
                    NpkSlot* slot = openNpkRegion(activeRegions[r].name, zoom, (uint32_t)tileY);
                    UIMapManager::Npk2IndexEntry entry;
                    if (slot && findNpkTileInSlot(slot, (uint32_t)tileX, (uint32_t)tileY, &entry)
                        && pendingCount < 18) {
                        pendingReads[pendingCount++] = {
                            slot, entry, tileOffsetX, tileOffsetY,
                            tileX, tileY, regionIdx
                        };
                    }
                }
            }
        }

        // Sort pending reads: by file offset if all fit, center-outward otherwise
        size_t totalPendingSize = 0;
        for (int i = 0; i < pendingCount; i++)
            totalPendingSize += pendingReads[i].entry.size;

        bool hasCapacity = isNavPoolActive() ? (getAvailableNavSlots() >= pendingCount) : (totalPendingSize <= heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));

        if (hasCapacity) {
            std::sort(pendingReads, pendingReads + pendingCount,
                      [](const PendingTileRead& a, const PendingTileRead& b) {
                          if (a.slot != b.slot) return a.slot < b.slot;
                          return a.entry.offset < b.entry.offset;
                      });
        } else {
            std::sort(pendingReads, pendingReads + pendingCount,
                      [centerTileIdxX, centerTileIdxY](const PendingTileRead& a, const PendingTileRead& b) {
                          int adx = a.tileX - centerTileIdxX, ady = a.tileY - centerTileIdxY;
                          int bdx = b.tileX - centerTileIdxX, bdy = b.tileY - centerTileIdxY;
                          return (adx*adx + ady*ady) < (bdx*bdx + bdy*bdy);
                      });
        }

        // Proactive eviction: free PSRAM before loading new tiles
        static constexpr size_t PSRAM_RENDER_MARGIN = 128 * 1024;  // Margin for render vectors growth
        if (pendingCount > 0) {
            size_t freeBlock = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
            size_t largestTile = 0;
            for (int i = 0; i < pendingCount; i++)
                if (pendingReads[i].entry.size > largestTile) largestTile = pendingReads[i].entry.size;
            if (isNavPoolActive()) {
                if (getAvailableNavSlots() < pendingCount) {
                    evictUnusedNavCache(inUseData, largestTile);
                }
            } else {
                if (freeBlock < largestTile + PSRAM_RENDER_MARGIN) {
                    evictUnusedNavCache(inUseData, largestTile + PSRAM_RENDER_MARGIN);
                }
            }
        }

        // Read pending tiles from SD into navCache (IceNav pattern: alloc, evict+retry, skip)
        for (int i = 0; i < pendingCount; i++) {
            esp_task_wdt_reset();

            auto& pr = pendingReads[i];
            uint8_t* data = nullptr;
            size_t fileSize = 0;

            if (!readNpkTileData(pr.slot, &pr.entry, &data, &fileSize)) {
                // Alloc failed — evict unpinned cache entries and retry
                if (evictUnusedNavCache(inUseData, pr.entry.size)) {
                    readNpkTileData(pr.slot, &pr.entry, &data, &fileSize);
                }
                if (!data) {
                    ESP_LOGW(TAG, "Tile (%d,%d) skipped: %u KB needed, largest block %u KB",
                             pr.tileX, pr.tileY, (unsigned)(pr.entry.size / 1024),
                             (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024));
                    continue;
                }
            }
            if (!data) continue;

            if (memcmp(data, "NAV1", 4) != 0) {
                ESP_LOGE(TAG, "Tile %d/%d: invalid header", pr.tileX, pr.tileY);
                if (isNavPoolActive()) releaseNavSlot(data);
                else free(data);
                continue;
            }

            addNavCache(pr.regionIdx, zoom, pr.tileX, pr.tileY, data, fileSize, &inUseData);
            navCacheMisses++;

            if (resolvedCount < 9) {
                resolved[resolvedCount++] = { data, fileSize, pr.tileOffsetX, pr.tileOffsetY };
                inUseData.push_back(data);
            }
        }

        uint64_t loadEnd = esp_timer_get_time();

        // ================================================================
        // Phase 2 — DISPATCH: single pass, like IceNav renderNavTile.
        // Layers pre-reserved at 1024 (one-time), realloc if exceeded.
        // Cap at 20000 features to keep render time < 8s on dense tiles.
        // ================================================================
        static constexpr uint32_t MAX_FEATURE_POOL_SIZE = 20000;
        uint32_t totalDispatchedFeatures = 0;
        for (int t = 0; t < resolvedCount; t++) {
            uint8_t* data = resolved[t].data;
            size_t fileSize = resolved[t].size;
            int16_t tileOffsetX = resolved[t].tileOffsetX;
            int16_t tileOffsetY = resolved[t].tileOffsetY;

            uint16_t feature_count;
            memcpy(&feature_count, data + 4, 2);
            uint8_t* p = data + 22;

            bool psramExhausted = false;

            for (uint16_t i = 0; i < feature_count && !psramExhausted && totalDispatchedFeatures < MAX_FEATURE_POOL_SIZE; i++) {
                if ((i & 63) == 0) esp_task_wdt_reset();
                if (p + 13 > data + fileSize) break;

                uint8_t geomType = p[0];
                uint8_t zoomPriority = p[3];
                uint16_t coordCount;
                memcpy(&coordCount, p + 9, 2);
                uint16_t payloadSize;
                memcpy(&payloadSize, p + 11, 2);
                if (p + 13 + payloadSize > data + fileSize) break;

                uint8_t minZoom = zoomPriority >> 4;
                if (minZoom > zoom) { p += 13 + payloadSize; continue; }

                // Semantic culling: polygons < 3×3px (Z9-Z11) or < 1×1px, lines < 2×2px always
                uint8_t bx1 = p[5], by1 = p[6], bx2 = p[7], by2 = p[8];
                if (geomType == GEOM_POLYGON) {
                    uint8_t minDim = (zoom >= 9 && zoom <= 11) ? 3 : 1;
                    if ((bx2 - bx1) < minDim && (by2 - by1) < minDim) {
                        p += 13 + payloadSize; continue;
                    }
                } else if (geomType == GEOM_LINE) {
                    if ((bx2 - bx1) < 2 && (by2 - by1) < 2) {
                        p += 13 + payloadSize; continue;
                    }
                }

                uint8_t priority = zoomPriority & 0x0F;
                if (priority >= 16) priority = 15;

                FeatureRef ref;
                ref.ptr = p;
                ref.geomType = geomType;
                ref.payloadSize = payloadSize;
                ref.coordCount = coordCount;
                ref.tileOffsetX = tileOffsetX;
                ref.tileOffsetY = tileOffsetY;

                try {
                    if (geomType == GEOM_TEXT) {
                        textRefs.push_back(ref);
                    } else if (geomType == GEOM_TEXT_LINE) {
                        if (zoom >= 15) waterwayRefs.push_back(ref);
                    } else {
                        globalLayers[priority].push_back(ref);
                    }
                    totalDispatchedFeatures++;
                } catch (const std::bad_alloc&) {
                    ESP_LOGW(TAG, "PSRAM exhausted at feature %u/%u, rendering partial",
                             i, feature_count);
                    psramExhausted = true;
                }

                p += 13 + payloadSize;
            }
        }

        int totalFeatures = 0;
        for (int i = 0; i < 16; i++) totalFeatures += globalLayers[i].size();
        totalFeatures += textRefs.size() + waterwayRefs.size();
        if (totalDispatchedFeatures >= MAX_FEATURE_POOL_SIZE) {
            ESP_LOGW(TAG, "Feature pool capped at %u (limit %u), some features skipped",
                          totalDispatchedFeatures, MAX_FEATURE_POOL_SIZE);
        }
        ESP_LOGD(TAG, "Load: %llu ms, tiles: %d, features: %d, grid: 3x3 fixed",
                      (loadEnd - startTime) / 1000, resolvedCount, totalFeatures);

        // --- Render all layers ---
        // Fill background with color from NAV background polygon
        map.fillSprite(bgColor);

        map.startWrite();
        try {

        struct LabelRect { int16_t x, y, w, h; };
        std::vector<LabelRect> placedLabels;
        placedLabels.reserve(128);

        int featureCount = 0;
        uint64_t lastYieldUs = esp_timer_get_time();
        for (int pri = 0; pri < 16; pri++) {
            if (globalLayers[pri].empty()) continue;

            for (const auto& ref : globalLayers[pri]) {
                // Yield every 20ms to let WiFi/BLE breathe on Core 0
                if ((++featureCount & 15) == 0) {
                    uint64_t nowUs = esp_timer_get_time();
                    if (nowUs - lastYieldUs > 20000) {
                        map.endWrite();
                        vTaskDelay(1);
                        map.startWrite();
                        lastYieldUs = esp_timer_get_time();
                    }
                }

                renderSingleFeature(map, ref, viewportW, viewportH, zoom);
            }
            globalLayers[pri].clear();
            esp_task_wdt_reset();
        }

        // Label pass — rendered after all geometry so labels appear on top
        map.clearClipRect();
        map.setFont((lgfx::GFXfont*)&OpenSans_Bold6pt7b);
        map.setTextSize(1);
        // Set VLW Unicode font for text labels (must be before textWidth/drawString)
        if (vlwFontLoaded) {
            map.setFont(&vlwFont);
            map.setTextSize(1.0f);
        } else {
            map.setFont((lgfx::GFXfont*)&OpenSans_Bold6pt7b);
            map.setTextSize(1);
        }

        for (const auto& ref : textRefs) {
            if ((++featureCount & 31) == 0) esp_task_wdt_reset();
            uint8_t* fp = ref.ptr;
            uint16_t colorRgb565;
            memcpy(&colorRgb565, fp + 1, 2);
            int16_t* coords = (int16_t*)(fp + 13);  // Text payload is raw int16, NOT VarInt
            int px = (coords[0] >> 4) + ref.tileOffsetX;
            int py = (coords[1] >> 4) + ref.tileOffsetY;
            uint8_t fontSize = fp[4];
            uint8_t textLen = *(fp + 13 + 4);
            if (textLen == 0 || textLen >= 128) continue;
            char textBuf[128];
            memcpy(textBuf, fp + 13 + 5, textLen);
            textBuf[textLen] = '\0';

            // Scale VLW font based on fontSize (0=small, 1=medium, 2=large)
            if (vlwFontLoaded) {
                float scale = (fontSize == 0) ? 0.65f : (fontSize == 1) ? 0.8f : 1.0f;
                map.setTextSize(scale);
            }

            int tw = map.textWidth(textBuf);
            int th = map.fontHeight();
            int lx = px - tw / 2;
            int ly = py - th;
            const int PAD = 4;
            if (lx + tw < 0 || lx >= viewportW || ly + th < 0 || ly >= viewportH) continue;
            bool collision = false;
            for (const auto& r : placedLabels) {
                if (lx - PAD < r.x + r.w && lx + tw + PAD > r.x &&
                    ly - PAD < r.y + r.h && ly + th + PAD > r.y) {
                    collision = true; break;
                }
            }
            if (collision) continue;
            map.setTextColor(colorRgb565);
            map.setTextDatum(lgfx::top_center);
            map.drawString(textBuf, px, ly);
            map.setTextDatum(lgfx::top_left);
            placedLabels.push_back({(int16_t)lx, (int16_t)ly, (int16_t)tw, (int16_t)th});
        }
        textRefs.clear();

        // ---- Waterway curvilinear label pass (GEOM_TEXT_LINE) ----
        // Text follows the waterway path geometry (text-along-path, OSM style).
        // Payload: uint8_t path_count | [int16_t px, int16_t py] x N | uint8_t text_len | uint8_t[text_len] text
        map.clearClipRect();
        // Waterway labels use bitmap font (no anti-aliasing) for clean
        // color-key transparency with pushRotateZoom
        map.setFont((lgfx::GFXfont*)&OpenSans_Bold6pt7b);
        map.setTextSize(1);

        // Ensure glyph sprite exists (reused across all characters)
        int maxGlyphH = map.fontHeight() + 2;
        int maxGlyphW = maxGlyphH * 2;  // widest glyph won't exceed 2× height
        if (glyphSprite && (glyphSpriteW < maxGlyphW || glyphSpriteH < maxGlyphH)) {
            glyphSprite->deleteSprite(); delete glyphSprite;
            glyphSprite = nullptr;
        }
        if (!glyphSprite) {
            glyphSprite = new LGFX_Sprite(&map);
            glyphSprite->setColorDepth(16);
            glyphSprite->setPsram(true);
            if (glyphSprite->createSprite(maxGlyphW, maxGlyphH)) {
                glyphSpriteW = maxGlyphW;
                glyphSpriteH = maxGlyphH;
            } else {
                delete glyphSprite; glyphSprite = nullptr;
                glyphSpriteW = glyphSpriteH = 0;
            }
        }

        for (const auto& ref : waterwayRefs) {
            if (!wlScreenX || !wlScreenY || !wlArcLen) break;  // PSRAM alloc failed
            if ((++featureCount & 31) == 0) esp_task_wdt_reset();

            uint8_t* fp = ref.ptr;
            uint16_t colorRgb565;
            memcpy(&colorRgb565, fp + 1, 2);
            uint8_t fontSize = fp[4];

            // --- Parse GEOM_TEXT_LINE payload (raw int16, NOT VarInt) ---
            const uint8_t* payload = fp + 13;
            if (ref.payloadSize < 2) continue;

            uint8_t pathCount = payload[0];
            if (pathCount < 2) continue;
            const uint8_t pathCountOrig = pathCount;
            if (pathCount > WLABEL_MAX_PTS - 1) pathCount = WLABEL_MAX_PTS - 1;

            size_t minSize = 1u + (size_t)pathCountOrig * 4u + 1u;
            if (ref.payloadSize < minSize) continue;

            const int16_t* rawPts = (const int16_t*)(payload + 1);

            // Project to screen pixels (buffers in PSRAM)
            for (uint8_t j = 0; j < pathCount; j++) {
                int16_t cx = rawPts[j * 2];
                int16_t cy = rawPts[j * 2 + 1];
                wlScreenX[j] = (int)(cx * MAP_TILE_SIZE / 4096) + ref.tileOffsetX;
                wlScreenY[j] = (int)(cy * MAP_TILE_SIZE / 4096) + ref.tileOffsetY;
            }

            // Read text — use pathCountOrig to skip ALL path points in payload
            const uint8_t* afterPath = payload + 1 + (size_t)pathCountOrig * 4;
            uint8_t textLen = afterPath[0];
            if (textLen == 0 || textLen >= 128) continue;
            if (ref.payloadSize < minSize + textLen) continue;

            char textBuf[128];
            memcpy(textBuf, afterPath + 1, textLen);
            textBuf[textLen] = '\0';

            // Bitmap font — no scaling needed

            // RTL detection: if path goes right-to-left, reverse the path
            // (not the text) so characters face left-to-right
            float firstDx = (float)(wlScreenX[1] - wlScreenX[0]);
            bool reversePath = (firstDx < 0.0f);
            if (reversePath) {
                for (int i = 0, j = pathCount - 1; i < j; i++, j--) {
                    int tx = wlScreenX[i]; wlScreenX[i] = wlScreenX[j]; wlScreenX[j] = tx;
                    int ty = wlScreenY[i]; wlScreenY[i] = wlScreenY[j]; wlScreenY[j] = ty;
                }
            }

            ESP_LOGD(TAG, "WLABEL '%s' pts=%d fontSize=%d reversePath=%d",
                     textBuf, pathCount, fontSize, (int)reversePath);

            // Cumulative arc length (buffers in PSRAM)
            wlArcLen[0] = 0.0f;
            for (uint8_t j = 1; j < pathCount; j++) {
                float dx = (float)(wlScreenX[j] - wlScreenX[j - 1]);
                float dy = (float)(wlScreenY[j] - wlScreenY[j - 1]);
                wlArcLen[j] = wlArcLen[j - 1] + sqrtf(dx * dx + dy * dy);
            }
            float totalLen = wlArcLen[pathCount - 1];
            if (totalLen < 4.0f) continue;

            int textWidth = map.textWidth(textBuf);
            int charH = map.fontHeight();

            // Skip if text wider than path
            if ((float)textWidth > totalLen) continue;

            // Visibility: bounding box of path
            int minSX = wlScreenX[0], maxSX = wlScreenX[0];
            int minSY = wlScreenY[0], maxSY = wlScreenY[0];
            for (uint8_t j = 1; j < pathCount; j++) {
                if (wlScreenX[j] < minSX) minSX = wlScreenX[j];
                if (wlScreenX[j] > maxSX) maxSX = wlScreenX[j];
                if (wlScreenY[j] < minSY) minSY = wlScreenY[j];
                if (wlScreenY[j] > maxSY) maxSY = wlScreenY[j];
            }
            if (maxSX < 0 || minSX >= viewportW || maxSY < 0 || minSY >= viewportH) continue;

            // Collision detection
            const int PAD = 4;
            bool collision = false;
            for (const auto& r : placedLabels) {
                if (minSX - PAD < r.x + r.w && maxSX + PAD > r.x &&
                    minSY - PAD < r.y + r.h && maxSY + PAD > r.y) {
                    collision = true;
                    break;
                }
            }
            if (collision) continue;

            placedLabels.push_back({(int16_t)minSX, (int16_t)minSY,
                                    (int16_t)(maxSX - minSX), (int16_t)(maxSY - minSY)});

            // Center text on path
            float startDist = (totalLen - (float)textWidth) / 2.0f;
            if (startDist < 0.0f) startDist = 0.0f;

            map.setTextColor(colorRgb565);
            int seg = 0;
            float charDist = startDist;

            // Iterate by UTF-8 codepoints
            for (int ci = 0; textBuf[ci] != '\0'; ) {
                // Extract one UTF-8 codepoint
                uint8_t c0 = (uint8_t)textBuf[ci];
                int cpLen;
                if      (c0 < 0x80) cpLen = 1;
                else if (c0 < 0xE0) cpLen = 2;
                else if (c0 < 0xF0) cpLen = 3;
                else                cpLen = 4;

                char chBuf[5];
                memcpy(chBuf, textBuf + ci, cpLen);
                chBuf[cpLen] = '\0';
                ci += cpLen;

                int charW = map.textWidth(chBuf);

                // Advance along path
                while (seg < pathCount - 2 && wlArcLen[seg + 1] < charDist)
                    seg++;

                float segLen = wlArcLen[seg + 1] - wlArcLen[seg];
                float t = (segLen > 0.001f) ? (charDist - wlArcLen[seg]) / segLen : 0.0f;

                float px = wlScreenX[seg] + t * (wlScreenX[seg + 1] - wlScreenX[seg]);
                float py = wlScreenY[seg] + t * (wlScreenY[seg + 1] - wlScreenY[seg]);

                if (px >= -charW && px < viewportW + charW && py >= -charH && py < viewportH + charH) {
                    float dx = (float)(wlScreenX[seg + 1] - wlScreenX[seg]);
                    float dy = (float)(wlScreenY[seg + 1] - wlScreenY[seg]);
                    float angleDeg = atan2f(dy, dx) * (180.0f / (float)M_PI);

                    if (glyphSprite) {
                        // Reuse persistent glyph sprite — draw character centered
                        // so pushRotateZoom pivot (sprite center) aligns with glyph center
                        glyphSprite->fillSprite(0xF81F);
                        glyphSprite->setFont((lgfx::GFXfont*)&OpenSans_Bold6pt7b);
                        glyphSprite->setTextDatum(lgfx::middle_center);
                        glyphSprite->setTextColor(colorRgb565);
                        glyphSprite->drawString(chBuf, glyphSpriteW / 2, glyphSpriteH / 2);
                        glyphSprite->pushRotateZoom(&map,
                            (int)px, (int)py, angleDeg, 1.0f, 1.0f, 0xF81F);
                    } else {
                        // Fallback: draw upright
                        map.setTextDatum(lgfx::top_left);
                        map.drawString(chBuf, (int)px, (int)py - charH / 2);
                    }
                }

                charDist += (float)charW;
            }
        }
        waterwayRefs.clear();

        } catch (const std::bad_alloc&) {
            ESP_LOGW(TAG, "PSRAM exhausted during render, partial output");
        }
        map.endWrite();

        // Tile buffers are now owned by navCache — do NOT free here

        uint64_t endTime = esp_timer_get_time();

        ESP_LOGD(TAG, "Viewport: %llu ms (load %llu ms), %d features, cache: %d hit / %d miss, PSRAM free: %u",
                      (endTime - startTime) / 1000, (loadEnd - startTime) / 1000,
                      totalFeatures, navCacheHits, navCacheMisses, ESP.getFreePsram());

        bool result = (resolvedCount > 0);

        // Release render lock + process deferred operations
        renderActive_ = false;
        if (renderLock) xSemaphoreGive(renderLock);

        return result;
    }

} // namespace MapEngine
#endif
