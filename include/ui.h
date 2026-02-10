#ifndef UI_H
#define UI_H

#include <Arduino.h>

// ===== SCREEN STATES =====
// 16 compass screens + boot + settings + color picker
enum Screen {
    SCREEN_BOOT,
    SCREEN_HOME,
    // Top tier — device/group management
    SCREEN_DEVICE_LIST,
    SCREEN_GROUP_HOTKEYS,
    SCREEN_GROUP_LIST,
    SCREEN_MANAGE_GROUPS,
    // Middle tier — MIDI + future
    SCREEN_MIDI_1,
    SCREEN_MIDI_2,
    SCREEN_MIDI_3,
    SCREEN_MIDI_4,
    SCREEN_SCENES,            // future placeholder
    // Bottom tier — effects/palettes/presets
    SCREEN_FX_DRAWER,
    SCREEN_FX_FAVORITES,
    SCREEN_FX_CATEGORIES,
    SCREEN_FX_LIST,
    SCREEN_PAL_FAVORITES,
    SCREEN_PAL_LIST,
    SCREEN_PRESETS,
    // Off-compass screens
    SCREEN_SETTINGS,
    SCREEN_COLOR_PICKER,
    // Sentinel — must be last
    SCREEN_COUNT
};

// Initialize UI (call after all other modules init)
void ui_init();

// Call every loop — handles input routing and screen updates
void ui_update();

// Get current screen
Screen ui_get_screen();

#endif