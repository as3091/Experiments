#include <Arduino.h>
#include <ArduinoOTA.h>

#include "secrets.h"
#include "wifi_helper.h"


void setup() {
    Serial.begin(115200);
    delay(100);

    // pinMode(SOLENOID_PIN, OUTPUT);
    // digitalWrite(SOLENOID_PIN, HIGH);  // solenoid OFF on boot

    wifi_setup(WIFI_SSID, WIFI_PASS);

  // Set a hostname so you don't have to hunt for the IP address
  ArduinoOTA.setHostname(ArduinoOTA_Hostname); 
  ArduinoOTA.setPassword(ArduinoOTA_Password);

  ArduinoOTA.begin();
}

void loop() {
  ArduinoOTA.handle(); // This is critical!
}