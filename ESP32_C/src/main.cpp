#include <Arduino.h>
#include "time.h"
#include <WiFi.h>
#include <esp_sleep.h>
#include "wifi_helper.h"
#include "get_the_time.h"
#include "soil_sensor.h"
#include "listener.h"
#include "secrets.h"

// #ifndef SLEEP_SEC
// #error SLEEP_SEC is not defined from .env
// #endif

RTC_DATA_ATTR static struct tm next_listen_time;

void go_to_deep_sleep(String the_reason_to_start_over_new = "")
{
  if (esp_reset_reason() != ESP_RST_DEEPSLEEP)
  {
    Serial.printf("the_reason_to_start_over_new:%s\n", the_reason_to_start_over_new);  
  } 
  Serial.printf("Sleeping for %d seconds...\n", SLEEP_SEC);
  Serial.flush();
  esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_SEC * 1000000ULL);
  esp_deep_sleep_start();
}


void print_time(const char* label, struct tm t) {
  Serial.printf("%s\t: %04d-%02d-%02d %02d:%02d:%02d\n",
    label,
    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
    t.tm_hour, t.tm_min, t.tm_sec);
}

void setup() {
  Serial.begin(115200);
  analogSetAttenuation(ADC_11db);
  struct tm time_now;
  // WiFi + NTP only needed on first boot; RTC retains time through deep sleep
  // wifi_scan();
  if (!wifi_setup()) {
    go_to_deep_sleep("WiFi failed." );
  }
  while (!local_time_setup()) {
    delay(1000);
  }
  getLocalTime(&time_now);
  if (esp_reset_reason() != ESP_RST_DEEPSLEEP) {
    next_listen_time = get_next_listen_time(time_now);
  }
  
  // DB must be re-opened every wake (RAM is cleared during deep sleep)
  if (soil_db_init()){
    print_time("next_listen_time", next_listen_time);
    print_time("time_now", time_now);
    take_reading(time_now);
    soil_db_print_all();
  }

  
  // getLocalTime(&time_now);

  // int raw = analogRead(SOIL_SENSOR_PIN);
  // Serial.printf("[%02d:%02d:%02d] Raw: %d | Approx moisture: higher=dry\n",
  // time_now.tm_hour, time_now.tm_min, time_now.tm_sec, raw);
  // soil_db_insert(time_now, raw);
// } else {
//     Serial.printf("[--:--:--] Raw: %d | Approx moisture: higher=dry\n", raw);
//   }

  if(mktime(&next_listen_time) < mktime(&time_now))
  {
    if (wifi_setup()) {
      next_listen_time = mqtt_check(time_now);
    } else {
      Serial.println("WiFi failed — skipping MQTT check.");
    }
  }
  go_to_deep_sleep("normal stuff");
}

void loop() {}
