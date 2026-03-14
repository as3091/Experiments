// #include <Arduino.h>
/*
// put function declarations here:
int myFunction(int, int);

void setup() {
  // put your setup code here, to run once:
  int result = myFunction(2, 3);
}

void loop() {
  // put your main code here, to run repeatedly:
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}

 
/*
  Simple Blink sketch
  simple-blink.cpp
  Use for PlatformIO demo
 
  From original Arduino Blink Sketch
  https://www.arduino.cc/en/Tutorial/Blink
  
  DroneBot Workshop 2021
  https://dronebotworkshop.com
*/
 
// Set LED_BUILTIN if undefined or not pin 13
// #define LED_BUILTIN 2
 
// void setup()
// {
//   // Initialize LED pin as an output.
//   pinMode(LED_BUILTIN, OUTPUT);
// }
 
// void loop()
// {
//   // Set the LED HIGH
//   digitalWrite(LED_BUILTIN, HIGH);
 
//   // Wait for a second
//   delay(1000);
 
//   // Set the LED LOW
//   digitalWrite(LED_BUILTIN, LOW);
 
//    // Wait for a second
//   delay(1000);
// }

#include <Arduino.h>

unsigned long previousMillis = 0;  // Tracks last toggle time
bool ledState = false;              // LED state tracker

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.println("Starting millis blink test...");
}


void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= 1000) {  // Check every 1 second
    previousMillis = currentMillis;
    ledState = !ledState;  // Toggle state
    if (ledState) {
      digitalWrite(LED_BUILTIN, HIGH);  // ON (active-high for external LED)
      Serial.println("LED ON");
    } else {
      digitalWrite(LED_BUILTIN, LOW);   // OFF
      Serial.println("LED OFF");
    }
  }
}