# Versioning Schema

Format: **vMAJOR.MINOR.PATCH** (semver)

## Rules

| Type | When to increment | Example |
|------|-------------------|---------|
| **MAJOR** | Breaking changes, UI overhaul, new architecture | v1.0.0 → v2.0.0 |
| **MINOR** | New features | v1.3.0 → v1.4.0 |
| **PATCH** | Bug fixes, optimizations | v1.3.0 → v1.3.1 |

## Current Version

**v2.7.3+dev** (2026.02.21)

## Version History

| Version | Date | Changes |
|---------|------|---------|
| v2.7.3+dev | 2026.03.03 | **Perf**: SD chunked DMA reads + display DMA flush (IceNav-v3) |
| v2.7.2+dev | 2026.03.02 | **Core**: Migrate logger.log() → ESP_LOG*, fix Web-Conf reentrancy |
| v2.7.1+dev | 2026.03.01 | **UI**: Fix Web-Config touch/watchdog + PSRAM LVGL allocator |
| v2.7.0+dev | 2026.03.01 | **Map**: NPK2 support, multi-region roaming & Zoom 6-17 |
| v2.6.0+dev | 2026.02.21 | NAV Delta+ZigZag+VarInt format, Jordi C++ generator compat |
| v2.5.1+dev | 2026.02.18 | GPX trace recorder, HDOP jitter filter, station traces TTL |
| v2.5.0+dev | 2026.02.04 | Stats persistence, NAV cache PSRAM, VLW Unicode font |
| v2.4.2 | 2026.01.26 | Stable release: vector maps, dual modes, memory fixes |
| v2.4.1 | 2026.01.25 | Map engine optimizations, memory fixes, UI polish |
| v1.6.0 | 2026.01.20 | **Map**: Vector Map support (NAV format) |
| v1.5.0 | 2026.01.15 | **Core**: WiFi Station mode, APRS-IS client, Sound menu |
| v1.4.0 | 2026.01.10 | **Core**: Watchdog timer, BLE eco mode, popup fixes |
| v1.3.0 | 2025.01.22 | Fix message deletion popup, sent messages in conversations |
| v1.2.0 | 2025.01.xx | Contact management |
| v1.1.0 | 2025.01.xx | OSM map with offline tiles |
| v1.0.0 | 2025.01.xx | First stable LVGL UI release |

## Commit Convention

```
feat: new feature           → MINOR++
fix: bug fix                → PATCH++
refactor: code refactoring  → depends on impact
breaking: breaking change   → MAJOR++
docs: documentation only    → no version change
```

## Files to Update

When releasing a new version:
1. `docs/index.html` - Update version number and date
2. `docs/firmware/` - Copy new firmware binaries
3. `VERSIONING.md` - Update version history
