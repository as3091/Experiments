#include <Arduino.h>

const int ledPin = LED_BUILTIN;  // GPIO2 for LED
const int onButtonPin = 13;      // Change to GPIO13 for ON button
const int offButtonPin = 26;     // Change to GPIO26 for OFF button

unsigned long lastDebugPrint = 0;  // For timed debug

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  pinMode(onButtonPin, INPUT_PULLUP);   // Internal pull-up
  pinMode(offButtonPin, INPUT_PULLUP);  // Internal pull-up
  digitalWrite(ledPin, LOW);            // Start OFF
  Serial.println("Button control ready. Debug mode ON.");
}

void loop() {
  // Button checks
  if (digitalRead(onButtonPin) == LOW) {
    digitalWrite(ledPin, HIGH);
    Serial.println("LED turned ON via button");
    delay(200);  // Debounce
  }

  if (digitalRead(offButtonPin) == LOW) {
    digitalWrite(ledPin, LOW);
    Serial.println("LED turned OFF via button");
    delay(200);  // Debounce
  }

  // Debug: Print raw pin states every 500ms
  unsigned long currentMillis = millis();
  if (currentMillis - lastDebugPrint >= 500) {
    lastDebugPrint = currentMillis;
    Serial.print("ON Button (GPIO");
    Serial.print(onButtonPin);
    Serial.print("): ");
    Serial.println(digitalRead(onButtonPin) == HIGH ? "HIGH (unpressed)" : "LOW (pressed)");
    Serial.print("OFF Button (GPIO");
    Serial.print(offButtonPin);
    Serial.print("): ");
    Serial.println(digitalRead(offButtonPin) == HIGH ? "HIGH (unpressed)" : "LOW (pressed)");
  }
}
