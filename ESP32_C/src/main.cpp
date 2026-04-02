#include <Arduino.h>
#include "time.h"
#include <WiFi.h>  // For WiFi functionality
#include "wifi_helper.h"  // For WiFi setup and scanning

const char* NTP_SERVER    = "pool.ntp.org";
const long GMT_OFFSET_SEC  = 5.5*60*60;      // UTC offset in seconds (e.g. -18000 for UTC-5)
const int DAYLIGHT_OFFSET_SEC = 0;  // DST offset in seconds (e.g. 3600 for +1h)

void connectWiFi() {
  Serial.printf("Connecting to %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nWiFi connected. IP: %s\n", WiFi.localIP().toString().c_str());
}

// void printLocalTime() {
//   struct tm timeinfo;
//   if (!getLocalTime(&timeinfo)) {
//     Serial.println("Failed to obtain time");
//     return;
//   }
//   Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
// }

void setup() {

  Serial.begin(115200);
  analogSetAttenuation(ADC_11db);  // Important for ESP32 to read up to ~3.3V properly

  wifi_scan();
  if (wifi_setup()) {  // Assuming your function; replace if different
      Serial.println("\nWiFi connected! IP: " + WiFi.localIP().toString());
      
      while (!local_time_setup()){}
        delay(1000);
      }
      
      
  }
  else {
      Serial.println("WiFi failed.");
  }   

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

  struct tm timeinfo;
  int raw = analogRead(SENSOR_PIN);
  if (getLocalTime(&timeinfo)) {
    Serial.printf("[%02d:%02d:%02d] Raw: %d | Approx moisture: higher=dry\n",
      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, raw);
  } else {
    Serial.printf("[--:--:--] Raw: %d | Approx moisture: higher=dry\n", raw);
  }
  delay(1000);
}