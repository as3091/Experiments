#include <Arduino.h>

const int ledPin = LED_BUILTIN;  // GPIO2 for LED
unsigned long previousMillis = 0;  // Tracks last toggle time
bool ledState = false;              // LED state tracker

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.println("Starting millis blink test...");
}

void led_blink(int ledPin, bool &ledState, unsigned long &previousMillis) {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= 1000) {  // Check every 1 second
    previousMillis = currentMillis;
    ledState = !ledState;  // Toggle state
    if (ledState) {
      digitalWrite(ledPin, HIGH);  // ON (active-high for external LED)
      Serial.println("LED ON");
    } else {
      digitalWrite(ledPin, LOW);   // OFF
      Serial.println("LED OFF");
    }
  }
}

void loop() {
  led_blink(LED_BUILTIN, ledState, previousMillis);
  // Update previousMillis and ledState after the function call
  // previousMillis = millis(); // Update to current time for next check
  // ledState = !ledState; // Toggle state for next call}
}

