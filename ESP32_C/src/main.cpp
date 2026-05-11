#include <Arduino.h>
#include <ArduinoOTA.h>
#include "secrets.h"
#include "wifi_helper.h"
#include "MQTT_stuff.h"
#include "get_the_time.h"

void setup() {
  Serial.begin(115200);
  delay(1000);

  // pinMode(SOLENOID_PIN, OUTPUT);
  // digitalWrite(SOLENOID_PIN, HIGH);  // solenoid OFF on boot

  if (wifi_setup(WIFI_SSID, WIFI_PASS))
  {
    Serial.println("\nWiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // Set a hostname so you don't have to hunt for the IP address
    // ArduinoOTA.setHostname(ArduinoOTA_Hostname);


    // Set a hostname so you don't have to hunt for the IP address
    ArduinoOTA.setHostname(ArduinoOTA_Hostname);  
    ArduinoOTA.setPassword(ArduinoOTA_Password);

    ArduinoOTA.begin();
    mqttConnect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS, MQTT_HOST, MQTT_PORT,TOPIC_DATA);
    local_time_setup(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  }
}

void loop() {
  delay(5000);
  ArduinoOTA.handle();
  publish_event(TOPIC_DATA, getFormattedTime().c_str());
}
