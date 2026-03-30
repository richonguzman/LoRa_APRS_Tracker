# 7. Troubleshooting

---

## Screen stays black on startup

- Hold the side button for 2 seconds
- If no response: charge the battery (plug in USB)
- If the issue persists: reflash the firmware via the Web Flasher

---

## GPS cannot get a fix

- Go outdoors, in an open area
- Wait 1–2 minutes (cold start)
- Check the HDOP indicator on the dashboard (should show `+` or `-`)
- An `X` means the signal is too weak

---

## No LoRa packets received

1. Check the frequency in **Settings → LoRa Frequency** (433.775 MHz for Europe)
2. Make sure the antenna is properly attached
3. Check that there are active stations in your area on [aprs.fi](https://aprs.fi)

---

## Map is grey / no tiles

- SD card not inserted or not recognized
- Tiles not in the correct location (see [SD Card](06-sd-card.md))
- Check that the SD card is formatted as FAT32
- Reboot after inserting the SD card

---

## Bluetooth won't start

- Make sure WiFi is disabled (WiFi and BLE are mutually exclusive)
- Reboot and try again
- If the error persists, it is a memory constraint — see note below

> On the ESP32-S3, WiFi and BLE share the same internal DRAM. They cannot be active at the same time.

---

## BLE disconnects when I open the map

This is normal. Bluetooth is automatically paused when the map is opened to free memory, and resumes automatically when the map is closed.

---

## Messages are not saved

- Check that the SD card is present and recognized
- Without an SD card, messages are lost on reboot

---

## Device reboots on its own

- Low battery: charge via USB
- Insufficient memory during a heavy operation: reboot and avoid using WiFi + Map at the same time
- Check the serial logs (115200 baud) to identify the cause

---

## Full Reset

To erase all configuration:

1. Reflash with the **Erase device** option checked in the Web Flasher
2. OR delete `config.json` from the SD card

---

## Getting Help

- GitHub Issues: [moricef/LoRa_APRS_Tracker](https://github.com/moricef/LoRa_APRS_Tracker/issues)
- French APRS forum: [f5len.org](https://www.f5len.org)
