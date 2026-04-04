#include <Arduino.h>
#include <PubSubClient.h>
#include "subscriber.h"
#include "db_stuff.h"
#include "secrets.h"

static int  s_published = 0;
static bool s_publishOk = true;

static void publishRow(const char* date_time_str, int reading) {
    char payload[19];
    snprintf(payload, sizeof(payload), "%s|%04d", date_time_str, reading);

    if (mqttClient.publish(TOPIC_DATA, payload)) {
        Serial.printf("[MQTT] Published -> %s\n", payload);
        s_published++;
    } else {
        Serial.printf("[MQTT] Publish failed for row %s\n", date_time_str);
        s_publishOk = false;
    }
    delay(20);
}

void publishReading() {
    s_published = 0;
    s_publishOk = true;

    db_foreach(publishRow);

    if (s_published == 0) {
        Serial.println("[MQTT] No rows in DB to publish.");
        return;
    }

    Serial.printf("[MQTT] Published %d row(s). Waiting for ack to delete.\n", s_published);
}
