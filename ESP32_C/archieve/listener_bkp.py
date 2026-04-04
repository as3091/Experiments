# ── Config ────────────────────────────────────────────────────────────────────
from pathlib import Path
from dotenv import dotenv_values

# .env is one level up from archieve/
_env = dotenv_values(Path.cwd().parent / ".env")

BROKER    = _env["MQTT_HOST"]
PORT      = int(_env["MQTT_PORT"])
MQTT_USER = _env["MQTT_USER"]
MQTT_PASS = _env["MQTT_PASS"]
CLIENT_ID = "PC_Soil_Subscriber"

TOPIC_COMMAND = "soil_sensor_01/command"  # we publish "send" here
TOPIC_DATA    = "soil_sensor_01/data"     # ESP32 publishes readings here

SEND_INTERVAL = 120   # seconds between "send" commands
DB_PATH       = "soil_readings.db"

print(f"Broker: {BROKER}:{PORT}  user: {MQTT_USER}")


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

con = db_init(DB_PATH)
print(f"DB ready: {DB_PATH}")


# ── MQTT client ───────────────────────────────────────────────────────────────
import json
import threading
import ssl
from datetime import datetime
import paho.mqtt.client as mqtt

_send_timer: threading.Timer | None = None

def schedule_send(client: mqtt.Client):
    """Publish 'send' now, then reschedule for SEND_INTERVAL seconds later."""
    global _send_timer
    now = datetime.now().strftime("%H:%M:%S")
    result = client.publish(TOPIC_COMMAND, "send", qos=1, retain=True)
    print(f"[{now}] → Published 'send' to {TOPIC_COMMAND}  (mid={result.mid})")
    _send_timer = threading.Timer(SEND_INTERVAL, schedule_send, args=[client])
    _send_timer.daemon = True
    _send_timer.start()


def on_connect(client, userdata, flags, reason_code, properties):
    if reason_code == 0:
        print(f"Connected to {BROKER}")
        client.subscribe(TOPIC_DATA, qos=1)
        print(f"Subscribed to {TOPIC_DATA}")
        # Fire the first 'send' immediately, then every SEND_INTERVAL seconds
        schedule_send(client)
    else:
        print(f"Connection failed: {reason_code}")


def on_message(client, userdata, msg):
    raw_str = msg.payload.decode()
    now     = datetime.now().strftime("%H:%M:%S")
    try:
        data    = json.loads(raw_str)
        date    = data.get("date", datetime.now().strftime("%Y-%m-%d"))
        time_s  = data.get("time", now)
        reading = int(data["raw"])
        db_insert(userdata["con"], date, time_s, reading)
        print(f"[{now}] ← Saved: date={date}  time={time_s}  raw={reading}")
    except (json.JSONDecodeError, KeyError) as e:
        print(f"[{now}] ← Bad payload ({e}): {raw_str}")


def on_disconnect(client, userdata, flags, reason_code, properties):
    if _send_timer:
        _send_timer.cancel()
    if reason_code != 0:
        print(f"Unexpected disconnect (rc={reason_code}), paho will retry…")


client = mqtt.Client(
    mqtt.CallbackAPIVersion.VERSION2,
    client_id=CLIENT_ID,
    clean_session=True,
)
client.username_pw_set(MQTT_USER, MQTT_PASS)
client.tls_set(tls_version=ssl.PROTOCOL_TLS_CLIENT)  # verifies broker cert
client.on_connect    = on_connect
client.on_message    = on_message
client.on_disconnect = on_disconnect
client.user_data_set({"con": con})

print("MQTT client configured.")

# ── Connect and run ───────────────────────────────────────────────────────────
# Blocks until you interrupt the kernel (Ctrl+C or ■ Stop button)
try:
    client.connect(BROKER, PORT, keepalive=60)
    client.loop_forever(retry_first_connection=True)
except KeyboardInterrupt:
    print("\nStopped by user.")
finally:
    if _send_timer:
        _send_timer.cancel()
    client.disconnect()
    con.close()
    print("Disconnected. DB closed.")