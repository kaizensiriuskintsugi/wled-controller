#ifndef DISCOVERY_H
#define DISCOVERY_H

#include <Arduino.h>

struct WledDevice {
    char name[32];
    char ip[16];
};

void discovery_init();
void discovery_scan();
int discovery_get_count();
WledDevice* discovery_get_device(int index);

#endif