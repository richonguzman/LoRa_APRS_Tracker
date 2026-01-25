# Claude Code Context

## Project

LoRa APRS Tracker - Fork with LVGL touchscreen UI for T-Deck Plus

## Versioning

**Format:** vMAJOR.MINOR.PATCH (semver)

- MAJOR: Breaking changes
- MINOR: New features
- PATCH: Bug fixes

Current: **v1.4.0**

See `VERSIONING.md` for full details and history.

## Key Files

- `src/lvgl_ui.cpp` - LVGL initialization + navigation (~730 lines)
- `src/ui_dashboard.cpp` - Dashboard screen
- `src/ui_messaging.cpp` - Messages, conversations, contacts, compose, frames, stats
- `src/ui_settings.cpp` - Settings screens (callsign, display, sound, wifi, speed)
- `src/ui_popups.cpp` - Popups TX/RX/notifications
- `src/ui_map_manager.cpp` - Map functionality
- `src/msg_utils.cpp` - Message handling
- `docs/index.html` - Web flasher (update version here)
- `docs/firmware/` - Compiled binaries for web flasher

## Rôles

- **Claude** : Écriture et modification du code uniquement. **NE JAMAIS compiler** (`pio run`).
- **Utilisateur** : Compilation (`pio run`) et upload (`pio run -t upload`). C'est l'utilisateur qui compile, pas Claude.

## Workflow

1. Code changes → compile → test
2. Commit with convention: `feat:`, `fix:`, `refactor:`
3. Push to `fork` remote (not `origin`)
4. Update `docs/firmware/*.bin` and `docs/index.html` version

## Remotes

- `origin` = upstream (richonguzman/LoRa_APRS_Tracker)
- `fork` = user fork (moricef/LoRa_APRS_Tracker) ← push here

## LVGL Fonts

Polices Montserrat disponibles (à activer dans lv_conf.h si nécessaire) :

| Macro | Taille | Usage dans code |
|-------|--------|-----------------|
| LV_FONT_MONTSERRAT_12 | 12px | `&lv_font_montserrat_12` |
| LV_FONT_MONTSERRAT_14 | 14px | `&lv_font_montserrat_14` |
| LV_FONT_MONTSERRAT_16 | 16px | `&lv_font_montserrat_16` |
| LV_FONT_MONTSERRAT_18 | 18px | `&lv_font_montserrat_18` |
| LV_FONT_MONTSERRAT_20 | 20px | `&lv_font_montserrat_20` |
| ... | ... | ... |
| LV_FONT_MONTSERRAT_48 | 48px | `&lv_font_montserrat_48` |

**Note:** Seules les polices activées dans `lv_conf.h` sont disponibles à la compilation.

## Refactoring (Completed)

### Modularisation lvgl_ui.cpp ✓

| Module | Lignes | Contenu |
|--------|--------|---------|
| `ui_dashboard.cpp` | 476 | Écran principal |
| `ui_messaging.cpp` | 1676 | Messages, conversations, contacts, compose, frames, stats |
| `ui_popups.cpp` | 505 | Popups TX/RX/notifications |
| `ui_settings.cpp` | 1822 | Écrans de configuration |
| `ui_map_manager.cpp` | 1739 | Carte |
| `lvgl_ui.cpp` | 730 | Init LVGL + navigation |

**Total : ~7300 lignes** réparties en 6 modules.
