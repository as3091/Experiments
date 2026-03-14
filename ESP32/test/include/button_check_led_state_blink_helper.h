// src/led_button.h
#ifndef BUTTON_CHECK_LED_STATE_BLINK_HELPER_H
#define BUTTON_CHECK_LED_STATE_BLINK_HELPER_H

// const int buttonPin = BUTTON_PIN;
// const int ledPin = LED_PIN;
// // const int buttonPin = 0;
// // const int ledPin = LED_BUILTIN;

void initPins();
void handleButtonLED(unsigned long &lastInputMillis, unsigned long &previousMillis);

#endif