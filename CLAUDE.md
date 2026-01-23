# Claude Code Context

## Project

LoRa APRS Tracker - Fork with LVGL touchscreen UI for T-Deck Plus

## Versioning

**Format:** vMAJOR.MINOR.PATCH (semver)

- MAJOR: Breaking changes
- MINOR: New features
- PATCH: Bug fixes

Current: **v1.3.0**

See `VERSIONING.md` for full details and history.

## Key Files

- `src/lvgl_ui.cpp` - Main LVGL UI code
- `src/msg_utils.cpp` - Message handling
- `docs/index.html` - Web flasher (update version here)
- `docs/firmware/` - Compiled binaries for web flasher

## Workflow

1. Code changes → compile → test
2. Commit with convention: `feat:`, `fix:`, `refactor:`
3. Push to `fork` remote (not `origin`)
4. Update `docs/firmware/*.bin` and `docs/index.html` version

## Remotes

- `origin` = upstream (richonguzman/LoRa_APRS_Tracker)
- `fork` = user fork (moricef/LoRa_APRS_Tracker) ← push here

## TODO - Refactoring

### Modulariser lvgl_ui.cpp (~4500 lignes)

Extraire en modules séparés :
- `ui_messaging.cpp` - Messages, conversations, contacts
- `ui_settings.cpp` - Écrans de configuration (callsign, display, sound, wifi, speed)
- `ui_popups.cpp` - Popups TX/RX/notifications
- `ui_dashboard.cpp` - Écran principal dashboard
- `ui_compose.cpp` - Écran de composition de messages

Garder dans `lvgl_ui.cpp` :
- Initialisation LVGL
- Navigation principale
- Variables globales partagées
