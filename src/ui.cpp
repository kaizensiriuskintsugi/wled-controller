#include "ui.h"
#include "display.h"
#include "input.h"
#include "discovery.h"
#include "wled_api.h"
#include "home_ring.h"
#include <math.h>

// ===== NAVIGATION MAP =====
struct NavTarget {
    Screen up;
    Screen down;
    Screen left;
    Screen right;
};

#define NAV_NONE SCREEN_COUNT

static const NavTarget navMap[SCREEN_COUNT] = {
    { NAV_NONE, NAV_NONE, NAV_NONE, NAV_NONE },           // BOOT
    { SCREEN_GROUP_HOTKEYS, SCREEN_FX_DRAWER, SCREEN_SCENES, SCREEN_MIDI_1 }, // HOME
    { NAV_NONE, SCREEN_GROUP_HOTKEYS, NAV_NONE, NAV_NONE },// DEVICE_LIST
    { SCREEN_DEVICE_LIST, SCREEN_HOME, SCREEN_MANAGE_GROUPS, SCREEN_GROUP_LIST }, // GROUP_HOTKEYS
    { NAV_NONE, NAV_NONE, SCREEN_GROUP_HOTKEYS, NAV_NONE },// GROUP_LIST
    { NAV_NONE, NAV_NONE, NAV_NONE, SCREEN_GROUP_HOTKEYS },// MANAGE_GROUPS
    { SCREEN_MIDI_3, SCREEN_MIDI_4, SCREEN_HOME, SCREEN_MIDI_2 }, // MIDI_1
    { NAV_NONE, NAV_NONE, SCREEN_MIDI_1, NAV_NONE },       // MIDI_2
    { NAV_NONE, SCREEN_MIDI_1, NAV_NONE, NAV_NONE },       // MIDI_3
    { SCREEN_MIDI_1, NAV_NONE, NAV_NONE, NAV_NONE },       // MIDI_4
    { NAV_NONE, NAV_NONE, NAV_NONE, SCREEN_HOME },         // SCENES
    { SCREEN_HOME, SCREEN_PRESETS, SCREEN_FX_FAVORITES, SCREEN_PAL_FAVORITES }, // FX_DRAWER
    { NAV_NONE, NAV_NONE, SCREEN_FX_CATEGORIES, SCREEN_FX_DRAWER }, // FX_FAVORITES
    { NAV_NONE, NAV_NONE, SCREEN_FX_LIST, SCREEN_FX_FAVORITES },    // FX_CATEGORIES
    { NAV_NONE, NAV_NONE, NAV_NONE, SCREEN_FX_CATEGORIES },         // FX_LIST
    { NAV_NONE, NAV_NONE, SCREEN_FX_DRAWER, SCREEN_PAL_LIST },      // PAL_FAVORITES
    { NAV_NONE, NAV_NONE, SCREEN_PAL_FAVORITES, NAV_NONE },         // PAL_LIST
    { SCREEN_FX_DRAWER, NAV_NONE, NAV_NONE, NAV_NONE },             // PRESETS
    { NAV_NONE, NAV_NONE, NAV_NONE, NAV_NONE },                     // SETTINGS
    { NAV_NONE, NAV_NONE, NAV_NONE, NAV_NONE },                     // COLOR_PICKER
};

// ===== SCREEN NAMES =====
static const char* screenNames[SCREEN_COUNT] = {
    "BOOT", "HOME", "DEVICE LIST", "GROUP HOT KEYS",
    "GROUP LIST", "MANAGE GROUPS", "MIDI PG 1", "MIDI PG 2",
    "MIDI PG 3", "MIDI PG 4", "SCENES (FUTURE)", "EFFECTS DRAWER",
    "FX FAVORITES", "FX CATEGORIES", "FX LIST", "PAL FAVORITES",
    "PAL LIST", "PRESETS", "SETTINGS", "COLOR PICKER",
};

// ===== STATE =====
static Screen currentScreen = SCREEN_BOOT;
static Screen previousScreen = SCREEN_HOME;
static bool needsRedraw = true;

// Home page state
static int activeDeviceIndex = 0;
static WledState activeState = {};
static unsigned long lastStateRefresh = 0;
static const unsigned long STATE_REFRESH_MS = 3000;

// Device list state
static int devListSel = 0;
static int devListScroll = 0;

// ===== GENERIC BROWSER STATE =====
struct BrowserState {
    int sel;
    int scroll;
    int count;
};

typedef const char* (*ItemNameFn)(int index);

// Browser instances
static BrowserState fxListBrowser = {0, 0, 0};
static BrowserState palListBrowser = {0, 0, 0};

// Cached counts (fetched once per device)
static int cachedEffectCount = 0;
static int cachedPaletteCount = 0;

// ===== ACTIVE DEVICE HELPERS =====

static void refreshActiveDeviceState() {
    int count = discovery_get_count();
    if (count == 0) {
        activeState.valid = false;
        return;
    }
    if (activeDeviceIndex >= count) activeDeviceIndex = 0;
    WledDevice* dev = discovery_get_device(activeDeviceIndex);
    if (dev) {
        activeState = wled_get_state(dev->ip);
    } else {
        activeState.valid = false;
    }
}

static void ensureEffectCount() {
    if (cachedEffectCount > 0) return;
    WledDevice* dev = discovery_get_device(activeDeviceIndex);
    if (dev) {
        int c = wled_get_effects_count(dev->ip);
        if (c > 0) cachedEffectCount = c;
    }
    if (cachedEffectCount == 0) cachedEffectCount = 177;
}

static void ensurePaletteCount() {
    if (cachedPaletteCount > 0) return;
    WledDevice* dev = discovery_get_device(activeDeviceIndex);
    if (dev) {
        int c = wled_get_palettes_count(dev->ip);
        if (c > 0) cachedPaletteCount = c;
    }
    if (cachedPaletteCount == 0) cachedPaletteCount = 75;
}

// ===== DRAWING HELPERS =====

static void drawHBar(int x, int y, int width, const char* label,
                     int value, int maxVal, uint16_t fillColor) {
    TFT_eSPI& tft = display_get_tft();

    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(COLOR_DIM);
    tft.drawString(label, x - 4, y, 1);

    int trackH = 6;
    tft.fillRect(x, y - trackH / 2, width, trackH, 0x1082);

    int fillW = (value * width) / maxVal;
    if (fillW > 0) {
        tft.fillRect(x, y - trackH / 2, fillW, trackH, fillColor);
    }

    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COLOR_DIM);
    char val[8];
    snprintf(val, sizeof(val), "%d", value);
    tft.drawString(val, x + width + 4, y, 1);
}

// ===== GENERIC BROWSER =====

static void drawBrowser(const char* title, uint16_t titleColor,
                        const char* subtitle, BrowserState& state,
                        ItemNameFn getName, int highlightId = -1) {
    TFT_eSPI& tft = display_get_tft();
    tft.fillScreen(COLOR_BG);

    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(titleColor);
    tft.drawString(title, 120, 28, 2);

    if (subtitle) {
        tft.setTextColor(COLOR_DIM);
        tft.drawString(subtitle, 120, 46, 1);
    }

    tft.setTextColor(0x2104);
    tft.setTextDatum(BC_DATUM);
    tft.drawString("hold = home", 120, 224, 1);

    if (state.count == 0) {
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_DIM);
        tft.drawString("Empty", 120, 130, 2);
        return;
    }

    const int listTop = 60;
    const int itemH = 24;
    const int maxVisible = 6;
    const int listLeft = 36;
    const int listRight = 204;

    if (state.sel < state.scroll) state.scroll = state.sel;
    if (state.sel >= state.scroll + maxVisible) state.scroll = state.sel - maxVisible + 1;
    if (state.scroll < 0) state.scroll = 0;

    int visible = min(maxVisible, state.count);

    for (int i = 0; i < visible; i++) {
        int idx = state.scroll + i;
        if (idx >= state.count) break;

        int y = listTop + i * itemH;
        bool isSel = (idx == state.sel);
        bool isActive = (idx == highlightId);

        if (isSel) {
            tft.fillRoundRect(listLeft - 2, y, listRight - listLeft + 4, itemH - 2, 4, 0x2100);
        }

        if (isActive) {
            tft.fillCircle(listLeft + 4, y + itemH / 2 - 1, 3, 0x07E0);
        }

        tft.setTextDatum(MR_DATUM);
        tft.setTextColor(COLOR_DIM);
        char numStr[8];
        snprintf(numStr, sizeof(numStr), "%d", idx);
        tft.drawString(numStr, listLeft + 22, y + itemH / 2 - 1, 1);

        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(isSel ? COLOR_ACCENT : 0xAD55);
        const char* name = getName(idx);
        tft.drawString(name, listLeft + 26, y + itemH / 2 - 1, 2);
    }

    if (state.scroll > 0) {
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(COLOR_DIM);
        tft.drawString("^", 120, 54, 1);
    }
    if (state.scroll + maxVisible < state.count) {
        tft.setTextDatum(BC_DATUM);
        tft.setTextColor(COLOR_DIM);
        tft.drawString("v", 120, listTop + visible * itemH + 2, 1);
    }
}

static bool handleBrowserInput(BrowserState& state) {
    int delta = input_encoder_delta();
    if (delta != 0 && state.count > 0) {
        state.sel += (delta > 0) ? 1 : -1;
        if (state.sel >= state.count) state.sel = state.count - 1;
        if (state.sel < 0) state.sel = 0;
        needsRedraw = true;
    }

    if (input_button_pressed()) {
        if (state.count > 0 && state.sel >= 0 && state.sel < state.count) {
            return true;
        }
    }
    return false;
}

// ===== HOME PAGE =====

static void drawHome() {
    TFT_eSPI& tft = display_get_tft();

    tft.pushImage(0, 0, 240, 240, homeRing);

    int devCount = discovery_get_count();

    if (devCount == 0) {
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_DIM);
        tft.drawString("No devices", 120, 110, 4);
        tft.setTextColor(0x2104);
        tft.drawString("Scanning...", 120, 140, 2);
        return;
    }

    if (activeDeviceIndex >= devCount) activeDeviceIndex = 0;
    WledDevice* dev = discovery_get_device(activeDeviceIndex);
    if (!dev) return;

    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(COLOR_ACCENT);
    tft.drawString(dev->name, 120, 52, 2);

    tft.setTextColor(COLOR_DIM);
    char counter[16];
    snprintf(counter, sizeof(counter), "%d / %d", activeDeviceIndex + 1, devCount);
    tft.drawString(counter, 120, 70, 1);

    if (!activeState.valid) {
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_WARN);
        tft.drawString("Connecting...", 120, 120, 2);
        return;
    }

    uint16_t pwrColor = activeState.on ? 0x07E0 : 0xF800;
    tft.fillCircle(75, 88, 4, pwrColor);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COLOR_DIM);
    tft.drawString(activeState.on ? "ON" : "OFF", 83, 88, 1);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE);
    const char* fxName = wled_get_effect_name(activeState.effectId);
    tft.drawString(fxName, 120, 108, 2);

    tft.setTextColor(COLOR_DIM);
    char fxId[16];
    snprintf(fxId, sizeof(fxId), "FX #%d", activeState.effectId);
    tft.drawString(fxId, 120, 124, 1);

    tft.setTextColor(COLOR_ACCENT);
    const char* palName = wled_get_palette_name(activeState.paletteId);
    tft.drawString(palName, 120, 142, 2);

    tft.setTextColor(COLOR_DIM);
    char palId[16];
    snprintf(palId, sizeof(palId), "PAL #%d", activeState.paletteId);
    tft.drawString(palId, 120, 158, 1);

    drawHBar(64, 178, 80, "BRI", activeState.brightness, 255, 0xFD20);

    tft.setTextDatum(BC_DATUM);
    tft.setTextColor(0x2104);
    tft.drawString("hold = settings", 120, 198, 1);
}

// ===== DEVICE LIST =====

static void drawDeviceList() {
    TFT_eSPI& tft = display_get_tft();
    tft.fillScreen(COLOR_BG);

    int devCount = discovery_get_count();

    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(COLOR_ACCENT);
    tft.drawString("DEVICES", 120, 28, 2);

    tft.setTextColor(COLOR_DIM);
    char sub[32];
    snprintf(sub, sizeof(sub), "%d found", devCount);
    tft.drawString(sub, 120, 46, 1);

    tft.setTextDatum(BC_DATUM);
    tft.setTextColor(0x2104);
    tft.drawString("v groups", 120, 224, 1);
    tft.drawString("hold = home", 120, 212, 1);

    if (devCount == 0) {
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_DIM);
        tft.drawString("Scanning...", 120, 130, 2);
        return;
    }

    const int listTop = 62;
    const int itemH = 26;
    const int maxVisible = 5;
    const int listLeft = 36;
    const int listRight = 204;

    if (devListSel < devListScroll) devListScroll = devListSel;
    if (devListSel >= devListScroll + maxVisible) devListScroll = devListSel - maxVisible + 1;
    if (devListScroll < 0) devListScroll = 0;

    int visible = min(maxVisible, devCount);

    for (int i = 0; i < visible; i++) {
        int idx = devListScroll + i;
        if (idx >= devCount) break;

        WledDevice* dev = discovery_get_device(idx);
        if (!dev) continue;

        int y = listTop + i * itemH;
        bool isSel = (idx == devListSel);
        bool isActive = (idx == activeDeviceIndex);

        if (isSel) {
            tft.fillRoundRect(listLeft - 2, y - 1, listRight - listLeft + 4, itemH - 2, 4, 0x2100);
        }

        if (isActive) {
            tft.fillCircle(listLeft + 4, y + itemH / 2 - 2, 3, 0x07E0);
        }

        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(isSel ? COLOR_ACCENT : 0xAD55);
        tft.drawString(dev->name, listLeft + 14, y + itemH / 2 - 2, 2);

        const char* ip = dev->ip;
        const char* lastDot = strrchr(ip, '.');
        if (lastDot) {
            tft.setTextDatum(MR_DATUM);
            tft.setTextColor(COLOR_DIM);
            tft.drawString(lastDot, listRight, y + itemH / 2 - 2, 1);
        }
    }

    if (devListScroll > 0) {
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(COLOR_DIM);
        tft.drawString("^", 120, 56, 1);
    }
    if (devListScroll + maxVisible < devCount) {
        tft.setTextDatum(BC_DATUM);
        tft.setTextColor(COLOR_DIM);
        tft.drawString("v", 120, listTop + visible * itemH + 4, 1);
    }
}

// ===== EFFECTS DRAWER =====

static void drawFxDrawer() {
    TFT_eSPI& tft = display_get_tft();
    tft.fillScreen(COLOR_BG);

    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(COLOR_ACCENT);
    tft.drawString("EFFECTS", 120, 24, 2);

    tft.setTextColor(0x2104);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("^ home", 120, 10, 1);
    tft.setTextDatum(BC_DATUM);
    tft.drawString("v presets", 120, 230, 1);

    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(0x2104);
    tft.drawString("< fav", 10, 120, 1);
    tft.setTextDatum(MR_DATUM);
    tft.drawString("pal >", 230, 120, 1);

    if (!activeState.valid) {
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_DIM);
        tft.drawString("No device", 120, 120, 2);
        return;
    }

    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COLOR_DIM);
    tft.drawString("Effect", 44, 54, 1);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE);
    const char* fxName = wled_get_effect_name(activeState.effectId);
    tft.drawString(fxName, 120, 72, 2);

    tft.setTextColor(COLOR_DIM);
    char fxId[16];
    snprintf(fxId, sizeof(fxId), "#%d", activeState.effectId);
    tft.drawString(fxId, 120, 90, 1);

    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COLOR_DIM);
    tft.drawString("Palette", 44, 110, 1);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_ACCENT);
    const char* palName = wled_get_palette_name(activeState.paletteId);
    tft.drawString(palName, 120, 128, 2);

    tft.setTextColor(COLOR_DIM);
    char palId[16];
    snprintf(palId, sizeof(palId), "#%d", activeState.paletteId);
    tft.drawString(palId, 120, 146, 1);

    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COLOR_DIM);
    tft.drawString("Brightness", 44, 168, 1);
    drawHBar(54, 184, 100, "", activeState.brightness, 255, 0xFD20);

    tft.setTextDatum(BC_DATUM);
    tft.setTextColor(0x2104);
    tft.drawString("encoder = brightness", 120, 214, 1);
}

static void handleFxDrawerInput() {
    int delta = input_encoder_delta();
    if (delta != 0 && activeState.valid) {
        int newBri = activeState.brightness + delta * 5;
        if (newBri > 255) newBri = 255;
        if (newBri < 0) newBri = 0;
        activeState.brightness = newBri;

        WledDevice* dev = discovery_get_device(activeDeviceIndex);
        if (dev) {
            wled_set_brightness(dev->ip, newBri);
        }
        needsRedraw = true;
    }
    input_button_pressed();
}

// ===== FX LIST =====

static void drawFxList() {
    char subtitle[32];
    snprintf(subtitle, sizeof(subtitle), "%d effects", fxListBrowser.count);
    drawBrowser("ALL EFFECTS", COLOR_ACCENT, subtitle,
                fxListBrowser, wled_get_effect_name, activeState.effectId);
}

static void handleFxListInput() {
    if (handleBrowserInput(fxListBrowser)) {
        WledDevice* dev = discovery_get_device(activeDeviceIndex);
        if (dev) {
            wled_set_effect(dev->ip, fxListBrowser.sel);
            activeState.effectId = fxListBrowser.sel;
            Serial.printf("[UI] Effect set: %d %s\n", fxListBrowser.sel,
                          wled_get_effect_name(fxListBrowser.sel));
            needsRedraw = true;
        }
    }
}

// ===== PALETTE LIST =====

static void drawPalList() {
    char subtitle[32];
    snprintf(subtitle, sizeof(subtitle), "%d palettes", palListBrowser.count);
    drawBrowser("ALL PALETTES", 0xA01F, subtitle,
                palListBrowser, wled_get_palette_name, activeState.paletteId);
}

static void handlePalListInput() {
    if (handleBrowserInput(palListBrowser)) {
        WledDevice* dev = discovery_get_device(activeDeviceIndex);
        if (dev) {
            wled_set_palette(dev->ip, palListBrowser.sel);
            activeState.paletteId = palListBrowser.sel;
            Serial.printf("[UI] Palette set: %d %s\n", palListBrowser.sel,
                          wled_get_palette_name(palListBrowser.sel));
            needsRedraw = true;
        }
    }
}

// ===== PLACEHOLDER =====

static void drawPlaceholder() {
    TFT_eSPI& tft = display_get_tft();
    tft.fillScreen(COLOR_BG);

    const NavTarget& nav = navMap[currentScreen];

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_ACCENT);
    tft.drawString(screenNames[currentScreen], 120, 120, 4);

    tft.setTextColor(COLOR_DIM);
    if (nav.up < SCREEN_COUNT) {
        tft.setTextDatum(TC_DATUM);
        String hint = String("^ ") + screenNames[nav.up];
        tft.drawString(hint, 120, 30, 2);
    }
    if (nav.down < SCREEN_COUNT) {
        tft.setTextDatum(BC_DATUM);
        String hint = String("v ") + screenNames[nav.down];
        tft.drawString(hint, 120, 215, 2);
    }
    if (nav.left < SCREEN_COUNT) {
        tft.setTextDatum(ML_DATUM);
        tft.drawString("<", 15, 120, 2);
    }
    if (nav.right < SCREEN_COUNT) {
        tft.setTextDatum(MR_DATUM);
        tft.drawString(">", 225, 120, 2);
    }

    if (currentScreen == SCREEN_SETTINGS || currentScreen == SCREEN_COLOR_PICKER) {
        tft.setTextDatum(BC_DATUM);
        tft.setTextColor(COLOR_WARN);
        tft.drawString("swipe down = back", 120, 195, 2);
    } else {
        tft.setTextDatum(BC_DATUM);
        tft.setTextColor(0x3186);
        tft.drawString("long press = home", 120, 195, 2);
    }
}

// ===== INPUT ROUTING =====

static void switchScreen(Screen next) {
    if (next >= SCREEN_COUNT) return;
    if (next == currentScreen) return;

    previousScreen = currentScreen;
    display_clear();
    currentScreen = next;
    needsRedraw = true;

    // Screen entry setup
    if (next == SCREEN_DEVICE_LIST) {
        devListSel = activeDeviceIndex;
        devListScroll = max(0, devListSel - 2);
    }
    if (next == SCREEN_FX_LIST) {
        ensureEffectCount();
        fxListBrowser.count = cachedEffectCount;
        fxListBrowser.sel = activeState.effectId;
        fxListBrowser.scroll = max(0, fxListBrowser.sel - 2);
    }
    if (next == SCREEN_PAL_LIST) {
        ensurePaletteCount();
        palListBrowser.count = cachedPaletteCount;
        palListBrowser.sel = activeState.paletteId;
        palListBrowser.scroll = max(0, palListBrowser.sel - 2);
    }

    Serial.printf("[UI] %s -> %s\n", screenNames[previousScreen], screenNames[currentScreen]);
}

static void handleNavInput() {
    Gesture g = input_gesture();
    const NavTarget& nav = navMap[currentScreen];

    switch (g) {
        case GESTURE_SWIPE_UP:
            if (nav.down < SCREEN_COUNT) switchScreen(nav.down);
            break;
        case GESTURE_SWIPE_DOWN:
            if (currentScreen == SCREEN_SETTINGS || currentScreen == SCREEN_COLOR_PICKER) {
                switchScreen(previousScreen);
            } else if (nav.up < SCREEN_COUNT) {
                switchScreen(nav.up);
            }
            break;
        case GESTURE_SWIPE_LEFT:
            if (nav.right < SCREEN_COUNT) switchScreen(nav.right);
            break;
        case GESTURE_SWIPE_RIGHT:
            if (nav.left < SCREEN_COUNT) switchScreen(nav.left);
            break;
        default:
            break;
    }

    if (input_button_long_pressed()) {
        if (currentScreen == SCREEN_HOME) {
            switchScreen(SCREEN_SETTINGS);
        } else {
            switchScreen(SCREEN_HOME);
        }
    }
}

static void handleHomeInput() {
    int delta = input_encoder_delta();
    if (delta != 0) {
        int count = discovery_get_count();
        if (count > 0) {
            activeDeviceIndex += (delta > 0) ? 1 : -1;
            if (activeDeviceIndex >= count) activeDeviceIndex = 0;
            if (activeDeviceIndex < 0) activeDeviceIndex = count - 1;
            refreshActiveDeviceState();
            cachedEffectCount = 0;
            cachedPaletteCount = 0;
            needsRedraw = true;
            lastStateRefresh = millis();
            Serial.printf("[UI] Home: device %d/%d\n", activeDeviceIndex + 1, count);
        }
    }
    input_button_pressed();
}

static void handleDeviceListInput() {
    int devCount = discovery_get_count();

    int delta = input_encoder_delta();
    if (delta != 0 && devCount > 0) {
        devListSel += (delta > 0) ? 1 : -1;
        if (devListSel >= devCount) devListSel = devCount - 1;
        if (devListSel < 0) devListSel = 0;
        needsRedraw = true;
    }

    if (input_button_pressed()) {
        if (devCount > 0 && devListSel >= 0 && devListSel < devCount) {
            activeDeviceIndex = devListSel;
            refreshActiveDeviceState();
            cachedEffectCount = 0;
            cachedPaletteCount = 0;
            lastStateRefresh = millis();
            Serial.printf("[UI] Device selected: %d\n", activeDeviceIndex);
            switchScreen(SCREEN_HOME);
        }
    }
}

// ===== PUBLIC FUNCTIONS =====

void ui_init() {
    currentScreen = SCREEN_HOME;
    previousScreen = SCREEN_HOME;
    needsRedraw = true;
    refreshActiveDeviceState();
    lastStateRefresh = millis();
    Serial.println("[UI] Initialized — 8B Navigation (16 screens)");
}

void ui_update() {
    handleNavInput();

    switch (currentScreen) {
        case SCREEN_HOME:
            handleHomeInput();
            break;
        case SCREEN_DEVICE_LIST:
            handleDeviceListInput();
            break;
        case SCREEN_FX_DRAWER:
            handleFxDrawerInput();
            break;
        case SCREEN_FX_LIST:
            handleFxListInput();
            break;
        case SCREEN_PAL_LIST:
            handlePalListInput();
            break;
        default:
            input_button_pressed();
            input_encoder_delta();
            break;
    }

    if (currentScreen == SCREEN_HOME) {
        unsigned long now = millis();
        if (now - lastStateRefresh >= STATE_REFRESH_MS) {
            WledState oldState = activeState;
            refreshActiveDeviceState();
            lastStateRefresh = now;
            if (memcmp(&oldState, &activeState, sizeof(WledState)) != 0) {
                needsRedraw = true;
            }
        }
    }

    if (needsRedraw) {
        switch (currentScreen) {
            case SCREEN_HOME:
                drawHome();
                break;
            case SCREEN_DEVICE_LIST:
                drawDeviceList();
                break;
            case SCREEN_FX_DRAWER:
                drawFxDrawer();
                break;
            case SCREEN_FX_LIST:
                drawFxList();
                break;
            case SCREEN_PAL_LIST:
                drawPalList();
                break;
            default:
                drawPlaceholder();
                break;
        }
        needsRedraw = false;
    }
}

Screen ui_get_screen() {
    return currentScreen;
}