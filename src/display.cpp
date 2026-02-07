#include "display.h"

static TFT_eSPI tft = TFT_eSPI();

void display_init() {
    pinMode(2, OUTPUT);
    digitalWrite(2, HIGH);
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(COLOR_BG);
    Serial.println("[DISPLAY] Initialized");
}

void display_clear() {
    tft.fillScreen(COLOR_BG);
}

void display_status(const char* text) {
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(text, CENTER_X, CENTER_Y, 4);
}

void display_splash() {
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(COLOR_ACCENT);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("WLED", CENTER_X, 90, 4);
    tft.drawString("Controller", CENTER_X, 125, 4);
    tft.setTextColor(COLOR_DIM);
    tft.drawString("v1.0", CENTER_X, 165, 2);
}

void display_boot_status(const char* line1, const char* line2) {
    tft.fillRect(0, 180, SCREEN_WIDTH, 60, COLOR_BG);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_TEXT);
    tft.drawString(line1, CENTER_X, 195, 2);
    if (line2) {
        tft.setTextColor(COLOR_DIM);
        tft.drawString(line2, CENTER_X, 215, 2);
    }
}

TFT_eSPI& display_get_tft() {
    return tft;
}