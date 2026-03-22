# Plan : Reecriture renderSingleFeature() — transposition IceNav

## Contexte

Le rendu NAV dans `map_nav_render.cpp` est une accumulation de patches LLM qui diverge
d'IceNav sans raison valable. Bug constate : routes en pointille Z9-Z11, cause par le
semantic culling qui ejectait les GEOM_LINE < 3x3px (features courtes sautees = trous).

Le fix temporaire (exclure GEOM_LINE du culling) est en place et fonctionne.
L'objectif est de reecrire `renderSingleFeature()` comme une transposition fidele d'IceNav,
sans patches inventes.

## Fichiers de reference

- **IceNav** : `/home/fab2/Developpement/LoRa_APRS/IceNav-v3/lib/maps/src/maps.cpp`
  - `renderNavLineString()` L1007-1062
  - `renderNavPolygon()` L1067-1158
  - `renderNavPoint()` L1163-1174
  - `renderNavFeature()` L1179-1197 (dispatch 2-pass)
  - `renderNavTile()` L1292-1404 (iteration features + culling + featurePool)

- **Tracker** : `src/map/map_nav_render.cpp`
  - `renderSingleFeature()` L207-370
  - Streaming Z9 : L500-605
  - globalLayers Z10+ : L1060-1155

## Differences factuelles IceNav vs Tracker

### renderSingleFeature / GEOM_LINE

| Aspect | IceNav | Tracker | Action |
|--------|--------|---------|--------|
| Decodage coords | inline dans la fonction (readVarInt + zigzag + accumulation), stocke pixels (>>4 + tileOffset) | `decodeFeatureCoords()` stocke raw, >>4 + tileOffset dans la boucle de rendu | OK — garder le decodage separe du tracker |
| Largeur | `(width == 0 ? 2 : width) / 2.0f` | `(fp[4] & 0x7F) / 2.0f` + `*= 1.5 si zoom <= 9` | Garder le x1.5 Z9 (valide par utilisateur) |
| LOD | `(zoom >= 15) ? 2 : 1`, boucle unique | Idem apres fix | OK |
| Culling segment | per-segment : skip si les 2 points du meme cote hors tile | Idem apres fix (mais contre viewportW/H au lieu de tileW/H) | **A verifier** : tileW/H vs viewportW/H. IceNav culle contre la taille du sprite. Le tracker rend sur un sprite viewport, donc viewportW/H est correct |
| Dessin | drawLine si <=1, drawWideLine sinon | Idem | OK |
| setClipRect apres drawWideLine | NON | OUI (L348) | **Supprimer** — inutile vu que clearClipRect() est appele en interne par drawWideLine |
| Casing (2-pass) | Oui : pass 1 = wide+dark, pass 2 = thin+normal | NON — pas de casing pour les lignes | Hors scope pour l'instant |

### renderSingleFeature / GEOM_POLYGON

| Aspect | IceNav | Tracker | Action |
|--------|--------|---------|--------|
| Decodage | Inline, stocke pixels (>>4 + offset) | `decodeFeatureCoords()` stocke raw | OK |
| Buffers HP | `projBuf32X/Y` (int[]), coords en pixels | `proj32X/Y` (int[]), coords raw | OK — coords raw passees a fillPolygonGeneral avec tileOffset en parametre |
| Ring parsing | Inline apres coords (lit uint16 ringCount + ringEnds) | Dans `decodeFeatureCoords()` | OK |
| LOD points | skip si ringCount==0 && < 1px && pas dernier point | Idem | OK |
| BBox culling | APRES decodage, contre tileW/H | AVANT decodage (L212-217), contre viewportW/H | Tracker plus efficace (early exit), garder |
| fillPolygonGeneral | (map, px, py, count, color, **0, 0**, rings...) — coords deja en pixels | (map, px, py, count, color, **tileOffsetX, tileOffsetY**, rings...) — coords raw | Coherent : le tracker passe l'offset car coords sont raw |
| Outline | `ref.casing && zoom >= 16`, drawLine sur px[j]/py[j] (pixels) | `(fp[4] & 0x80) && zoom >= 16`, drawLine sur `(px_hp[j] >> 4) + tileOffset` (raw->pixels) | Coherent |

### renderSingleFeature / GEOM_POINT

| Aspect | IceNav | Tracker | Action |
|--------|--------|---------|--------|
| Decodage | readVarInt + zigzag + >>4 + offset | `decodeFeatureCoords()` + >>4 + offset dans le case | OK |
| Bounds | `px >= 0 && px < tileWidth` | `px >= 0 && px < viewportW` | viewportW est correct pour le tracker |
| Dessin | `fillCircle(px, py, 3, color)` | Idem | OK |

### Iteration features + culling

| Aspect | IceNav | Tracker Z9 streaming | Tracker Z10+ globalLayers |
|--------|--------|---------------------|--------------------------|
| Semantic culling | Polygon+Line, minDim adaptatif | **Polygon seulement** (fix en cours) | **Polygon seulement** (fix en cours) |
| Pool/dispatch | featurePool + layers[16] + 2-pass | Render immediat, pas de pool | globalLayers[16] + render sequentiel |
| Yield | `millis()` check, endWrite/startWrite | `esp_timer_get_time()` check, idem | Idem |

## Etapes de reecriture

### Etape 1 : Nettoyer GEOM_LINE (petite, isolee)

Dans `renderSingleFeature()`, case GEOM_LINE (L289-351) :

1. **Supprimer `proj16X`/`proj16Y`** : plus utilises apres le fix boucle unique. Retirer les declarations dans `map_internal.h` et `map_engine.cpp` (reserve + shrink).
2. **Supprimer le `setClipRect` apres `drawWideLine`** (L348) : IceNav ne le fait pas, et `draw_gradient_wedgeline` appelle `clearClipRect()` en interne de toute facon.
3. **Verifier que la boucle unique LOD+dessin est bien en place** (fix precedent).

Pas de changement fonctionnel — juste suppression de code mort.

### Etape 2 : Nettoyer le semantic culling (deja fait)

Confirmer que les deux occurrences (Z9 streaming L566-572, Z10+ L1074-1082) excluent
GEOM_LINE. Deja en place.

### Etape 3 : Aligner fillPolygonGeneral (verification)

Verifier que `fillPolygonGeneral()` gere correctement les offsets. IceNav passe (0,0) car
les coords sont deja en pixels. Le tracker passe (tileOffsetX/Y) car les coords sont raw.
S'assurer que `fillPolygonGeneral` fait le >>4 + offset en interne.

### Etape 4 : Supprimer le `setClipRect` per-feature (L220) ?

IceNav ne pose PAS de clipRect per-feature. Le tracker pose
`map.setClipRect(ref.tileOffsetX, ref.tileOffsetY, MAP_TILE_SIZE, MAP_TILE_SIZE)` avant
chaque feature (L220). A evaluer :
- Pro : empeche un feature de deborder sur les tuiles voisines
- Contra : IceNav n'en a pas besoin, et `drawWideLine`/`clearClipRect()` le detruit de toute facon
- **Decision** : garder pour l'instant, evaluer apres les tests

### Etape 5 (optionnelle) : Casing lignes

IceNav fait un 2-pass pour les lignes avec casing (pass 1 = wide+dark, pass 2 = thin+normal).
Le tracker ne le fait pas. A implementer plus tard si necessaire — pas lie au bug actuel.

## Ce qu'on NE touche PAS

- `decodeFeatureCoords()` — fonctionne correctement, architecture differente d'IceNav mais coherente
- Pipeline de streaming Z9 / globalLayers Z10+ — l'iteration et le pooling fonctionnent
- Cache tuiles, pan, architecture async — hors scope
- fillPolygonGeneral — sauf verification des offsets

## Verification

1. Compiler : `pio run -e ttgo_t_deck_plus_433`
2. Tester sur device : Z9, Z10, Z11, Z12, Z15 — routes continues a tous les zooms
3. Tester polygones (batiments Z16) : remplissage + outline corrects
4. Tester points : POI visibles aux bons endroits
5. Tester pan : pas de regression sur le pan/scroll

## Risques

- Supprimer le `setClipRect` per-feature pourrait causer des debordements de dessin entre tuiles
- Le casing (etape 5) necessite un 2-pass qui n'existe pas encore dans le tracker
