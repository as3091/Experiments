#include <Arduino.h>

const int ledPin = LED_BUILTIN;  // GPIO2 for LED
unsigned long previousMillis = 0;  // Tracks last toggle time
bool ledState = digitalRead(ledPin); // LED state tracker

const int ButtonPin = 13;      // Change to GPIO13 for ON button

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ButtonPin, INPUT_PULLUP);   // Internal pull-up
  Serial.println("Starting button test...");
}

void button_check_led_state_blink(int ledPin, unsigned long &previousMillis) {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= 1000)  {  // Check every 1 second
      previousMillis = currentMillis;
      Serial.print("Current button state: ");
      Serial.println(digitalRead(ButtonPin) == LOW ? "PRESSED" : "RELEASED");
      if (digitalRead(ButtonPin) == LOW)
      {
        bool currentState = digitalRead(ledPin);   // read actual pin state from chip
        digitalWrite(ledPin, !currentState);        // toggle it
        Serial.println(!currentState ? "LED ON" : "LED OFF");
      }
  }
}

void loop() {
  button_check_led_state_blink(LED_BUILTIN, previousMillis);
  // led_blink(LED_BUILTIN, ledState, previousMillis);
  // Update previousMillis and ledState after the function call
  // previousMillis = millis(); // Update to current time for next check
  // ledState = !ledState; // Toggle state for next call}
}
