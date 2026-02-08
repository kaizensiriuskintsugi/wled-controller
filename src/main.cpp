/*
 * WLED Touchscreen Controller
 * Phase 8A: Refinements
 */

#include <Arduino.h>
#include "display.h"
#include "wifi_manager.h"
#include "discovery.h"
#include "wled_api.h"
#include "input.h"
#include "ui.h"

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n=== WLED Controller Boot ===");

    #ifdef MOCK_MODE
    Serial.println("*** MOCK MODE ACTIVE ***");
    #endif

    // Display
    display_init();
    display_splash();
    delay(1500);

    // WiFi
    display_boot_status("Connecting WiFi...");
    wifi_init();

    if (wifi_is_connected()) {
        String ip = "IP: " + String(wifi_get_ip());
        display_boot_status("WiFi Connected", ip.c_str());
        delay(1000);

        // Discovery
        discovery_init();
        display_boot_status("Scanning for WLED...");
        discovery_scan();
        int count = discovery_get_count();
        String found = String(count) + " devices found";
        display_boot_status("Scan Complete", found.c_str());
        delay(1000);
    } else {
        display_boot_status("WiFi Failed", "Check credentials");
        delay(3000);
        // Don't halt — allow UI to show device list (empty) with rescan option
    }

    // Input
    input_init();

    // UI — will auto-connect to last device if found
    display_boot_status("Loading UI...");
    ui_init();
}

void loop() {
    wifi_update();
    input_update();
    ui_update();
}
