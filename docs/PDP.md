# WLED TOUCHSCREEN CONTROLLER
## Product Development Plan v2.0

**Created:** February 4, 2026
**Updated:** February 8, 2026
**Status:** Phase 8B Design Complete — Ready for 8B Implementation
**Platform:** Waveshare ESP32-S3-Touch-LCD-1.28

**See also:** `docs/SOP.md` for development workflow, Git procedures, and hardware lessons.

---

## EXECUTIVE SUMMARY

**What we're building:**
A standalone handheld controller for WLED-powered art installations (Wayfinder Landmarks). Discovers WLED devices on the local network and provides intuitive touchscreen + encoder control over brightness, effects, palettes, and color — without needing a phone or laptop.

**Why standalone first:**
This controller shares the same hardware and many of the same patterns as the planned poi controller (Phase 8 of LED-Prop-Controller-PDP). Building it first lets us prove out WiFi, WLED API, UI, and modular architecture in a simpler context before adding motion-reactivity complexity.

**Core hardware:**
- Waveshare ESP32-S3-Touch-LCD-1.28 (1.28" round display, touch, encoder)
- WiFi for WLED device communication
- No LEDs attached — this controls external WLED devices only

---

## PHASE STATUS OVERVIEW

| Phase | Name | Status | Notes |
|-------|------|--------|-------|
| 1 | Project Skeleton | ✅ Complete | PlatformIO, serial verified |
| 2 | WiFi Connection | ✅ Complete | Hardcoded creds, reconnect working |
| 3 | Device Discovery | ✅ Complete | mDNS scan, 7 WLED devices found |
| 4 | Display | ✅ Complete | GC9A01 working, boot sequence with splash |
| 5 | WLED API | ✅ Complete | All commands working, 228 effects, 75 palettes |
| 6 | Input Systems | ✅ Complete | Encoder, button, touch, gestures all working |
| 7A | UI: Device List | ✅ Complete | Scrollable list, encoder nav, tap/press to select |
| 7B | UI: Device Control | ✅ Complete | Brightness arc, encoder control, two-pass rendering |
| 7C | UI: Effect Browser | ✅ Complete | 50 named effects, scrollable, encoder select |
| 7D | UI: Palette Browser | ✅ Complete | 50 named palettes, shared browser pattern |
| 7E | UI: Color Picker | ✅ Complete | HSV hue ring, touch select, encoder saturation |
| 7F | UI: Power & Navigation | ✅ Complete | Swipe up power toggle, all gestures wired |
| 8A | Refinements | ✅ Complete | Caching, NVS persistence, smart boot, error handling, rescan |
| 8B | Unified Control Design | ✅ Complete | 9-view architecture, all overlays designed, sizing validated |
| 8B | Unified Control Build | ⬜ Pending | Implementation from spec |
| 8C | Device Groups | ⬜ Pending | Group storage, UDP sync, manage UI |
| 8D | Presets | ⬜ Pending | Save/recall, MIDI grid (future) |

---

## ARCHITECTURE

### File Structure
```
src/
  main.cpp            — Orchestrator: init and update calls only
  wifi_manager.cpp    — WiFi connect, reconnect, status (mock-aware)
  discovery.cpp       — mDNS scan, device list management (mock-aware)
  wled_api.cpp        — HTTP JSON commands to WLED devices (mock-aware)
  display.cpp         — TFT_eSPI init, drawing primitives
  input.cpp           — Encoder + touch handling
  ui.cpp              — Screen states, navigation, ties modules together
include/
  wifi_manager.h / discovery.h / wled_api.h / display.h / input.h / ui.h
docs/
  PDP.md                        — This file (project status, phases, decisions)
  SOP.md                        — Development workflow, Git procedures, hardware lessons
  UNIFIED-CONTROL-SPEC.md       — Phase 8B implementation blueprint
  unified-control-v6.html       — Visual mockup reference (open in browser)
```

### Module Architecture
See `docs/SOP.md` → Modular Architecture Rules for the dependency matrix.

---

## COMPLETED PHASES (1-7)

### Phases 1-6 Summary
- Project skeleton with PlatformIO, verified build flags
- WiFi connection with reconnect handling
- mDNS device discovery (7 WLED devices on network)
- GC9A01 round display working with TFT_eSPI
- Full WLED JSON API (228 effects, 75 palettes, all commands verified)
- Encoder (interrupt-driven, direction corrected), touch (CST816S with gesture lockout), all inputs consume-on-read

### Phase 7: UI Integration ✅ COMPLETE

**Navigation Map (Current — Will Be Replaced by 8B)**

| From | Action | To |
|------|--------|----|
| Device List | Tap / Encoder Press | Device Control |
| Device Control | Encoder Rotate | Adjust Brightness |
| Device Control | Encoder Press | Effect Browser |
| Device Control | Swipe Left | Palette Browser |
| Device Control | Swipe Right | Color Picker |
| Device Control | Swipe Up | Toggle Power ON/OFF |
| Device Control | Swipe Down | Device List |
| Device Control | Long Press (enc/touch) | Identify Device (pulse) |
| Effect Browser | Encoder Rotate/Press | Scroll / Apply → Control |
| Palette Browser | Encoder Rotate/Press | Scroll / Apply → Control |
| Color Picker | Touch ring / Encoder | Select hue / Adjust saturation |

**Key Learnings:**
- Generic browser functions eliminate duplicate code — effect and palette browsers share 100% of scroll/select logic
- HSV color model maps naturally to circular display (angle = hue)
- 50 named effects and 50 named palettes in lookup tables; falls back to ID for higher numbers
- Code was rebuilt after context loss between sessions — PDP spec was detailed enough to reconstruct from
- **Git lesson:** Always commit working code BEFORE updating the PDP to mark it complete (see SOP)

---

## PHASE 8A: REFINEMENTS ✅ COMPLETE

### Completed
- [x] Effect/palette count caching (avoid HTTP round-trip on browser entry)
- [x] NVS persistence (last device, brightness, effect restored on boot)
- [x] Smart boot sequence (WiFi → scan → restore last device → show control)
- [x] Error handling (WiFi lost, device unreachable, scan empty)
- [x] Rescan capability from device list
- [x] Swipe sensitivity improvements

---

## PHASE 8B: UNIFIED CONTROL

### Design ✅ COMPLETE
Full design documented in `docs/UNIFIED-CONTROL-SPEC.md` with visual reference in `docs/unified-control-v6.html`.

**Key design decisions:**
- 9-view architecture: 1 base screen + 8 overlays
- Overlay system replaces multi-screen navigation
- "Stage and send" pattern for batched API updates
- 14 effect categories for organized browsing
- Device groups with UDP sync capability
- Preset save/recall system
- Color source indicator (palette vs custom color)
- Focus mode: tap parameter → encoder adjusts → press to exit

### Implementation ⬜ PENDING
Suggested sub-phases from spec:
- [ ] 8B-1: Overlay framework (state machine, gesture routing)
- [ ] 8B-2: Main control screen (arc, hero, bars, swatches)
- [ ] 8B-3: Device panel + groups (multi-select, UDP)
- [ ] 8B-4: Effect system (categories, drawer, full params)
- [ ] 8B-5: Presets (save/recall, NVS storage)

---

## PHASE 8C: DEVICE GROUPS & PRESETS ⬜ PENDING

### Device Groups
- [ ] Create named device groups (e.g., "North Wall", "All Beacons")
- [ ] Group selection UI — tap to load, auto-check devices
- [ ] Group control via UDP unicast (specific devices) or broadcast (all)
- [ ] NVS storage: `grp_N_name`, `grp_N_devs` (comma-separated device names)
- [ ] Manage groups UI (create, edit, delete)
- [ ] Max ~10 groups, ~10 devices per group

### Presets
- [ ] Preset save: snapshot current staged state + name
- [ ] Preset recall: encoder scroll on main screen preset box
- [ ] NVS storage: `pre_N_name`, `pre_N_data` (JSON string)
- [ ] Max ~20 presets
- [ ] Apply to any device or group (not tied to specific hardware)

### MIDI Grid (Future — Phase 8D or 9)
- 4×3 trigger pad grid, 4 pages = 48 preset slots
- Tap to fire preset, long-press to reassign
- Placeholder in 8B design, implementation deferred

---

## HARDWARE REFERENCE

### Full Pin Mapping

**Display (GC9A01 - SPI)**
| Function | GPIO |
|----------|------|
| MOSI | 11 |
| SCLK | 10 |
| CS | 9 |
| DC | 8 |
| RST | 14 |
| MISO | 12 |
| Backlight | 2 |

**Touch (CST816S - I2C)**
| Function | GPIO |
|----------|------|
| SDA | 6 |
| SCL | 7 |
| INT | 5 |
| RST | 13 |

**Rotary Encoder (External)**
| Function | GPIO |
|----------|------|
| CLK | 15 |
| DT | 16 |
| SW | 17 |

### Board Specs
- **MCU:** ESP32-S3
- **Flash:** 16MB
- **PSRAM:** 2MB
- **Display:** GC9A01, 240x240, round, SPI
- **Touch:** CST816S, I2C
- **IMU:** QMI8658 (present but unused)
- **USB:** CH343P (UART, not native CDC)
- **Note:** Non-ring-encoder board variant

---

## DECISION LOG

| Date | Decision | Rationale |
|------|----------|-----------|
| 2026-02-04 | Standalone controller first | Simpler context before poi complexity |
| 2026-02-04 | Modular architecture from day one | Previous monolithic code caused issues |
| 2026-02-04 | HTTP JSON API first, UDP later | Simpler to debug |
| 2026-02-05 | Match poi project platformio.ini exactly | Missing flags caused black screen |
| 2026-02-05 | Never end test sequences on full white | Max current risks PSU damage |
| 2026-02-05 | Gesture lockout until finger lift | CST816S repeats gesture ID while touching |
| 2026-02-05 | Consume-on-read input pattern | Prevents double-processing of events |
| 2026-02-05 | Break Phase 7 into sub-phases | Test each screen before building next |
| 2026-02-05 | Two-pass rendering in ui_update | Prevents screen transition timing bugs |
| 2026-02-06 | Mock mode via compile flag | Enables UI dev without WiFi/WLED devices |
| 2026-02-07 | Generic browser functions | Effect + palette browsers share all logic |
| 2026-02-07 | HSV hue ring for color picker | Natural fit for round display |
| 2026-02-07 | Validate on hardware before Git commit | Ensures repo always has working code |
| 2026-02-07 | PDP lives in repo (docs/) | Documentation stays with the code |
| 2026-02-07 | Staged edits for unified control | Compose a look before sending — no half-state flicker |
| 2026-02-07 | Break Phase 8 into sub-phases | Refinements → Unified UI → Groups/Presets progression |
| 2026-02-08 | 9-view overlay architecture | Everything visible on one screen, details in overlays |
| 2026-02-08 | 14 effect categories | Reduces 228 effects to manageable browsing |
| 2026-02-08 | Stage and send pattern | Batch changes before transmitting, reduces API calls |
| 2026-02-08 | UDP for group control | Near-instant sync vs sequential HTTP |
| 2026-02-08 | Design spec + HTML mockup in repo | Visual reference + text blueprint for implementation |
| 2026-02-08 | Separate PDP from SOP | Prevent operational lessons being lost during PDP rewrites |

---

## VERSION HISTORY

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-02-04 | Initial PDP |
| 1.1 | 2026-02-05 | Phases 1-3 complete |
| 1.2 | 2026-02-05 | Phase 4 in progress, display debugging |
| 1.3 | 2026-02-05 | Phase 4 complete |
| 1.4 | 2026-02-05 | Phase 5 complete. WLED API verified. |
| 1.5 | 2026-02-05 | Phase 6 complete. All inputs working. |
| 1.6 | 2026-02-05 | Phase 7 broken into sub-phases 7A-7E. |
| 1.7 | 2026-02-06 | 7A+7B complete. Mock mode added. |
| 1.8 | 2026-02-07 | Phase 7 complete. All screens working. GitHub workflow established. |
| 1.9 | 2026-02-07 | Phase 7 rebuilt after context loss. Phase 8 planned as A/B/C. Git workflow lessons added. |
| 2.0 | 2026-02-08 | Phase 8A complete. Phase 8B design complete. Separated operational knowledge into SOP.md. Added UNIFIED-CONTROL-SPEC.md and visual mockup. |


