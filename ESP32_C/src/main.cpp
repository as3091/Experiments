#include <Arduino.h>
#include <esp_sleep.h>
#include "secrets.h"
#include "device_wifi.h"

void go_to_deep_sleep(String reason = "")
{
  if (reason.length()) Serial.printf("Sleeping: %s\n", reason.c_str());
  Serial.printf("Sleeping for %d seconds...\n", SLEEP_SEC);
  Serial.flush();
  esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_SEC * 1000000ULL);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(100);

  check_wifi_trigger();

  Serial.println("Normal boot mode. Waiting...");
}

void loop() {
  // delay(SLEEP_SEC*1000);
}
