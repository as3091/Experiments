#include <Arduino.h>
#include <LittleFS.h>
#include <sqlite3.h>
#include "soil_sensor.h"
#include "secrets.h"

// #define DB_PATH "/littlefs/soil_readings.db"

// #ifndef DB_PATH
// #error DB_PATH is not defined from .env
// #endif
static sqlite3 *db = nullptr;

bool soil_db_init() {
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed");
        return false;
    }

    sqlite3_initialize();

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        Serial.printf("Failed to open DB: %s\n", sqlite3_errmsg(db));
        return false;
    }

    const char *sql =
        "CREATE TABLE IF NOT EXISTS readings ("
        "  date TEXT NOT NULL,"
        "  time TEXT NOT NULL,"
        "  reading INTEGER NOT NULL"
        ");";

    char *errmsg = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &errmsg) != SQLITE_OK) {
        Serial.printf("Failed to create table: %s\n", errmsg);
        sqlite3_free(errmsg);
        return false;
    }

    // Serial.println("Soil DB ready.");
    return true;
}
void take_reading(struct tm &time_now)
{

  int raw = analogRead(SOIL_SENSOR_PIN);
  Serial.printf("[%02d:%02d:%02d] Raw: %d | Approx moisture: higher=dry\n",
  time_now.tm_hour, time_now.tm_min, time_now.tm_sec, raw);
  soil_db_insert(time_now, raw);
}

bool soil_db_insert(struct tm &timeinfo, int raw) {
    if (!db) {
        Serial.println("DB not initialized");
        return false;
    }

    char date[11], time_str[9];
    strftime(date,     sizeof(date),     "%Y-%m-%d", &timeinfo);
    strftime(time_str, sizeof(time_str), "%H:%M:%S", &timeinfo);

    sqlite3_stmt *stmt;
    const char *sql =
        "INSERT INTO readings (date, time, reading) VALUES (?, ?, ?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Serial.printf("Prepare failed: %s\n", sqlite3_errmsg(db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, date,     -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, time_str, -1, SQLITE_STATIC);
    sqlite3_bind_int (stmt, 3, raw);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok) {
        Serial.printf("Insert failed: %s\n", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
    return ok;
}

void soil_db_foreach(soil_row_cb cb) {
    if (!db || !cb) return;

    sqlite3_stmt *stmt;
    const char *sql = "SELECT date, time, reading FROM readings ORDER BY date, time;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Serial.printf("soil_db_foreach prepare failed: %s\n", sqlite3_errmsg(db));
        return;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* date     = (const char*)sqlite3_column_text(stmt, 0);
        const char* time_str = (const char*)sqlite3_column_text(stmt, 1);
        int reading          = sqlite3_column_int(stmt, 2);
        cb(date, time_str, reading);
    }
    sqlite3_finalize(stmt);
}

void soil_db_print_all() {
    if (!db) { Serial.println("[DB] Not initialized"); return; }

    sqlite3_stmt *stmt;
    const char *sql = "SELECT date, time, reading FROM readings ORDER BY date, time;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Serial.printf("[DB] print_all prepare failed: %s\n", sqlite3_errmsg(db));
        return;
    }
    int count = 0;
    Serial.println("[DB] --- All readings ---");
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* date     = (const char*)sqlite3_column_text(stmt, 0);
        const char* time_str = (const char*)sqlite3_column_text(stmt, 1);
        int reading          = sqlite3_column_int(stmt, 2);
        Serial.printf("  %s %s  raw=%d\n", date, time_str, reading);
        count++;
    }
    sqlite3_finalize(stmt);
    if (count == 0) Serial.println("  (no rows)");
    Serial.printf("[DB] --- %d row(s) ---\n", count);
}

void soil_db_clear() {
    if (!db) return;
    char *errmsg = nullptr;
    if (sqlite3_exec(db, "DELETE FROM readings;", nullptr, nullptr, &errmsg) != SQLITE_OK) {
        Serial.printf("soil_db_clear failed: %s\n", errmsg);
        sqlite3_free(errmsg);
    }
}
