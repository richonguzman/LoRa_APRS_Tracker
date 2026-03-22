# Plan : Reecriture renderSingleFeature() — transposition IceNav

## Contexte

Le rendu NAV dans `map_nav_render.cpp` etait une accumulation de patches LLM qui divergeait
d'IceNav sans raison valable. Bug principal : routes en pointille Z9-Z11, cause par le
semantic culling qui ejectait les GEOM_LINE < 3x3px (features courtes sautees = trous).

## Etat actuel (post-reecriture)

### Ce qui a ete fait

1. **renderSingleFeature() reecrit** — 3 fonctions separees transposees d'IceNav :
   - `renderNavLine()` : boucle unique LOD+dessin (IceNav L1042-1061)
   - `renderNavPolygon()` : fill + outline, bbox culling pixel-space (IceNav L1067-1158)
   - `renderNavPoint()` : fillCircle rayon 3 (IceNav L1163-1174)
   - `renderSingleFeature()` : dispatch simple (IceNav L1179-1197)

2. **proj16X/proj16Y supprimes** — code mort depuis la boucle unique.

3. **setClipRect apres drawWideLine supprime** — IceNav ne le fait pas,
   `draw_gradient_wedgeline` appelle `clearClipRect()` en interne.

4. **setClipRect per-feature supprime** — IceNav ne le fait pas.

### Semantic culling — etat final

Le culling < 3x3px sur GEOM_LINE causait des routes en pointille a Z9-Z11.
Exclure GEOM_LINE entierement faisait exploser le nombre de features (+40%)
et le temps de rendu (7-9s au lieu de 4-5s), bloquant le pan.

**Solution retenue** : seuils differencies par type de geometrie :
- **GEOM_POLYGON** : < 3x3px (Z9-Z11) ou < 1x1px (Z12+) — inchange
- **GEOM_LINE** : < 1x1px a tous les zooms — elimine les features invisibles
  sans creer de trous dans les routes

Applique aux deux chemins : Z9 streaming (~L584) et Z10+ batch (~L1097).

**Mystere non resolu** : IceNav utilise le meme seuil < 3x3px pour les lignes
(maps.cpp L1386-1389) avec le meme generateur de tuiles et le meme hardware,
mais ne presente pas le bug des routes en pointille. Cause inconnue.

### Pool cap — etat final

Le pool de features Z10+ (globalLayers) etait cappe a 16384. En zone dense
(Toulouse Z10-Z12), les 9 tuiles generent 16K-23K features. Le cap causait
des routes/batiments manquants (motorways comprises).

**Solution retenue** : cap remonte a 20000 (x1.2). Le `try/catch bad_alloc`
reste comme filet de securite pour le vrai OOM. Le log `ESP_LOGW` signale
quand le cap est atteint.

### Largeur routes Z9-Z11

`widthF *= 1.05f` pour zoom 9-11. Le generateur stocke `width * 2` en uint8,
le firmware fait `widthRaw / 2.0f`. Le x1.05 force les routes de 1.0px a passer
par `drawWideLine` (anti-aliasing) au lieu de `drawLine`.

## Fichiers modifies

- `src/map/map_nav_render.cpp` : renderSingleFeature + semantic culling + pool cap
- `src/map/map_engine.cpp` : suppression proj16X/Y
- `src/map/map_internal.h` : suppression extern proj16X/Y

## Fichiers de reference IceNav

- `/home/fab2/Developpement/LoRa_APRS/IceNav-v3/lib/maps/src/maps.cpp`
  - `renderNavLineString()` L1007-1062
  - `renderNavPolygon()` L1067-1158
  - `renderNavPoint()` L1163-1174
  - `renderNavFeature()` L1179-1197 (dispatch 2-pass)
  - `renderNavTile()` L1292-1404 (iteration features + culling + featurePool)

## Problemes ouverts

1. **Temps de rendu Z9-Z10** : 4-9s pour 15K-33K features. Le goulot est le
   dessin pixel par pixel (drawLine/drawWideLine) sur PSRAM. Pas de solution
   simple sans reduire le nombre de features ou changer l'architecture.

2. **Tile load stopped < 400KB** : en zone tres dense, le chargement des tuiles
   s'arrete quand la PSRAM descend sous 400KB. Seuil conserve — en dessous
   c'est le crash.

3. **Casing lignes** : IceNav fait un 2-pass (pass 1 = wide+dark, pass 2 =
   thin+normal). Le tracker ne le fait pas. Hors scope pour l'instant.

## Ce qu'on NE touche PAS

- `decodeFeatureCoords()` — fonctionne, architecture differente d'IceNav mais coherente
- Pipeline streaming Z9 / globalLayers Z10+ — l'iteration et le pooling fonctionnent
- Cache tuiles, pan, architecture async — hors scope
- `fillPolygonGeneral` — verifie, coherent (tracker passe tileOffset car coords raw)
