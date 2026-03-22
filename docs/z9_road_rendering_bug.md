---
name: Z9-Z11 roads en pointillé — diff IceNav vs Tracker
description: Bug actif — routes hachées/pointillées Z9-Z11, causé par une différence de code avec IceNav dans le rendu GEOM_LINE
type: project
---

## Bug : routes en pointillé Z9-Z11

Les routes sont en pointillé (hachées) à Z9-Z11. Correctement dessinées à Z12+. Le renderview (outil de test Python) n'a pas ce problème → les données NAV sont correctes, le bug est dans le firmware.

## Cause identifiée : différence de code avec IceNav

Commit `604e9722` ("port IceNav optimizations") introduit par Claude — le LOD a été mal transposé.

### IceNav (correct) — `lib/maps/src/maps.cpp` L1034-1061

LOD + dessin **dans la même boucle**. Quand un point est trop proche du précédent, `continue` saute le dessin ET la mise à jour de lastPx/lastPy. Le segment suivant est dessiné directement depuis le dernier point gardé.

```cpp
for (uint16_t i = 0; i < ref.coordCount; i++) {
    int16_t px = coords[i * 2];
    int16_t py = coords[i * 2 + 1];
    if (i > 0) {
        if (abs(px - lastPx) < lodThreshold && abs(py - lastPy) < lodThreshold)
            continue;
        // culling check + draw inline
        map.drawLine(lastPx, lastPy, px, py, color);
    }
    lastPx = px;
    lastPy = py;
}
```

### Tracker (cassé) — `src/map/map_nav_render.cpp` L319-349

LOD **filtre les points dans un tableau séparé** (pxArr/pyArr), puis dessine tous les segments entre points restants dans une **seconde boucle**. Claude a affirmé "ça devrait donner le même résultat" sans vérifier — c'est faux, le résultat est des routes en pointillé.

```cpp
// Phase 1 : filtre LOD dans un tableau
int16_t lodThreshold = (zoom >= 15) ? 2 : 1;
for (size_t j = 0; j < numCoords; j++) {
    int16_t px = (coords[j * 2] >> 4) + ref.tileOffsetX;
    int16_t py = (coords[j * 2 + 1] >> 4) + ref.tileOffsetY;
    if (validPoints > 0) {
        if (abs(px - lastPx) < lodThreshold && abs(py - lastPy) < lodThreshold
            && j < numCoords - 1) continue;
    }
    pxArr[validPoints] = px; pyArr[validPoints] = py;
    lastPx = px; lastPy = py; validPoints++;
}
// Phase 2 : dessin séparé
for (size_t j = 1; j < validPoints; j++) {
    if (widthF <= 1.0f) map.drawLine(...);
    else { map.drawWideLine(...); map.setClipRect(...); }
}
```

Différences notables :
- **`&& j < numCoords - 1`** : garde toujours le dernier point même s'il est au même pixel → segments de longueur 0 que IceNav n'a pas
- **setClipRect après drawWideLine** : IceNav ne le fait PAS
- **Séparation filtre/dessin en 2 boucles** : IceNav fait tout inline


## Multiplicateur largeur Z9

`if (zoom <= 9) widthF *= 1.5f;` ajouté à L294 — fait passer les motorways de 1px à 1.5px à Z9. L'utilisateur a dit que 2px (×2.0) était trop gros. Le ×1.5 n'a PAS été signalé comme problématique — le garder.

## Fichiers concernés

- `src/map/map_nav_render.cpp` : `renderSingleFeature()`, `case GEOM_LINE` (L289-352)
- Référence IceNav : `/home/fab2/Developpement/LoRa_APRS/IceNav-v3/lib/maps/src/maps.cpp` L1034-1061
