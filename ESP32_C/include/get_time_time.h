// src/wifi.h
#ifndef GET_THE_TIME_H
#define GET_THE_TIME_H

#include <WiFi.h>  // Or other needed libs
#include "time.h"

extern const char* NTP_SERVER;
extern const long GMT_OFFSET_SEC;
extern const int DAYLIGHT_OFFSET_SEC;

// bool wifi_setup();    // Function prototype
// void wifi_scan();     // Another example
bool local_time_setup();    // Function prototype
bool printLocalTime():
#endif