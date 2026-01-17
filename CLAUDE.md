# Claude Code Project Notes

## Directives

- **NE JAMAIS flasher** - L'utilisateur flashe lui-même. Claude ne fait que builder.
- Build uniquement avec: `~/.platformio/penv/bin/pio run -e ttgo_t_deck_plus_433`

## Build Commands

```bash
# PlatformIO path
~/.platformio/penv/bin/pio

# Build T-Deck Plus 433 MHz
~/.platformio/penv/bin/pio run -e ttgo_t_deck_plus_433

# Flash (utilisateur uniquement)
~/.platformio/penv/bin/pio run -e ttgo_t_deck_plus_433 -t upload

# Monitor serial
~/.platformio/penv/bin/pio device monitor -b 115200
```

## Project Structure

- **src/lvgl_ui.cpp** - LVGL touchscreen UI for T-Deck Plus
- **include/lvgl_ui.h** - LVGL UI header
- **src/storage_utils.cpp** - SD card / SPIFFS abstraction layer
- **include/storage_utils.h** - Storage utilities header
- **src/msg_utils.cpp** - Message handling
- **src/wifi_utils.cpp** - WiFi connection management
- **src/LoRa_APRS_Tracker.cpp** - Main application

## LVGL UI Features

### Implémenté
- Splash screen at boot
- Dashboard with GPS, LoRa, WiFi, Battery, Storage info
- Setup menu with: Callsign, LoRa Frequency, LoRa Speed, Display, Sound, WiFi, Bluetooth, Reboot
- TX/RX packet popups (LVGL msgbox)
- WiFi eco mode support
- Messages screen avec tabs APRS/Winlink
- Compose screen avec clavier virtuel (optionnel)
- Clavier physique T-Deck intégré
- SD card support avec fallback SPIFFS

### En cours / À faire
- Gestion contacts (CRUD + fichier JSON)
- Gestion messages par contact (inbox/outbox)
- UI écran Contacts LVGL
- UI écran Messages par contact LVGL
- Éviter stockage messages en double
- Rafraîchir liste messages automatiquement
- Réduire durée splash screen

## Storage Structure (SD Card)

```
/LoRa_Tracker/
  ├── Messages/
  │   ├── inbox/      (messages reçus)
  │   └── outbox/     (messages envoyés)
  ├── Contacts/       (répertoire adresses)
  └── Maps/           (cartes offline - futur)
```

- SD card pin: BOARD_SDCARD_CS = 39 (shared SPI with display/LoRa)
- Fallback to SPIFFS if no SD card

## Environment

- Board: T-Deck Plus (ESP32-S3)
- Display: 320x240 TFT with GT911 touch
- Physical keyboard via I2C
- PlatformIO environment: `ttgo_t_deck_plus_433`
