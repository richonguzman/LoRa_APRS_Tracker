# 3. Map

Access from Dashboard → **MAP** button.

![Vector map](../../tdeck_vector_map.jpg)

---

## Interface

### Title Bar (green, top)
- **< BACK** — Return to dashboard
- **MAP (Zx)** — Title with the current zoom level
- **GPS button** — Re‑centers the map on your position
  - Blue‑dark: GPS tracking active
  - Orange: GPS tracking disabled (manual pan)
- **+** / **−** — Zoom in / zoom out
- **GPX** — Start/stop GPX track recording

### Info Bar (blue‑dark, bottom)
Displays:
- Map center coordinates (latitude, longitude)
- Number of visible stations (Stn)
- GPS delta (d): distance between raw GPS position and filtered position, in meters
- Alpha coefficient (a): 1.00 = no filtering, < 1.00 = active filtering (reduces noise)

---

## Navigation

| Gesture | Action |
|---------|--------|
| **Drag** | Pan the map |
| **+** / **−** | Change zoom level |
| **Double-tap** | Toggle fullscreen (hides bars) |

> Pinch-to-zoom (two fingers) is not available.

After a pan, the map continues briefly with inertia. To re-enable automatic GPS tracking, tap the GPS button.

---

## Display Modes

### Raster (Z6–Z8)
OpenStreetMap‑style JPEG/PNG tiles. Requires tiles on the SD card.

### Vector / NAV (Z9–Z17)
Vector rendering with roads, paths, buildings, waterways. More detailed, based on `.nav` files on the SD card.

The raster→vector transition is automatic from Z9 onwards.

---

## Station Display

- APRS symbols of received stations with their callsigns
- Labels automatically offset to avoid overlaps
- Your own position is shown with your APRS symbol
- **GPS Track**: the last 500 points of your route (cleared after 60 min)

### Send a message to a station

Tap a station icon to open the message compose screen with its callsign pre‑filled in the **To:** field.

---

## GPX Recording

1. Press the **GPX** button to start recording
2. The button turns red while recording
3. Press again to stop
4. The file is saved on the SD card in `/LoRa_Tracker/GPX/`


