#include <Arduino.h>

const int ledPin = LED_BUILTIN;  // GPIO2 for LED
unsigned long previousMillis = 0;  // Tracks last toggle time
// bool ledState = digitalRead(ledPin); // LED state tracker

const int ButtonPin = 27;      // Change to GPIO13 for button

unsigned long startMillis = 0;          // set in setup()
const unsigned long SLEEP_AFTER = 60000; // 1 minute in ms

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ButtonPin, INPUT_PULLUP);   // Internal pull-up
  Serial.println("Starting button test...");
  startMillis = millis(); // Initialize start time
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
  if (millis() - startMillis >= SLEEP_AFTER) {
    Serial.println("1 minute elapsed — entering deep sleep.");
    Serial.flush();
    digitalWrite(LED_BUILTIN, LOW);      // turn LED off before sleep
    esp_deep_sleep_start();              // wake requires reset or touch/ext wakeup
  }

  button_check_led_state_blink(LED_BUILTIN, previousMillis);
}
