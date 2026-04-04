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
        "  date_time TEXT NOT NULL,"
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

  char date_time_now_string[15];
  strftime(date_time_now_string, sizeof(date_time_now_string), "%Y%m%d%H%M%S", &time_now);
  int raw = analogRead(SOIL_SENSOR_PIN);
  Serial.printf("%s : %d | Approx moisture: higher=dry\n",date_time_now_string, raw);

  soil_db_insert(date_time_now_string, raw);
}

bool soil_db_insert(const char* date_time_now_string, int raw) {
    if (!db) {
        Serial.println("DB not initialized");
        return false;
    }

    // char date[11], time_str[9];
    // strftime(date,     sizeof(date),     "%Y-%m-%d", &timeinfo);
    // strftime(time_str, sizeof(time_str), "%H:%M:%S", &timeinfo);

    sqlite3_stmt *stmt;
    const char *sql =
        "INSERT INTO readings (date_time, reading) VALUES ( ?, ?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Serial.printf("Prepare failed: %s\n", sqlite3_errmsg(db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, date_time_now_string,     -1, SQLITE_STATIC);
    sqlite3_bind_int (stmt, 2, raw);

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
    const char *sql = "SELECT date_time, reading FROM readings ORDER BY date_time;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Serial.printf("soil_db_foreach prepare failed: %s\n", sqlite3_errmsg(db));
        return;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* date_time_now_string  = (const char*)sqlite3_column_text(stmt, 0);
        int reading          = sqlite3_column_int(stmt, 1);
        cb(date_time_now_string, reading);
    }
    sqlite3_finalize(stmt);
}

void soil_db_print_all() {
    if (!db) { Serial.println("[DB] Not initialized"); return; }

    sqlite3_stmt *stmt;
    const char *sql = "SELECT date_time, reading FROM readings ORDER BY date_time;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Serial.printf("[DB] print_all prepare failed: %s\n", sqlite3_errmsg(db));
        return;
    }
    int count = 0;
    Serial.println("[DB] --- All readings ---");
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* date_time = (const char*)sqlite3_column_text(stmt, 0);
        int reading           = sqlite3_column_int(stmt, 1);
        Serial.printf("  %s  raw=%d\n", date_time, reading);
        count++;
    }
    sqlite3_finalize(stmt);
    if (count == 0) Serial.println("  (no rows)");
    Serial.printf("[DB] --- %d row(s) ---\n", count);
}

bool soil_db_delete(const char* date_time) {
    if (!db) return false;
    sqlite3_stmt *stmt;
    const char *sql = "DELETE FROM readings WHERE date_time = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, date_time, -1, SQLITE_STATIC);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

void soil_db_clear() {
    if (!db) return;
    char *errmsg = nullptr;
    if (sqlite3_exec(db, "DELETE FROM readings;", nullptr, nullptr, &errmsg) != SQLITE_OK) {
        Serial.printf("soil_db_clear failed: %s\n", errmsg);
        sqlite3_free(errmsg);
    }
}
