#include <Arduino.h>
#include "time.h"
#include <WiFi.h>
#include <esp_sleep.h>
#include "wifi_helper.h"
#include "get_the_time.h"
#include "soil_sensor.h"

#ifndef SLEEP_SEC
#error SLEEP_SEC is not defined from .env
#endif

void setup() {
  Serial.begin(115200);
  analogSetAttenuation(ADC_11db);

  // WiFi + NTP only needed on first boot; RTC retains time through deep sleep
  if (esp_reset_reason() != ESP_RST_DEEPSLEEP) {
    // wifi_scan();
    if (wifi_setup()) {
      while (!local_time_setup()) {
        delay(1000);
      }
    } else {
      Serial.println("WiFi failed.");
    }
  }

  // DB must be re-opened every wake (RAM is cleared during deep sleep)
  soil_db_init();

  struct tm timeinfo;
  int raw = analogRead(SOIL_SENSOR_PIN);
  if (getLocalTime(&timeinfo)) {
    Serial.printf("[%02d:%02d:%02d] Raw: %d | Approx moisture: higher=dry\n",
      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, raw);
    soil_db_insert(timeinfo, raw);
  } else {
    Serial.printf("[--:--:--] Raw: %d | Approx moisture: higher=dry\n", raw);
  }

  Serial.printf("Sleeping for %d seconds...\n", SLEEP_SEC);
  Serial.flush();
  esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_SEC * 1000000ULL);
  esp_deep_sleep_start();
}

void loop() {}
