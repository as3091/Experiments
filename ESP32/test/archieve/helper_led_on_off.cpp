#include <Arduino.h>

// unsigned long previousMillis = 0;  // Tracks last toggle time
// bool ledState = false;              // LED state tracker

void led_blink(int ledPin, bool ledState,unsigned long previousMillis) {
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