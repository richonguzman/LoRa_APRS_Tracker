# 6. Préparation de la carte SD

La carte SD est optionnelle mais fortement recommandée pour les cartes et la persistance des données.

**Format requis : FAT32**

---

## Structure des dossiers

```
/LoRa_Tracker/
├── config.json          ← Configuration de l'appareil
├── stats.json           ← Statistiques des stations
├── MapTiles/            ← Tuiles raster (JPEG/PNG)
│   └── REGION/
│       └── Z8/
│           └── X/
│               └── Y.jpg
├── VectMaps/            ← Tuiles vectorielles (NAV)
│   └── REGION/
│       ├── Z9.nav
│       ├── Z10.nav
│       └── ...
└── GPX/                 ← Traces enregistrées
    └── track_YYYYMMDD_HHMMSS.gpx
```

---

## Tuiles raster (JPEG/PNG)

Les tuiles raster sont des images OpenStreetMap standard (format XYZ/TMS).

### Téléchargement recommandé
Utiliser **[MOBAC](https://mobac.sourceforge.io/)** (Mobile Atlas Creator) :

1. Sélectionner la source : OpenStreetMap
2. Sélectionner la zone géographique
3. Niveaux de zoom : 6, 7 et 8
4. Format de sortie : **Custom** → dossier `MapTiles/NOM_REGION/`

### Nommage des fichiers
```
/LoRa_Tracker/MapTiles/FRANCE/Z8/X/Y.jpg
```

---

## Tuiles vectorielles (NAV)

Les tuiles NAV offrent un rendu plus détaillé à haut zoom (Z9+). Elles sont compatibles avec le format [Tile-Generator](https://github.com/jgauchia/Tile-Generator).

### Génération
Les fichiers `.nav` se génèrent depuis des données OpenStreetMap avec l'outil de conversion Tile-Generator.

```
/LoRa_Tracker/VectMaps/FRANCE_SUD/Z9.nav
/LoRa_Tracker/VectMaps/FRANCE_SUD/Z10.nav
...
/LoRa_Tracker/VectMaps/FRANCE_SUD/Z15.nav
```

> Un seul fichier `.nav` par niveau de zoom et par région.

---

## Sans carte SD

Le tracker fonctionne sans SD card avec des fonctionnalités réduites :

| Fonction | Sans SD |
|----------|---------|
| Carte raster | Fond gris uniquement |
| Carte vectorielle | Non disponible |
| Symboles APRS sur carte | Cercles rouges |
| Messages | Non persistants (perdus au reboot) |
| Stats stations | Non persistantes |
| Traces GPX | Non disponibles |
| Configuration | Sauvegardée en flash interne |
