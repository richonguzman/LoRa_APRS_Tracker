# Firmware ESP32-C3 — LoRa APRS Tracker

Boîtier radio/GPS autonome. ESP-IDF v5.3.5 natif, communication UART avec ESP32-S3.

## Pinout

| Fonction     | GPIO | Notes                        |
|-------------|------|------------------------------|
| **GNSS TX** | 0    | UART1, C3 ← GPS              |
| **GNSS RX** | 1    | UART1, C3 → GPS              |
| **GNSS PPS**| 2    | Optionnel                    |
| **LoRa SCK**| 4    | SPI2                         |
| **LoRa MISO**| 5   |                              |
| **LoRa MOSI**| 6   |                              |
| **LoRa CS** | 7    |                              |
| **LoRa RST**| 3    |                              |
| **LoRa BUSY**| 8   |                              |
| **LoRa DIO1**| 10  | IRQ RX/TX done               |
| **S3 TX**   | 21   | UART0, C3 → S3, 460800 baud  |
| **S3 RX**   | 20   | UART0, C3 ← S3, 460800 baud  |

## Fonctionnalités

- **LoRa SX1262** (RadioLib + EspC3Hal) — RX continu, TX commandé par S3
- **GNSS NMEA** (UART1, 9600 baud) — filtrage qualité (HDOP, satellites, fix type)
- **SmartBeaconing** autonome — transmission APRS basée cap/vitesse/distance
- **Protocole binaire UART** S3↔C3 — trames avec CRC16
- **Stack optimisé** — ~170 KB DRAM libre, 270 KB flash utilisé

## Console debug

Console sur USB_SERIAL_JTAG (GPIO18/19). Sur DevKitM-1, ces pins sont sur headers — pas de connecteur USB. Pour debug :

```bash
# Brancher USB-Serial sur GPIO18/19
# Ou passer temporairement la console sur UART0 dans sdkconfig:
#   CONFIG_ESP_CONSOLE_UART_DEFAULT=y
#   (désactiver proto_uart_init() dans main.c)
```

## Build

```bash
source ~/.espressif/v5.3.5/esp-idf/export.sh  # depuis un shell propre (pas PlatformIO)
idf.py set-target esp32c3
idf.py build
```

v5.3.5 supporte nativement C3 eco7 (v1.1) — pas de patch Kconfig nécessaire.

## Flash

```bash
esptool.py --chip esp32c3 -p /dev/ttyUSB0 -b 460800 \
  --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_size keep --flash_freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x10000 build/lora_aprs_c3.bin
```

## Sessions de dev

| Date | Commit | Description |
|------|--------|-------------|
| 2026-04-27 | `abc123` | Stable : LoRa RX + GPS NMEA + SmartBeacon + proto UART |
| 2026-04-17 | initial | Structure ESP-IDF, modules de base |
