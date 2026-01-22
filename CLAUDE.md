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
