# Plan de refactoring `ui_map_manager.cpp` (2247 lignes -> 4+1 fichiers)

## Objectif

Rendre le code carte lisible, KISS, maintenable par un dev externe.
Chaque fichier = une responsabilite. Zero circularite.

## Fichiers resultants

| Fichier | Lignes | Responsabilite |
|---------|--------|----------------|
| `map_state.h` + `.cpp` | ~180 | Etat partage (variables extern) |
| `map_tiles.cpp` + `.h` | ~530 | Chargement tuiles, PNG, decouverte regions, preload async |
| `map_render.cpp` + `.h` | ~575 | Rendu : stations, traces, overlays, sprite copy |
| `map_input.cpp` + `.h` | ~375 | Touch, scroll, pan, zoom handlers |
| `ui_map_manager.cpp` | ~500 | Glue : create_screen, timer, redraw_map_canvas |

## Graphe de dependances (sens unique, zero circularite)

```
map_state.h          (etat pur, zero logique)
    ^   ^   ^   ^
    |   |   |   |
    |   |   |   +-- ui_map_manager.cpp  (glue : create_screen, timer, redraw)
    |   |   +------ map_input.cpp       (touch, zoom, scroll)
    |   +---------- map_render.cpp      (dessin stations, traces, viewport)
    +-------------- map_tiles.cpp       (SD, PNG, regions, preload)

Appels inter-modules :
  map_input  -->  ui_map_manager (redraw_map_canvas)
  ui_map_manager --> map_render (applyRenderedViewport, refreshStationOverlay)
  ui_map_manager --> map_tiles  (discoverNavRegions, loadTileFromSD)
  map_render --> map_tiles      (getSymbolCacheEntry pour symboles)
```

## Etape 1 : Extraire l'etat — `map_state.h` + `map_state.cpp`

**Quoi :** Toutes les ~50 variables static du namespace `UIMapManager` migrent vers `MapState::`. Les fonctions restent dans `ui_map_manager.cpp`. Chaque .cpp fait `using namespace MapState;` pour eviter le prefixe.

**Variables concernees :**
- LVGL widgets : screen_map, map_canvas, map_canvas_buf, btn_zoomin/out/recenter, bars, labels
- Position/zoom : map_center_lat/lon, map_current_zoom, map_zoom_index, centerTileX/Y, renderTileX/Y
- Pan : offsetX/Y, navSubTileX/Y, velocityX/Y, isScrollingMap, dragStarted, map_follow_gps
- Render : redraw_in_progress, navModeActive, navRenderPending, mainThreadLoading, back/frontViewportSprite
- Regions : map_current_region, navRegions[], navRegionCount, defaultLat/Lon
- GPS : gpsFilter, pendingResetPan
- Hit zones : stationHitZones[], stationHitZoneCount

**Constantes** (restent dans le header comme constexpr) :
- nav_zooms[], raster_zooms[], PAN_FRICTION, START_THRESHOLD, PAN_TILE_THRESHOLD

**Les variables locales a une fonction** (`gpsUpdateCounter`, `refreshCounter`, `firstTapTime`, etc.) restent en `static` local dans leur fonction. Pas dans map_state.

**Validation :** Compile ok. Aucun changement fonctionnel.

## Etape 2 : Extraire `map_tiles.cpp`

**Fonctions :**
- `loadTileFromSD()` — chargement tuile raster
- `preloadTileToCache()` — prechargement en cache
- `tilePreloadTaskFunc()` — task FreeRTOS Core 1
- `startTilePreloadTask()` / `stopTilePreloadTask()`
- `initSymbolCache()` / `loadSymbolFromSD()` / `getSymbolCacheEntry()` / `getSymbol()`
- `pngOpenFile/Close/Read/Seek` + `pngSymbolCallback`
- `discoverAndSetMapRegion()` / `discoverDefaultPosition()` / `discoverNavRegions()` / `regionContainsTile()`
- `switchZoomTable()` / `initCenterTileFromLatLon()`

**File-scope statics** (restent dans map_tiles.cpp, pas dans map_state) :
- `notFoundCache`, `notFoundCacheIndex`
- `symbolCache[]`, `symbolCombinedBuffer`, `symbolPNG`, `pngFileOpened`

**Validation :** Compile ok. Test : tuiles raster et NAV chargent correctement.

## Etape 3 : Extraire `map_render.cpp`

**Fonctions :**
- `copyBackToFront()` — double buffer sprite
- `applyRenderedViewport()` — applique sprite, compense offset, refresh UI
- `refreshStationOverlay()` — refresh leger sans re-render
- `drawStationOnCanvas()` — dessin station individuelle
- `update_station_objects()` — boucle sur toutes les stations
- `draw_station_traces()` / `draw_own_trace()`
- `cleanup_station_buttons()` — reset hit zones
- `parseAndGetSymbol()` — parsing symbole APRS

**Validation :** Compile ok. Test : stations + traces + overlays visibles.

## Etape 4 : Extraire `map_input.cpp`

**Fonctions :**
- `map_touch_event_cb()` — state machine touch (pressed/pressing/released/double-tap)
- `scrollMap()` — defilement avec seuil
- `shiftMapCenter()` — deplacement centre par delta tuiles
- `commitVisualCenter()` / `resetPanOffset()` / `resetZoom()`
- `toggleMapFullscreen()`
- `btn_map_zoomin_clicked()` / `btn_map_zoomout_clicked()`
- `btn_map_recenter_clicked()` / `btn_map_back_clicked()`
- `btn_gpx_rec_clicked()` / `updateGpxRecButton()`
- `map_station_clicked()`

**Validation :** Compile ok. Test : pan, zoom, recenter, double-tap, retour dashboard.

## Etape 5 : Nettoyage

- Supprimer `map_touch_controller.h/cpp` (code mort, le touch est inline)
- Nettoyer `ui_map_manager.h` (ne garder que les declarations publiques)
- Mettre a jour CLAUDE.md (tableau des modules)

## Risques

| Risque | Mitigation |
|--------|------------|
| `redraw_map_canvas` appelee depuis input ET timer | La garder dans ui_map_manager.cpp (glue), pas dans render |
| `applyRenderedViewport` lit/ecrit beaucoup d'etat | map_state.h doit etre fait EN PREMIER |
| Thread safety : preload Core 1 vs render Core 0 | Deja protege par MapEngine::spriteMutex, ne pas changer |
| Variables locales confondues avec etat partage | Regle : si `static` dans une fonction = reste local. Si `static` au namespace = map_state |
