#include <Arduino.h>

const int relayPin = 26;  // GPIO 26

void setup() {
  Serial.begin(115200);
  pinMode(relayPin, OUTPUT);
  
  // Start with relay OFF (HIGH for most active-low relay modules)
  digitalWrite(relayPin, HIGH);
  Serial.println("Relay initialized - OFF");
}

void loop() {
  // Turn relay ON for 5 seconds
  digitalWrite(relayPin, LOW);   // Activates most 5V relay modules
  Serial.println("Relay ON");
  delay(5000);
  
  // Turn relay OFF
  digitalWrite(relayPin, HIGH);
  Serial.println("Relay OFF");
  delay(5000);
}