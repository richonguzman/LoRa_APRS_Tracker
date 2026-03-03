# LoRa APRS Tracker - LVGL UI Edition

![PlatformIO](https://img.shields.io/badge/PlatformIO-ready-orange)
![Version](https://img.shields.io/badge/version-2.7.3%2Bdev-blue)
![License](https://img.shields.io/badge/license-GPL-green)

**ESP32-S3 LoRa APRS tracker with modern touchscreen interface for Lilygo T-Deck Plus**

This is a fork of [CA2RXU's LoRa APRS Tracker](https://github.com/richonguzman/LoRa_APRS_Tracker) featuring a complete LVGL-based touchscreen interface with vector map rendering (NAV format compatible with [IceNav-v3](https://github.com/jgauchia/IceNav-v3)), LovyanGFX graphics library for enhanced performance, advanced APRS messaging, and optimized memory management.

**⚠️ Development Branch** - This version includes experimental features. For stable release, see [main branch](https://github.com/moricef/LoRa_APRS_Tracker/tree/main).

## Screenshots

|<img src="docs/tdeck_dashboard.jpg" width="250">|<img src="docs/tdeck_vector_map.jpg" width="250">|<img src="docs/tdeck_messaging.jpg" width="250">|
|:-:|:-:|:-:|
| **Dashboard** | **Vector Map** | **Messaging** |

---

## What's New in v2.7.x
- **SD DMA + Display DMA** - Ported from IceNav-v3 for ultra-fast tile loading and smooth rendering
- **ESP_LOG migration** - Replaced standard Serial.print with native ESP-IDF logging framework
- **PSRAM LVGL Allocator** - Redirected UI memory to PSRAM to free up critical DRAM
- **NPK2 Multi-region support** - Support for "split packs" NPK2 map files with multi-region roaming
- **Wide Zoom Range** - Map support from Zoom 6 up to 17 with adaptive raster/vector switching
- **Web-Conf Stability** - Fixed touch reentrancy and watchdog issues in configuration mode
- **GPX trace recorder** - Start/Stop button on map, saves tracks to SD card
- **Delta+ZigZag+VarInt NAV format** - 30-50% smaller tiles, compatible with IceNav-v3
- **HDOP adaptive jitter filter** - GPS trace smoothing based on signal quality
- **NAV raw data cache** - PSRAM cache avoids SD re-reads after pan (30 tiles LRU)
- **Station traces with TTL** - 60-minute time-to-live for received station positions
- **Vector map rendering** - NAV format tiles with roads, paths, water bodies, buildings
- **Dual map modes** - Raster (JPEG/PNG) and Vector (NAV) with adaptive zoom
- **Statistics persistence** - LinkStats and per-station data saved to SD card
- **WiFi Station mode** - Connect to existing networks for internet access
- **Display ECO slider** - Configurable timeout for screen dimming
- **Memory optimizations** - Fixed leaks, improved PSRAM usage
- **Stability fixes** - BLE wake crash, SD logger infinite loop, color rendering

## Key Features

### Interface
- Full-color touchscreen UI (320x240) with LVGL 8.4
- **LovyanGFX** graphics library (replaces TFT_eSPI for better performance)
- Dashboard with real-time GPS, LoRa, WiFi, battery status
- Interactive map with dual rendering (raster/vector)
- APRS messaging with conversation view
- Touch-friendly configuration screens
- Physical QWERTY keyboard support

### Map System
- **Raster mode**: JPEG/PNG tiles (OpenStreetMap format)
  - Zoom levels: 8, 10, 12, 14, 16, 18 (step 2)
- **Vector mode**: NAV binary format (IceNav-v3 compatible)
  - Zoom levels: 8 to 18 (step 1)
  - Sub-pixel precision rendering
  - Dynamic background color from tiles
- APRS station display with symbols (primary/alternate tables)
- Label rendering with anti-collision detection
- GPS tracking with auto-follow and manual pan

### APRS & Messaging
- Full LoRa APRS support (433MHz / 868MHz)
- Threaded message conversations
- Contact management (add/edit/delete)
- Quick reply from map or station list
- Message history on SD card
- LinkStats and per-station statistics (max 20 stations)

### Hardware
- **Board**: Lilygo T-Deck Plus (ESP32-S3, 16MB Flash, 8MB PSRAM)
- **Display**: 320x240 IPS touchscreen with brightness control
- **LoRa**: SX1262 module
- **GPS**: Internal GPS module
- **Storage**: SD card (A1 class recommended for fast tile loading)
- **Connectivity**: WiFi (AP + Station modes), Bluetooth LE

## Installation

### Option 1: Web Flasher (Recommended)

**Stable version:**
https://moricef.github.io/LoRa_APRS_Tracker/

**Development version (this branch):**
https://moricef.github.io/LoRa_APRS_Tracker/devel.html

Requirements:
- Google Chrome, Microsoft Edge, or Chromium browser (Firefox not supported)
- USB cable
- Close PlatformIO/Arduino IDE if open
- If port not detected: hold BOOT button while connecting USB

### Option 2: Build from Source

Prerequisites: [PlatformIO](http://platformio.org/) and [git](http://git-scm.com/)

```bash
git clone https://github.com/moricef/LoRa_APRS_Tracker.git
cd LoRa_APRS_Tracker

# Build and upload (433MHz variant)
pio run -e ttgo_t_deck_plus_433 --target upload

# Upload filesystem
pio run -e ttgo_t_deck_plus_433 --target uploadfs

# For 868MHz: use ttgo_t_deck_plus_868 environment
```

## SD Card Setup

### Directory Structure

Firmware creates these directories automatically on first boot:

```
/LoRa_Tracker/
├── Maps/          # Raster tiles (JPEG/PNG)
├── VectMaps/      # Vector tiles (NAV format)
├── Symbols/       # APRS symbols (24x24 PNG)
│   ├── primary/
│   └── alternate/
├── Messages/
│   ├── inbox/
│   └── outbox/
└── Contacts/
```

### Map Tiles

**Raster tiles (JPEG/PNG)** - Using Python script:
```bash
cd tools/
python download_tiles.py --region france --zoom 8 10 12 14 16 18
# Copy tools/tiles/* to SD:/LoRa_Tracker/Maps/
```

**Raster tiles (JPEG/PNG)** - Using [MOBAC](https://mobac.sourceforge.io/):
1. Select region and tile source (OpenStreetMap)
2. Choose zoom levels: 8, 10, 12, 14, 16, 18
3. Output format: "OSMTracker tile storage"
4. Copy to SD:/LoRa_Tracker/Maps/

**Vector tiles (NAV format)** - Using [Tile-Generator](https://github.com/jgauchia/Tile-Generator):
```bash
git clone https://github.com/jgauchia/Tile-Generator.git
cd Tile-Generator
python3 -m venv venv
source venv/bin/activate
pip install shapely pygame osmium

python tile_generator.py region.pbf output_dir features.json --zoom 8-18
# Copy output_dir/* to SD:/LoRa_Tracker/VectMaps/
```

**Note**: Vector mode activates automatically when `/LoRa_Tracker/VectMaps/` directory exists.

### APRS Symbols

Download: [APRS Symbols Pack (Mega)](https://mega.nz/folder/6FUi3DDD#cSdv5-zt18KTWeooz7ZvlA)

Extract to SD card: `/LoRa_Tracker/Symbols/` (primary + alternate folders)

Each symbol: 24x24 PNG file named with hex ASCII code (e.g., `3E.png` for car `>`)

## Configuration

### Web Interface

1. Connect to WiFi AP `LoRa-Tracker-AP`
2. Open browser: `http://192.168.4.1`
3. Configure callsign, APRS settings, preferences

### Configuration File

Edit `data/tracker.json` for advanced settings:
```json
{
  "callsign": "YOURCALL",
  "ssid": 9,
  "symbol": ">",
  "overlay": "/",
  "comment": "LoRa APRS Tracker"
}
```

## Usage

**Dashboard**: BEACON (send position) | MSG (messaging) | MAP (interactive map) | SETUP (settings)

**Map controls**: Touch to pan | +/- buttons to zoom | GPS button to recenter | Tap stations for info

**Keyboard shortcuts**: Enter (send/confirm) | Escape (back/cancel) | Shift (uppercase) | Sym (symbols)

## Operation Without SD Card

Tracker works in degraded mode without SD:
- Map: gray background (no tiles)
- Symbols: red circles (no APRS symbols)
- Messages: not saved persistently
- Config: uses SPIFFS defaults

**Recommendation**: Use SD card with A1 rating (e.g., SanDisk Extreme) for optimal tile loading.

## Technical Details

**Memory**: ~88KB DRAM free during operation, PSRAM for tile/symbol cache and sprites

**Optimizations**:
- **Direct Memory Access (DMA)**: Hardware acceleration for SD card reads (32KB chunks) and display flushing
- **PSRAM Asset Management**: UI allocator and tile/symbol cache redirected to PSRAM
- Synchronous tile rendering (decode + copy + cache)
- RGB565 byte-swap for correct LVGL colors
- Persistent viewport sprite (no fragmentation)
- LRU cache for raster and vector tiles
- Negative cache to avoid repeated SD scans

**Power Management**:
- Display: configurable ECO timeout
- WiFi: periodic sleep
- BLE: auto-disable after 5 min
- GPS: sleep between beacons

## Credits

**Original firmware**: [CA2RXU - Ricardo](https://github.com/richonguzman/LoRa_APRS_Tracker)

**Libraries & inspiration**:
- [LVGL](https://lvgl.io/) - UI library
- [LovyanGFX](https://github.com/lovyan03/LovyanGFX) - Graphics library
- [IceNav-v3](https://github.com/jgauchia/IceNav-v3) - NAV format and vector rendering

**LVGL UI development**: F4MLV with Claude AI assistance

## Support This Project

If you find this project useful:

[PayPal donation](https://paypal.me/moricef09) | [GitHub Sponsors](https://github.com/sponsors/moricef)

**Original project**: Support [CA2RXU](https://github.com/sponsors/richonguzman) too!

## License

Same license as original CA2RXU LoRa APRS Tracker.

Map data: © [OpenStreetMap contributors](https://www.openstreetmap.org/copyright)

## Issues & Support

**LVGL UI issues**: [Open issue on this fork](https://github.com/moricef/LoRa_APRS_Tracker/issues)

**General tracker issues**: See [original project](https://github.com/richonguzman/LoRa_APRS_Tracker)

---

73! F4MLV
