#include "button_check_led_state_blink_helper.h"
#include <Arduino.h>

#ifndef OFF_BUTTON_PIN
#error OFF_BUTTON_PIN is not defined from .env
#endif
#ifndef ON_BUTTON_PIN
#error ON_BUTTON_PIN is not defined from .env
#endif
#ifndef LED_PIN
#error LED_PIN is not defined from .env
#endif


void initPins() {
    pinMode(OFF_BUTTON_PIN, INPUT_PULLUP);
    pinMode(ON_BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
}

void handleButtonLED(unsigned long &lastInputMillis, unsigned long &previousMillis) {

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= 150)
    {  // Check every 0.15 second
      previousMillis = currentMillis;
      int off_buttonState = digitalRead(OFF_BUTTON_PIN);
      int on_buttonState = digitalRead(ON_BUTTON_PIN);
      int ledState = digitalRead(LED_PIN);  // HIGH means LED currently ON
      if (off_buttonState == LOW && on_buttonState == LOW){
        // Both buttons pressed: do nothing
        lastInputMillis = currentMillis;    // reset inactivity timer
        Serial.println("Both buttons pressed -> ignoring input");
      }
      else if (off_buttonState == LOW){
      //  && ledState == HIGH) {
        digitalWrite(LED_PIN, LOW);         // turn OFF only if currently ON
        lastInputMillis = currentMillis;
        Serial.println("OFF button pressed -> LED OFF");
      } else if (on_buttonState == LOW){
        // && ledState == LOW) {
        digitalWrite(LED_PIN, HIGH);        // optional: ON button behavior
        lastInputMillis = currentMillis;
        Serial.println("ON button pressed -> LED ON");
      }
    }
}