# LoRa FieldOps APRS Tracker — Changelog

Running log of changes in this fork ([KJ7NYE/LoRa_FieldOps_APRS_Tracker](https://github.com/KJ7NYE/LoRa_FieldOps_APRS_Tracker))
on top of upstream [richonguzman/LoRa_APRS_Tracker](https://github.com/richonguzman/LoRa_APRS_Tracker).

Fork divergence point: upstream commit [`bfd531a`](https://github.com/richonguzman/LoRa_APRS_Tracker/commit/bfd531a) — *"winlink challenge fisher-yates update"* (2026-04-23).

Newest entries first. Format: `YYYY-MM-DD — short title (commit)` followed by a brief description.

---

## 2026-11-02 — Sync with upstream `richonguzman/LoRa_APRS_Tracker` (V4.0.1 + 4 bugfixes)

Merged ~6.5 months of upstream work (35 commits since merge-base `bfd531a`, 2026-04-23) into
`loranger-v2`. Target was `upstream/main` (commit `5bb2adc`, one day past the `V4.0.1` tag) so we pick
up two bugfixes — SmartBeacon shortest-angle heading-delta and the `gpsCardinalDirection` typo —
beyond the tagged release.

**Features and bugfixes adopted:**

- **Channel Activity Detection** (CAD + DIFS + BEB) in `src/lora_utils.cpp` — listen-before-talk
  reduces collisions on busy channels. New `Config.cadActive` field (default `true`) plumbed through
  `configuration.cpp`/`.h`, the web POST handler, and the first-boot config. The CAD notice display
  uses our `bootStatus()` API (upstream's `displayShow()` doesn't exist in our display header).
  **Known gap:** no UI toggle for `cadActive` yet in `index.html` / `serial_config.html` / Android
  app — queued as a sync-config-pathways follow-up.
- **GPS baud-rate autodetect** (`detectBaudRate()` / `probeBaudRate()` / SPIFFS persistence).
  Wrapped in `#ifndef ARDUINO_ARCH_NRF52` — the probe calls `gpsSerial.begin(baud, SERIAL_8N1, TX, RX)`
  which the Adafruit nRF52 Serial class doesn't accept. nRF52 continues using fixed `GPS_BAUD`.
- **`gpsCardinalDirection` typo fix** — `bearing >= 5.675` → `5.625` (one of the two upstream
  commits past V4.0.1 that justified merging `upstream/main` instead of the tag).
- **`setDateFromData` year validation** — rejects GPS dates outside 2026–2080 so a cold-boot GPS
  can't stamp the clock with 1970.
- **SmartBeacon shortest-angle heading-delta fix** — upstream used `fabs()` + `360 - delta` wraparound.
  Our fork's `calculateHeadingDelta()` already had the shortest-angle math plus our stronger `speed > 1
  && lastTx >= txInterval` gating; we merged in `fabs()` for symmetry.
- **`BATTERY_CELLS` compile-time multi-cell define** — default `1`, so our current 1S variants are
  unaffected. Groundwork for the Rev 2 LoRanger 2S pack (see `LORANGER_V2_PORT.md` / memory
  `project_rev2_battery.md`). Adoption of the runtime scaling in `station_utils.cpp`
  (`sleepVoltage * BATTERY_CELLS`) deferred — our restructured `sendBeacon()` uses a different code
  path than upstream.
- **`TTGO_T_BEAM_1W` radio setup** — DIO2-as-RF-switch + 800 µs PA ramp time, added to both `setup()`
  and `changeFreq()` (since `setOutputPower()` resets the ramp time to default).
- **`THERMAL_Utils` integration in TX path** — upstream calls `onTxStart()` / `onTxEnd()` around
  `radio.transmit()`. Our `thermal_utils.cpp` (upstream credited our fork in their derivative) now
  participates in TX cooldown timing.
- **Library version bumps** — Adafruit BusIO 1.17.4, ArduinoJson 7.4.3, AsyncTCP 3.5.0,
  ESPAsyncWebServer 3.12.1, TinyGPSPlus 1.1.0, Adafruit GFX 1.12.6, Adafruit SSD1306 2.5.17.
- **CI version bumps** — `actions/checkout@v7`, `setup-python@v7`, Python 3.13.

**Deliberately not adopted:**

- `peterus/esp-logger` and `richonguzman/APRSPacketLib` registry pins — we vendor `lib/Logger/` and
  `lib/APRSPacketLib/1.0.4-loranger` (which carries our `generateObjectPacket()` / PHG /
  Base91 T-byte additions). Our object + PHG features are queued for upstream contribution to
  `richonguzman/APRSPacketLib` in a separate PR session.
- `disableGPS`, `email`, `simplifiedTrackerMode` config fields — removed in our fork pre-merge.
- Menu / Winlink / WX / telemetry / notification / touch / keyboard / joystick / `custom_characters` /
  `custom_colors` source files and headers — fork architectural decision; the features aren't in our
  single-role firmware.
- ~30 unsupported variants (`ESP32C3_SuperMini_*`, `ESP32S3_DIY_*`, `TTGO_T_Deck_*`, `heltec_wifi_lora_32_V3_*`,
  `TTGO_T_BEAM_v*`, `OE5HWN_MeshCom`, `QRPLabs_LightTracker_Plus_1_0`, `RPC_MicroTracker`, `Hydra_*`, etc.) —
  our matrix is `heltec_t114`, `heltec_v3_433_aprs`, `tbeam_433_aprs`, `tbeam_433_1w_aprs`,
  `lilygo_t3_433_aprs`, `LoRanger_V1`, `heltec_wireless_tracker_433_aprs`.
- Upstream V4.0 web UI rewrite in `data_embed/index.html` and `data_embed/script.js` — fork UI is an
  intentional field-ops layout (roles, tactical callsigns, PHG, remote config, log viewer).
- Upstream's `thermal_utils.cpp` — our fork's version is the superset (adds `isOverTemp()` warn flag
  and an `85 °C` shutdown guard). Upstream's file header already credits our fork.
- Fail-hard `while (true) {}` display init in `src/display.cpp` — we keep our fail-soft logger
  error + retry.
- RPC_MicroTracker LED tweaks and HELTEC_WSL_V3_TNC guards — targets we don't build.

**Build validation (all 7 default envs on `loranger-v2`, 2026-11-03):**

| Env | Status |
|---|---|
| `heltec_t114` | ✅ |
| `heltec_v3_433_aprs` | ✅ |
| `heltec_wireless_tracker_433_aprs` | ✅ |
| `lilygo_t3_433_aprs` | ✅ |
| `LoRanger_V1` | ✅ |
| `tbeam_433_1w_aprs` | ✅ |
| `tbeam_433_aprs` | ✅ |

**Follow-ups queued:**

- Add `cadActive` UI toggle to `index.html`, `serial_config.html`, Android app for config-pathway
  parity (see CLAUDE.md "Synchronized configuration pathways" rule).
- Push `generateObjectPacket()` + PHG uncompressed-beacon + Base91 T-byte fix back to upstream
  `richonguzman/APRSPacketLib`.
- Heltec V3 OLED contrast (0xFF for SSD1315 panels) + display init retry — upstream contribution
  candidates to `richonguzman/LoRa_APRS_Tracker` (see memory `project_upstream_contribution_candidates.md`).

Merge commit: [`c8bde5f`](https://github.com/KJ7NYE/LoRa_FieldOps_APRS_Tracker/commit/c8bde5f).

---

## 2026-07-30 — Serial config tool: WiFi STA "Add network" modal + IP-on-connect

The WiFi STA "Add network" flow in the serial config tool only asked for an SSID via a bare
`prompt()` — no way to set a password inline — and Scan was a per-row control, so it only appeared
once a network slot already existed. Replaced both with a single modal (`openAddNetworkModal()`,
built with `bootstrap.Modal` to match the existing Flash Firmware modal's pattern): it scans
automatically on open, shows nearby networks as a clickable list (SSID, signal, lock icon), and lets
you set SSID + password together before committing. Selecting a scan result pre-fills the SSID and
jumps focus to Password; "Enter network manually" skips straight to a blank form for hidden networks.
Per-row Scan buttons/datalists were removed since scanning no longer needs an existing slot to attach
to.

Also added the device's IP address to the "WiFi STA connected" status line (both the serial config
tool and the web config UI), since knowing you're connected without knowing the address it landed on
wasn't very actionable.

Files changed:

- [src/serial_setup.cpp](src/serial_setup.cpp) — `wifista status` now prints `ip=<addr>` when connected
- [src/web_utils.cpp](src/web_utils.cpp) — `/wifi-sta-status.json` now includes `"ip"` when connected
- [serial_config.html](serial_config.html) — new Add Network modal; status line shows IP
- [data_embed/script.js](data_embed/script.js) — status line shows IP

---

## 2026-07-29 — Add Heltec Wireless Tracker (433 MHz) support

New `heltec_wireless_tracker_433_aprs` PlatformIO environment for the Heltec Wireless Tracker V1.1
(ESP32-S3FN8, SX1262, onboard UC6580 GNSS, 0.96" 160×80 ST7735 TFT). This board's support in
`display.cpp`/`power_utils.cpp`/`battery_utils.cpp` (gated on `HELTEC_WIRELESS_TRACKER`) was already
present from upstream but never had a wired-up PlatformIO environment in this fork — this change adds
the variant's `board_pinout.h` + `platformio.ini` (TFT_eSPI compile-time config, native-USB monitor
settings, 8 MB OTA partitions) and threads the new target through CI, the web flasher, and the serial
config tool. Pin map cross-verified against upstream's own `heltec_wireless_tracker` variant and the
Zephyr `heltec_wireless_tracker` board support package.

Files changed:

- [variants/heltec_wireless_tracker_433_aprs/board_pinout.h](variants/heltec_wireless_tracker_433_aprs/board_pinout.h), [variants/heltec_wireless_tracker_433_aprs/platformio.ini](variants/heltec_wireless_tracker_433_aprs/platformio.ini) — new variant
- [platformio.ini](platformio.ini) — add to `default_envs`
- [.github/workflows/commit.yml](.github/workflows/commit.yml), [.github/workflows/flasher.yml](.github/workflows/flasher.yml) — add to CI build matrices
- [tools/generate_manifests.py](tools/generate_manifests.py) — add to web-flasher manifest generator
- [serial_config.html](serial_config.html) — add to board dropdown + chip-family map
- [README.md](README.md), [CLAUDE.md](CLAUDE.md) — hardware tables

---

## 2026-07-11 — iGate: stop re-gating remote IS→RF rebroadcast echoes (duplicate uploads)

A nearby station appeared on APRS-IS twice, both `qAR,<ouriGate>`: once from our direct RX of the
tracker's beacon, and again ~1–2 s later when a distant iGate (running CA2RXU's LoRa_APRS_iGate)
re-transmitted the same packet from APRS-IS back onto RF — rewritten with its own iGate tocall
(`APLRG1`), a fresh `WIDE1-1,WIDE2-1` path, and no `TCPIP` marker, making the echo look like a
brand-new RF beacon. The existing content dedup couldn't collapse the two because the round-trip
through APRS-IS and back out the remote LoRa TX perturbs a byte of the compressed position field,
changing its hash. `processLoRaPacket()` now drops IS→RF echoes up front — recognizing a known
iGate tocall, a `TCPIP` marker, or a third-party (`}`) info field — before the dedup/upload step,
so each beacon is gated exactly once. Also affected the whole echo firehose, not just one station.

Files changed:

- [src/aprs_is_utils.cpp](src/aprs_is_utils.cpp) — echo guard at the top of `processLoRaPacket()`

---

## 2026-07-07 — heltec_v3_433_aprs: fix permanently dark OLED by targeting V3.2 hardware only

`vext_ctrl_ON()`/`OFF()` in `power_utils.cpp` drove `VEXT_CTRL` HIGH-for-on for `HELTEC_V3_433_APRS`, matching the original Heltec WiFi LoRa 32 V3 polarity. Heltec's V3.2 revision inverts this (LOW = peripherals on). On true V3.2 silkscreen boards — which is what's actually sold and what this variant now exclusively targets — the OLED's VEXT rail was never powered, so the display stayed dark in every mode (boot, running, TX) regardless of display/eco-mode/invert settings. Confirmed on two separate V3.2 units.

Moved `HELTEC_V3_433_APRS` into the inverted-polarity branch alongside `HELTEC_V3_2_GPS`/`HELTEC_V3_2_TNC`. This firmware no longer supports original (non-.2) V3 boards for this target.

Files changed:

- [src/power_utils.cpp](src/power_utils.cpp) — invert VEXT polarity for `HELTEC_V3_433_APRS`
- [variants/heltec_v3_433_aprs/board_pinout.h](variants/heltec_v3_433_aprs/board_pinout.h), [variants/heltec_v3_433_aprs/platformio.ini](variants/heltec_v3_433_aprs/platformio.ini) — document V3.2-only support
- [README.md](README.md), [CLAUDE.md](CLAUDE.md) — rename board references from "V3" to "V3.2"
- [tools/gen_manifests.py](tools/gen_manifests.py), [tools/generate_manifests.py](tools/generate_manifests.py), [flasher/manifests/targets.json](flasher/manifests/targets.json), [flasher/manifests/heltec_v3_433_aprs.json](flasher/manifests/heltec_v3_433_aprs.json), [flasher/manifests/heltec_v3_433_aprs_update.json](flasher/manifests/heltec_v3_433_aprs_update.json) — update flasher UI labels

---

## 2026-07-02 — ACK plain messages, not just queries

Previously only APRS capability queries (`?PING?`, `?APRSD`, etc.) got an ACK from the firmware — a plain free-text message addressed to the tracker's callsign or tactical name was displayed (via the "Msg:" indicator) but never acked, leaving the sender unsure whether it was received. The tracker now ACKs any addressed message that carries a sequence number, whether or not it's a query, and does so unconditionally rather than assuming an attached KISS client (phone app, TNC software) will handle it — the common deployment (Tracker/iGate/Digipeater in the field) has no client attached at all. No automated reply is generated for free text, only the ack; duplicate messages from the same sender within 60 s are suppressed the same way duplicate queries already were.

Files changed:

- [src/query_utils.cpp](src/query_utils.cpp), [include/query_utils.h](include/query_utils.h) — ACK plain messages addressed to us, independent of the existing query-dispatch path
- [README.md](README.md) — document the new ACK behavior in the APRS Station Queries section

---

## 2026-07-02 — Documentation sync ahead of release

Brought README.md, CLAUDE.md, SERIAL_SETUP.md, and NRF52_PORT_NOTES.md in line with the current codebase, verified directly against source rather than against each other. README gained the Jetboat SmartBeacon profile, PHG (Power-Height-Gain) beaconing, tactical-object query routing, and the "Msg:"/object-name-"Last:" display behavior, plus a link to SERIAL_SETUP.md. CLAUDE.md picked up the missing `tbeam_433_1w_aprs` environment and a `query_utils.cpp` row in the source file map, and corrected a stale `build.yml` CI filename reference. SERIAL_SETUP.md had a substantial correction pass: it documented a multi-beacon, multi-region-preset, `notif`/`telem`/`winlink` command architecture that doesn't exist in this fork's single-beacon `serial_setup.cpp` — the Command Reference, Behavior Notes, and Config Field Map sections were rebuilt directly from the actual command dispatch table. NRF52_PORT_NOTES.md, a historical design/handoff doc, got a full accuracy pass: removed a "notification subsystem" pin-conflict section describing config fields that never existed in this repository's history, corrected claims that the T114 TFT uses software SPI (it uses a second hardware SPI peripheral), moved already-shipped "T114 hardware glue" and "hardware SPI" items out of Future PRs, and fixed stale `heltec_wireless_tracker`/`LoRa_APRS_Tracker.cpp` references to their current names.

Also fixed two related non-doc inconsistencies: the CI build matrix (`commit.yml`) was missing the `tbeam_433_1w_aprs` environment that every other build/release list already had, and `lib/APRSPacketLib/library.json` declared an MIT license that contradicted its own `LICENSE` file and README (both GPL-3.0).

Files changed:

- [README.md](README.md), [CLAUDE.md](CLAUDE.md), [SERIAL_SETUP.md](SERIAL_SETUP.md), [NRF52_PORT_NOTES.md](NRF52_PORT_NOTES.md)
- [.github/workflows/commit.yml](.github/workflows/commit.yml) — add missing `tbeam_433_1w_aprs` to the CI build matrix
- [lib/APRSPacketLib/library.json](lib/APRSPacketLib/library.json) — fix license field to match the actual GPL-3.0 license

---

## 2026-07-02 — Show object names on "Last:" and a "Msg:" received indicator

Two status-display improvements. First, when the most recently heard packet is an APRS Object Report (`;OBJECTNAME*...`), the "Last:" line now shows the reported object's name instead of the transmitting station's callsign-SSID — the `?APRSD`/`?APRSH`/`?APRSL` heard-station log still tracks the real callsign, only the display summary changed. Second, any addressed or broadcast message packet (a query like `?PING?` or a plain free-text message) now flashes a `Msg: <text>` indicator on the T114's bottom display row; it stays up until the next RX event refreshes the heard-station log, rather than on a fixed timer.

Files changed:

- [src/station_utils.cpp](src/station_utils.cpp), [include/station_utils.h](include/station_utils.h) — object-name-aware `updateLastHeard()`; new `setPendingMessage()`/`getPendingMessage()` transient message state
- [src/query_utils.cpp](src/query_utils.cpp) — captures addressed message text into the pending-message display state
- [src/main.cpp](src/main.cpp) — `updateLastHeard()` now runs before digi/forwarding/query dispatch each RX cycle; line6 construction prefers a pending message over the last-heard summary

---

## 2026-07-02 — Answer queries addressed to the tactical object

APRS station capability queries (see 2026-06-15 entry below) are now also recognized when addressed to the configured tactical object name (`beacons.0.tacticalCallsign`), not just the device's real callsign. `?APRSP` / `?APRS?` addressed to the tactical name reply with the Object Report (as already emitted by `sendBeacon()` when a tactical name is set); all other directed queries (`?APRSD ?APRSH ?APRSL ?APRSS ?APRST ?APRSV ?PING? ?VER`) reply the same way they do for the real callsign. The digipeater's own-address guard was extended to match, so messages addressed to the tactical object also aren't needlessly re-relayed.

Files changed:

- [src/query_utils.cpp](src/query_utils.cpp), [include/query_utils.h](include/query_utils.h) — addressee routing also matches the configured tactical object name
- [src/digi_utils.cpp](src/digi_utils.cpp) — same-station relay guard extended to the tactical object name

---

## 2026-06-15 — Add APRS station capability query support

All device roles (Tracker, iGate, Digipeater) now respond to APRS station capability queries (APRS 1.01 §13). Queries arrive as directed APRS message packets addressed to the device's own callsign, or as undirected broadcasts to the `APRS` / `IGATE` symbolic addresses. Query keywords are matched case-insensitively. Duplicate queries from the same sender are suppressed for 60 seconds. Incoming messages with a sequence number receive an ACK before the response.

Supported queries:

| Query | Addressee | Responded by | Notes |
|---|---|---|---|
| `?APRS?` | `APRS` | All roles | Triggers a position beacon |
| `?APRSD` | Own callsign | All roles | Direct-heard station list |
| `?APRSH <CALL>` | Own callsign | All roles | Has-heard lookup with elapsed time |
| `?APRSL` | Own callsign | All roles | All recently heard stations |
| `?APRSP` | Own callsign | All roles | Position beacon |
| `?APRSS` | Own callsign | All roles | Status text from config |
| `?APRST` / `?PING?` | Own callsign | All roles | Ping reply |
| `?APRSV` / `?VER` | Own callsign | All roles | Firmware version string |
| `?IGATE?` | `IGATE` | iGate only | iGate-online confirmation |

Also adds a digipeater guard: message packets addressed to the device's own callsign are never relayed, preventing IS→RF→IS loops.

Files changed:

- New: [include/query_utils.h](include/query_utils.h), [src/query_utils.cpp](src/query_utils.cpp) — query parsing and dispatch
- [include/station_utils.h](include/station_utils.h), [src/station_utils.cpp](src/station_utils.cpp) — `updateLastHeard()` now takes the full raw packet; 20-slot heard-station ring buffer with direct/digipeated flag; new `getDirectHeardList()`, `getAllHeardList()`, `minutesSinceHeard()` accessors
- [src/digi_utils.cpp](src/digi_utils.cpp) — drop message packets addressed to own callsign before digipeating
- [src/main.cpp](src/main.cpp) — wire `QUERY_Utils::processLoRaPacket()` into the RX dispatch block

---

## 2026-05-03 — Add nRF52 platform layer + Heltec T114 variant (PR-1)

First nRF52840 board lands in the same repo as the ESP32 fleet — Heltec T114
(off-the-shelf nRF52840 + SX1262 + L76K GPS + ST7789 1.14" 240×135 TFT).
Future nRF boards (`LoRanger_V1_nRF`, RAK4631, etc.) become 2-file variant
additions on top of this layer. See [NRF52_PORT_NOTES.md](NRF52_PORT_NOTES.md)
for the design rationale and notes for future maintainers.

Five logical steps, six commits:

- **`9e64f69` — Steps 1+2** (capability flags + nRF platform): introduce
  `HAS_WIFI` / `HAS_NIMBLE` / `HAS_WEB_UI` / `HAS_DISPLAY` in
  [common_settings.ini](common_settings.ini) to gate ESP-only subsystems
  behind `#ifdef`; add `[nrf52_common]` block,
  [variants/heltec_t114/](variants/heltec_t114/), and
  [include/nrf52_shims/](include/nrf52_shims/) headers that intercept
  `<SPIFFS.h>` / `<logger.h>` on nRF builds; wire `#ifdef ARDUINO_ARCH_NRF52`
  arms across 12 source files (SPIFFS↔InternalFS, `ESP.restart()`↔
  `NVIC_SystemReset()`, `Wire/SPI.begin` no-args, `ledc` PWM→`tone()`,
  `gpsSerial`→`Serial1`, `esp_sleep_*`→`SYSTEMOFF`, etc).
- **`ba83b1f` — Step 3**: ST7789 driver path in [src/display.cpp](src/display.cpp)
  behind `HAS_TFT_ST7789` — software SPI on dedicated TFT pins,
  Adafruit_GFX-based, text-only rendering.
- **`247161e` — Step 4**: first-boot embedded defaults. New
  [tools/embed_config.py](tools/embed_config.py) pre-build script reads
  [data/tracker_conf.json](data/tracker_conf.json) and emits a generated
  C-string header that [src/configuration.cpp](src/configuration.cpp) writes
  straight to LittleFS on first boot. ESP32 first-boot path unchanged.
- **`15c1a75` — Step 5**: FCC TX-gate at [src/lora_utils.cpp](src/lora_utils.cpp)
  `sendNewPacket` chokepoint using the existing `APRSPacketLib::checkNocall()`
  validator on the packet's source-callsign field. Active on **both** platforms —
  every TX path (beacon, status, message, telemetry, digipeat, BLE/BT-Classic
  KISS injection) funnels through this single function.
- **`552ae38` — T114 hardware glue**: drive `VEXT_ENABLE` (P0.21) HIGH for
  L76K GPS power; gate battery ADC reads with `ADC_CTRL_PIN` (P0.6) HIGH
  before sample, LOW after.

`heltec_t114` build: flash 40.9 % (333 KB / 815 usable), RAM 32.3 %
(80 KB / 248 usable). UF2-flashable `firmware.zip` produced in
`.pio/build/heltec_t114/`. ESP32 envs unchanged from baseline
(`heltec_wireless_tracker` measured at 46.9 % flash / 17.3 % RAM throughout).

## 2026-05-03 — Enlarge Serial RX buffer so `import` paste survives LoRa stalls

The default ESP32 Arduino `Serial` RX buffer is 256 bytes — about 22 ms of
115200-baud traffic. The main loop ([src/LoRa_APRS_Tracker.cpp](src/LoRa_APRS_Tracker.cpp))
can stall longer than that during LoRa RX/TX, which silently dropped bytes
during a multi-KB `import` paste in the serial setup CLI; with bytes lost,
the brace-balance heuristic never fires and the import appears to hang.

- [src/LoRa_APRS_Tracker.cpp](src/LoRa_APRS_Tracker.cpp) — `Serial.setRxBufferSize(16384)`
  before `Serial.begin()`. Matches `SERIAL_Setup::PASTE_MAX_BYTES`, so any
  legal import paste fits even if the loop hangs the entire transfer.
  Costs 16 KB of the 327 KB heap (≈5 %).

## 2026-05-02 — Add APRS Object reports via per-beacon tactical callsign

New per-beacon `tacticalCallsign` field. When set (≤9 chars), the tracker emits
an APRS Object report (DTI `;`) labeled with that name instead of a Position
report. Source address remains the operator's licensed callsign — the tactical
name is purely the displayed object label, satisfying the "tactical event
support" use case (SAR, ultra-marathons, jetboat racing) without violating FCC
station-ID requirements. Empty value preserves today's position-report behavior.
Mic-E is silently bypassed when tactical is set (it can't carry an object name).

- [lib/APRSPacketLib/](lib/APRSPacketLib/) — APRSPacketLib forked locally
  (`1.0.4-loranger`); registry pin in `common_settings.ini` removed.
- [lib/APRSPacketLib/src/APRSPacketLib.cpp](lib/APRSPacketLib/src/APRSPacketLib.cpp),
  [lib/APRSPacketLib/include/APRSPacketLib.h](lib/APRSPacketLib/include/APRSPacketLib.h) —
  new `generateObjectPacket()` helper, symmetrical with the existing
  `generateBase91GPSBeaconPacket()` / `generateMiceGPSBeaconPacket()`. Pads
  object name to the spec-required 9 chars in one place.
- [include/configuration.h](include/configuration.h),
  [src/configuration.cpp](src/configuration.cpp),
  [data/tracker_conf.json](data/tracker_conf.json) — new
  `tacticalCallsign` field on each beacon, defaults to empty.
- [src/station_utils.cpp](src/station_utils.cpp) — `sendBeacon()` branches on
  tactical-callsign; builds DDHHMMz timestamp from current GPS time.
- [src/serial_setup.cpp](src/serial_setup.cpp) — `beacon tactical <text>` CLI
  command; field shown in `beacon list` output.
- [src/web_utils.cpp](src/web_utils.cpp),
  [data_embed/script.js](data_embed/script.js) — web config form gains a
  tactical-callsign input.
- [SERIAL_SETUP.md](SERIAL_SETUP.md) — beacon command table updated.

## 2026-05-02 — Add user-configurable SmartBeacon profile (index 3 = Custom)

A 4th SmartBeacon profile is now editable at runtime via the serial CLI, so
event-specific cadence/turn settings can be dialed in without recompiling.

- [include/smartbeacon_utils.h](include/smartbeacon_utils.h),
  [src/smartbeacon_utils.cpp](src/smartbeacon_utils.cpp) — array grows from 3
  to 4 (Runner / Bike / Car / **Custom**); `checkSettings()` clamps
  out-of-range values to 0 with a warning instead of crashing; new
  `setCustomValues()` and `profileLabel()` helpers.
- [include/configuration.h](include/configuration.h),
  [src/configuration.cpp](src/configuration.cpp) — new `customSmartBeacon`
  object in `tracker_conf.json` (8 fields: `slowRate`, `slowSpeed`,
  `fastRate`, `fastSpeed`, `minTxDist`, `minDeltaBeacon`, `turnMinDeg`,
  `turnSlope`). Missing keys auto-rewrite on first boot.
- [src/serial_setup.cpp](src/serial_setup.cpp) — new `smartcustom` command
  group (`show`, `slowrate`, `slowspeed`, `fastrate`, `fastspeed`,
  `mintxdist`, `mindelta`, `turnmindeg`, `turnslope`); changes take effect
  live (no save+reboot required to retune). `beacon smartset` now validates
  `0..3` and rejects out-of-range values; `show beacons` annotates the
  profile name (Runner/Bike/Car/Custom). Full reference:
  [SERIAL_SETUP.md](SERIAL_SETUP.md).

## 2026-05-02 — Add `import` / `export` commands to serial setup CLI ([`d56d83b`](../../commit/d56d83b))

[src/serial_setup.cpp](src/serial_setup.cpp) gains two top-level commands for
JSON-level config replication without WiFi or external tooling:

- **`export`** — streams the on-disk `tracker_conf.json` to the terminal between
  `---- BEGIN ----` / `---- END ----` markers.
- **`import`** — accepts a pasted full JSON, auto-detects end of paste via
  string-aware brace balancing, validates (parse + non-empty
  `beacons[0].callsign`), writes to SPIFFS, and reboots. 16 KB buffer cap;
  Ctrl-C aborts cleanly. Refuses to start if there are unsaved CLI edits.

Also: the unused `boolStr` helper was removed to silence a `-Wunused-function`
warning. Full reference: [SERIAL_SETUP.md](SERIAL_SETUP.md).

## 2026-05-02 — `tracker_conf` standard settings for event support ([`032df10`](../../commit/032df10))

Updated the default [data/tracker_conf.json](data/tracker_conf.json) to event-friendly defaults:

- `other.sendCommentAfterXBeacons`: **10 → 4**
- `other.path`: **`WIDE1-1` → `WIDE2-2`**
- `lora[0]` (EU 433.775 MHz): **SF12/125 kHz → SF8/62.5 kHz**

Goal: more frequent comments and broader digi reach during event use, with a faster RF profile on the EU channel.

## 2026-05-02 — Tune Car SmartBeacon `minDeltaBeacon` 12s → 10s ([`9bdebcc`](../../commit/9bdebcc))

[src/smartbeacon_utils.cpp](src/smartbeacon_utils.cpp) — shortens the minimum spacing
between beacons in the Car profile from 12s to 10s.

## 2026-05-02 — Add serial setup CLI for USB config without WiFi AP ([`be37b9f`](../../commit/be37b9f))

New USB-serial command-line interface (115200 baud) for first-flash provisioning,
scripted bulk configuration, or quick edits while tethered — no need to bring up
the `LoRaTracker-AP` web UI.

- New: [include/serial_setup.h](include/serial_setup.h), [src/serial_setup.cpp](src/serial_setup.cpp)
- Wired into [src/LoRa_APRS_Tracker.cpp](src/LoRa_APRS_Tracker.cpp) and [src/configuration.cpp](src/configuration.cpp)
- Logger is paused (errors only) while in setup mode
- Full reference: [SERIAL_SETUP.md](SERIAL_SETUP.md)

## 2026-05-01 — 30-second startup AP window for web config ([`87be561`](../../commit/87be561))

[src/wifi_utils.cpp](src/wifi_utils.cpp) — every boot now opens `LoRaTracker-AP`
for 30 seconds so the web config is reachable without needing `wifiAP.active` or
`NOCALL-7`. If a client connects during the window, the existing 2-minute idle
timeout takes over so config sessions aren't cut off.

## 2026-04-30 — Tune Car SmartBeacon profile ([`4ec4b9f`](../../commit/4ec4b9f))

[src/smartbeacon_utils.cpp](src/smartbeacon_utils.cpp):

- `fastRate`: **60s → 10s**
- `fastSpeed`: **70 → 110 km/h**

Denser beaconing at highway speeds with a higher kick-in threshold.

## 2026-04-30 — Add LoRanger V1 board variant ([`1898fa2`](../../commit/1898fa2))

Initial fork commit. Adds the LoRanger V1 hardware variant — custom ESP32-S3
+ 1 W SX1268 + ATGM336H GPS tracker with hardwired LDO rails.

- New: [variants/LoRanger_V1/board_pinout.h](variants/LoRanger_V1/board_pinout.h),
  [variants/LoRanger_V1/platformio.ini](variants/LoRanger_V1/platformio.ini)
- Touched: [include/configuration.h](include/configuration.h),
  [src/battery_utils.cpp](src/battery_utils.cpp),
  [src/configuration.cpp](src/configuration.cpp),
  [src/lora_utils.cpp](src/lora_utils.cpp)
- Some upstream `LIGHTTRACKER_PLUS_1_0` `#ifdef` branches were broadened to also
  match `LORANGER_V1`; others were intentionally left alone.

---

## How to update this file

When you add a fork commit, prepend a new section above using:

```
## YYYY-MM-DD — Short title ([`d56d83b`](../../commit/d56d83b))

One- or two-paragraph description: what changed, which files, and why.
```

The `../../commit/<hash>` link resolves on GitHub from any branch view of this file.
