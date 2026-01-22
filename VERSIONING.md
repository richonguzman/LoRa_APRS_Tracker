# Versioning Schema

Format: **vMAJOR.MINOR.PATCH** (semver)

## Rules

| Type | When to increment | Example |
|------|-------------------|---------|
| **MAJOR** | Breaking changes, UI overhaul, new architecture | v1.0.0 → v2.0.0 |
| **MINOR** | New features | v1.3.0 → v1.4.0 |
| **PATCH** | Bug fixes, optimizations | v1.3.0 → v1.3.1 |

## Current Version

**v1.3.0** (2025.01.22)

## Version History

| Version | Date | Changes |
|---------|------|---------|
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
