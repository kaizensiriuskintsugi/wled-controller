#ifndef WLED_API_H
#define WLED_API_H

#include <Arduino.h>

// Device state returned from queries
struct WledState {
    bool on;
    uint8_t brightness;
    uint8_t effectId;
    uint8_t paletteId;
    uint8_t colorR;
    uint8_t colorG;
    uint8_t colorB;
    bool valid;  // false if query failed
};

// Power
bool wled_set_power(const char* ip, bool on);

// Brightness (0-255)
bool wled_set_brightness(const char* ip, uint8_t value);

// Effect by ID
bool wled_set_effect(const char* ip, uint8_t effectId);

// Palette by ID
bool wled_set_palette(const char* ip, uint8_t paletteId);

// Primary color
bool wled_set_color(const char* ip, uint8_t r, uint8_t g, uint8_t b);

// Read current state
WledState wled_get_state(const char* ip);

// Get counts (for browsing effects/palettes)
int wled_get_effects_count(const char* ip);
int wled_get_palettes_count(const char* ip);

#endif