# WLED CONTROLLER — DEVELOPMENT SOP
## Standard Operating Procedures
**Version:** 1.0
**Created:** February 8, 2026
**Status:** Living document — update only when new lessons learned

---

## PURPOSE

This document captures HOW we work — tools, workflows, hard-won lessons, and operational patterns. It is separate from the PDP (which tracks WHAT we're building and WHERE we are) because the PDP gets rewritten frequently as phases complete. This SOP changes rarely and only when we bump into something new worth documenting.

---

## BUILD → TEST → COMMIT CYCLE

1. Claude provides complete file contents (no partial snippets)
2. Copy → paste into VS Code files (one file at a time, full path stated)
3. Build (✓ button in PlatformIO toolbar)
4. Upload to board (→ button)
5. Test on hardware — verify functionality
6. Refine if needed (repeat 1-5)
7. **Only after hardware validation:** Commit and push via GitHub Desktop

### Critical Rule
**Never update the PDP to mark a phase "complete" until the code is committed and pushed.** PDP describes what IS in the repo, not what was tested and lost. If the PDP says complete but repo code doesn't match → the PDP was updated prematurely.

---

## CODE DELIVERY FORMAT

When Claude provides code changes:
- ONE file at a time
- State the FULL file path clearly (e.g., "File: src/main.cpp")
- Provide the COMPLETE file contents — no partial snippets to hunt and insert
- Say "Copy this → Paste into [filename]" explicitly
- If a change spans multiple files, list which files will be touched up front, then deliver them ALL in sequence — no waiting between files
- After all files are delivered, say what to do next (build, upload, check serial, etc.)
- Explain errors and code concepts along the way — this is a learning experience too

This accommodates ADHD workflow — no hunting, no guessing where code goes.

---

## GITHUB INTEGRATION

### Repository
- **Repo:** github.com/kaizensiriuskintsugi/wled-controller
- **Branch:** main
- **Tool:** GitHub Desktop for commit/push

### Context Continuity Between Sessions
Claude fetches current code from raw GitHub URLs at conversation start. New sessions begin with Kai pasting the URL block and Claude fetching all files to establish project context.

### GitHub Desktop Workflow
GitHub Desktop has a **two-step process**: Commit (bottom left button) then Push (top bar "Push origin").

- **Commit** = saves to local Git history only
- **Push** = sends to GitHub remote
- Forgetting to Push is silent — GitHub Desktop shows "Fetch origin" as if everything is synced
- After committing, always verify the top bar changes from "Push origin ↑1" to "Fetch origin"
- **Verification:** `git log --oneline -5` in terminal shows local commits. If `origin/main` appears next to the latest commit, the push landed.

### raw.githubusercontent.com CDN Cache
- GitHub's raw file CDN can serve stale content for **up to 5 minutes** after a push
- If Claude pulls files immediately after a push and sees old code, wait a few minutes and re-pull
- Don't assume the push failed based on stale CDN content alone
- **Better verification:** Check github.com in browser (not raw URLs) or use `git log` to confirm

---

## CONTEXT LOSS PREVENTION

### The Problem
Claude sessions have context windows. If a session ends before code is committed, work can be lost. The PDP and committed code are the only things that survive between sessions.

### Prevention Rules
1. Commit working code frequently — don't accumulate multiple phases of uncommitted changes
2. PDP describes what IS committed, not what was tested in the current session
3. If rebuilding from PDP after context loss, the spec should be detailed enough to reconstruct from
4. New Claude sessions pull from GitHub raw URLs to verify actual state — trust the repo, not memory

### Recovery Pattern
If code was lost between sessions:
1. Claude fetches current repo state from GitHub URLs
2. Compare repo code against PDP phase descriptions
3. Identify gap between what PDP claims and what repo contains
4. Rebuild from PDP spec (this has worked — Phase 7 was fully rebuilt from PDP v1.9 after context loss)

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

## MODULAR ARCHITECTURE RULES

### One Feature Per File
Modules don't know about each other. The UI layer is the only thing that connects them. This keeps each file small enough to maintain context during development.

| Module | Knows About | Does Not Know About |
|--------|-------------|---------------------|
| wifi_manager | WiFi library | Display, WLED, UI |
| discovery | WiFi (needs connection) | Display, input, UI |
| wled_api | WiFi (sends HTTP) | Display, input, UI |
| display | TFT_eSPI | WiFi, WLED, input |
| input | GPIO pins | Display, WiFi, WLED |
| ui | All modules | — (this is the glue) |
| main | All modules (init/update) | Internal module details |

### Why This Matters
Previous monolithic code became too large to maintain context during development. Modular architecture means Claude can work on one module at a time without needing the full codebase in context.

---

## HARDWARE-SPECIFIC LESSONS

### ESP32-S3 + CH343P
- CH343P chip = traditional UART, not native USB CDC
- `CDC_ON_BOOT=0` prevents boot loops
- `USE_HSPI_PORT=1` required for ESP32-S3 SPI display

### Display (GC9A01)
- Must match poi project platformio.ini exactly — missing flags cause black screen
- TFT_eSPI pinned to ^2.5.43
- 240×240 round display, corners clipped — keep content within ~220px diameter

### Touch (CST816S)
- Touch requires hardware reset sequence: LOW 10ms → HIGH 50ms
- Gesture register repeats same value while finger on screen — must lock out after first fire, reset on finger lift
- Swipe up/down register values (0x01/0x02) were reversed from expected — swapped in mapping
- Touch coordinates flood every poll cycle — only update display on >3px movement to reduce flicker

### Encoder
- Board is the non-ring-encoder variant — external encoder wired to GPIO 15/16/17
- Direction depends on wiring — swapped ++/-- in ISR to correct
- Button needs 50ms debounce minimum
- Accumulator with threshold (ENC_LIST_THRESHOLD = 4) for list scrolling
- noInterrupts()/interrupts() wrapper needed when reading volatile encoder delta

### WLED API
- DynamicJsonDocument(4096) for /json/info and /json/state
- DynamicJsonDocument(8192) for effects list
- Never end test sequences on full white — max current draw risks PSU damage
- HTTP to port 80, not HTTPS
- mDNS discovery order is not guaranteed — use device name matching

---

## UI DEVELOPMENT PATTERNS

### Two-Pass Rendering
In `ui_update()`, process input FIRST (pass 1), then draw (pass 2). This prevents screen transition timing bugs where the old screen's draw runs after the new screen's input handler has already switched state.

### Consume-on-Read Input Pattern
Calling `input_encoder_delta()` returns the value AND resets to 0. Same for button and gesture. Prevents double-processing of events.

### Generic Browser Functions
Effect and palette browsers share 100% of scroll/select logic via `drawBrowserList()` and `handleBrowserInput()`. When adding new list-based screens, reuse this pattern.

### Overlay Architecture (Phase 8B+)
Overlays draw ON TOP of main screen with near-opaque background. Only one overlay active at a time. Overlay input handler runs INSTEAD of main input handler when active. Closing overlay reveals main screen without redraw.

---

## DOCUMENT MANAGEMENT

### Repo Documentation Structure
```
docs/
  PDP.md                        — Project status, phases, decisions (changes often)
  SOP.md                        — This file — how we work (changes rarely)
  UNIFIED-CONTROL-SPEC.md       — Phase 8B implementation blueprint
  unified-control-v6.html       — Visual mockup reference (open in browser)
```

### PDP vs SOP
- **PDP** gets rewritten as phases complete — tracks what/where
- **SOP** only updated when we learn something new — tracks how
- This separation prevents operational lessons from being accidentally trimmed during PDP rewrites

### Design Documents
- **Markdown specs** (like UNIFIED-CONTROL-SPEC.md) = what Claude reads for implementation
- **HTML mockups** = visual reference for humans (open in browser)
- Both live in `docs/` and are committed to Git
- Claude doesn't need to fetch HTML mockups — they're not useful as raw code

---

## SAFETY RULES

- Never end WLED test sequences on full white — max current risks PSU damage
- Always validate on hardware before marking anything complete
- Don't trust mock mode results as final — real devices reveal timing and memory issues mock mode hides

---

## VERSION HISTORY

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-02-08 | Initial SOP — extracted from PDP v1.9 to prevent operational knowledge loss during PDP rewrites |
