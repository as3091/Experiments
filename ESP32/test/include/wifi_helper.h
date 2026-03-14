// src/wifi.h
#ifndef WIFI_HELPER_H
#define WIFI_HELPER_H

#include <WiFi.h>  // Or other needed libs

extern String wifiSSID;  // If using globals; prefer functions instead
extern String wifiPass;

bool wifi_setup();    // Function prototype
void wifi_scan();     // Another example

#endif