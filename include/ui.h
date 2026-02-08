#ifndef UI_H
#define UI_H

#include <Arduino.h>

// Screen states
enum Screen {
    SCREEN_BOOT,
    SCREEN_DEVICE_LIST,
    SCREEN_DEVICE_CONTROL,
    SCREEN_EFFECT_BROWSER,
    SCREEN_PALETTE_BROWSER,
    SCREEN_COLOR_PICKER
};

// Initialize UI (call after all other modules init)
// Checks NVS for last device and auto-connects if found
void ui_init();

// Call every loop — handles input routing and screen updates
void ui_update();

// Get current screen
Screen ui_get_screen();

#endif