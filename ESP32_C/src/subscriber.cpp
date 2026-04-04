#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "db_stuff.h"
#include "secrets.h"
#include "publisher.h"

static WiFiClientSecure tlsClient;
PubSubClient            mqttClient(tlsClient);  // extern in subscriber.h

static volatile bool sendDataFlag  = false;
static volatile bool ackFlag       = false;
static char          ackPayload[512] = {0};

// ---------------------------------------------------------------------------
// MQTT callback — fires on every incoming message
// ---------------------------------------------------------------------------
static void onMessage(char* topic, byte* payload, unsigned int length) {
    char msg[64] = {0};
    size_t len = min((size_t)length, sizeof(msg) - 1);
    memcpy(msg, payload, len);

    Serial.printf("[MQTT] %s -> %s\n", topic, msg);

    if (strcmp(topic, TOPIC_COMMAND) == 0) {
        if (strcmp(msg, "send") == 0) {
            sendDataFlag = true;
        } else if (strncmp(msg, "ack ", 4) == 0) {
            strncpy(ackPayload, msg + 4, sizeof(ackPayload) - 1);
            ackFlag = true;
        }
    }
}

// ---------------------------------------------------------------------------
// (Re)connect to broker
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void setup_listener() {
    tlsClient.setInsecure();
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCallback(onMessage);
    mqttClient.setBufferSize(256);
    mqttClient.setKeepAlive(60);
    mqttConnect();
}

void process_loop() {
    if (!mqttClient.connected()) {
        delay(5000);
        mqttConnect();
    }

    mqttClient.loop();

    if (sendDataFlag) {
        sendDataFlag = false;
        publishReading();
    }

    if (ackFlag) {
        ackFlag = false;
        char buf[512];
        strncpy(buf, ackPayload, sizeof(buf) - 1);
        char* token = strtok(buf, "|");
        int deleted = 0;
        while (token) {
            if (db_delete(token)) {
                Serial.printf("[DB] Deleted: %s\n", token);
                deleted++;
            }
            token = strtok(nullptr, "|");
        }
        Serial.printf("[DB] Ack processed — %d row(s) deleted.\n", deleted);
    }
}

struct tm get_next_listen_time(struct tm time_now) {
    struct tm new_time = time_now;
    new_time.tm_min += LISTEN_SLEEP_MIN;
    mktime(&new_time);
    return new_time;
}

struct tm mqtt_check(struct tm time_now) {
    setup_listener();
    unsigned long start = millis();
    while (millis() - start < 10000) {
        process_loop();
        delay(50);
    }
    return get_next_listen_time(time_now);
}
