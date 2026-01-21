# LoRa APRS Tracker - LVGL UI Edition (T-Deck Plus)

This is a fork of [CA2RXU's LoRa APRS Tracker](https://github.com/richonguzman/LoRa_APRS_Tracker) with a modern graphical user interface using **LVGL 8.4** specifically designed for the **Lilygo T-Deck Plus** with its 320x240 color touchscreen.

<!-- TODO: Add T-Deck Plus screenshot -->

## Features

### Modern Touch Interface
- **Dashboard**: Main screen with real-time GPS, LoRa, WiFi status and quick access buttons
- **Interactive Map**: Offline tile-based map with GPS tracking and station display
- **Messaging**: Full APRS messaging with conversation view, contacts management
- **Setup**: Touch-friendly configuration screens

### Map Features
- **Offline Tiles**: JPEG/PNG tile support stored on SD card (OpenStreetMap format)
- **Multi-zoom**: Support for zoom levels **8, 10, 12, 14**
- **Station Display**: Clickable APRS stations with symbols on map
- **GPS Tracking**: Auto-follow GPS position with manual pan mode
- **APRS Symbols**: Full symbol set with primary and alternate tables

### Messaging
- **Conversation View**: Threaded message display per contact
- **Quick Reply**: Click on station (map or list) to compose message
- **Contact Management**: Add, edit, delete contacts
- **Message History**: Persistent storage on SD card

### Hardware Support
- **Display**: 320x240 IPS touchscreen with brightness control
- **Touch**: Capacitive touch with gesture support
- **Keyboard**: Physical QWERTY keyboard with symbol layer
- **GPS**: Internal GPS module
- **LoRa**: SX1262 module (433MHz or 868MHz variants)
- **Storage**: SD card for maps, messages, and configuration
- **WiFi**: Web configuration interface
- **Bluetooth**: BLE support for external apps

## Installation

### Prerequisites
- PlatformIO (VSCode extension recommended)
- Lilygo T-Deck Plus board
- SD card with map tiles (optional but recommended) - **A1 class recommended** (e.g., SanDisk Extreme A1) for fast random read access required by tile loading

### Build
```bash
# Clone the repository
git clone https://github.com/moricef/LoRa_APRS_Tracker.git
cd LoRa_APRS_Tracker

# Build for T-Deck Plus 433MHz
pio run -e ttgo_t_deck_plus_433

# Or for 868MHz variant
pio run -e ttgo_t_deck_plus_868

# Upload firmware
pio run -e ttgo_t_deck_plus_433 -t upload

# Upload filesystem (configuration files)
pio run -e ttgo_t_deck_plus_433 -t uploadfs
```

### SD Card Setup

#### Map Tiles
Create the following directory structure on your SD card.

**Supported zoom levels: 8, 10, 12, 14** (you need tiles for each zoom level you want to use)

**Tile format: JPEG (.jpg) recommended** - PNG also supported but JPEG loads faster and uses less space

```
/LoRa_Tracker/
├── Maps/
│   └── YourRegion/       # Region name (e.g., "France", "Europe")
│       ├── 8/            # Zoom level 8 (country view)
│       │   ├── 127/      # X tile coordinate
│       │   │   ├── 93.jpg
│       │   │   └── 94.jpg
│       │   └── 128/
│       │       └── 93.jpg
│       ├── 10/           # Zoom level 10 (region view)
│       │   └── ...
│       ├── 12/           # Zoom level 12 (city view)
│       │   └── ...
│       └── 14/           # Zoom level 14 (street view)
│           └── ...
└── Symbols/
    ├── primary/          # Primary APRS symbols (/)
    │   ├── 21.png        # ! symbol (hex 21)
    │   ├── 23.png        # # symbol (hex 23)
    │   ├── 26.png        # & symbol (iGate)
    │   ├── 3E.png        # > symbol (car)
    │   └── ...
    └── alternate/        # Alternate APRS symbols (\)
        ├── 3E.png        # > symbol (red car)
        └── ...
```

#### Download Map Tiles

**Option 1: Using the included Python script**
```bash
cd tools/
python download_tiles.py --region france --zoom 8 10 12 14 --output /path/to/sd/LoRa_Tracker/Maps
```
See `python download_tiles.py --help` for all options.

**Option 2: Using MOBAC**
Use [Mobile Atlas Creator (MOBAC)](https://mobac.sourceforge.io/) to download OpenStreetMap tiles:
1. Select your region on the map
2. Choose a tile source (e.g., "OpenStreetMap")
3. Select zoom levels: **8, 10, 12, 14**
4. Choose output format: **OSMTracker tile storage** (exports PNG tiles)
5. Create the atlas and copy tiles to SD card

#### Convert PNG to JPEG (Recommended)

PNG tiles work fine but JPEG is recommended for smaller file size (3-5x smaller). Use the included conversion script:

```bash
cd tools/
python convert_to_jpeg.py /path/to/tiles -j 16 -q 85
```

Options:
- `-j 16`: Use 16 parallel threads for faster conversion
- `-q 85`: JPEG quality (85 is a good balance between size and quality)
- `--keep-png`: Keep original PNG files after conversion

**Note**: The tracker supports PNG, JPEG, and RGB565 RAW formats. PNG requires no conversion but uses more space.

#### APRS Symbols
The tracker uses 24x24 PNG images for APRS symbols. You need to provide them on the SD card.

**Symbol file naming**: Use the hexadecimal ASCII code of the symbol character.
- Example: `>` (car) = ASCII 62 = hex `3E` → filename: `3E.png`
- Example: `#` (digipeater) = ASCII 35 = hex `23` → filename: `23.png`

**Where to get symbols**:
- Extract from [APRS symbol sets](http://www.aprs.org/symbols.html) and convert to 24x24 PNG
- Use image editing software to create custom symbols
- Each symbol should be 24x24 pixels with transparency (PNG format)

### Operation Without SD Card

The tracker can operate without an SD card in **degraded mode**:
- **Map**: Displays gray background instead of tiles
- **Symbols**: Displays red circles instead of APRS symbols
- **Messages**: Cannot be saved persistently
- **Configuration**: Uses default settings from SPIFFS

**Recommendation**: Use an SD card for full functionality.

## Configuration

### Web Interface
1. On first boot, the device creates a WiFi access point
2. Connect to `LoRa_Tracker_XXXXXX` network
3. Open `http://192.168.4.1` in your browser
4. Configure your callsign, APRS settings, and preferences

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

### Dashboard
- **BEACON**: Send position beacon immediately
- **MSG**: Open messaging screen
- **MAP**: Open interactive map
- **SETUP**: Open configuration

### Map Navigation
- **Pan**: Touch and drag to move the map
- **Zoom**: Use +/- buttons
- **Recenter**: Press GPS button to return to current position
- **Station Info**: Tap on a station to send a message

### Keyboard Shortcuts
- **Enter**: Send message / Confirm
- **Escape**: Back / Cancel
- **Shift**: Toggle uppercase
- **Sym**: Toggle symbol layer

## APRS Symbols

**Note**: Symbol display on the map is functional but does not yet fully comply with the APRS standard. Some symbols may not be displayed correctly. A patch to the APRSPacketLib library is planned to fix this.

## Technical Details

### Memory Usage
- PSRAM: Used for map tile cache and symbol cache
- Heap: ~88KB free during normal operation
- SD Card: Recommended for maps, symbols, and message storage (see "Operation Without SD Card")

### Power Management
- Display eco mode: Auto-dim after timeout
- WiFi eco mode: Periodic sleep
- BLE eco mode: Auto-disable after 5 minutes of inactivity
- GPS eco mode: Sleep between beacons

## Credits

- **Original Firmware**: [CA2RXU - Ricardo](https://github.com/richonguzman/LoRa_APRS_Tracker)
- **LVGL Library**: [LVGL](https://lvgl.io/)
- **APRSPacketLib**: CA2RXU
- **LVGL UI Development**: F4MLV / Claude AI

## License

This project is licensed under the same terms as the original CA2RXU LoRa APRS Tracker.

## Support

For issues specific to the LVGL interface, please open an issue on this fork.
For general LoRa APRS Tracker issues, refer to the [original project](https://github.com/richonguzman/LoRa_APRS_Tracker).

---

73! F4MLV
