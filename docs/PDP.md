# WLED TOUCHSCREEN CONTROLLER
## Product Development Plan v1.8

**Created:** February 4, 2026
**Updated:** February 7, 2026
**Status:** Phase 7 Complete — All UI screens working, hardware validated
**Platform:** Waveshare ESP32-S3-Touch-LCD-1.28

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
| 8 | Polish | ⬜ Pending | Persistence, multi-device, error handling |

---

## MOCK MODE

**Purpose:** Enables UI development without WiFi or WLED devices present.

**Activation:** Add `-DMOCK_MODE=1` to platformio.ini build_flags
**Deactivation:** Remove or comment out the flag (don't set to 0 — ifdef checks existence, not value)

**What it does:**
- WiFi: Skips real connection, reports "Mock WiFi" connected
- Discovery: Populates 5 fake devices (Lighthouse-Main, Beacon-North, etc.)
- WLED API: Returns fake state, logs writes to serial
- UI: Works identically — doesn't know the difference

---

## ARCHITECTURE

### Modular Design Principle
One feature per file. Modules don't know about each other. The UI layer is the only thing that connects them.

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
  WLED-Controller-PDP-v1.8.md — This file
```

### Module Responsibilities

| Module | Knows About | Does Not Know About |
|--------|-------------|---------------------|
| wifi_manager | WiFi library | Display, WLED, UI |
| discovery | WiFi (needs connection) | Display, input, UI |
| wled_api | WiFi (sends HTTP) | Display, input, UI |
| display | TFT_eSPI | WiFi, WLED, input |
| input | GPIO pins | Display, WiFi, WLED |
| ui | All modules | — (this is the glue) |
| main | All modules (init/update) | Internal module details |

---

## COMPLETED PHASES (1-6)

See PDP v1.6 for full details on completed phases.

### Key Learnings Carried Forward
- CH343P = UART, not native USB CDC — `CDC_ON_BOOT=0`
- WLED /json/info needs DynamicJsonDocument(4096)
- Must match poi project platformio.ini exactly
- DynamicJsonDocument(8192) for effects list
- Never end test sequences on full white
- CST816S gesture lockout until finger lift
- Consume-on-read input pattern prevents double-processing
- Two-pass rendering (input then draw) prevents screen transition bugs
- Encoder accumulator with threshold for list scrolling

---

## PHASE 7: UI INTEGRATION ✅ COMPLETE

### Full Navigation Map (All Validated on Hardware)

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
| Effect Browser | Encoder Rotate | Scroll list |
| Effect Browser | Encoder Press | Apply effect → Device Control |
| Effect Browser | Swipe Down | Cancel → Device Control |
| Palette Browser | Encoder Rotate | Scroll list |
| Palette Browser | Encoder Press | Apply palette → Device Control |
| Palette Browser | Swipe Down | Cancel → Device Control |
| Color Picker | Touch ring | Select hue |
| Color Picker | Encoder Rotate | Adjust saturation |
| Color Picker | Encoder Press | Apply color → Device Control |
| Color Picker | Swipe Down | Cancel → Device Control |

### 7A: Device List
- Scrollable with encoder accumulator (threshold 4)
- Tap or encoder press to select device
- Fetches device state on selection

### 7B: Device Control
- Large brightness number with arc visualization
- Effect and palette names displayed (not just IDs)
- ON/OFF status indicator
- Partial redraw for brightness changes (no full screen flicker)

### 7C: Effect Browser
- 50 named effects in shared lookup table
- Generic `drawBrowserList()` / `handleBrowserInput()` reused by palette browser
- Current active effect shown in green
- Position indicator at bottom

### 7D: Palette Browser
- 50 named palettes in shared lookup table
- Identical interaction pattern to effect browser (code reuse)
- Entered via swipe left from control screen

### 7E: Color Picker
- HSV hue ring rendered pixel-by-pixel around circular display
- Touch the ring to select hue angle (atan2 calculation)
- Encoder adjusts saturation (0-255)
- Center preview circle shows selected color
- Indicator dot tracks position, erases cleanly on move
- Encoder press confirms → converts HSV to RGB → sends to device

### 7F: Power & Navigation
- Swipe up on control screen toggles power
- All gesture transitions wired and tested
- Swipe down is universal "back/cancel" gesture

### Phase 7 Key Learnings
- Generic browser functions eliminate duplicate code — effect and palette browsers share 100% of scroll/select logic
- HSV color model maps naturally to circular display (angle = hue)
- Indicator cleanup requires tracking old position and redrawing ring segment
- Swipe speed sensitivity affects gesture detection — noted for refinement
- Control responsiveness has room for improvement — noted for refinement

---

## PHASE 8: POLISH & EXPANSION

### Tasks
- [ ] Settings persistence via NVS (last device, brightness, preferences)
- [ ] Boot sequence: connect WiFi → scan → restore last device → show control
- [ ] Cache effect/palette counts per device (avoid HTTP round-trip on each browser entry)
- [ ] Group control: select multiple devices, control simultaneously
- [ ] Preset save/recall: store favorite combinations
- [ ] Error handling: WiFi lost, device unreachable, scan empty
- [ ] Rescan capability from UI
- [ ] AP mode for field use
- [ ] Power: battery monitoring if on LiPo

### Refinement Backlog (UX Issues Noted During Phase 7)
- [ ] Swipe speed sensitivity — fast swipes sometimes not detected
- [ ] Control responsiveness — general input feel improvements
- [ ] Effect/palette browser entry lag — HTTP fetch for count on every entry
- [ ] Additional UX ideas from hands-on testing (to be captured)

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
| 2026-02-07 | Swipe up for power toggle | Intuitive — swipe up = wake/power |
| 2026-02-07 | Validate on hardware before Git commit | Ensures repo always has working code |
| 2026-02-07 | PDP lives in repo (docs/) | Documentation stays with the code it describes |

---

## DEVELOPMENT WORKFLOW

### Build → Test → Commit Cycle
1. Claude provides code changes
2. Paste into VS Code files
3. Build (✓ button in PlatformIO toolbar)
4. Upload to board (→ button)
5. Test on hardware — verify functionality
6. Refine if needed (repeat 1-5)
7. **Only after validation:** Commit and push via GitHub Desktop

### GitHub Integration
- **Repo:** github.com/kaizensiriuskintsugi/wled-controller
- **Branch:** main
- **Tool:** GitHub Desktop for commit/push
- **Context continuity:** Claude fetches current code from raw GitHub URLs at conversation start

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
| 1.8 | 2026-02-07 | Phase 7 complete. All screens working — effect browser, palette browser, color picker, power toggle, full navigation. GitHub workflow established. PDP moved to repo. |