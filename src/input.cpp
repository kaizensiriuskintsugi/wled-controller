#include "input.h"
#include <Wire.h>

// ===== ENCODER PINS =====
#define ENC_CLK 15
#define ENC_DT  16
#define ENC_SW  17

// ===== TOUCH PINS =====
#define TOUCH_SDA 6
#define TOUCH_SCL 7
#define TOUCH_INT 5
#define TOUCH_RST 13
#define CST816S_ADDR 0x15

// ===== ENCODER STATE =====
static volatile int encDelta = 0;
static uint8_t lastEncState = 0;

// Button state — tracks both short press and long press
static bool btnHeld = false;
static unsigned long btnDownTime = 0;
static bool shortPressReady = false;
static bool shortPressConsumed = true;
static bool longPressFired = false;
static bool longPressConsumed = true;
#define BTN_DEBOUNCE_MS 50
#define BTN_LONG_PRESS_MS 500

// ===== TOUCH STATE =====
static bool touchActive = false;
static bool wasTouching = false;
static int touchX = 0;
static int touchY = 0;
static Gesture lastGesture = GESTURE_NONE;
static bool gestureConsumed = true;
static bool gestureLockedOut = false;

// ===== ENCODER ISR =====
void IRAM_ATTR encoderISR() {
    uint8_t clk = digitalRead(ENC_CLK);
    uint8_t dt = digitalRead(ENC_DT);
    uint8_t state = (clk << 1) | dt;

    uint8_t combined = (lastEncState << 2) | state;
    switch (combined) {
        case 0b0001: case 0b0111: case 0b1110: case 0b1000:
            encDelta--;
            break;
        case 0b0010: case 0b1011: case 0b1101: case 0b0100:
            encDelta++;
            break;
    }
    lastEncState = state;
}

// ===== TOUCH I2C READ =====
static bool touchRead(uint8_t reg, uint8_t* buf, uint8_t len) {
    Wire.beginTransmission(CST816S_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission() != 0) return false;
    Wire.requestFrom((uint8_t)CST816S_ADDR, len);
    for (uint8_t i = 0; i < len && Wire.available(); i++) {
        buf[i] = Wire.read();
    }
    return true;
}

// ===== PUBLIC FUNCTIONS =====

void input_init() {
    pinMode(ENC_CLK, INPUT_PULLUP);
    pinMode(ENC_DT, INPUT_PULLUP);
    pinMode(ENC_SW, INPUT_PULLUP);

    lastEncState = (digitalRead(ENC_CLK) << 1) | digitalRead(ENC_DT);
    attachInterrupt(digitalPinToInterrupt(ENC_CLK), encoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENC_DT), encoderISR, CHANGE);

    Serial.println("[INPUT] Encoder initialized");

    // Touch controller reset sequence
    pinMode(TOUCH_RST, OUTPUT);
    digitalWrite(TOUCH_RST, LOW);
    delay(10);
    digitalWrite(TOUCH_RST, HIGH);
    delay(50);

    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    Wire.setClock(400000);

    Wire.beginTransmission(CST816S_ADDR);
    if (Wire.endTransmission() == 0) {
        Serial.println("[INPUT] Touch controller found");
    } else {
        Serial.println("[INPUT] Touch controller NOT found!");
    }

    Serial.println("[INPUT] Initialized");
}

void input_update() {
    // === BUTTON with short press + long press ===
    bool btnState = (digitalRead(ENC_SW) == LOW);

    if (btnState && !btnHeld) {
        // Just pressed down
        btnHeld = true;
        btnDownTime = millis();
        longPressFired = false;
    }

    if (btnState && btnHeld && !longPressFired) {
        // Still held — check for long press
        if (millis() - btnDownTime > BTN_LONG_PRESS_MS) {
            longPressFired = true;
            longPressConsumed = false;
            Serial.println("[INPUT] Long press detected");
        }
    }

    if (!btnState && btnHeld) {
        // Released
        if (!longPressFired && (millis() - btnDownTime > BTN_DEBOUNCE_MS)) {
            // Was a short press (released before long threshold)
            shortPressReady = true;
            shortPressConsumed = false;
        }
        btnHeld = false;
    }

    // === TOUCH ===
    uint8_t buf[6];
    if (touchRead(0x01, buf, 6)) {
        uint8_t points = buf[1];
        touchActive = (points > 0);

        if (touchActive) {
            touchX = ((buf[2] & 0x0F) << 8) | buf[3];
            touchY = ((buf[4] & 0x0F) << 8) | buf[5];

            uint8_t gestRaw = buf[0];
            Gesture g = GESTURE_NONE;
            switch (gestRaw) {
                case 0x01: g = GESTURE_SWIPE_UP;    break;
                case 0x02: g = GESTURE_SWIPE_DOWN;  break;
                case 0x03: g = GESTURE_SWIPE_LEFT;  break;
                case 0x04: g = GESTURE_SWIPE_RIGHT; break;
                case 0x05: g = GESTURE_TAP;         break;
                case 0x0C: g = GESTURE_LONG_PRESS;  break;
            }
            if (g != GESTURE_NONE && !gestureLockedOut) {
                lastGesture = g;
                gestureConsumed = false;
                gestureLockedOut = true;
            }
        } else {
            gestureLockedOut = false;
            wasTouching = false;
        }

        wasTouching = touchActive;
    }
}

int input_encoder_delta() {
    noInterrupts();
    int d = encDelta;
    encDelta = 0;
    interrupts();
    return d;
}

bool input_button_pressed() {
    if (!shortPressConsumed) {
        shortPressConsumed = true;
        return true;
    }
    return false;
}

bool input_button_long_pressed() {
    if (!longPressConsumed) {
        longPressConsumed = true;
        return true;
    }
    return false;
}

bool input_touch_active() {
    return touchActive;
}

int input_touch_x() {
    return touchX;
}

int input_touch_y() {
    return touchY;
}

Gesture input_gesture() {
    if (!gestureConsumed) {
        gestureConsumed = true;
        return lastGesture;
    }
    return GESTURE_NONE;
}