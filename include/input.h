#ifndef INPUT_H
#define INPUT_H

#include <Arduino.h>

// Gesture types from CST816S
enum Gesture {
    GESTURE_NONE = 0,
    GESTURE_TAP,
    GESTURE_SWIPE_UP,
    GESTURE_SWIPE_DOWN,
    GESTURE_SWIPE_LEFT,
    GESTURE_SWIPE_RIGHT,
    GESTURE_LONG_PRESS
};

// Initialize encoder and touch hardware
void input_init();

// Call every loop — processes encoder and polls touch
void input_update();

// Encoder: returns clicks since last call, then resets to 0
int input_encoder_delta();

// Encoder button: true if short-pressed since last call, then resets
bool input_button_pressed();

// Encoder button: true if held >500ms since last call, then resets
bool input_button_long_pressed();

// Touch: true if screen is being touched right now
bool input_touch_active();

// Touch: X and Y position (0-239)
int input_touch_x();
int input_touch_y();

// Touch: gesture detected since last call, then resets
Gesture input_gesture();

#endif