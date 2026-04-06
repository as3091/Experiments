#include <Arduino.h>
#include <SPIFFS.h>
#include <sqlite3.h>
#include "db_stuff.h"
#include "secrets.h"

static sqlite3 *db = nullptr;

// Path used by SPIFFS file API (strip the /spiffs mount prefix from DB_PATH)
static constexpr const char* DB_FS_PATH = "/soil_readings.db";

// ---------------------------------------------------------------------------
// Validate the SQLite file header WITHOUT opening the DB through SQLite.
// Returns true if the file looks healthy (or simply does not exist yet).
// A corrupt/truncated file would otherwise hand SQLite a page_size of 0,
// triggering the IntegerDivideByZero crash that breaks the boot loop.
// ---------------------------------------------------------------------------
static bool db_header_valid() {
    if (!SPIFFS.exists(DB_FS_PATH)) return true;  // new file — nothing to check

    File f = SPIFFS.open(DB_FS_PATH, "r");
    if (!f) return false;

    uint8_t hdr[18];
    size_t  n = f.read(hdr, sizeof(hdr));
    f.close();

    if (n < 18) {
        Serial.println("[DB] Header too short — file truncated.");
        return false;
    }

    // Bytes 0-15: "SQLite format 3\0"
    static const uint8_t MAGIC[16] = {
        'S','Q','L','i','t','e',' ','f','o','r','m','a','t',' ','3','\0'
    };
    if (memcmp(hdr, MAGIC, 16) != 0) {
        Serial.println("[DB] Bad magic — not a SQLite file.");
        return false;
    }

    // Bytes 16-17: page size big-endian.  1 encodes 65536; 0 is always invalid.
    uint16_t ps = ((uint16_t)hdr[16] << 8) | hdr[17];
    if (ps == 0) {
        Serial.println("[DB] page_size == 0 in header — corrupt (crash-interrupted write).");
        return false;
    }
    // Must be a power of two between 512 and 65536 (encoded as 1)
    if (ps != 1 && (ps < 512 || (ps & (ps - 1)) != 0)) {
        Serial.printf("[DB] Invalid page_size %u in header.\n", ps);
        return false;
    }

    return true;
}

static void db_delete_file() {
    if (db) { sqlite3_close(db); db = nullptr; }
    SPIFFS.remove(DB_FS_PATH);
    Serial.println("[DB] Deleted corrupt database file — will create fresh.");
}

bool db_init() {
    if (!SPIFFS.begin(true)) {
        Serial.println("[DB] SPIFFS mount failed");
        return false;
    }

    sqlite3_initialize();

    // Guard: check header before SQLite can crash on a page_size of 0
    if (!db_header_valid()) {
        db_delete_file();
    }

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        Serial.printf("[DB] Failed to open: %s\n", sqlite3_errmsg(db));
        return false;
    }

    // Reduce memory pressure; journal_mode=OFF avoids creating a journal file.
    sqlite3_exec(db, "PRAGMA journal_mode=OFF;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "PRAGMA synchronous=OFF;",  nullptr, nullptr, nullptr);
    sqlite3_exec(db, "PRAGMA cache_size=1;",     nullptr, nullptr, nullptr);

    const char *sql =
        "CREATE TABLE IF NOT EXISTS readings ("
        "  date_time TEXT NOT NULL,"
        "  reading INTEGER NOT NULL"
        ");";

    char *errmsg = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &errmsg) != SQLITE_OK) {
        Serial.printf("[DB] Failed to create table: %s\n", errmsg);
        sqlite3_free(errmsg);
        return false;
    }

    Serial.println("[DB] Ready.");
    return true;
}

bool db_insert(const char* date_time, int raw) {
    if (!db) { Serial.println("[DB] Not initialized"); return false; }
    // Use sqlite3_exec (not prepare+bind+step) — avoids a crash in the
    // Sqlite3Esp32 SPIFFS VFS when sqlite3_step writes the first data page.
    // date_time is always 14 decimal digits so no SQL-injection risk.
    char sql[80];
    snprintf(sql, sizeof(sql),
             "INSERT INTO readings (date_time, reading) VALUES ('%s', %d);",
             date_time, raw);
    char *errmsg = nullptr;
    bool ok = (sqlite3_exec(db, sql, nullptr, nullptr, &errmsg) == SQLITE_OK);
    if (!ok) {
        Serial.printf("[DB] Insert failed: %s\n", errmsg);
        sqlite3_free(errmsg);
    }
    return ok;
}

bool db_delete(const char* date_time) {
    if (!db) return false;
    char sql[64];
    snprintf(sql, sizeof(sql),
             "DELETE FROM readings WHERE date_time = '%s';", date_time);
    return sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

int db_delete_range(const char* start_dt, const char* end_dt) {
    if (!db) return 0;
    char sql[96];
    snprintf(sql, sizeof(sql),
             "DELETE FROM readings WHERE date_time >= '%s' AND date_time <= '%s';",
             start_dt, end_dt);
    if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) != SQLITE_OK) return 0;
    return sqlite3_changes(db);
}

void db_clear() {
    if (!db) return;
    char *errmsg = nullptr;
    if (sqlite3_exec(db, "DELETE FROM readings;", nullptr, nullptr, &errmsg) != SQLITE_OK) {
        Serial.printf("db_clear failed: %s\n", errmsg);
        sqlite3_free(errmsg);
    }
}

void db_foreach(db_row_cb cb) {
    if (!db || !cb) return;

    sqlite3_stmt *stmt;
    const char *sql = "SELECT date_time, reading FROM readings ORDER BY date_time;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Serial.printf("db_foreach prepare failed: %s\n", sqlite3_errmsg(db));
        return;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* date_time = (const char*)sqlite3_column_text(stmt, 0);
        int reading           = sqlite3_column_int(stmt, 1);
        cb(date_time, reading);
    }
    sqlite3_finalize(stmt);
}

void db_print_all() {
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
