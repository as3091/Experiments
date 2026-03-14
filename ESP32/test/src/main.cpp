#include <Arduino.h>

const int ledPin = LED_BUILTIN;  // GPIO2 for LED
unsigned long previousMillis = 0;  // Tracks last toggle time
unsigned long lastInputMillis = 0;
// int lastButtonState = HIGH;   // INPUT_PULLUP idle state

const int ButtonPin = 27;      // Change to GPIO13 for button
const unsigned long SLEEP_AFTER = 60000; // 1 minute in ms

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ButtonPin, INPUT_PULLUP);   // Internal pull-up
  Serial.println("Starting button test...");
  lastInputMillis = millis(); // Initialize start time
}

void button_check_led_state_blink(int ledPin, unsigned long &previousMillis)
{
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= 500)
    {  // Check every 0.5 second
      previousMillis = currentMillis;
      int buttonState = digitalRead(ButtonPin);

      Serial.print("Current button state: ");
      Serial.println(buttonState == LOW ? "PRESSED" : "RELEASED");
      if (buttonState == LOW) {
        lastInputMillis = currentMillis;  // reset inactivity timer
        {
          bool currentState = digitalRead(ledPin);   // read actual pin state from chip
          digitalWrite(ledPin, !currentState);        // toggle it
          Serial.println(!currentState ? "LED ON" : "LED OFF");
        }
    }
  }
}

void loop() {
  button_check_led_state_blink(LED_BUILTIN, previousMillis);
  if (millis() - lastInputMillis >= SLEEP_AFTER) {
    Serial.println("1 minute elapsed — entering deep sleep.");
    Serial.flush();
    digitalWrite(LED_BUILTIN, LOW);      // turn LED off before sleep
    esp_deep_sleep_start();              // wake requires reset or touch/ext wakeup
  }

  
}
