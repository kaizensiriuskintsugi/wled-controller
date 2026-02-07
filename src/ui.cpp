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

// Display layout
#define VISIBLE_ITEMS 4
#define LIST_START_Y 60
#define LIST_ITEM_HEIGHT 35
#define LIST_X 120

// Arc settings
#define ARC_CENTER_Y 115
#define ARC_RADIUS 95
#define ARC_THICKNESS 3  // draws radius-3 to radius+3 = 7px wide

// ===== HELPERS =====

static void switchScreen(Screen next) {
    // Force full clear on every screen transition
    display_clear();
    currentScreen = next;
    needsRedraw = true;
    encAccum = 0;
    Serial.printf("[UI] Screen → %d\n", next);
}

static void drawArc(int brightness) {
    TFT_eSPI& tft = display_get_tft();
    int arcAngle = map(brightness, 0, 255, 0, 240);
    int arcStart = 150;

    for (int a = 0; a < 240; a++) {
        float rad = (arcStart + a) * DEG_TO_RAD;
        uint16_t color = (a < arcAngle) ? COLOR_ACCENT : COLOR_DIM;

        // Draw multiple radii for thickness
        for (int r = -ARC_THICKNESS; r <= ARC_THICKNESS; r++) {
            int x = LIST_X + cos(rad) * (ARC_RADIUS + r);
            int y = ARC_CENTER_Y + sin(rad) * (ARC_RADIUS + r);
            tft.drawPixel(x, y, color);
        }
    }
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

    // Effect and palette
    tft.setTextColor(COLOR_DIM);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("FX: " + String(deviceState.effectId), LIST_X, 185, 2);
    tft.drawString("PAL: " + String(deviceState.paletteId), LIST_X, 200, 2);
}
static void drawBrightnessOnly() {
    TFT_eSPI& tft = display_get_tft();

    int bri = (pendingBrightness >= 0) ? pendingBrightness : deviceState.brightness;

    // Clear brightness number area
    tft.fillRect(20, 70, 200, 60, COLOR_BG);
    tft.setTextColor(COLOR_TEXT);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(String(bri), LIST_X, 100, 7);

    // Redraw arc fully (overwrites old pixels)
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

    // Encoder short press = effect browser (Phase 7C)
    if (input_button_pressed()) {
        Serial.println("[UI] Button — effect browser (coming in 7C)");
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
        needsRedraw = true;  // refresh after flash
    }

    // Touch gestures
    Gesture g = input_gesture();
    if (g == GESTURE_SWIPE_DOWN) {
        Serial.println("[UI] Swipe down — back to list");
        pendingBrightness = -1;
        switchScreen(SCREEN_DEVICE_LIST);
        return;  // exit immediately, don't process more this frame
    }
    if (g == GESTURE_SWIPE_LEFT) {
        Serial.println("[UI] Swipe left — palette browser (coming in 7D)");
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
            default:
                break;
        }
        needsRedraw = false;
    }
}

Screen ui_get_screen() {
    return currentScreen;
}