# 5. Settings

Access from Dashboard → **SET** button.

---

## Main Menu

| Option | Description |
|--------|-------------|
| **Callsign** | Your APRS callsign |
| **LoRa Frequency** | Transmit/receive frequency |
| **LoRa Speed** | LoRa data rate |
| **Display** | Brightness, power saving |
| **Sound** | Volume and beep types |
| **Repeater** | APRS repeater mode |
| **GPS** | GPS settings (strict 3D fix) |
| **WiFi** | Network connection |
| **Bluetooth** | BLE configuration |
| **Web-Conf Mode** | Advanced configuration via browser |
| **Reboot** | Restart the device |
| **About** | Firmware version |

---

## Callsign

Enter your full APRS callsign, with SSID if desired.

Examples: `F4ABC`, `F4ABC-9`, `F4ABC-7`

Common SSIDs:
- `-9`: car
- `-7`: bicycle
- `-5`: pedestrian
- `-1` to `-4`: fixed station

---

## LoRa Frequency

Select the frequency for your region:

| Region | Frequency |
|--------|-----------|
| EU/WORLD | 433.775 MHz |
| POLAND | 434.855 MHz |
| UK | 439.913 MHz |

The active frequency is displayed in green.

---

## LoRa Speed

Select the LoRa data rate. A lower rate improves range but increases transmission duration.

Available rates:
- **1200 bps** (SF9)
- **610 bps** (SF10)
- **300 bps** (SF12) – default
- **244 bps** (SF12)
- **209 bps** (SF12)
- **183 bps** (SF12)

---

## Display

| Parameter | Description |
|-----------|-------------|
| **ECO Mode** | Turns off the screen after inactivity |
| **ECO Timeout** | Delay before screen off (2–15 min) |
| **Brightness** | Screen brightness (5–100%) |

> ECO Timeout and Brightness are only adjustable when the toggle is active (unlocked).

---

## GPS

GPS receiver settings.

| Parameter | Description |
|-----------|-------------|
| **Strict 3D Fix (Mountain)** | Enables PDOP (≤5.0) instead of HDOP checking for APRS beacons. Requires at least 6 satellites. |

### Transmission Criteria

| Mode | Criterion | Description |
|------|-----------|-------------|
| **Normal (OFF)** | HDOP ≤ 5.0 | Horizontal accuracy only |
| **Strict 3D (ON)** | PDOP ≤ 5.0 | Combined 3D accuracy (horizontal + vertical) |

> The yellow GPS 3D icon on the dashboard indicates this mode is enabled.

> **Note**: If the criterion is not met, the beacon is silently skipped (SmartBeacon continues). PDOP mode ensures accurate altitudes in mountainous terrain, but may cause delays in forest/city where the sky is partially obscured.

---

## Sound

| Parameter | Description |
|-----------|-------------|
| **Sound** | Enable/disable all sounds |
| **Volume** | Master volume (0–100%) |
| **TX Beep** | Beep on each transmission |
| **Message Beep** | Beep on message reception |
| **Station Beep** | Beep when a new station is detected |

---

## Repeater

Enables repeater mode: the tracker retransmits all received APRS packets.

> Uses more power. Only use if you are operating a digi-repeater.

---

## WiFi

| State | Description |
|-------|-------------|
| **OFF (disabled)** | WiFi disabled |
| **Connecting...** | Connection attempt in progress |
| **Connected** | Connected, IP shown |
| **Eco (retry)** | Eco mode, periodic reconnection |

Once connected, the local IP and WiFi RSSI are displayed.

> **WiFi and BLE cannot operate simultaneously.** Enabling one disables the other.

---

## Bluetooth (BLE)

Allows connecting a mobile app (e.g. APRSDroid via BLE).

| State | Description |
|-------|-------------|
| **OFF** | BLE disabled |
| **ON (waiting)** | Waiting for connection |
| **Connected** | Client connected |

> **WiFi and BLE cannot operate simultaneously.** Enabling one disables the other.

---

## Web-Conf Mode

Launches a WiFi access point for advanced configuration:

1. Enable **Web-Conf Mode**
2. Connect to the WiFi network `LoRa-Tracker-AP` (password: `1234567890`)
3. Open `http://192.168.4.1`
4. Configure SmartBeacon, APRS filters, beacon comment, etc.
5. Save, then **Reboot** on the T-Deck
