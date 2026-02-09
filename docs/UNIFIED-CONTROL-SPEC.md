# WLED UNIFIED CONTROL — IMPLEMENTATION SPEC
## Phase 8B Design Blueprint
**Version:** 1.0
**Created:** February 8, 2026
**Status:** Design complete — ready for implementation
**Visual Reference:** `docs/unified-control-v6.html` (open in browser)

---

## OVERVIEW

Phase 8B replaces the current multi-screen navigation (Phase 7) with a unified control interface. All controls visible on one main screen. Overlays slide in for detail work. "Stage and send" pattern batches changes before transmitting.

### What Changes from Phase 7
- **Removed:** Separate brightness, effect browser, palette browser, color picker screens
- **Added:** Unified main control with everything visible at once
- **Added:** Overlay system for detail views (devices, groups, effects, colors)
- **Added:** Effect categories for organized browsing (14 categories)
- **Added:** Device groups with UDP sync
- **Added:** Preset save/recall system
- **Added:** Speed, intensity, reverse, mirror, transition controls
- **Added:** Color source indicator (palette vs custom color active)

### What Stays the Same
- All existing modules (wifi_manager, discovery, wled_api, display, input)
- Boot sequence (splash → WiFi → mDNS → smart boot)
- Hardware: same encoder, touch, display
- Modular architecture principle

---

## SCREEN ARCHITECTURE

### 9 Views

| # | View | Type | Entry | Exit |
|---|------|------|-------|------|
| ① | Main Control | Base screen | Boot / close any overlay | — |
| ② | Devices | Overlay (top-down) | Swipe ↓ from main | Swipe ↑ / tap apply |
| ③ | Groups | Overlay (top-down) | Swipe ← from Devices | Swipe → to Devices / tap group |
| ④ | Manage Groups | Overlay (top-down) | Swipe → from Devices | Swipe ← to Devices |
| ⑤ | Effect Drawer | Overlay (bottom-up) | Swipe ↑ from main | Swipe ↓ |
| ⑥ | Categories | Overlay | Tap effect in FX Drawer | Swipe ↑ / tap back |
| ⑦ | Effects List | Overlay | Tap category in ⑥ | Encoder press / back |
| ⑧ | Save Preset | Overlay | Long-press preset box on main | Save / cancel |
| ⑨ | MIDI Grid | Overlay (left) | Swipe ← from main | Swipe → | Future: Phase 8C/9 |

### Navigation Flow
```
                              ⑨ MIDI Grid (future)
                                    ↕ swipe ←→
③ Groups ↔ ② Devices ↔ ④ Manage    ① MAIN CONTROL    ⑤ FX Drawer
         swipe ←→                  ↕ swipe ↑↓         ↓ tap effect/palette
                                                  ⑥ Categories → ⑦ FX List
                                   ⑧ Save Preset (long-press)
                                   Color Picker (tap swatch)
                                   Palette Browser (tap palette)
```

---

## VIEW ① — MAIN CONTROL

### Layout (top to bottom within 240px circle)

**Device Header** (top center)
- Device name: 14px, orange (#ff6b2b), uppercase
- Dropdown arrow: 10px, indicates swipe ↓ opens device panel
- Multi-device indicator: "(+N)" count when multiple selected

**Top Row** (two columns)
- Left: Preset box — "P03" in green, preset name below (9px)
- Right: Brightness value (20px, amber #ffaa44) + power indicator dot

**Effect Hero** (center, largest element)
- Category icon + label: 13px, gray
- Effect name: 28px, bold, white — hero element of the screen
- Text shadow for depth

**Speed / Intensity** (below effect)
- Two horizontal bars side by side
- Labels: 10px uppercase ("SPEED", "INTENSITY")
- Bars: 110px × 10px, gradient fills
- Values: 11px numeric

**Color Zone** (below speed/intensity)
- Row 1: Three color swatches (32px circles) + color/palette source tag
- Row 2: Palette strip (130px gradient) + palette source tag
- Active source: glows, full opacity, scaled up slightly
- Inactive source: dimmed to 30% opacity
- Source tags: "color ✓" (orange) or "palette" (purple)

**Brightness Arc** (background, around edge of circle)
- 270° arc from ~7 o'clock to ~5 o'clock
- Gradient: dark amber → bright amber → yellow
- Fills proportional to brightness value
- Track (unfilled portion) in dark gray

**Nav Hints** (edges, very subtle)
- Bottom: "▲ effects · ▾ devices" (11px, near-invisible #252530)
- Left edge: "← midi" (10px, vertical)

### Main Control Interactions
| Input | Action |
|-------|--------|
| Encoder rotate | Adjust brightness (default) or focused parameter |
| Encoder press | Exit focus mode / confirm |
| Encoder long press | Identify device (brightness pulse) |
| Tap effect name | → Categories ⑥ |
| Tap color swatch | → HSV color picker for that slot (1st/2nd/3rd) |
| Tap palette strip | → Palette browser |
| Tap power dot | Toggle on/off |
| Tap speed bar | Focus → encoder adjusts speed |
| Tap intensity bar | Focus → encoder adjusts intensity |
| Tap preset box | Focus → encoder scrolls through presets |
| Long-press preset | → Save Preset ⑧ |
| Swipe ↓ | → Devices ② |
| Swipe ↑ | → Effect Drawer ⑤ |
| Swipe ← (R→L) | → MIDI Grid ⑨ (future) |

### Focus Mode
When user taps speed, intensity, or preset box:
- That parameter highlights (glow/border change)
- Encoder temporarily controls that value instead of brightness
- Encoder press or tap elsewhere exits focus → returns encoder to brightness

---

## VIEW ② — DEVICES (Overlay)

### Entry/Exit
- Entry: Swipe ↓ from Main
- Exit: Swipe ↑ to close, tap "Apply" to confirm selection

### Layout
- Title: "DEVICES" (16px, orange)
- Subtitle: "tap name = solo · ☐ = multi" (10px)
- Scrollable device list
- Bottom buttons: All | Clear | Apply (N)
- Navigation: "← Groups | Manage →"

### Device Rows
- Height: 38px minimum
- Checkbox: 24×24px, left side, 2px border
- Checked state: orange border, checkmark, orange tint background
- Device name: 13px
- IP suffix: 10px, gray, right-aligned (e.g., ".96")
- Selected device: orange text, highlighted background

### Interactions
| Input | Action |
|-------|--------|
| Tap device name | Solo select (deselects all others) |
| Tap checkbox | Toggle multi-select for that device |
| Tap "All" | Check all devices |
| Tap "Clear" | Uncheck all |
| Tap "Apply (N)" | Confirm selection, close overlay |
| Swipe ← | → Groups ③ |
| Swipe → | → Manage ④ |
| Swipe ↑ | Close overlay |
| Encoder rotate | Scroll device list |

---

## VIEW ③ — GROUPS (Overlay)

### Entry/Exit
- Entry: Swipe ← from Devices
- Exit: Tap group (auto-loads and closes), swipe → back to Devices

### Layout
- Title: "GROUPS" (16px, purple #aa66ff)
- Subtitle: "tap to load · UDP sync" (10px)
- Scrollable group list
- Navigation: "Devices → | Manage →→"

### Group Rows
- Height: 52px (biggest rows — single tap to load)
- Icon: 20px, "◈" for user groups, "◇" for All Devices
- Name format: "North Wall (3)" — inline device count, 16px text
- Active group: purple text
- "All Devices (UDP)": italic, grayed, special broadcast mode

### Interactions
| Input | Action |
|-------|--------|
| Tap group | Load group → auto-check devices → close overlay → return to main |
| Swipe → | → Devices ② |
| Encoder rotate | Scroll groups |

### Group Behavior
Loading a group:
1. Checks the devices in that group in the Devices view
2. Sets target list for commands
3. Returns to main control
4. All subsequent commands go to group via UDP unicast (or broadcast for "All")

---

## VIEW ④ — MANAGE GROUPS (Overlay)

### Entry/Exit
- Entry: Swipe → from Devices
- Exit: Swipe ← back to Devices

### Layout
- Title: "MANAGE GROUPS" (16px, blue #44aaff)
- Subtitle: "edit · delete" (10px)
- Group list with edit/delete actions
- "+ New Group" button at bottom (green)

### Manage Rows
- Height: 48px
- Name: 15px, "Name (N)" format with inline count
- Edit button: blue "edit" text
- Delete button: red "×"

### Group Edit Flow (Future Detail)
- Edit opens device checklist for that group
- New group: name entry → device selection → save
- Groups stored in NVS (see storage section)

---

## VIEW ⑤ — EFFECT DRAWER (Overlay)

### Entry/Exit
- Entry: Swipe ↑ from Main
- Exit: Swipe ↓

### Layout (top to bottom)
**Section: Current**
- Effect row: label "Effect" + current name + "▸" arrow (tap → Categories)
- Palette row: label "Palette" + current name + "▸" arrow (tap → Palette browser)
- Row height: 38px, 14px value text

**Section: Parameters**
- Speed bar: label (12px) + fill bar (flex-grow, 12px tall) + value (13px)
- Intensity bar: same layout
- Tap bar → focus → encoder adjusts

**Section: Options**
- Reverse toggle: 20×20px checkbox + "Reverse" label (12px)
- Mirror toggle: same format
- Transition slider: label + bar (max 140px × 10px) + "700ms" value

**Section: Colors**
- Three rows: Primary, Secondary, Tertiary
- Each: 24px color swatch circle + label (12px) + "tap → pick" hint (10px, blue)
- Tap swatch → HSV color picker for that slot

### Drawer Sizing
- Content width: max 360px
- Padding: 38px top, standard overlay sides
- All parameter bars stretch to fill available width

---

## VIEW ⑥ — CATEGORIES (Overlay)

### Entry/Exit
- Entry: Tap effect name on Main, or tap Effect row in Drawer
- Exit: Swipe ↑ to close, tap category → Effects List

### Layout
- Title: "CATEGORIES" (16px, orange)
- Subtitle: "encoder scroll · tap to enter" (10px)
- Scrollable category list

### Category Rows
- Height: 42px
- Icon: 16px, left (■, ⚡, →, ►►, 🌈, 🔥, ✦, ~, ≋, etc.)
- Name: 14px, "Category Name (N)" with inline effect count
- Selected category: orange text, subtle background highlight

### Categories (14 total + Extended)

| Category | Icon | Effect IDs | Count |
|----------|------|------------|-------|
| Solid/Static | ■ | 0 | 1 |
| Blink/Strobe | ⚡ | 1, 2, 23-26, 57 | 7 |
| Wipe/Sweep | → | 3-4, 6, 36, 48-51, 55 | 9 |
| Chase | ►► | 28-33, 37-38, 44, 52-54 | 12 |
| Rainbow | 🌈 | 8-9, 14, 24, 26, 33, 63, 67 | 8 |
| Scan/Comet | ↔ | 10-11, 40-41, 59-60, 76-77 | 8 |
| Twinkle/Sparkle | ✦ | 17, 20-22, 74, 80-81 | 7 |
| Dissolve/Fade | ◐ | 5, 7, 12, 18-19, 46, 56 | 7 |
| Running/Wave | ~ | 15-16, 34-35, 39, 61-62, 65, 68 | 9 |
| Fire/Flicker | 🔥 | 45, 66 | 2 |
| Rain/Firework | 💥 | 42-43, 79 | 3 |
| Noise | ≋ | 69-73, 75 | 6 |
| Theater | 🎭 | 13-14, 27, 47, 58, 64, 78, 82 | 8 |
| Holiday | 🎄 | 44, 53 | 2 |
| Extended | + | 83-227 | ~145 |

**Implementation note:** Store category-to-ID mapping as a lookup table in code. The "Extended" category is a catch-all for IDs 83+.

---

## VIEW ⑦ — EFFECTS LIST (Overlay)

### Entry/Exit
- Entry: Tap category in ⑥
- Exit: Encoder press (applies effect), tap back arrow, swipe ↑

### Layout
- Title: "🔥 Fire / Flicker" (category icon + name, 16px)
- Subtitle: "encoder scroll · press = apply" (10px)
- Back link: "◂ Back to categories" (12px, with category header border)
- Effect list within selected category

### Effect Rows
- Height: 40px
- Effect ID: 10px, gray, left (e.g., "45")
- Effect name: 15px
- Currently active effect: orange text, bold, highlighted background

### Interactions
| Input | Action |
|-------|--------|
| Encoder rotate | Scroll through effects in category |
| Encoder press | Apply selected effect → return to main |
| Tap effect | Apply effect → return to main |
| Tap "◂ Back" | → Categories ⑥ |
| Swipe ↑ | Close to main |

---

## VIEW ⑧ — SAVE PRESET (Overlay)

### Entry/Exit
- Entry: Long-press preset box on Main
- Exit: Tap save, or cancel

### Layout
- Title: "SAVE PRESET" (16px, green #00cc55)
- Subtitle: "snapshot → named recipe" (10px)
- State summary box (rounded, bordered, ~300px wide):
  - Effect: current name
  - Speed / Int: current values
  - Palette: current name
  - Primary: color swatch + hex
  - Secondary: color swatch + hex
  - Brightness: current value
  - Rev / Mirror: current state
- Name field: 240px wide, 16px text, centered
- "✓ Save Preset" button: green border, green text

### Preset Storage
- NVS key pattern: `pre_N_name`, `pre_N_data`
- Data = JSON string of the full staged packet
- Max ~20 presets

---

## VIEW ⑨ — MIDI GRID (Future — Phase 8C/9)

### Layout
- 4×3 grid of trigger pads
- 4 pages = 48 total slots
- Each pad: ID (P01-P48), assigned preset name
- Active pad: orange border + glow
- Assigned pad: slightly brighter background
- Empty pad: gray border, "empty" text
- Page indicator dots at bottom

### Interactions
| Input | Action |
|-------|--------|
| Tap pad | Fire preset (apply to current devices) |
| Long-press pad | Reassign slot |
| Swipe ↑↓← | Change pages |
| Swipe → | Return to main |

---

## OVERLAY ARCHITECTURE

### Visual Treatment
- Background: rgba(0,0,0,0.94) — near-opaque black
- Circular mask matches display shape
- Content padded inside circle: 42px top, 20px sides, 36px bottom
- Scrollable content area: `max-width: 380px`, `width: 100%`

### Overlay Types
- **Top-down** (Devices, Groups, Manage): "▲ close" hint at top, swipe ↑ to dismiss
- **Bottom-up** (FX Drawer): "▼ close" hint at bottom, swipe ↓ to dismiss
- **Nested** (Categories, FX List): entered from within other overlays
- **Modal** (Save Preset): entered via long-press, explicit save/cancel

### Transition Behavior
On the physical 240px display, overlays don't animate — they appear/disappear instantly. The overlay concept is purely about layering, not animation.

---

## GESTURE VOCABULARY (Complete)

| Gesture | From Main | From Device Panel | From FX Drawer |
|---------|-----------|-------------------|----------------|
| Swipe ↓ | → Devices ② | — | Close drawer |
| Swipe ↑ | → FX Drawer ⑤ | Close panel | — |
| Swipe ← (R→L) | → MIDI ⑨ | → Groups ③ | — |
| Swipe → (L→R) | — | → Manage ④ | — |
| Tap | Context-dependent | Select/toggle | Focus/navigate |
| Long press (touch) | Identify device | — | — |
| Long press (preset) | → Save Preset ⑧ | — | — |
| Encoder rotate | Brightness / focused param | Scroll list | Scroll (if applicable) |
| Encoder press | Exit focus | — | — |
| Encoder long press | Identify device | — | — |

---

## COLOR SOURCE LOGIC

Effects can use either **custom colors** (user-set RGB) or **palette colors** (WLED palette).

### Display Rules
- Both color swatches AND palette strip always visible on main screen
- **Active source** (the one the effect actually uses): full opacity, slight glow, scale(1.1)
- **Inactive source**: dimmed to 30% opacity
- Source tags show which is active: "color ✓" (orange) or "palette" (purple)

### Determining Active Source
Initial approach: show both, let user see what happens when they change either one. Some effects respond to colors, some to palettes, some to both.

Future enhancement: build a lookup table mapping effect IDs to their color source behavior, and auto-indicate which controls are active for the current effect.

---

## STAGE AND SEND PATTERN

### Concept
Instead of sending every change immediately (current Phase 7 behavior), compose a full state on-screen then send as one batched HTTP POST.

### Packet Structure
```json
{
  "on": true,
  "bri": 180,
  "transition": 7,
  "seg": [{
    "fx": 42,
    "sx": 128,
    "ix": 200,
    "pal": 5,
    "col": [[255, 100, 0], [0, 0, 50], [34, 0, 68]],
    "rev": false,
    "mi": false
  }]
}
```

### Send Targets
- **Single device:** HTTP POST to device IP
- **Group (specific devices):** UDP unicast to each device IP in group
- **All devices:** UDP broadcast to 192.168.1.255:21324

### UDP Packet Format (WLED Notifier, 24 bytes)
```
Byte 0:  Protocol (0)
Byte 1:  callMode
Byte 2:  Brightness (0-255)
Byte 3-5:  RGB primary
Byte 6:  Nightlight info
Byte 7:  Effect ID
Byte 8:  Effect speed
Byte 9:  White primary (RGBW)
Byte 10: Version byte
Byte 11-13: RGB secondary
Byte 14: White secondary
Byte 15: Effect intensity
Byte 16: Transition (×100ms)
Byte 17: Palette ID
```

---

## ENCODER ACCELERATION

### Current: Fixed threshold
`ENC_LIST_THRESHOLD = 4` (4 ticks = 1 list item)

### Proposed: Velocity-based
Track ticks per 200ms window:

| Speed | Threshold | Jump |
|-------|-----------|------|
| Slow (< 3 ticks/200ms) | Fine | 1 item |
| Medium (3-8 ticks/200ms) | Page | 5 items |
| Fast (> 8 ticks/200ms) | Category | 10-20 items |

---

## NVS STORAGE SCHEMAS

### Device Groups
```
"grp_count" = int (max ~10)
"grp_0_name" = "North Wall"
"grp_0_devs" = "wled-37cac0,wled-2c55b4,wled-f7b158"
"grp_1_name" = "South Entrance"
"grp_1_devs" = "wled-37cac0-2,wled-37cac0-3"
```

### Presets
```
"pre_count" = int (max ~20)
"pre_0_name" = "Sunset Fire"
"pre_0_data" = "{\"bri\":180,\"seg\":[{\"fx\":66,\"sx\":128,...}]}"
```

### Existing NVS (from Phase 8A)
```
"last_device" = device name string
"last_bri" = brightness int
"last_fx" = effect ID int
```

---

## DISPLAY SIZING REFERENCE

All sizes are physical pixels on 240×240 round display.

### Text Sizes (physical px)
| Element | Size | Weight | Color |
|---------|------|--------|-------|
| Effect name (hero) | 28px | Bold | White |
| Brightness value | 20px | SemiBold | Amber #ffaa44 |
| Overlay titles | 16px | Regular | Orange/purple/blue/green |
| Group names | 16px | Regular | Gray/purple |
| Device names | 13-15px | Regular | Gray/orange |
| Effect list names | 15px | Regular | Gray/orange |
| Category names | 14px | Regular | Gray/orange |
| Parameter labels | 10-12px | Regular | Gray |
| Nav hints | 10-11px | Regular | Near-invisible |
| Source tags | 9px | SemiBold | Orange/purple |

### Touch Targets (physical px, minimum heights)
| Element | Height | Notes |
|---------|--------|-------|
| Group rows | 52px | Biggest — single tap to load |
| Manage rows | 48px | Edit/delete actions |
| Category rows | 42px | Tap to enter |
| Effect list rows | 40px | Tap or encoder to select |
| Device rows | 38px | Checkbox + name |
| FX Drawer selector rows | 38px | Tap to navigate |
| Color swatches (main) | 32px | Tap for color picker |
| Color swatches (drawer) | 24px | Tap for color picker |
| Checkboxes | 24×24px | Toggle |
| Toggles (rev/mirror) | 20×20px | On/off |
| Power indicator | 20×20px | Tap toggle |

### Layout Constants
| Element | Value |
|---------|-------|
| Display diameter | 240px |
| Usable content area | ~220px diameter |
| Brightness arc width | 16px (around edge) |
| Overlay padding top | 42px |
| Overlay padding sides | 20px |
| Overlay padding bottom | 36px |
| Overlay content max-width | ~200px (380px at 2× mockup) |
| Speed/intensity bar width | 110px |
| Speed/intensity bar height | 10px |
| Palette strip width | 130px |
| Palette strip height | 10px |

### Color Palette
| Use | Color | Hex |
|-----|-------|-----|
| Primary accent | Orange | #ff6b2b |
| Secondary accent | Purple | #aa66ff |
| Tertiary accent | Blue | #44aaff |
| Success/confirm | Green | #00cc55 |
| Danger/delete | Red | #ff4444 |
| Brightness/amber | Amber | #ffaa44 |
| Background | Near-black | #000000 |
| Overlay bg | 94% black | rgba(0,0,0,0.94) |
| Card/panel bg | Dark gray | #0f0f16 |
| Border | Dark gray | #1a1a22 / #2a2a35 |
| Primary text | Light gray | #ccc / #e0e0e0 |
| Secondary text | Mid gray | #666 / #777 |
| Hint text | Dark gray | #333 / #444 |

---

## IMPLEMENTATION APPROACH

### Phase 8B Sub-phases (Suggested)

**8B-1: Overlay Framework**
- Implement overlay state machine (which overlay is active)
- Overlay drawing (dark background, content area)
- Gesture routing (different gestures depending on active overlay)

**8B-2: Main Control Screen**
- Brightness arc rendering
- Effect hero display
- Speed/intensity bars
- Color swatches + palette strip
- Power indicator
- Focus mode for encoder parameter switching

**8B-3: Device Panel + Groups**
- Device list overlay with multi-select
- Group list overlay with tap-to-load
- Group storage in NVS
- UDP send for group control

**8B-4: Effect System**
- Categories overlay with category map
- Effects list within category
- Effect drawer with full parameters
- Speed, intensity, reverse, mirror, transition controls

**8B-5: Presets**
- Save preset flow
- Preset storage in NVS
- Preset recall via encoder scroll on main screen

### Key Architecture Decision
The main screen (View ①) renders the base layer. Overlays draw ON TOP with near-opaque background. Only one overlay active at a time. This means:
- `ui_update()` always processes main screen state
- If overlay active, overlay input handler runs INSTEAD of main input handler
- Overlay draws AFTER main screen (painters algorithm)
- Closing overlay reveals main screen without redraw

---

## WHAT'S NOT IN THIS SPEC

- **MIDI Grid implementation** — Phase 8C/9, placeholder view only
- **Palette categories** — optional enhancement, flat list works initially
- **fxdata dynamic filtering** — future enhancement to filter unsupported effects
- **Animation/transitions** — instant overlay show/hide on 240px display
- **Main screen layout finalization** — user indicated this will be explored separately during implementation

---

## VERSION HISTORY

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-02-08 | Initial spec from v1-v6 design iterations |
