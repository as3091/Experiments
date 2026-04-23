#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>
#include "secrets.h"
#include "wifi_helper.h"
#include "solenoid.h"

#define SOLENOID_PIN      26
#define AUTO_OFF_MS       (30UL * 60UL * 1000UL)  // 30 minutes

static WiFiClientSecure tlsClient;
static PubSubClient     mqttClient(tlsClient);

static bool          waterOn      = false;
static unsigned long waterOnTime  = 0;

static void timestamp(char* buf, size_t len) {
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    strftime(buf, len, "%Y%m%d_%H%M%S", &t);
}

static void publish_event(const char* event) {
    char ts[20];
    timestamp(ts, sizeof(ts));
    char payload[64];
    snprintf(payload, sizeof(payload), "%s at %s", event, ts);
    mqttClient.publish(TOPIC_DATA, payload);
    Serial.printf("[MQTT] → %s: %s\n", TOPIC_DATA, payload);
}

static void water_start() {
    turn_water_on();
    waterOn     = true;
    waterOnTime = millis();
    publish_event("water started");
}

static void water_stop(const char* reason = "") {
    turn_water_off();
    waterOn = false;
    if (reason && *reason) Serial.printf("Water stopped: %s\n", reason);
    publish_event("water stopped");
}

static void on_message(char* topic, byte* payload, unsigned int length) {
    char msg[64] = {0};
    memcpy(msg, payload, min((unsigned int)sizeof(msg) - 1, length));
    Serial.printf("[MQTT] ← %s: %s\n", topic, msg);

    if (strcmp(msg, "start watering") == 0) {
        water_start();
    } else if (strcmp(msg, "stop watering") == 0) {
        water_stop("command");
    }
}

static bool mqttConnect() {
    Serial.print("Connecting to MQTT broker...");
    tlsClient.setInsecure();
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCallback(on_message);

    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)) {
        Serial.println(" connected.");
        mqttClient.subscribe(TOPIC_COMMAND);
        Serial.printf("Subscribed to: %s\n", TOPIC_COMMAND);
        return true;
    }
    Serial.printf(" failed (rc=%d).\n", mqttClient.state());
    return false;
}

void setup() {
    Serial.begin(115200);
    delay(100);

    pinMode(SOLENOID_PIN, OUTPUT);
    digitalWrite(SOLENOID_PIN, HIGH);  // solenoid OFF on boot

    wifi_setup();
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    Serial.print("Waiting for NTP sync...");
    while (time(nullptr) < 1577836800L) { delay(200); }
    Serial.println(" done.");
    mqttConnect();
}

void loop() {
    // Reconnect MQTT if dropped
    if (!mqttClient.connected()) {
        mqttConnect();
    }
    mqttClient.loop();

    // Auto shut-off after 30 minutes
    if (waterOn && (millis() - waterOnTime >= AUTO_OFF_MS)) {
        water_stop("30 min auto shut-off");
    }
}
