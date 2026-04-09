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

static void blynk_on_message(char* topic, byte* payload, unsigned int length) {
    Serial.printf("[Blynk] ← topic: %s  payload: ", topic);
    for (unsigned int i = 0; i < length; i++) Serial.print((char)payload[i]);
    Serial.println();
}

static void blynk_connect() {
    blynkTls.setInsecure();
    blynkMqtt.setServer(BLYNK_MQTT_HOST, BLYNK_MQTT_PORT);
    blynkMqtt.setCallback(blynk_on_message);
    Serial.print("Connecting to Blynk MQTT...");
    blynkMqtt.setBufferSize(512);
    blynkMqtt.setKeepAlive(45);
    if (blynkMqtt.connect("esp32_blynk", "device", BLYNK_AUTH_TOKEN)) {
        Serial.println(" connected.");

        // Required: tell Blynk about this device so the dashboard activates it
        char info[128];
        snprintf(info, sizeof(info),
                 "{\"type\":\"TMCx\",\"ver\":\"0.0.1\",\"build\":\"%s %s\","
                 "\"tmpl\":\"%s\",\"rxbuff\":512}",
                 __DATE__, __TIME__, BLYNK_TEMPLATE_ID);
        blynkMqtt.publish("info/mcu", info);
        Serial.printf("[Blynk] Published info/mcu: %s\n", info);

        // Subscribe to all downlink messages from Blynk server/dashboard
        blynkMqtt.subscribe("downlink/#");
        Serial.println("[Blynk] Subscribed to downlink/#");
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
    if (batch_idx == 0) return;
    if (!blynkMqtt.connected()) blynk_connect();

    // Build [[ts_ms, val], ...] — Blynk timestamped_batch format
    char payload[BATCH_SIZE * 32 + 4];
    int  pos = 0;
    pos += snprintf(payload + pos, sizeof(payload) - pos, "[");
    for (int i = 0; i < batch_idx; i++) {
        if (i > 0) pos += snprintf(payload + pos, sizeof(payload) - pos, ",");
        pos += snprintf(payload + pos, sizeof(payload) - pos,
                        "[%lld,%d]", batch[i].ts_ms, batch[i].strength);
    }
    snprintf(payload + pos, sizeof(payload) - pos, "]");

    blynkMqtt.publish(BLYNK_TOPIC_BATCH, payload);
    Serial.printf("[Blynk] → %s  %s\n", BLYNK_TOPIC_BATCH, payload);
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

    // Wait for NTP sync before collecting (time() returns < 2020 until synced)
    Serial.print("Waiting for NTP sync...");
    while (time(nullptr) < 1577836800L) { delay(200); }
    Serial.println(" done.");

    blynk_connect();

    // Collect and publish one reading immediately so we don't wait 5 minutes
    collect_reading();
    publish_batch();
}

void loop() {
    unsigned long now = millis();

    // Check WiFi every SLEEP_SEC seconds; collect a reading at the same time
    static unsigned long lastWifiCheck = 0;
    if (now - lastWifiCheck >= (unsigned long)SLEEP_SEC * 1000UL) {
        lastWifiCheck = now;
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[WiFi] Disconnected — reconnecting...");
            wifi_setup();
        } else {
            Serial.println("[WiFi] OK");
        }
        collect_reading();
    }

    // Send batch to Blynk every LISTEN_SLEEP_MIN minutes
    static unsigned long lastPublish = 0;
    if (now - lastPublish >= (unsigned long)LISTEN_SLEEP_MIN * 60UL * 1000UL) {
        lastPublish = now;
        publish_batch();
    }

    blynkMqtt.loop();
}
