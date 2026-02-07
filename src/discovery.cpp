#include "discovery.h"
#include "wifi_manager.h"

static const int MAX_DEVICES = 10;
static WledDevice devices[MAX_DEVICES];
static int deviceCount = 0;

#ifdef MOCK_MODE

// ===== MOCK MODE — Fake devices =====

void discovery_init() {
    Serial.println("[DISCOVERY-MOCK] Mock discovery ready");
}

void discovery_scan() {
    Serial.println("[DISCOVERY-MOCK] Populating fake devices...");
    deviceCount = 0;

    // 5 art-installation-themed fake devices
    const char* names[] = {
        "Lighthouse-Main",
        "Beacon-North",
        "Lantern-Garden",
        "Torch-Entry",
        "Glow-Studio"
    };
    const char* ips[] = {
        "192.168.1.100",
        "192.168.1.101",
        "192.168.1.102",
        "192.168.1.103",
        "192.168.1.104"
    };

    for (int i = 0; i < 5; i++) {
        strncpy(devices[deviceCount].name, names[i], 31);
        devices[deviceCount].name[31] = '\0';
        strncpy(devices[deviceCount].ip, ips[i], 15);
        devices[deviceCount].ip[15] = '\0';
        Serial.printf("  [MOCK] %s (%s)\n", devices[deviceCount].name, devices[deviceCount].ip);
        deviceCount++;
    }

    Serial.printf("[DISCOVERY-MOCK] %d mock devices ready\n", deviceCount);
}

#else

// ===== REAL MODE — mDNS discovery =====
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

static bool isWledDevice(const char* ip) {
    HTTPClient http;
    String url = String("http://") + ip + "/json/info";
    http.begin(url);
    http.setTimeout(2000);
    int code = http.GET();
    bool isWled = false;

    if (code == 200) {
        String body = http.getString();
        DynamicJsonDocument doc(4096);
        if (deserializeJson(doc, body) == DeserializationError::Ok) {
            if (doc.containsKey("ver")) {
                isWled = true;
            }
        }
    }
    http.end();
    return isWled;
}

void discovery_init() {
    if (!MDNS.begin("wled-controller")) {
        Serial.println("mDNS init failed");
    }
}

void discovery_scan() {
    if (!wifi_is_connected()) {
        Serial.println("Discovery: no WiFi");
        return;
    }

    deviceCount = 0;
    Serial.println("Scanning for WLED devices...");

    int found = MDNS.queryService("http", "tcp");
    Serial.printf("Found %d HTTP services\n", found);

    for (int i = 0; i < found && deviceCount < MAX_DEVICES; i++) {
        String ip = MDNS.IP(i).toString();
        String hostname = MDNS.hostname(i);

        Serial.printf("  Checking: %s (%s) ... ", hostname.c_str(), ip.c_str());

        char ipBuf[16];
        ip.toCharArray(ipBuf, 16);

        if (isWledDevice(ipBuf)) {
            ip.toCharArray(devices[deviceCount].ip, 16);
            hostname.toCharArray(devices[deviceCount].name, 32);
            Serial.println("WLED!");
            deviceCount++;
        } else {
            Serial.println("not WLED");
        }
    }

    Serial.printf("WLED devices found: %d\n", deviceCount);
}

#endif

// ===== SHARED (both modes use these) =====

int discovery_get_count() {
    return deviceCount;
}

WledDevice* discovery_get_device(int index) {
    if (index >= 0 && index < deviceCount) {
        return &devices[index];
    }
    return nullptr;
}