# Plan de refactoring `ui_map_manager.cpp` (2247 lignes -> 4+1 fichiers)

## Objectif

Rendre le code carte lisible, KISS, maintenable par un dev externe.
Chaque fichier = une responsabilite. Zero circularite.

## Fichiers resultants

| Fichier | Lignes | Responsabilite |
|---------|--------|----------------|
| `map_state.h` + `.cpp` | ~180 | Etat partage (variables extern, types partages) |
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

## Namespaces

Les nouveaux fichiers (`map_tiles.cpp`, `map_render.cpp`, `map_input.cpp`) utilisent
leur propre namespace (`MapTiles`, `MapRender`, `MapInput`), **PAS** `UIMapManager`.

Acces a l'etat partage :
```cpp
// En tete de chaque nouveau .cpp :
#include "map_state.h"
using namespace MapState;   // au file scope, AVANT le namespace du module
```

`using namespace MapState;` au file scope fonctionne car les noms non qualifies
resolvent vers `MapState::` avant d'entrer dans le namespace du module.

**ATTENTION** : `using namespace MapState;` **a l'interieur** de `namespace UIMapManager {}`
ne fonctionne PAS comme attendu — le compilateur cherche d'abord dans `UIMapManager::`
puis dans le scope englobant. C'est pourquoi `ui_map_manager.cpp` garde ses propres
variables tant que les fonctions ne sont pas extraites.

## Types partages dans map_state.h

Les types utilises par plusieurs modules doivent etre definis dans `map_state.h` :
- `StationHitZone` — ecrit par render, lu par input
- `CachedSymbol` — cree par tiles, lu par render (deplacer depuis `ui_map_manager.h`)
- `MapTile` — utilise par tiles et glue (deplacer depuis `ui_map_manager.h`)

## Etape 1 : Creer `map_state.h` + `map_state.cpp` (SANS modifier ui_map_manager.cpp)

**Quoi :** Creer le header et le .cpp avec toutes les ~50 variables. Les memes
variables restent dans `ui_map_manager.cpp` en doublon. Aucun changement fonctionnel,
zero risque. Les doublons seront supprimes au fur et a mesure des etapes 2-4.

**Variables concernees :**
- LVGL widgets : screen_map, map_canvas, map_canvas_buf, btn_zoomin/out/recenter, bars, labels
- Position/zoom : map_center_lat/lon, map_current_zoom, map_zoom_index, centerTileX/Y, renderTileX/Y
- Pan : offsetX/Y, navSubTileX/Y, velocityX/Y, isScrollingMap, dragStarted, map_follow_gps
- Render : redraw_in_progress, navModeActive, navRenderPending, mainThreadLoading, back/frontViewportSprite
- Regions : map_current_region, navRegions[], navRegionCount, defaultLat/Lon
- GPS : gpsFilter, pendingResetPan
- Hit zones : stationHitZones[], stationHitZoneCount
- UI : mapFullscreen

**Constantes** (constexpr dans le header) :
- nav_zooms[], raster_zooms[], PAN_FRICTION, PAN_FRICTION_BUSY, START_THRESHOLD, PAN_TILE_THRESHOLD
- NAV_MAX_REGIONS, MAP_STATIONS_HIT_MAX

**Piege constexpr arrays** : `constexpr int nav_zooms[]` dans un header donne une copie
par TU (linkage interne). OK pour lire les valeurs, mais ne JAMAIS comparer les pointeurs
(`map_available_zooms == nav_zooms` serait faux entre TU differentes).

**Les variables locales a une fonction** (`gpsUpdateCounter`, `refreshCounter`,
`firstTapTime`, `last_x/y/time`, etc.) restent en `static` local dans leur fonction.
Pas dans map_state.

**Validation :** `map_state.cpp` compile. `ui_map_manager.cpp` compile sans modification.

## Etape 2 : Extraire `map_tiles.cpp`

**Fonctions (deplacees depuis ui_map_manager.cpp, supprimees de l'original) :**
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
- `symbolCacheAccessCounter`, `symbolCacheInitialized`

**Piege includes** : PNGdec.h et JPEGDEC.h definissent les memes macros
(`INTELSHORT`, `INTELLONG`, `MOTOSHORT`, `MOTOLONG`). Il faut `#undef` entre les deux :
```cpp
#include <JPEGDEC.h>
#undef INTELSHORT
#undef INTELLONG
#undef MOTOSHORT
#undef MOTOLONG
#include <PNGdec.h>
```

**Suppression doublons** : une fois les fonctions deplacees et compilant dans
`map_tiles.cpp`, supprimer les copies dans `ui_map_manager.cpp` et les variables
file-scope correspondantes. Remplacer les appels `switchZoomTable(...)` par
`MapTiles::switchZoomTable(...)`, etc.

**Suppression #define** : les `#define START_THRESHOLD 12` et `#define PAN_TILE_THRESHOLD 128`
dans `ui_map_manager.cpp` deviennent inutiles (remplaces par `MapState::START_THRESHOLD`
et `MapState::PAN_TILE_THRESHOLD` via le `using namespace`). Les supprimer pour
eviter les conflits.

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

**Prerequis** : `CachedSymbol` doit etre dans `map_state.h` (deplace depuis
`ui_map_manager.h`) pour que `map_render.cpp` puisse appeler
`MapTiles::getSymbolCacheEntry()` qui retourne `CachedSymbol*`.

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

**File-scope statics** (restent dans map_input.cpp) :
- `last_x`, `last_y`, `last_time` (touch diagnostics)
- `firstTapTime`, `firstTapX`, `firstTapY` (double-tap detection)

**Validation :** Compile ok. Test : pan, zoom, recenter, double-tap, retour dashboard.

## Etape 5 : Nettoyage

- Supprimer `map_touch_controller.h/cpp` (code mort, le touch est inline)
- Nettoyer `ui_map_manager.h` : supprimer les `extern` dupliques (ils sont dans `map_state.h`)
- Supprimer `CachedSymbol`, `MapTile`, `StationHitZone` de `ui_map_manager.h` (deplace dans `map_state.h`)
- Verifier qu'aucune variable n'est definie a la fois dans `MapState` et `UIMapManager`
- Mettre a jour CLAUDE.md (tableau des modules)

## Risques

| Risque | Mitigation |
|--------|------------|
| `redraw_map_canvas` appelee depuis input ET timer | La garder dans ui_map_manager.cpp (glue), pas dans render |
| `applyRenderedViewport` lit/ecrit beaucoup d'etat | map_state.h doit etre fait EN PREMIER |
| Thread safety : preload Core 1 vs render Core 0 | Deja protege par MapEngine::spriteMutex, ne pas changer |
| Variables locales confondues avec etat partage | Regle : si `static` dans une fonction = reste local. Si `static` au namespace = map_state |
| `using namespace MapState;` dans `namespace UIMapManager {}` | NE PAS FAIRE — ne fonctionne pas. Les nouveaux .cpp utilisent leur propre namespace |
| constexpr arrays : pointeurs differents par TU | Ne jamais comparer `map_available_zooms == nav_zooms`. Utiliser uniquement pour indexer |
| PNGdec/JPEGDEC macro collision | Toujours `#undef INTELSHORT/INTELLONG/MOTOSHORT/MOTOLONG` entre les deux includes |
| `#define` vs `constexpr` conflit | Supprimer les `#define` de ui_map_manager.cpp des que les constexpr MapState sont utilises |

## Lecons apprises (session 2026-03-20)

1. **Ne pas modifier ui_map_manager.cpp a l'etape 1.** Creer map_state.h/cpp en parallele,
   avec les variables en doublon. Les doublons se resolvent naturellement quand les
   fonctions sont extraites (etapes 2-4).

2. **`using namespace X;` dans `namespace Y {}` est un piege C++.** Le compilateur
   cherche d'abord dans Y::, pas dans X::. Solution : les nouveaux fichiers utilisent
   leur propre namespace (MapTiles, MapRender, MapInput) avec `using namespace MapState;`
   au file scope.

3. **Commencer par des stubs qui compilent**, puis deplacer le vrai code fonction par
   fonction. Un stub qui compile vaut mieux qu'une migration incomplete qui ne compile pas.
