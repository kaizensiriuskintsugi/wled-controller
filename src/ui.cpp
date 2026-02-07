#include "ui.h"
#include "display.h"
#include "input.h"
#include "discovery.h"
#include "wled_api.h"

// ===== STATE =====
static Screen currentScreen = SCREEN_BOOT;
static int selectedIndex = 0;
static int scrollOffset = 0;
static bool needsRedraw = true;

// Encoder accumulator for list scrolling
static int encAccum = 0;
#define ENC_LIST_THRESHOLD 4

// Selected device for control screen
static WledDevice* activeDevice = nullptr;
static WledState deviceState = {};
static unsigned long lastStateRefresh = 0;
#define STATE_REFRESH_MS 3000

// Brightness tracking
static int pendingBrightness = -1;
static unsigned long lastBriSend = 0;
#define BRI_SEND_INTERVAL_MS 100

// Effect browser state
static int effectIndex = 0;
static int effectScrollOffset = 0;
static int effectCount = 0;
static int effectEncAccum = 0;

// Palette browser state
static int paletteIndex = 0;
static int paletteScrollOffset = 0;
static int paletteCount = 0;
static int paletteEncAccum = 0;

// Color picker state
static int pickerHue = 0;         // 0-359 degrees
static int pickerSat = 255;       // 0-255
static bool pickerNeedsRing = true; // full ring redraw needed
static int oldPickerHue = 0;    // track previous indicator position for clean up

// Display layout
#define VISIBLE_ITEMS 4
#define LIST_START_Y 60
#define LIST_ITEM_HEIGHT 35
#define LIST_X 120

// Arc settings
#define ARC_CENTER_Y 115
#define ARC_RADIUS 95
#define ARC_THICKNESS 3

// Color picker layout
#define PICKER_CX 120
#define PICKER_CY 120
#define RING_OUTER 105
#define RING_INNER 75
#define PREVIEW_RADIUS 40

// ===== HELPERS =====

static void switchScreen(Screen next) {
    display_clear();
    currentScreen = next;
    needsRedraw = true;
    encAccum = 0;
    effectEncAccum = 0;
    paletteEncAccum = 0;
    Serial.printf("[UI] Screen → %d\n", next);
}

static void drawArc(int brightness) {
    TFT_eSPI& tft = display_get_tft();
    int arcAngle = map(brightness, 0, 255, 0, 240);
    int arcStart = 150;

    for (int a = 0; a < 240; a++) {
        float rad = (arcStart + a) * DEG_TO_RAD;
        uint16_t color = (a < arcAngle) ? COLOR_ACCENT : COLOR_DIM;

        for (int r = -ARC_THICKNESS; r <= ARC_THICKNESS; r++) {
            int x = LIST_X + cos(rad) * (ARC_RADIUS + r);
            int y = ARC_CENTER_Y + sin(rad) * (ARC_RADIUS + r);
            tft.drawPixel(x, y, color);
        }
    }
}

// ===== HSV TO RGB =====
// h: 0-359, s: 0-255, v: 0-255 → r,g,b: 0-255

static void hsvToRgb(int h, int s, int v, uint8_t &r, uint8_t &g, uint8_t &b) {
    if (s == 0) {
        r = g = b = v;
        return;
    }

    int region = h / 60;
    int remainder = (h - (region * 60)) * 255 / 60;

    int p = (v * (255 - s)) >> 8;
    int q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    int t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region) {
        case 0:  r = v; g = t; b = p; break;
        case 1:  r = q; g = v; b = p; break;
        case 2:  r = p; g = v; b = t; break;
        case 3:  r = p; g = q; b = v; break;
        case 4:  r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
}

// Convert HSV to TFT 565 color
static uint16_t hsvTo565(int h, int s, int v) {
    uint8_t r, g, b;
    hsvToRgb(h, s, v, r, g, b);
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// ===== GENERIC BROWSER DRAW =====

static void drawBrowserList(const char* title, int count, int selIndex, int scrOffset,
                            int activeId, const char* (*getName)(int)) {
    TFT_eSPI& tft = display_get_tft();
    tft.fillScreen(COLOR_BG);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_ACCENT);
    tft.drawString(title, LIST_X, 25, 4);

    if (count == 0) {
        tft.setTextColor(COLOR_DIM);
        tft.drawString("None available", LIST_X, 120, 2);
        return;
    }

    for (int i = 0; i < VISIBLE_ITEMS && (i + scrOffset) < count; i++) {
        int idx = i + scrOffset;
        int y = LIST_START_Y + (i * LIST_ITEM_HEIGHT);

        const char* name = getName(idx);
        String label = "#" + String(idx) + " " + String(name);

        if (idx == selIndex) {
            tft.setTextColor(COLOR_ACCENT);
            tft.setTextDatum(ML_DATUM);
            tft.drawString(">", 15, y + 12, 2);
            tft.setTextDatum(MC_DATUM);
            tft.drawString(label, LIST_X + 5, y + 12, 2);
        } else if (idx == activeId) {
            tft.setTextColor(COLOR_SUCCESS);
            tft.setTextDatum(MC_DATUM);
            tft.drawString(label, LIST_X + 5, y + 12, 2);
        } else {
            tft.setTextColor(COLOR_TEXT);
            tft.setTextDatum(MC_DATUM);
            tft.drawString(label, LIST_X + 5, y + 12, 2);
        }
    }

    tft.setTextColor(COLOR_DIM);
    tft.setTextDatum(MC_DATUM);
    String pos = String(selIndex) + "/" + String(count - 1);
    tft.drawString(pos, LIST_X, 220, 2);
}

// ===== GENERIC BROWSER INPUT =====

static int handleBrowserInput(int& idx, int& scrOffset, int& accum, int count) {
    if (count == 0) return 0;

    int delta = input_encoder_delta();
    if (delta != 0) {
        accum += delta;

        int move = 0;
        if (accum >= ENC_LIST_THRESHOLD) {
            move = 1;
            accum = 0;
        } else if (accum <= -ENC_LIST_THRESHOLD) {
            move = -1;
            accum = 0;
        }

        if (move != 0) {
            idx += move;
            if (idx < 0) idx = 0;
            if (idx >= count) idx = count - 1;

            if (idx < scrOffset) scrOffset = idx;
            if (idx >= scrOffset + VISIBLE_ITEMS) scrOffset = idx - VISIBLE_ITEMS + 1;
            needsRedraw = true;
        }
    }

    if (input_button_pressed()) return 1;

    Gesture g = input_gesture();
    if (g == GESTURE_SWIPE_DOWN) return -1;

    return 0;
}

// ===== DEVICE LIST SCREEN =====

static void drawDeviceList() {
    TFT_eSPI& tft = display_get_tft();
    tft.fillScreen(COLOR_BG);

    int count = discovery_get_count();

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_ACCENT);
    tft.drawString("WLED Devices", LIST_X, 30, 4);

    if (count == 0) {
        tft.setTextColor(COLOR_DIM);
        tft.drawString("No devices found", LIST_X, 120, 2);
        tft.drawString("Check WiFi/network", LIST_X, 145, 2);
        return;
    }

    for (int i = 0; i < VISIBLE_ITEMS && (i + scrollOffset) < count; i++) {
        int deviceIdx = i + scrollOffset;
        WledDevice* dev = discovery_get_device(deviceIdx);
        if (!dev) continue;

        int y = LIST_START_Y + (i * LIST_ITEM_HEIGHT);

        if (deviceIdx == selectedIndex) {
            tft.setTextColor(COLOR_ACCENT);
            tft.setTextDatum(ML_DATUM);
            tft.drawString(">", 30, y + 12, 2);
            tft.setTextDatum(MC_DATUM);
            tft.drawString(dev->name, LIST_X, y + 12, 4);
        } else {
            tft.setTextColor(COLOR_TEXT);
            tft.setTextDatum(MC_DATUM);
            tft.drawString(dev->name, LIST_X, y + 12, 2);
        }
    }

    tft.setTextColor(COLOR_DIM);
    tft.setTextDatum(MC_DATUM);
    String footer = String(count) + " devices";
    tft.drawString(footer, LIST_X, 220, 2);
}

static void handleDeviceListInput() {
    int count = discovery_get_count();
    if (count == 0) return;

    int delta = input_encoder_delta();
    if (delta != 0) {
        encAccum += delta;

        int move = 0;
        if (encAccum >= ENC_LIST_THRESHOLD) {
            move = 1;
            encAccum = 0;
        } else if (encAccum <= -ENC_LIST_THRESHOLD) {
            move = -1;
            encAccum = 0;
        }

        if (move != 0) {
            selectedIndex += move;
            if (selectedIndex < 0) selectedIndex = 0;
            if (selectedIndex >= count) selectedIndex = count - 1;

            if (selectedIndex < scrollOffset) scrollOffset = selectedIndex;
            if (selectedIndex >= scrollOffset + VISIBLE_ITEMS) scrollOffset = selectedIndex - VISIBLE_ITEMS + 1;
            needsRedraw = true;
        }
    }

    bool select = false;
    if (input_button_pressed()) select = true;
    Gesture g = input_gesture();
    if (g == GESTURE_TAP) select = true;

    if (select) {
        activeDevice = discovery_get_device(selectedIndex);
        Serial.printf("[UI] Selected: %s (%s)\n", activeDevice->name, activeDevice->ip);

        deviceState = wled_get_state(activeDevice->ip);
        pendingBrightness = deviceState.brightness;
        lastStateRefresh = millis();

        switchScreen(SCREEN_DEVICE_CONTROL);
    }
}

// ===== DEVICE CONTROL SCREEN =====

static void drawDeviceControl() {
    TFT_eSPI& tft = display_get_tft();
    tft.fillScreen(COLOR_BG);

    if (!activeDevice || !deviceState.valid) {
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_ERROR);
        tft.drawString("No data", LIST_X, 120, 4);
        return;
    }

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_ACCENT);
    tft.drawString(activeDevice->name, LIST_X, 25, 2);

    tft.setTextColor(deviceState.on ? COLOR_SUCCESS : COLOR_ERROR);
    tft.drawString(deviceState.on ? "ON" : "OFF", LIST_X, 45, 2);

    int bri = (pendingBrightness >= 0) ? pendingBrightness : deviceState.brightness;
    tft.setTextColor(COLOR_TEXT);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(String(bri), LIST_X, 100, 7);

    drawArc(bri);

    tft.setTextColor(COLOR_DIM);
    tft.setTextDatum(MC_DATUM);
    String fxLabel = "FX: " + String(wled_get_effect_name(deviceState.effectId));
    String palLabel = "PAL: " + String(wled_get_palette_name(deviceState.paletteId));
    tft.drawString(fxLabel, LIST_X, 185, 2);
    tft.drawString(palLabel, LIST_X, 200, 2);
}

static void drawBrightnessOnly() {
    TFT_eSPI& tft = display_get_tft();

    int bri = (pendingBrightness >= 0) ? pendingBrightness : deviceState.brightness;

    tft.fillRect(20, 70, 200, 60, COLOR_BG);
    tft.setTextColor(COLOR_TEXT);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(String(bri), LIST_X, 100, 7);

    drawArc(bri);
}

static void handleDeviceControlInput() {
    if (!activeDevice) return;

    int delta = input_encoder_delta();
    if (delta != 0) {
        if (pendingBrightness < 0) pendingBrightness = deviceState.brightness;
        pendingBrightness += delta * 2;
        if (pendingBrightness < 0) pendingBrightness = 0;
        if (pendingBrightness > 255) pendingBrightness = 255;

        drawBrightnessOnly();

        if (millis() - lastBriSend > BRI_SEND_INTERVAL_MS) {
            wled_set_brightness(activeDevice->ip, pendingBrightness);
            lastBriSend = millis();
        }
    }

    if (input_button_pressed()) {
        Serial.println("[UI] Button — entering effect browser");
        effectCount = wled_get_effects_count(activeDevice->ip);
        effectIndex = deviceState.effectId;
        effectScrollOffset = max(0, effectIndex - 1);
        effectEncAccum = 0;
        switchScreen(SCREEN_EFFECT_BROWSER);
        return;
    }

    if (input_button_long_pressed()) {
        Serial.println("[UI] Encoder long press — identify device");
        uint8_t origBri = deviceState.brightness;
        wled_set_brightness(activeDevice->ip, 255);
        delay(300);
        wled_set_brightness(activeDevice->ip, 0);
        delay(300);
        wled_set_brightness(activeDevice->ip, 255);
        delay(300);
        wled_set_brightness(activeDevice->ip, origBri);
        needsRedraw = true;
    }

    Gesture g = input_gesture();
    if (g == GESTURE_SWIPE_DOWN) {
        Serial.println("[UI] Swipe down — back to list");
        pendingBrightness = -1;
        switchScreen(SCREEN_DEVICE_LIST);
        return;
    }
    if (g == GESTURE_SWIPE_LEFT) {
        Serial.println("[UI] Swipe left — entering palette browser");
        paletteCount = wled_get_palettes_count(activeDevice->ip);
        paletteIndex = deviceState.paletteId;
        paletteScrollOffset = max(0, paletteIndex - 1);
        paletteEncAccum = 0;
        switchScreen(SCREEN_PALETTE_BROWSER);
        return;
    }
    if (g == GESTURE_SWIPE_RIGHT) {
        Serial.println("[UI] Swipe right — entering color picker");
        // Initialize picker from current device color
        // We start with the device's current color as-is
        // Default to full sat, derive hue from RGB approximately
        pickerHue = 0;
        pickerSat = 255;
        pickerNeedsRing = true;
        switchScreen(SCREEN_COLOR_PICKER);
        return;
    }
    if (g == GESTURE_SWIPE_UP) {
        // Power toggle
        bool newState = !deviceState.on;
        Serial.printf("[UI] Swipe up — power %s\n", newState ? "ON" : "OFF");
        wled_set_power(activeDevice->ip, newState);
        deviceState.on = newState;
        needsRedraw = true;
        return;
    }
    if (g == GESTURE_LONG_PRESS) {
        Serial.println("[UI] Touch long press — identify device");
        uint8_t origBri = deviceState.brightness;
        wled_set_brightness(activeDevice->ip, 255);
        delay(300);
        wled_set_brightness(activeDevice->ip, 0);
        delay(300);
        wled_set_brightness(activeDevice->ip, 255);
        delay(300);
        wled_set_brightness(activeDevice->ip, origBri);
        needsRedraw = true;
    }

    if (millis() - lastStateRefresh > STATE_REFRESH_MS) {
        deviceState = wled_get_state(activeDevice->ip);
        lastStateRefresh = millis();
        if (pendingBrightness < 0) needsRedraw = true;
    }
}

// ===== EFFECT BROWSER SCREEN =====

static void drawEffectBrowser() {
    drawBrowserList("Effects", effectCount, effectIndex, effectScrollOffset,
                    deviceState.effectId, wled_get_effect_name);
}

static void handleEffectBrowserInput() {
    int result = handleBrowserInput(effectIndex, effectScrollOffset, effectEncAccum, effectCount);

    if (result == 1) {
        Serial.printf("[UI] Effect selected: #%d %s\n", effectIndex, wled_get_effect_name(effectIndex));
        wled_set_effect(activeDevice->ip, effectIndex);
        deviceState.effectId = effectIndex;
        switchScreen(SCREEN_DEVICE_CONTROL);
    } else if (result == -1) {
        Serial.println("[UI] Swipe down — back to control (no change)");
        switchScreen(SCREEN_DEVICE_CONTROL);
    }
}

// ===== PALETTE BROWSER SCREEN =====

static void drawPaletteBrowser() {
    drawBrowserList("Palettes", paletteCount, paletteIndex, paletteScrollOffset,
                    deviceState.paletteId, wled_get_palette_name);
}

static void handlePaletteBrowserInput() {
    int result = handleBrowserInput(paletteIndex, paletteScrollOffset, paletteEncAccum, paletteCount);

    if (result == 1) {
        Serial.printf("[UI] Palette selected: #%d %s\n", paletteIndex, wled_get_palette_name(paletteIndex));
        wled_set_palette(activeDevice->ip, paletteIndex);
        deviceState.paletteId = paletteIndex;
        switchScreen(SCREEN_DEVICE_CONTROL);
    } else if (result == -1) {
        Serial.println("[UI] Swipe down — back to control (no change)");
        switchScreen(SCREEN_DEVICE_CONTROL);
    }
}

// ===== COLOR PICKER SCREEN =====

static void drawColorRing() {
    TFT_eSPI& tft = display_get_tft();

    // Draw hue ring — each angle is a different hue
    for (int angle = 0; angle < 360; angle++) {
        float rad = angle * DEG_TO_RAD;
        uint16_t col = hsvTo565(angle, 255, 255);

        // Draw a few pixels thick for the ring
        for (int r = RING_INNER; r <= RING_OUTER; r++) {
            int x = PICKER_CX + cos(rad) * r;
            int y = PICKER_CY + sin(rad) * r;
            if (x >= 0 && x < 240 && y >= 0 && y < 240) {
                tft.drawPixel(x, y, col);
            }
        }
    }
}

static void drawPickerIndicator() {
    TFT_eSPI& tft = display_get_tft();

    // Small white dot on the ring at current hue
    float rad = pickerHue * DEG_TO_RAD;
    int midR = (RING_INNER + RING_OUTER) / 2;
    int ix = PICKER_CX + cos(rad) * midR;
    int iy = PICKER_CY + sin(rad) * midR;
    tft.fillCircle(ix, iy, 4, TFT_WHITE);
    tft.drawCircle(ix, iy, 5, TFT_BLACK);
}

static void drawPickerPreview() {
    TFT_eSPI& tft = display_get_tft();

    // Center preview circle showing selected color
    uint16_t previewCol = hsvTo565(pickerHue, pickerSat, 255);
    tft.fillCircle(PICKER_CX, PICKER_CY, PREVIEW_RADIUS, previewCol);

    // Saturation label below preview
    tft.fillRect(70, 175, 100, 20, COLOR_BG);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_DIM);
    int satPct = map(pickerSat, 0, 255, 0, 100);
    String satLabel = "SAT: " + String(satPct) + "%";
    tft.drawString(satLabel, PICKER_CX, 185, 2);
}

static void drawColorPicker() {
    TFT_eSPI& tft = display_get_tft();
    tft.fillScreen(COLOR_BG);

    // Title
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_ACCENT);
    tft.drawString("Color", PICKER_CX, 15, 2);

    drawColorRing();
    drawPickerIndicator();
    drawPickerPreview();

    // Hint at bottom
    tft.setTextColor(COLOR_DIM);
    tft.drawString("Tap=Set  Down=Cancel", PICKER_CX, 220, 1);

    pickerNeedsRing = false;
    oldPickerHue = pickerHue;
}

static void drawPickerUpdate() {
    TFT_eSPI& tft = display_get_tft();

    if (pickerNeedsRing) {
        drawColorPicker();
        return;
    }

    // Erase old indicator by filling over it with background
    float oldRad = oldPickerHue * DEG_TO_RAD;
    int midR = (RING_INNER + RING_OUTER) / 2;
    int ox = PICKER_CX + cos(oldRad) * midR;
    int oy = PICKER_CY + sin(oldRad) * midR;
    tft.fillCircle(ox, oy, 6, COLOR_BG);

    // Redraw ring segment around old position to fill the gap
    for (int a = oldPickerHue - 5; a <= oldPickerHue + 5; a++) {
        int angle = (a + 360) % 360;
        float rad = angle * DEG_TO_RAD;
        uint16_t col = hsvTo565(angle, 255, 255);
        for (int r = RING_INNER; r <= RING_OUTER; r++) {
            int x = PICKER_CX + cos(rad) * r;
            int y = PICKER_CY + sin(rad) * r;
            if (x >= 0 && x < 240 && y >= 0 && y < 240) {
                tft.drawPixel(x, y, col);
            }
        }
    }

    // Draw new indicator and preview
    drawPickerIndicator();
    drawPickerPreview();

    oldPickerHue = pickerHue;
}

static void handleColorPickerInput() {
    if (!activeDevice) return;

    // Encoder adjusts saturation
    int delta = input_encoder_delta();
    if (delta != 0) {
        pickerSat += delta * 8;
        if (pickerSat < 0) pickerSat = 0;
        if (pickerSat > 255) pickerSat = 255;
        drawPickerUpdate();
    }

    // Touch on ring area = select hue
    if (input_touch_active()) {
        int tx = input_touch_x() - PICKER_CX;
        int ty = input_touch_y() - PICKER_CY;
        float dist = sqrt(tx * tx + ty * ty);

        // Only respond to touches in the ring area
        if (dist >= RING_INNER - 10 && dist <= RING_OUTER + 10) {
            int newHue = (int)(atan2(ty, tx) * RAD_TO_DEG);
            if (newHue < 0) newHue += 360;

            if (abs(newHue - pickerHue) > 2) {  // deadzone to reduce flicker
                pickerHue = newHue;
                drawPickerUpdate();
            }
        }
    }

    // Encoder press OR tap = confirm color and send
    bool confirm = false;
    if (input_button_pressed()) confirm = true;

    Gesture g = input_gesture();
    if (g == GESTURE_TAP) {
        // Only confirm if tap is in center preview area
        // (ring taps are handled by touch_active above)
        // We'll just use encoder press for confirm, tap for hue selection
        // Actually let's make encoder press the confirm
    }
    if (g == GESTURE_SWIPE_DOWN) {
        Serial.println("[UI] Swipe down — cancel color picker");
        switchScreen(SCREEN_DEVICE_CONTROL);
        return;
    }

    if (confirm) {
        uint8_t r, g_val, b;
        hsvToRgb(pickerHue, pickerSat, 255, r, g_val, b);
        Serial.printf("[UI] Color set: H%d S%d → R%d G%d B%d\n",
                      pickerHue, pickerSat, r, g_val, b);
        wled_set_color(activeDevice->ip, r, g_val, b);
        deviceState.colorR = r;
        deviceState.colorG = g_val;
        deviceState.colorB = b;
        switchScreen(SCREEN_DEVICE_CONTROL);
    }
}

// ===== PUBLIC FUNCTIONS =====

void ui_init() {
    currentScreen = SCREEN_DEVICE_LIST;
    selectedIndex = 0;
    scrollOffset = 0;
    encAccum = 0;
    needsRedraw = true;
    Serial.println("[UI] Initialized — Device List");
}

void ui_update() {
    // Pass 1: Handle input for current screen
    switch (currentScreen) {
        case SCREEN_DEVICE_LIST:
            handleDeviceListInput();
            break;
        case SCREEN_DEVICE_CONTROL:
            handleDeviceControlInput();
            break;
        case SCREEN_EFFECT_BROWSER:
            handleEffectBrowserInput();
            break;
        case SCREEN_PALETTE_BROWSER:
            handlePaletteBrowserInput();
            break;
        case SCREEN_COLOR_PICKER:
            handleColorPickerInput();
            break;
        default:
            break;
    }

    // Pass 2: Draw current screen (may have changed during input)
    if (needsRedraw) {
        switch (currentScreen) {
            case SCREEN_DEVICE_LIST:
                drawDeviceList();
                break;
            case SCREEN_DEVICE_CONTROL:
                drawDeviceControl();
                break;
            case SCREEN_EFFECT_BROWSER:
                drawEffectBrowser();
                break;
            case SCREEN_PALETTE_BROWSER:
                drawPaletteBrowser();
                break;
            case SCREEN_COLOR_PICKER:
                drawColorPicker();
                break;
            default:
                break;
        }
        needsRedraw = false;
    }
}

Screen ui_get_screen() {
    return currentScreen;
}