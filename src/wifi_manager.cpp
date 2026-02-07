#include <Arduino.h>
#include "wifi_manager.h"

#ifdef MOCK_MODE

// ===== MOCK MODE — No real WiFi =====
static char ipAddress[16] = "192.168.1.99";

void wifi_init() {
    Serial.println("[WIFI-MOCK] Mock WiFi connected");
    Serial.printf("[WIFI-MOCK] Fake IP: %s\n", ipAddress);
}

void wifi_update() {
    // Nothing to do in mock mode
}

bool wifi_is_connected() {
    return true;  // Always "connected"
}

const char* wifi_get_ip() {
    return ipAddress;
}

#else

// ===== REAL MODE — Actual WiFi =====
#include <WiFi.h>

const char* WIFI_SSID = "TELUS1201";
const char* WIFI_PASS = "bbCk4fsYVJXA";

static unsigned long lastReconnectAttempt = 0;
static const unsigned long RECONNECT_INTERVAL = 5000;
static char ipAddress[16] = "0.0.0.0";

void wifi_init() {
    Serial.print("Connecting to WiFi: ");
    Serial.println(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    int timeout = 20;
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
        delay(500);
        Serial.print(".");
        timeout--;
    }

    if (WiFi.status() == WL_CONNECTED) {
        WiFi.localIP().toString().toCharArray(ipAddress, 16);
        Serial.println();
        Serial.print("Connected! IP: ");
        Serial.println(ipAddress);
    } else {
        Serial.println();
        Serial.println("WiFi connection FAILED");
    }
}

void wifi_update() {
    if (WiFi.status() != WL_CONNECTED) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > RECONNECT_INTERVAL) {
            lastReconnectAttempt = now;
            Serial.println("WiFi lost — reconnecting...");
            WiFi.begin(WIFI_SSID, WIFI_PASS);
        }
    }
}

bool wifi_is_connected() {
    return WiFi.status() == WL_CONNECTED;
}

const char* wifi_get_ip() {
    return ipAddress;
}

#endif