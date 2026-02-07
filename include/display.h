#ifndef DISPLAY_H
#define DISPLAY_H

#include <TFT_eSPI.h>

// Screen constants
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 240
#define CENTER_X      120
#define CENTER_Y      120

// Colors
#define COLOR_BG      0x0000  // Black
#define COLOR_TEXT    0xFFFF  // White
#define COLOR_ACCENT  0x07FF  // Cyan
#define COLOR_SUCCESS 0x07E0  // Green
#define COLOR_ERROR   0xF800  // Red
#define COLOR_WARN    0xFFE0  // Yellow
#define COLOR_DIM     0x4208  // Dark gray

// Initialize display hardware
void display_init();

// Clear screen to background color
void display_clear();

// Show centered status text (single line)
void display_status(const char* text);

// Show boot splash screen
void display_splash();

// Show boot progress info (WiFi status, device count, etc.)
void display_boot_status(const char* line1, const char* line2 = nullptr);

// Get the TFT object for advanced use (UI module will need this later)
TFT_eSPI& display_get_tft();

#endif