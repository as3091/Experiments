#include <Arduino.h>

const int SENSOR_PIN = 36;  // GPIO 26

void setup() {

  Serial.begin(115200);
  analogSetAttenuation(ADC_11db);  // Important for ESP32 to read up to ~3.3V properly
  // Serial.begin(115200);
  // pinMode(relayPin, OUTPUT);
  
  // // Start with relay OFF (HIGH for most active-low relay modules)
  // digitalWrite(relayPin, HIGH);
  // Serial.println("Relay initialized - OFF");
}

void loop() {
  // // Turn relay ON for 5 seconds
  // digitalWrite(relayPin, LOW);   // Activates most 5V relay modules
  // Serial.println("Relay ON");
  // delay(5000);
  
  // // Turn relay OFF
  // digitalWrite(relayPin, HIGH);
  // Serial.println("Relay OFF");
  // delay(5000);

  int raw = analogRead(SENSOR_PIN);
  Serial.printf("Raw: %d | Approx moisture: higher=dry\n", raw);
  delay(1000);
}