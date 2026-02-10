# WLED TOUCHSCREEN CONTROLLER
## Product Development Plan v2.1

**Created:** February 4, 2026
**Updated:** February 9, 2026
**Status:** Phase 8B Navigation Design Complete — Ready for Implementation
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
| 8B | Navigation Design | ✅ Complete | 3-tier 16-screen architecture, all flows mapped |
| 8B | Navigation Build | ⬜ Pending | Implementation from spec |
| 8C | Device Groups | ⬜ Pending | Group storage, UDP sync, manage UI |
| 8D | Presets & MIDI | ⬜ Pending | Save/recall, MIDI grid |
| 8E | Settings & Modes | ⬜ Pending | Live/Setup mode, inactivity timer, config |

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
  wled-nav-mockup.html          — Interactive navigation prototype (open in browser)
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

## PHASE 8B: NAVIGATION ARCHITECTURE

### Design ✅ COMPLETE

Replaces the previous 9-view overlay architecture with a 3-tier, 16-screen navigation model. Interactive prototype in `docs/wled-nav-mockup.html`.

### Screen Architecture — 3 Tiers, 16 Screens

```
TOP TIER — Device/Group Management
┌─────────────┐     ┌──────────────────┐     ┌───────────────┐
│ Device List │ ←→  │ Group Hot Keys   │ ←→  │ Manage Groups │
│             │     │ (hub)            │     │ / Group List  │
└─────────────┘     └──────────────────┘     └───────────────┘
                           ↕ swipe ↑↓
MIDDLE TIER — Central Hub
                    ┌──────────────────┐
  Scene Builder ←── │    HOME PAGE     │ ──→ MIDI Pages 1-4
  (future)          │ (read-only hub)  │     (cycle with swipe)
                    └──────────────────┘
                           ↕ swipe ↑↓
BOTTOM TIER — Effects, Palettes, Presets
┌────────────────┐     ┌───────────────┐     ┌────────────┐
│ Effects Drawer │ ←→  │ FX Favorites  │ ←→  │ Categories │ → FX List
│ (hub)          │     │               │     │            │
└────────────────┘     └───────────────┘     └────────────┘
        ↕ swipe ←→
┌────────────────┐     ┌───────────────┐
│Palette Favorites│ ←→ │ Palette List  │
└────────────────┘     └───────────────┘
        ↕ swipe ↑↓
┌────────────────┐     ┌───────────────┐     ┌───────────────┐
│ Preset Add     │ ←→  │ Preset Load   │ ←→  │ Preset Delete │
└────────────────┘     └───────────────┘     └───────────────┘
```

### Navigation Rules
- **Red arrows (swipe gestures):** Move between screens directionally
- **Green arrows (encoder press / tap):** Enter or confirm
- **Home is always one swipe away** from any tier hub
- **Encoder press from any hub → Home** (quick escape)
- **Long-press encoder from Home → Settings** (off the compass)

### Home Page — Read-Only Dashboard + Active Target Selector

The Home screen serves two critical functions:

1. **Status Display (read-only):** Shows current device/group name, active effect, brightness, power state. No interactive controls — prevents accidental changes from bumps during performance/installation.

2. **Active Target Selector:** Encoder rotation cycles through connected devices/groups. The device/group currently displayed on Home becomes the "active target" — all other screens operate on whatever was last shown on Home. No commands are sent during cycling; it's purely selecting what to control next.

This means Home is both the ambient status display AND the device/group switcher. Navigate away to any editing screen, and changes apply to the target last selected on Home.

### Operating Modes — Live vs Setup

A global mode toggle that affects how all editing screens behave:

- **Live Mode:** Every change fires immediately to the active target device(s). Good for performance, jamming, real-time interaction.
- **Setup Mode:** Changes are staged locally on the controller. Nothing transmits until explicit "send" command. Lets you compose an entire look — effect, palette, speed, colors, brightness — then push it atomically. No half-state flicker on the installation.

Mode indicator must be persistent and visible on all editing screens. Toggle configured in Settings.

### Preview System

Preview = physical 8x8 LED matrix connected as a dedicated preview device on the network. Not an on-screen simulation (240px round display can't meaningfully simulate LED effects). In Setup mode, preview device receives staged changes for visual confirmation before broadcasting to the full group.

### Inactivity Timer

Auto-return to Home after configurable idle period (30-60 seconds default). Any input (encoder, touch, gesture) resets the timer. Since Home is read-only, this is safe landing behavior. Home doubles as ambient status display when controller sits idle during installation.

### Settings Screen

Accessed via **long-press encoder from Home**. Intentionally off the swipe compass to prevent accidental entry.

Contents (planned):
- Inactivity timer duration
- Live/Setup mode toggle
- Other user preferences (TBD as needs emerge)

### Implementation ⬜ PENDING

Suggested sub-phases:
- [ ] 8B-1: Screen state machine + gesture routing framework (16 screens)
- [ ] 8B-2: Home page (read-only dashboard, encoder device cycling)
- [ ] 8B-3: Device list + group hot keys (top tier)
- [ ] 8B-4: Effects drawer + favorites + categories + list (bottom tier)
- [ ] 8B-5: Palette favorites + list
- [ ] 8B-6: MIDI grid (4 pages, cycle with swipe)
- [ ] 8B-7: Inactivity timer + Settings screen

---

## PHASE 8C: DEVICE GROUPS ⬜ PENDING

- [ ] Create named device groups (e.g., "North Wall", "All Beacons")
- [ ] Group hot keys screen — tap to load, auto-select devices
- [ ] Group control via UDP unicast (specific devices) or broadcast (all)
- [ ] NVS storage: `grp_N_name`, `grp_N_devs` (comma-separated device names)
- [ ] Manage groups UI (create, edit, delete)
- [ ] Max ~10 groups, ~10 devices per group

---

## PHASE 8D: PRESETS & MIDI ⬜ PENDING

### Presets
- [ ] Save current state as new preset (snapshot staged state + name)
- [ ] Load preset via encoder: scroll to number, send to active target
- [ ] Edit mode toggle on preset list (not separate duplicate page)
- [ ] 2-column layout for preset list (smaller items, more visible at once)
- [ ] Delete, rename operations
- [ ] NVS storage: `pre_N_name`, `pre_N_data` (JSON string)
- [ ] Max ~20 presets
- [ ] Apply to any device or group (not tied to specific hardware)

### MIDI Grid
- [ ] 4×3 trigger pad grid, 4 pages = 48 preset slots
- [ ] Tap to fire preset, long-press to reassign
- [ ] Swipe left/right to cycle pages
- [ ] Assign/unassign presets to grid slots

---

## PHASE 8E: SETTINGS & MODES ⬜ PENDING

- [ ] Settings screen UI (long-press encoder from Home)
- [ ] Live/Setup mode toggle + persistent indicator
- [ ] Inactivity timer configuration (30-60s range)
- [ ] Setup mode: stage changes locally, "send" command to push
- [ ] Preview device integration (8x8 matrix receives staged changes)
- [ ] Additional preferences as needs emerge

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
| 2026-02-07 | Break Phase 8 into sub-phases | Refinements → Unified UI → Groups/Presets progression |
| 2026-02-08 | Design spec + HTML mockup in repo | Visual reference + text blueprint for implementation |
| 2026-02-08 | Separate PDP from SOP | Prevent operational lessons being lost during PDP rewrites |
| 2026-02-09 | 3-tier 16-screen navigation | Replaces 9-view overlay — clearer mental model, directional swipe compass |
| 2026-02-09 | Home as read-only dashboard | Prevents accidental changes from bumps during performance |
| 2026-02-09 | Home as active target selector | Encoder cycles devices/groups — last shown = edit target for all screens |
| 2026-02-09 | Live vs Setup mode | Live = instant send, Setup = stage + push atomically |
| 2026-02-09 | Inactivity timer to Home | 30-60s configurable, safe landing since Home is read-only |
| 2026-02-09 | Long-press encoder from Home → Settings | Off the swipe compass, prevents accidental entry |
| 2026-02-09 | Preview = physical 8x8 matrix | On-screen simulation impractical on 240px round display |
| 2026-02-09 | Preset edit toggle (not separate page) | Same list, mode toggle avoids duplicate UI |
| 2026-02-09 | 2-column preset layout | More presets visible, smaller items for touch |
| 2026-02-09 | MIDI pages cycle with swipe | 4 pages × 12 slots = 48 presets, swipe right from Home |
| 2026-02-09 | Scene Builder placeholder (future) | Swipe left from Home reserved for future feature |

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
| 2.0 | 2026-02-08 | Phase 8A complete. Phase 8B design complete (9-view overlay). Separated SOP.md. |
| 2.1 | 2026-02-09 | Phase 8B redesigned: 3-tier 16-screen navigation replaces overlay model. Home as read-only dashboard + active target selector. Live/Setup modes. Inactivity timer. Settings via long-press. Preview system. Preset management updates. Phase 8 expanded to 8B-8E. |