#include <Arduino.h>
#include <WiFi.h>  // For WiFi functionality
#include "wifi_helper.h"  // For WiFi setup and scanning
#include "button_check_led_state_blink_helper.h"  // For button and LED handling

#ifndef LED_PIN
#error LED_PIN is not defined from .env
#endif

// const int ledPin = LED_BUILTIN;  // GPIO2 for LED
unsigned long previousMillis = 0;  // Tracks last toggle time
unsigned long lastInputMillis = 0;
// int lastButtonState = HIGH;   // INPUT_PULLUP idle state

// const int ButtonPin = 27;      // Change to GPIO13 for button
const unsigned long SLEEP_AFTER = 60000; // 1 minute in ms


void setup() {
    Serial.begin(115200);
    initPins();
    Serial.println("Starting WIFI button test...");
    lastInputMillis = millis(); // Initialize start time
    wifi_scan();
    wifi_setup();
    initPins();
    digitalWrite(LED_PIN, LOW);      // turn LED off before starting loop, to avoid it being left on after reset
}

void loop() {
    handleButtonLED(lastInputMillis, previousMillis);
    if (millis() - lastInputMillis >= SLEEP_AFTER) {
        Serial.println("1 minute elapsed — entering deep sleep.");
        Serial.flush();
        digitalWrite(LED_PIN, LOW);      // turn LED off before sleep
        esp_deep_sleep_start();              // wake requires reset or touch/ext wakeup
    }
}
