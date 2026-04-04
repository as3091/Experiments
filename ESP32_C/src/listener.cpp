#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "wifi_helper.h"
#include "get_the_time.h"
#include "soil_sensor.h"
#include "secrets.h"

// #ifndef MQTT_HOST
// #error MQTT_HOST is not defined from .env
// #endif
// #ifndef MQTT_PORT
// #error MQTT_PORT is not defined from .env
// #endif
// #ifndef MQTT_USER
// #error MQTT_USER is not defined from .env
// #endif
// #ifndef MQTT_PASS
// #error MQTT_PASS is not defined from .env
// #endif

// #ifndef LISTEN_SLEEP_MIN
// #error LISTEN_SLEEP_MIN is not defined from .env
// #endif

static WiFiClientSecure tlsClient;
static PubSubClient     mqttClient(tlsClient);
static volatile bool    sendDataFlag = false;

// ---------------------------------------------------------------------------
// MQTT callback — fires on every incoming message
// ---------------------------------------------------------------------------
static void onMessage(char* topic, byte* payload, unsigned int length) {
    char msg[64] = {0};
    size_t len = min((size_t)length, sizeof(msg) - 1);
    memcpy(msg, payload, len);

    Serial.printf("[MQTT] %s -> %s\n", topic, msg);

    if (strcmp(topic, TOPIC_COMMAND) == 0 && strcmp(msg, "send") == 0) {
        sendDataFlag = true;
    }
}

// ---------------------------------------------------------------------------
// (Re)connect to broker — called on startup and after any disconnect
// ---------------------------------------------------------------------------
static bool mqttConnect() {
    Serial.print("Connecting to MQTT broker...");
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)) {
        Serial.println(" connected.");
        mqttClient.subscribe(TOPIC_COMMAND);
        Serial.printf("Subscribed to: %s\n", TOPIC_COMMAND);
        return true;
    }
    Serial.printf(" failed (rc=%d). Will retry in 5 s.\n", mqttClient.state());
    return false;
}

// ---------------------------------------------------------------------------
// Publish every row stored in the DB, then clear the table
// ---------------------------------------------------------------------------
static int  s_published = 0;
static bool s_publishOk = true;

static void publishRow(const char* date, const char* time_str, int reading) {
    char payload[96];
    snprintf(payload, sizeof(payload),
             "{\"date\":\"%s\",\"time\":\"%s\",\"raw\":%d}",
             date, time_str, reading);

    if (mqttClient.publish(TOPIC_DATA, payload)) {
        Serial.printf("[MQTT] Published -> %s\n", payload);
        s_published++;
    } else {
        Serial.printf("[MQTT] Publish failed for row %s %s\n", date, time_str);
        s_publishOk = false;
    }
    delay(20);  // give broker a moment between messages
}

static void publishReading() {
    s_published = 0;
    s_publishOk = true;

    soil_db_foreach(publishRow);

    if (s_published == 0) {
        Serial.println("[MQTT] No rows in DB to publish.");
        return;
    }

    Serial.printf("[MQTT] Published %d row(s).\n", s_published);

    if (s_publishOk) {
        soil_db_clear();
        Serial.println("[DB] Cleared published rows.");
    } else {
        Serial.println("[DB] Some publishes failed — rows kept for retry.");
    }
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------
void setup_listener() {
    // Serial.begin(115200);
    // analogSetAttenuation(ADC_11db);

    // if (!wifi_setup()) {
    //     Serial.println("WiFi failed — halting.");
    //     while (true) delay(1000);
    // }

    // while (!local_time_setup()) delay(1000);
    // soil_db_init();

    // TLS: skip certificate verification (fine for dev; swap in CA cert for prod)
    tlsClient.setInsecure();

    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCallback(onMessage);
    mqttClient.setBufferSize(256);
    mqttClient.setKeepAlive(60);

    mqttConnect();
    // struct tm time_now;
    // getLocalTime(&time_now);
    // time_now.tm_min += 10;
    // mktime(&time_now);   // normalises overflow (e.g. min=75 → hr+1, min=15)
}

void process_loop() {

    // Reconnect if dropped
    if (!mqttClient.connected()) {
        delay(5000);
        mqttConnect();
    }

    mqttClient.loop();   // process incoming messages & keepalive

    if (sendDataFlag) {
        sendDataFlag = false;
        publishReading();
    }
}

struct tm get_next_listen_time(struct tm time_now)
{
    struct tm new_time = time_now;
    new_time.tm_min += LISTEN_SLEEP_MIN;
    mktime(&new_time);
    return new_time;
}

struct tm mqtt_check(struct tm time_now){
    setup_listener();
    unsigned long start = millis();
    while (millis() - start < 10000) {   // listen for 10 s
        process_loop();
        delay(50);
    }
    return get_next_listen_time(time_now);
}

// struct tm mqtt_check(struct tm time_now){
//     setup_listener();
//     process_loop();
//     return get_next_listen_time(time_now);
// }
