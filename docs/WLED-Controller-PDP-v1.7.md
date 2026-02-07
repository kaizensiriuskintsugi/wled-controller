# WLED TOUCHSCREEN CONTROLLER
## Product Development Plan v1.7

**Created:** February 4, 2026
**Updated:** February 6, 2026
**Status:** Phase 7B Complete — Mock Mode + 7C/7D/7E in progress
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
| 7B | UI: Device Control | ✅ Complete | Brightness arc, encoder control, two-pass rendering, identify device |
| 7C | UI: Effect Browser | 🔄 In Progress | Mock mode development — no network needed |
| 7D | UI: Palette Browser | ⬜ Pending | Same pattern as effect browser |
| 7E | UI: Navigation | ⬜ Pending | Full gesture wiring between all screens |
| 8 | Polish | ⬜ Pending | Persistence, multi-device, error handling |

---

## MOCK MODE

**Purpose:** Enables UI development without WiFi or WLED devices present.

**Activation:** Add `-DMOCK_MODE=1` to platformio.ini build_flags
**Deactivation:** Remove or comment out the flag — zero code changes needed

**What it does:**
- WiFi: Skips real connection, reports "Mock WiFi" connected
- Discovery: Populates 5 fake devices (Lighthouse-Main, Beacon-North, etc.)
- WLED API: Returns fake state (brightness 128, effect 0, palette 0), logs writes to serial
- UI: Works identically — doesn't know the difference

**Mock Devices:**
| Name | Fake IP |
|------|---------|
| Lighthouse-Main | 192.168.1.100 |
| Beacon-North | 192.168.1.101 |
| Lantern-Garden | 192.168.1.102 |
| Torch-Entry | 192.168.1.103 |
| Glow-Studio | 192.168.1.104 |

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
  wifi_manager.h
  discovery.h
  wled_api.h
  display.h
  input.h
  ui.h
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

## COMPLETED PHASES (1-6, 7A, 7B)

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
- Arc thickness via multiple radii drawing
- Encoder accumulator with threshold for list scrolling

---

## PHASE 7C: EFFECT BROWSER

**Goal:** Scroll through effect names, select one, apply to device.

### Tasks
- [ ] Mock: hardcode 20 common WLED effect names
- [ ] Encoder press on control screen → enter effect browser
- [ ] Effect list screen: scrollable, encoder navigates with accumulator
- [ ] Current effect highlighted differently (cyan vs white)
- [ ] Encoder press → select effect, update state, return to control
- [ ] Show effect ID and name
- [ ] Swipe down → back to control without changing effect

### Display Layout
```
┌─────────────────┐
│    Effects        │  ← title
│                   │
│  ► #42 Rainbow    │  ← highlighted (cyan)
│    #43 Chase      │  ← normal (white)
│    #44 Sparkle    │  ← normal (white)
│    #45 Fire       │  ← normal (white)
│                   │
│   42/228          │  ← position indicator
└─────────────────┘
```

---

## PHASE 7D: PALETTE BROWSER

**Goal:** Same scroll/select pattern as effect browser, for palettes.

### Tasks
- [ ] Mock: hardcode 15 common WLED palette names
- [ ] Swipe left on control screen → enter palette browser
- [ ] Same layout and interaction as effect browser
- [ ] Encoder press → select palette, update state, return to control

---

## PHASE 7E: FULL NAVIGATION

**Goal:** All gesture transitions wired and tested.

### Navigation Map
| From | Action | To |
|------|--------|----|
| Device List | Tap/Press | Device Control |
| Device Control | Encoder Press | Effect Browser |
| Device Control | Swipe Left | Palette Browser |
| Device Control | Swipe Down | Device List |
| Device Control | Long Press (enc/touch) | Identify Device |
| Effect Browser | Encoder Press | Device Control (effect applied) |
| Effect Browser | Swipe Down | Device Control (no change) |
| Palette Browser | Encoder Press | Device Control (palette applied) |
| Palette Browser | Swipe Down | Device Control (no change) |

---

## PHASE 8: POLISH & EXPANSION

### Tasks
- [ ] Settings persistence via NVS
- [ ] Color picker screen
- [ ] Group control: select multiple devices
- [ ] Preset save/recall
- [ ] Error handling: WiFi lost, device unreachable
- [ ] AP mode for field use
- [ ] Rescan capability from UI
- [ ] Power: battery monitoring if on LiPo

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
| 2026-02-05 | Break Phase 7 into sub-phases (7A-7E) | Test each screen before building next |
| 2026-02-05 | Two-pass rendering in ui_update | Prevents screen transition timing bugs |
| 2026-02-06 | Mock mode via compile flag | Enables UI dev without WiFi/WLED devices |

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
| 1.7 | 2026-02-06 | 7A+7B complete. Mock mode added for offline UI development. 7C-7E in progress. |
