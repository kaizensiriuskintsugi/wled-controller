#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

void wifi_init();
void wifi_update();
bool wifi_is_connected();
const char* wifi_get_ip();

#endif