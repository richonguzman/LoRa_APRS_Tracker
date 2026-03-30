# 1. Installation & First Boot

## Flashing the Firmware

The easiest method is the **Web Flasher** — no tools required.

1. Connect the T-Deck Plus via USB
2. Open the flasher: [https://moricef.github.io/LoRa_APRS_Tracker/](https://moricef.github.io/LoRa_APRS_Tracker/)
3. Click **Install** and select the USB port
4. For a fresh install, check **Erase device** to wipe any existing configuration

> The Web Flasher requires Chrome or Edge (not Firefox).

---

## First Boot — Automatic Web Configuration

On first boot (callsign is `NOCALL-7` by default):

1. **Splash screen** with firmware version
2. **Initialization screen** showing progress:
   - Storage (SPIFFS, SD card detection)
   - GPS module
   - LoRa radio
   - WiFi/BLE (disabled by default)
3. **First Time Setup screen** appears automatically because callsign is `NOCALL-7`:
   - WiFi access point `LoRa-Tracker-AP` starts (password: `1234567890`)
   - IP address: `192.168.4.1`
   - Connect a PC or phone to this WiFi network
   - Open `http://192.168.4.1` in a browser
4. **Web configuration interface**:
   - Set your APRS callsign (required)
   - Configure LoRa frequency and speed
   - Optionally set up a WiFi network for APRS‑IS connection and web‑conf mode
   - Configure SmartBeacon, filters, beacon comment, etc.
5. Click **Save**, then **Reboot** on the T‑Deck
6. After reboot, the **dashboard** screen appears with your configured callsign
7. GPS starts searching for satellites (may take 1–2 minutes outdoors)

> The automatic web configuration only runs if the callsign is `NOCALL-7`. Once configured, subsequent boots go directly to the dashboard.

---

## Minimum Configuration

Before use, configure at least:

| Parameter | Where | Value |
|-----------|-------|-------|
| Callsign | Web config → Callsign | Your APRS callsign (e.g. `F4ABC-9`) |
| LoRa Frequency | Web config → LoRa Frequency | 433.775 MHz (Europe) |
| LoRa Speed | Web config → LoRa Speed | 300 bps (default) |

WiFi and Bluetooth are **disabled by default** to save memory. Enable them only when needed via Settings.

---

## Manual Web‑Conf Mode (after first boot)

For advanced configuration changes later:

1. Go to **Settings → Web‑Conf Mode** and enable it
2. The tracker reboots and starts the WiFi access point `LoRa‑Tracker‑AP`
3. Connect to `http://192.168.4.1`
4. Configure parameters, then click **Save**
5. On the T‑Deck, press **Back** to exit Web‑Conf Mode and reboot

> Web‑Conf Mode runs a lightweight web server and displays a minimal configuration screen with WiFi connection info. The normal LVGL UI is suspended during this mode.
