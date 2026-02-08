#include "ui.h"
#include "display.h"
#include "input.h"
#include "discovery.h"
#include "wled_api.h"
#include <math.h>

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

// Display layout
#define VISIBLE_ITEMS 4
#define LIST_START_Y 60
#define LIST_ITEM_HEIGHT 35
#define LIST_X 120

// Arc settings
#define ARC_CENTER_Y 115
#define ARC_RADIUS 95
#define ARC_THICKNESS 3

// Browser state (shared between effect and palette browsers)
static int browserIndex = 0;
static int browserScroll = 0;
static int browserCount = 0;
static int browserEncAccum = 0;
#define BROWSER_VISIBLE 4
#define BROWSER_START_Y 55
#define BROWSER_ITEM_H 35

// Color picker state
static float pickerHue = 0;        // 0-360
static uint8_t pickerSat = 255;    // 0-255
static float oldIndicatorAngle = -1;
#define HUE_RING_OUTER 110
#define HUE_RING_INNER 85
#define PREVIEW_RADIUS 35

// ===== EFFECT NAME LOOKUP (50 common WLED effects) =====
static const char* effectNames[] = {
    "Solid", "Blink", "Breathe", "Wipe", "Wipe Random",
    "Random Colors", "Sweep", "Dynamic", "Colorloop", "Rainbow",
    "Scan", "Scan Dual", "Fade", "Theater", "Theater Rainbow",
    "Running", "Saw", "Twinkle", "Dissolve", "Dissolve Rnd",
    "Sparkle", "Sparkle Dark", "Sparkle+", "Strobe", "Strobe Rainbow",
    "Strobe Mega", "Blink Rainbow", "Android", "Chase", "Chase Random",
    "Chase Rainbow", "Chase Flash", "Chase Flash Rnd", "Rainbow Runner", "Colorful",
    "Traffic Light", "Sweep Random", "Chase 2", "Aurora", "Stream",
    "Scanner", "Lighthouse", "Fireworks", "Rain", "Tetrix",
    "Fire Flicker", "Gradient", "Loading", "Police", "Fairy"
};
#define EFFECT_NAME_COUNT 50

// ===== PALETTE NAME LOOKUP (50 common WLED palettes) =====
static const char* paletteNames[] = {
    "Default", "Random Cycle", "Color 1", "Colors 1&2", "Color Gradient",
    "Colors Only", "Party", "Cloud", "Lava", "Ocean",
    "Forest", "Rainbow", "Rainbow Bands", "Sunset", "Rivendell",
    "Breeze", "Red & Blue", "Yellowout", "Analogous", "Splash",
    "Pastel", "Sunset 2", "Beach", "Vintage", "Departure",
    "Landscape", "Beech", "Sherbet", "Hult", "Hult 64",
    "Drywet", "Jul", "Grintage", "Rewhi", "Tertiary",
    "Fire", "Icefire", "Cyane", "Light Pink", "Autumn",
    "Magenta", "Magred", "Yelmag", "Yelblu", "Orange & Teal",
    "Tiamat", "April Night", "Orangery", "C9", "Sakura"
};
#define PALETTE_NAME_COUNT 50

// ===== HELPERS =====

static void switchScreen(Screen next) {
    display_clear();
    currentScreen = next;
    needsRedraw = true;
    encAccum = 0;
    browserEncAccum = 0;
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

// ===== HSV TO RGB CONVERSION =====
static void hsvToRgb(float h, uint8_t s, uint8_t v, uint8_t &r, uint8_t &g, uint8_t &b) {
    float hf = h / 60.0f;
    int i = (int)hf;
    float f = hf - i;
    float sf = s / 255.0f;
    uint8_t p = v * (1.0f - sf);
    uint8_t q = v * (1.0f - sf * f);
    uint8_t t = v * (1.0f - sf * (1.0f - f));
    switch (i % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        case 5: r = v; g = p; b = q; break;
    }
}

// Convert HSV hue (0-360) to TFT 565 color
static uint16_t hueToColor565(float h) {
    uint8_t r, g, b;
    hsvToRgb(h, 255, 255, r, g, b);
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
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

            if (selectedIndex < scrollOffset) {
                scrollOffset = selectedIndex;
            }
            if (selectedIndex >= scrollOffset + VISIBLE_ITEMS) {
                scrollOffset = selectedIndex - VISIBLE_ITEMS + 1;
            }
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

    // Device name
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_ACCENT);
    tft.drawString(activeDevice->name, LIST_X, 25, 2);

    // Power indicator
    tft.setTextColor(deviceState.on ? COLOR_SUCCESS : COLOR_ERROR);
    tft.drawString(deviceState.on ? "ON" : "OFF", LIST_X, 45, 2);

    // Brightness — big number
    int bri = (pendingBrightness >= 0) ? pendingBrightness : deviceState.brightness;
    tft.setTextColor(COLOR_TEXT);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(String(bri), LIST_X, 100, 7);

    // Brightness arc
    drawArc(bri);

    // Effect name (or ID if beyond lookup table)
    tft.setTextColor(COLOR_DIM);
    tft.setTextDatum(MC_DATUM);
    if (deviceState.effectId < EFFECT_NAME_COUNT) {
        tft.drawString("FX: " + String(effectNames[deviceState.effectId]), LIST_X, 185, 2);
    } else {
        tft.drawString("FX: #" + String(deviceState.effectId), LIST_X, 185, 2);
    }
    if (deviceState.paletteId < PALETTE_NAME_COUNT) {
        tft.drawString("PAL: " + String(paletteNames[deviceState.paletteId]), LIST_X, 200, 2);
    } else {
        tft.drawString("PAL: #" + String(deviceState.paletteId), LIST_X, 200, 2);
    }
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

    // Encoder = brightness
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

    // Encoder short press = effect browser
    if (input_button_pressed()) {
        Serial.println("[UI] Button — entering effect browser");
        browserCount = wled_get_effects_count(activeDevice->ip);
        if (browserCount <= 0) browserCount = EFFECT_NAME_COUNT;
        browserIndex = deviceState.effectId;
        browserScroll = max(0, browserIndex - 1);
        switchScreen(SCREEN_EFFECT_BROWSER);
        return;
    }

    // Encoder long press = identify device
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

    // Touch gestures
    Gesture g = input_gesture();
    if (g == GESTURE_SWIPE_DOWN) {
        Serial.println("[UI] Swipe down — back to list");
        pendingBrightness = -1;
        switchScreen(SCREEN_DEVICE_LIST);
        return;
    }
    if (g == GESTURE_SWIPE_UP) {
        Serial.println("[UI] Swipe up — toggle power");
        deviceState.on = !deviceState.on;
        wled_set_power(activeDevice->ip, deviceState.on);
        needsRedraw = true;
        return;
    }
    if (g == GESTURE_SWIPE_LEFT) {
        Serial.println("[UI] Swipe left — palette browser");
        browserCount = wled_get_palettes_count(activeDevice->ip);
        if (browserCount <= 0) browserCount = PALETTE_NAME_COUNT;
        browserIndex = deviceState.paletteId;
        browserScroll = max(0, browserIndex - 1);
        switchScreen(SCREEN_PALETTE_BROWSER);
        return;
    }
    if (g == GESTURE_SWIPE_RIGHT) {
        Serial.println("[UI] Swipe right — color picker");
        // Initialize picker from current device color
        pickerHue = 0;
        pickerSat = 255;
        oldIndicatorAngle = -1;
        switchScreen(SCREEN_COLOR_PICKER);
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

    // Periodic state refresh
    if (millis() - lastStateRefresh > STATE_REFRESH_MS) {
        deviceState = wled_get_state(activeDevice->ip);
        lastStateRefresh = millis();
        if (pendingBrightness < 0) {
            needsRedraw = true;
        }
    }
}

// ===== GENERIC BROWSER (shared by effect + palette) =====

static void drawBrowserList(const char* title, const char** names, int nameCount, int activeId) {
    TFT_eSPI& tft = display_get_tft();
    tft.fillScreen(COLOR_BG);

    // Title
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_ACCENT);
    tft.drawString(title, LIST_X, 25, 4);

    // List items
    for (int i = 0; i < BROWSER_VISIBLE && (i + browserScroll) < browserCount; i++) {
        int idx = i + browserScroll;
        int y = BROWSER_START_Y + (i * BROWSER_ITEM_H);

        // Get name — use lookup if available, else show ID
        const char* name;
        char fallback[16];
        if (idx < nameCount) {
            name = names[idx];
        } else {
            snprintf(fallback, sizeof(fallback), "#%d", idx);
            name = fallback;
        }

        if (idx == browserIndex) {
            // Selected item — highlighted
            tft.setTextColor(COLOR_ACCENT);
            tft.setTextDatum(ML_DATUM);
            tft.drawString(">", 25, y + 12, 2);
            tft.setTextDatum(MC_DATUM);
            tft.drawString(name, LIST_X, y + 12, 4);
        } else if (idx == activeId) {
            // Currently active on device — green
            tft.setTextColor(COLOR_SUCCESS);
            tft.setTextDatum(MC_DATUM);
            tft.drawString(name, LIST_X, y + 12, 2);
        } else {
            tft.setTextColor(COLOR_TEXT);
            tft.setTextDatum(MC_DATUM);
            tft.drawString(name, LIST_X, y + 12, 2);
        }
    }

    // Position indicator
    tft.setTextColor(COLOR_DIM);
    tft.setTextDatum(MC_DATUM);
    String pos = String(browserIndex + 1) + "/" + String(browserCount);
    tft.drawString(pos, LIST_X, 220, 2);
}

// Returns true if browser should close (item selected or cancelled)
static bool handleBrowserInput(int &selectedId) {
    int delta = input_encoder_delta();
    if (delta != 0) {
        browserEncAccum += delta;

        int move = 0;
        if (browserEncAccum >= ENC_LIST_THRESHOLD) {
            move = 1;
            browserEncAccum = 0;
        } else if (browserEncAccum <= -ENC_LIST_THRESHOLD) {
            move = -1;
            browserEncAccum = 0;
        }

        if (move != 0) {
            browserIndex += move;
            if (browserIndex < 0) browserIndex = 0;
            if (browserIndex >= browserCount) browserIndex = browserCount - 1;

            if (browserIndex < browserScroll) {
                browserScroll = browserIndex;
            }
            if (browserIndex >= browserScroll + BROWSER_VISIBLE) {
                browserScroll = browserIndex - BROWSER_VISIBLE + 1;
            }
            needsRedraw = true;
        }
    }

    // Encoder press = select and apply
    if (input_button_pressed()) {
        selectedId = browserIndex;
        return true;
    }

    // Swipe down = cancel
    Gesture g = input_gesture();
    if (g == GESTURE_SWIPE_DOWN) {
        selectedId = -1;  // cancelled
        return true;
    }

    return false;
}

// ===== EFFECT BROWSER SCREEN =====

static void drawEffectBrowser() {
    drawBrowserList("Effects", effectNames, EFFECT_NAME_COUNT, deviceState.effectId);
}

static void handleEffectBrowserInput() {
    int selectedId = -1;
    if (handleBrowserInput(selectedId)) {
        if (selectedId >= 0 && activeDevice) {
            Serial.printf("[UI] Apply effect %d\n", selectedId);
            wled_set_effect(activeDevice->ip, selectedId);
            deviceState.effectId = selectedId;
        } else {
            Serial.println("[UI] Effect browser cancelled");
        }
        switchScreen(SCREEN_DEVICE_CONTROL);
        // Refresh state after returning
        deviceState = wled_get_state(activeDevice->ip);
        pendingBrightness = deviceState.brightness;
        lastStateRefresh = millis();
    }
}

// ===== PALETTE BROWSER SCREEN =====

static void drawPaletteBrowser() {
    drawBrowserList("Palettes", paletteNames, PALETTE_NAME_COUNT, deviceState.paletteId);
}

static void handlePaletteBrowserInput() {
    int selectedId = -1;
    if (handleBrowserInput(selectedId)) {
        if (selectedId >= 0 && activeDevice) {
            Serial.printf("[UI] Apply palette %d\n", selectedId);
            wled_set_palette(activeDevice->ip, selectedId);
            deviceState.paletteId = selectedId;
        } else {
            Serial.println("[UI] Palette browser cancelled");
        }
        switchScreen(SCREEN_DEVICE_CONTROL);
        deviceState = wled_get_state(activeDevice->ip);
        pendingBrightness = deviceState.brightness;
        lastStateRefresh = millis();
    }
}

// ===== COLOR PICKER SCREEN =====

static void drawHueRing() {
    TFT_eSPI& tft = display_get_tft();

    // Draw the hue ring pixel by pixel
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            int dx = x - LIST_X;
            int dy = y - LIST_X;  // center is 120,120
            float dist = sqrt(dx * dx + dy * dy);

            if (dist >= HUE_RING_INNER && dist <= HUE_RING_OUTER) {
                float angle = atan2(dy, dx) * RAD_TO_DEG;
                if (angle < 0) angle += 360;
                tft.drawPixel(x, y, hueToColor565(angle));
            }
        }
    }
}

static void drawIndicatorDot(float hue, uint16_t color) {
    TFT_eSPI& tft = display_get_tft();
    float rad = hue * DEG_TO_RAD;
    float midR = (HUE_RING_INNER + HUE_RING_OUTER) / 2.0f;
    int ix = LIST_X + cos(rad) * midR;
    int iy = LIST_X + sin(rad) * midR;
    tft.fillCircle(ix, iy, 5, color);
}

static void drawColorPicker() {
    TFT_eSPI& tft = display_get_tft();
    tft.fillScreen(COLOR_BG);

    // Title
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_ACCENT);
    tft.drawString("Color", LIST_X, 15, 2);

    // Hue ring
    drawHueRing();

    // Center preview
    uint8_t r, g, b;
    hsvToRgb(pickerHue, pickerSat, 255, r, g, b);
    uint16_t previewColor = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    tft.fillCircle(LIST_X, LIST_X, PREVIEW_RADIUS, previewColor);

    // Saturation label
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_DIM);
    tft.drawString("SAT: " + String(pickerSat), LIST_X, 215, 2);

    // Indicator dot
    drawIndicatorDot(pickerHue, COLOR_TEXT);
    oldIndicatorAngle = pickerHue;
}

static void handleColorPickerInput() {
    TFT_eSPI& tft = display_get_tft();
    bool previewChanged = false;

    // Encoder = adjust saturation
    int delta = input_encoder_delta();
    if (delta != 0) {
        int newSat = (int)pickerSat + delta * 8;
        if (newSat < 0) newSat = 0;
        if (newSat > 255) newSat = 255;
        pickerSat = (uint8_t)newSat;
        previewChanged = true;

        // Update saturation label
        tft.fillRect(60, 207, 120, 20, COLOR_BG);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_DIM);
        tft.drawString("SAT: " + String(pickerSat), LIST_X, 215, 2);
    }

    // Touch = select hue from ring position
    if (input_touch_active()) {
        int tx = input_touch_x();
        int ty = input_touch_y();
        int dx = tx - LIST_X;
        int dy = ty - LIST_X;
        float dist = sqrt(dx * dx + dy * dy);

        if (dist >= HUE_RING_INNER - 10 && dist <= HUE_RING_OUTER + 10) {
            float angle = atan2(dy, dx) * RAD_TO_DEG;
            if (angle < 0) angle += 360;

            // Only update if moved enough (>3 degrees)
            float diff = fabs(angle - pickerHue);
            if (diff > 3 && diff < 357) {
                // Erase old indicator by redrawing ring segment
                if (oldIndicatorAngle >= 0) {
                    drawIndicatorDot(oldIndicatorAngle, hueToColor565(oldIndicatorAngle));
                }

                pickerHue = angle;
                previewChanged = true;

                // Draw new indicator
                drawIndicatorDot(pickerHue, COLOR_TEXT);
                oldIndicatorAngle = pickerHue;
            }
        }
    }

    // Update center preview if anything changed
    if (previewChanged) {
        uint8_t r, g, b;
        hsvToRgb(pickerHue, pickerSat, 255, r, g, b);
        uint16_t previewColor = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        tft.fillCircle(LIST_X, LIST_X, PREVIEW_RADIUS, previewColor);
    }

    // Encoder press = confirm and apply color
    if (input_button_pressed()) {
        uint8_t r, g, b;
        hsvToRgb(pickerHue, pickerSat, 255, r, g, b);
        Serial.printf("[UI] Apply color R%d G%d B%d\n", r, g, b);
        if (activeDevice) {
            wled_set_color(activeDevice->ip, r, g, b);
        }
        switchScreen(SCREEN_DEVICE_CONTROL);
        deviceState = wled_get_state(activeDevice->ip);
        pendingBrightness = deviceState.brightness;
        lastStateRefresh = millis();
        return;
    }

    // Swipe down = cancel
    Gesture g = input_gesture();
    if (g == GESTURE_SWIPE_DOWN) {
        Serial.println("[UI] Color picker cancelled");
        switchScreen(SCREEN_DEVICE_CONTROL);
        deviceState = wled_get_state(activeDevice->ip);
        pendingBrightness = deviceState.brightness;
        lastStateRefresh = millis();
        return;
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