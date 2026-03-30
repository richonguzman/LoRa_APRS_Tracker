# 6. SD Card Setup

The SD card is optional but strongly recommended for maps and data persistence.

**Required format: FAT32**

---

## Folder Structure

```
/LoRa_Tracker/
├── config.json          ← Device configuration
├── stats.json           ← Station statistics
├── MapTiles/            ← Raster tiles (JPEG/PNG)
│   └── REGION/
│       └── Z8/
│           └── X/
│               └── Y.jpg
├── VectMaps/            ← Vector tiles (NAV)
│   └── REGION/
│       ├── Z9.nav
│       ├── Z10.nav
│       └── ...
└── GPX/                 ← Recorded tracks
    └── track_YYYYMMDD_HHMMSS.gpx
```

---

## Raster Tiles (JPEG/PNG)

Raster tiles are standard OpenStreetMap images (XYZ/TMS format).

### Recommended Download
Use **[MOBAC](https://mobac.sourceforge.io/)** (Mobile Atlas Creator):

1. Select source: OpenStreetMap
2. Select the geographic area
3. Zoom levels: 8, 10, 12, 14 (Z8 alone is enough to start)
4. Output format: **Custom** → folder `MapTiles/REGION_NAME/`

### File Naming
```
/LoRa_Tracker/MapTiles/FRANCE/Z8/X/Y.jpg
```

---

## Vector Tiles (NAV)

NAV tiles offer more detailed rendering at high zoom (Z9+). They are compatible with the [IceNav-v3](https://github.com/jgauchia/IceNav-v3) format.

### Generation
`.nav` files are generated from OpenStreetMap data using the IceNav conversion tool.

```
/LoRa_Tracker/VectMaps/FRANCE_SOUTH/Z9.nav
/LoRa_Tracker/VectMaps/FRANCE_SOUTH/Z10.nav
...
/LoRa_Tracker/VectMaps/FRANCE_SOUTH/Z15.nav
```

> One `.nav` file per zoom level per region.

---

## Without SD Card

The tracker works without an SD card with reduced functionality:

| Feature | Without SD |
|---------|-----------|
| Raster map | Grey background only |
| Vector map | Not available |
| APRS symbols on map | Red circles |
| Messages | Not persistent (lost on reboot) |
| Station stats | Not persistent |
| GPX tracks | Not available |
| Configuration | Saved in internal flash |
