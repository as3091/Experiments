# ── Config ────────────────────────────────────────────────────────────────────
from pathlib import Path
from dotenv import dotenv_values

# .env is one level up from archieve/
_env = dotenv_values(Path.cwd().parent / ".env")

BROKER    = _env["MQTT_HOST"]
PORT      = int(_env["MQTT_PORT"])
MQTT_SUB_USER = _env["MQTT_SUB_USER"]
MQTT_SUB_PASS = _env["MQTT_SUB_PASS"]
SUB_CLIENT_ID = _env["SUB_CLIENT_ID"]

TOPIC_COMMAND = _env["TOPIC_COMMAND"]
TOPIC_DATA = _env["TOPIC_DATA"]

SEND_INTERVAL = int(_env["LISTEN_SLEEP_MIN"])*60
Server_DB_PATH = _env["Server_DB_PATH"]

print(f"Broker: {BROKER}:{PORT}  user: {MQTT_SUB_USER}")


# ── DB setup ──────────────────────────────────────────────────────────────────
import sqlite3

def db_init(path: str) -> sqlite3.Connection:
    con = sqlite3.connect(path, check_same_thread=False)
    con.execute("""
        CREATE TABLE IF NOT EXISTS readings (
            id        INTEGER PRIMARY KEY AUTOINCREMENT,
            date      TEXT NOT NULL,
            time      TEXT NOT NULL,
            reading   INTEGER NOT NULL,
            received_at TEXT DEFAULT (datetime('now'))
        )
    """)
    con.commit()
    return con

def db_insert(con: sqlite3.Connection, date: str, time_str: str, reading: int):
    con.execute(
        "INSERT INTO readings (date, time, reading) VALUES (?, ?, ?)",
        (date, time_str, reading)
    )
    con.commit()

con = db_init(Server_DB_PATH)
print(f"DB ready: {Server_DB_PATH}")