#include "wled_api.h"

#ifdef MOCK_MODE

// ===== MOCK MODE — No real HTTP =====

// Simulated device state (shared across all mock devices)
static uint8_t mockBrightness = 128;
static uint8_t mockEffect = 0;
static uint8_t mockPalette = 0;
static bool mockPower = true;

bool wled_set_power(const char* ip, bool on) {
    mockPower = on;
    Serial.printf("[WLED-MOCK] %s power → %s\n", ip, on ? "ON" : "OFF");
    return true;
}

bool wled_set_brightness(const char* ip, uint8_t value) {
    mockBrightness = value;
    Serial.printf("[WLED-MOCK] %s brightness → %d\n", ip, value);
    return true;
}

bool wled_set_effect(const char* ip, uint8_t effectId) {
    mockEffect = effectId;
    Serial.printf("[WLED-MOCK] %s effect → %d\n", ip, effectId);
    return true;
}

bool wled_set_palette(const char* ip, uint8_t paletteId) {
    mockPalette = paletteId;
    Serial.printf("[WLED-MOCK] %s palette → %d\n", ip, paletteId);
    return true;
}

bool wled_set_color(const char* ip, uint8_t r, uint8_t g, uint8_t b) {
    Serial.printf("[WLED-MOCK] %s color → R%d G%d B%d\n", ip, r, g, b);
    return true;
}

WledState wled_get_state(const char* ip) {
    WledState state;
    state.on = mockPower;
    state.brightness = mockBrightness;
    state.effectId = mockEffect;
    state.paletteId = mockPalette;
    state.colorR = 255;
    state.colorG = 128;
    state.colorB = 0;
    state.valid = true;
    return state;
}

int wled_get_effects_count(const char* ip) {
    return 20;  // Mock: 20 effects
}

int wled_get_palettes_count(const char* ip) {
    return 15;  // Mock: 15 palettes
}

#else

// ===== REAL MODE — HTTP JSON API =====
#include <HTTPClient.h>
#include <ArduinoJson.h>

static bool sendState(const char* ip, const char* json) {
    HTTPClient http;
    String url = String("http://") + ip + "/json/state";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(3000);

    int code = http.POST((uint8_t*)json, strlen(json));
    http.end();

    if (code == 200) {
        return true;
    }

    Serial.printf("[WLED API] POST failed to %s — code: %d\n", ip, code);
    return false;
}

bool wled_set_power(const char* ip, bool on) {
    char json[32];
    snprintf(json, sizeof(json), "{\"on\":%s}", on ? "true" : "false");
    return sendState(ip, json);
}

bool wled_set_brightness(const char* ip, uint8_t value) {
    char json[32];
    snprintf(json, sizeof(json), "{\"bri\":%d}", value);
    return sendState(ip, json);
}

bool wled_set_effect(const char* ip, uint8_t effectId) {
    char json[32];
    snprintf(json, sizeof(json), "{\"seg\":[{\"fx\":%d}]}", effectId);
    return sendState(ip, json);
}

bool wled_set_palette(const char* ip, uint8_t paletteId) {
    char json[32];
    snprintf(json, sizeof(json), "{\"seg\":[{\"pal\":%d}]}", paletteId);
    return sendState(ip, json);
}

bool wled_set_color(const char* ip, uint8_t r, uint8_t g, uint8_t b) {
    char json[48];
    snprintf(json, sizeof(json), "{\"seg\":[{\"col\":[[%d,%d,%d]]}]}", r, g, b);
    return sendState(ip, json);
}

WledState wled_get_state(const char* ip) {
    WledState state = {false, 0, 0, 0, 0, 0, 0, false};

    HTTPClient http;
    String url = String("http://") + ip + "/json/state";
    http.begin(url);
    http.setTimeout(3000);

    int code = http.GET();
    if (code == 200) {
        String body = http.getString();
        DynamicJsonDocument doc(4096);
        if (deserializeJson(doc, body) == DeserializationError::Ok) {
            state.on = doc["on"] | false;
            state.brightness = doc["bri"] | 0;

            JsonArray seg = doc["seg"].as<JsonArray>();
            if (seg.size() > 0) {
                state.effectId = seg[0]["fx"] | 0;
                state.paletteId = seg[0]["pal"] | 0;

                JsonArray col = seg[0]["col"][0];
                if (col.size() >= 3) {
                    state.colorR = col[0];
                    state.colorG = col[1];
                    state.colorB = col[2];
                }
            }
            state.valid = true;
        }
    }

    http.end();

    if (!state.valid) {
        Serial.printf("[WLED API] GET state failed from %s — code: %d\n", ip, code);
    }
    return state;
}

int wled_get_effects_count(const char* ip) {
    HTTPClient http;
    String url = String("http://") + ip + "/json/effects";
    http.begin(url);
    http.setTimeout(3000);
    int count = 0;

    int code = http.GET();
    if (code == 200) {
        String body = http.getString();
        DynamicJsonDocument doc(8192);
        if (deserializeJson(doc, body) == DeserializationError::Ok) {
            count = doc.as<JsonArray>().size();
        }
    }
    http.end();
    return count;
}

int wled_get_palettes_count(const char* ip) {
    HTTPClient http;
    String url = String("http://") + ip + "/json/palettes";
    http.begin(url);
    http.setTimeout(3000);
    int count = 0;

    int code = http.GET();
    if (code == 200) {
        String body = http.getString();
        DynamicJsonDocument doc(4096);
        if (deserializeJson(doc, body) == DeserializationError::Ok) {
            count = doc.as<JsonArray>().size();
        }
    }
    http.end();
    return count;
}

#endif