# 2. Dashboard

The main screen is displayed at startup and shows all real-time information.

![Dashboard](../../tdeck_dashboard.jpg)

---

## Status Bar (top)

| Element | Description |
|---------|-------------|
| **Callsign** | Your APRS callsign |
| **APRS Symbol** | Icon representing your station |
| **Date/Time** | Format DD/MM HH:MM (GPS time) |
| **GPS 3D** | Golden icon when strict 3D fix mode is enabled in settings (beacons then require PDOP ≤5.0 instead of HDOP) |
| **WiFi** | Green icon when connected |
| **Bluetooth** | Green icon when a BLE client is connected |
| **Battery** | Percentage + color (green >50%, orange 20–50%, red <20%) |

---

## Center Area

### GPS
- **Satellites** — Number of satellites + HDOP quality indicator:
  - `+` HDOP ≤ 2.0 (excellent)
  - `-` HDOP 2–5 (good)
  - `X` HDOP > 5 (poor)
- **Locator** — 8-character Maidenhead grid square (e.g. `JN03AA12`)
- **Position** — Latitude / Longitude in decimal degrees
- **Altitude** — in meters
- **Speed** — in km/h

### LoRa
- **Frequency** — In MHz (e.g. `433.775`)
- **Speed** — In bps

### Last Received Stations
The 4 most recently received stations with their RSSI (dBm) and SNR (dB).

---

## Action Buttons (bottom)

| Button | Color | Action |
|--------|-------|--------|
| **BCN** | Red | Send an APRS beacon immediately (if GPS quality sufficient: ≥6 satellites and HDOP ≤5.0) |
| **MSG** | Blue | Open messaging |
| **MAP** | Green | Open the map |
| **SET** | Purple | Open settings |

---

## Automatic Beacon

The tracker sends beacons automatically using **SmartBeacon** logic:
- More frequent when moving fast
- Less frequent when stationary
- **GPS requirements**: at least 6 satellites and HDOP ≤5.0 (or PDOP ≤5.0 in strict 3D mode)

The **BCN** button allows sending a manual beacon at any time, under the same GPS quality conditions.
