#include <Arduino.h>
#include <esp_sleep.h>
#include <WiFi.h>
#include <time.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "secrets.h"
#include "wifi_helper.h"

// ── HiveMQ (existing) ────────────────────────────────────────────────────────
static WiFiClientSecure tlsClient;
PubSubClient            mqttClient(tlsClient);

static bool mqttConnect() {
    Serial.print("Connecting to MQTT broker...");
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)) {
        Serial.println(" connected.");
        mqttClient.subscribe(TOPIC_COMMAND);
        Serial.printf("Subscribed to: %s\n", TOPIC_COMMAND);
        return true;
    }
    Serial.printf(" failed (rc=%d).\n", mqttClient.state());
    return false;
}

// ── Blynk MQTT ───────────────────────────────────────────────────────────────
static WiFiClientSecure blynkTls;
static PubSubClient     blynkMqtt(blynkTls);

static void blynk_connect() {
    blynkTls.setInsecure();
    blynkMqtt.setServer(BLYNK_MQTT_HOST, BLYNK_MQTT_PORT);
    Serial.print("Connecting to Blynk MQTT...");
    blynkMqtt.setBufferSize(512);
    blynkMqtt.setKeepAlive(45);
    if (blynkMqtt.connect("esp32_blynk", "device", BLYNK_AUTH_TOKEN)) {
        Serial.println(" connected.");
    } else {
        Serial.printf(" failed (rc=%d).\n", blynkMqtt.state());
    }
}

#define BATCH_SIZE 5

struct Reading {
    long long ts_ms;
    int       strength;
};

static Reading batch[BATCH_SIZE];
static int     batch_idx = 0;

static long long now_ms() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return (long long)tv.tv_sec * 1000LL + tv.tv_usec / 1000;
}

static int rssi_to_percent(int rssi) {
    if (rssi <= -100) return 0;
    if (rssi >= -30)  return 100;
    return 2 * (rssi + 100);
}

static void collect_reading() {
    batch[batch_idx++] = { now_ms(), rssi_to_percent(WiFi.RSSI()) };
    Serial.printf("[Blynk] Collected reading %d/%d — %lld ms  %d%%\n",
                  batch_idx, BATCH_SIZE, batch[batch_idx-1].ts_ms, batch[batch_idx-1].strength);
}

static void publish_batch() {
    if (!blynkMqtt.connected()) blynk_connect();

    // Build [[ts,val],[ts,val],...] — max ~30 chars per entry
    char payload[BATCH_SIZE * 32 + 4];
    int  pos = 0;
    pos += snprintf(payload + pos, sizeof(payload) - pos, "[");
    for (int i = 0; i < BATCH_SIZE; i++) {
        if (i > 0) pos += snprintf(payload + pos, sizeof(payload) - pos, ",");
        pos += snprintf(payload + pos, sizeof(payload) - pos,
                        "[%lld,%d]", batch[i].ts_ms, batch[i].strength);
    }
    snprintf(payload + pos, sizeof(payload) - pos, "]");

    blynkMqtt.publish(BLYNK_VPIN_WIFI_STRENGTH, payload);
    Serial.printf("[Blynk] Batch sent: %s\n", payload);
    batch_idx = 0;
}

// ── Core ─────────────────────────────────────────────────────────────────────
void go_to_deep_sleep(String reason = "") {
    if (reason.length()) Serial.printf("Sleeping: %s\n", reason.c_str());
    Serial.printf("Sleeping for %d seconds...\n", SLEEP_SEC);
    Serial.flush();
    esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_SEC * 1000000ULL);
    esp_deep_sleep_start();
}

void setup() {
    Serial.begin(115200);
    delay(100);

    wifi_setup();
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    blynk_connect();
}

void loop() {
    static unsigned long lastCollect = 0;
    if (millis() - lastCollect >= 1000) {
        collect_reading();
        lastCollect = millis();
    }
    if (batch_idx >= BATCH_SIZE) {
        publish_batch();
    }
    blynkMqtt.loop();
}
