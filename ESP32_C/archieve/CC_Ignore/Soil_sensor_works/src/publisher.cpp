#include <Arduino.h>
#include <PubSubClient.h>
#include <time.h>
#include "subscriber.h"
#include "db_stuff.h"
#include "secrets.h"

#define MAX_ROWS    512
#define MAX_PAYLOAD 1020   // stay comfortably under 1 KB

struct Row {
    char dt[15];   // YYYYMMDDHHmmss + null
    int  reading;
};

static Row  s_rows[MAX_ROWS];
static int  s_row_count = 0;
static bool s_publishOk = true;

// ---------------------------------------------------------------------------
// Parse YYYYMMDDHHmmss → epoch (uses local TZ, consistent for diff only)
// ---------------------------------------------------------------------------
static time_t dt_to_epoch(const char* dt) {
    struct tm t = {};
    sscanf(dt, "%4d%2d%2d%2d%2d%2d",
           &t.tm_year, &t.tm_mon, &t.tm_mday,
           &t.tm_hour, &t.tm_min, &t.tm_sec);
    t.tm_year -= 1900;
    t.tm_mon  -= 1;
    t.tm_isdst = -1;
    return mktime(&t);
}

// ---------------------------------------------------------------------------
// Callback: collect rows into static buffer
// ---------------------------------------------------------------------------
static void collectRow(const char* dt, int reading) {
    if (s_row_count < MAX_ROWS) {
        strncpy(s_rows[s_row_count].dt, dt, 14);
        s_rows[s_row_count].dt[14] = '\0';
        s_rows[s_row_count].reading = reading;
        s_row_count++;
    }
}

// ---------------------------------------------------------------------------
// Publish one payload string, log result
// ---------------------------------------------------------------------------
static void doPublish(const char* payload, int count, const char* start_dt) {
    if (mqttClient.publish(TOPIC_DATA, payload)) {
        Serial.printf("[MQTT] Published %d reading(s) from %s\n", count, start_dt);
    } else {
        Serial.println("[MQTT] Publish failed");
        s_publishOk = false;
    }
    delay(20);
}

// ---------------------------------------------------------------------------
// Publish one contiguous block [start, end) with uniform interval_sec.
// Splits into <MAX_PAYLOAD chunks; each chunk starts with a new header
// anchored to its first reading's timestamp.
// ---------------------------------------------------------------------------
static void publishBlock(int start, int end, long interval_sec) {
    char payload[MAX_PAYLOAD + 1];
    int  payload_len = 0;
    int  chunk_start = start;

    for (int i = start; i < end; i++) {
        char num[8];
        int  nlen = snprintf(num, sizeof(num), "%d", s_rows[i].reading);

        if (i == chunk_start) {
            // First reading of a chunk: write header then reading (always fits)
            payload_len = snprintf(payload, sizeof(payload),
                                   "%s|%ld|", s_rows[i].dt, interval_sec);
            memcpy(payload + payload_len, num, nlen);
            payload_len += nlen;
        } else if (payload_len + 1 + nlen >= MAX_PAYLOAD) {
            // Next reading won't fit — flush and start new chunk at i
            payload[payload_len] = '\0';
            doPublish(payload, i - chunk_start, s_rows[chunk_start].dt);

            chunk_start = i;
            payload_len = snprintf(payload, sizeof(payload),
                                   "%s|%ld|", s_rows[i].dt, interval_sec);
            memcpy(payload + payload_len, num, nlen);
            payload_len += nlen;
        } else {
            payload[payload_len++] = ',';
            memcpy(payload + payload_len, num, nlen);
            payload_len += nlen;
        }
    }

    // Flush remainder
    if (payload_len > 0) {
        payload[payload_len] = '\0';
        doPublish(payload, end - chunk_start, s_rows[chunk_start].dt);
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void publishReading() {
    s_row_count = 0;
    s_publishOk = true;
    db_foreach(collectRow);

    if (s_row_count == 0) {
        Serial.println("[MQTT] No rows in DB to publish.");
        return;
    }

    // Walk rows, find contiguous blocks where the interval between every
    // consecutive pair of timestamps is identical, then publish each block.
    int block_start = 0;
    while (block_start < s_row_count) {
        long interval_sec;
        int  block_end;

        if (s_row_count - block_start < 2) {
            interval_sec = 0;
            block_end    = s_row_count;
        } else {
            interval_sec = (long)difftime(
                dt_to_epoch(s_rows[block_start + 1].dt),
                dt_to_epoch(s_rows[block_start].dt));

            block_end = block_start + 2;
            while (block_end < s_row_count) {
                long gap = (long)difftime(
                    dt_to_epoch(s_rows[block_end].dt),
                    dt_to_epoch(s_rows[block_end - 1].dt));
                if (gap != interval_sec) break;
                block_end++;
            }
        }

        publishBlock(block_start, block_end, interval_sec);
        block_start = block_end;
    }

    Serial.printf("[MQTT] Done publishing %d row(s).\n", s_row_count);
}
