#include <Arduino.h>
#include <WiFi.h>  // For WiFi functionality
// #include <PioDotEnv.h>  // For reading .env constants

#ifndef NTP_SERVER
#error NTP_SERVER is not defined from .env
#endif
#ifndef GMT_OFFSET_SEC
#error GMT_OFFSET_SEC is not defined from .env
#ifndef DAYLIGHT_OFFSET_SEC
#error DAYLIGHT_OFFSET_SEC is not defined from .env
#endif

bool printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return false;
  }
  Serial.println("\nTime synced!");
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  return true
}

bool local_time_setup()
{
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    Serial.print("Syncing NTP time");
    struct tm timeinfo;
    while (!getLocalTime(&timeinfo)) {
        Serial.print(".");
        delay(500);
    }
    return printLocalTime();
}

