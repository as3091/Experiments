"""
central_server.py  —  Soil-sensor central server

Connects to the same MQTT broker as the ESP32, periodically commands the
sensor to publish its data, persists each reading in a local SQLite DB,
verifies the inserts, and ACKs each block so the sensor can free its flash.

Protocol (matches publisher.cpp / subscriber.cpp on the ESP32):
  Server  → TOPIC_COMMAND : "send"
  ESP32   → TOPIC_DATA    : "<start_dt>|<interval_sec>|<r1>,<r2>,..."
                            (multiple payloads possible per cycle)
  Server  → TOPIC_COMMAND : "ack <start_dt>|<interval_sec>|<end_dt>"

Timestamp format throughout: YYYYMMDDHHmmss  (14 chars, no separators)
"""

from __future__ import annotations

import logging
import ssl
import sqlite3
import threading
from datetime import datetime, timedelta
from pathlib import Path

import paho.mqtt.client as mqtt
from dotenv import dotenv_values

# ── Config ────────────────────────────────────────────────────────────────────

_env = dotenv_values(Path(__file__).parent.parent / ".env")

BROKER        = _env["MQTT_HOST"]
PORT          = int(_env["MQTT_PORT"])
MQTT_USER     = _env["MQTT_SUB_USER"]
MQTT_PASS     = _env["MQTT_SUB_PASS"]
CLIENT_ID     = _env["SUB_CLIENT_ID"]
TOPIC_COMMAND = _env["TOPIC_COMMAND"]
TOPIC_DATA    = _env["TOPIC_DATA"]
SEND_INTERVAL = int(_env["LISTEN_SLEEP_MIN"]) * 60  # convert minutes → seconds
DB_PATH       = _env["Server_DB_PATH"]

# How long to wait after the last incoming message before processing the batch.
# Should be long enough for all back-to-back payloads to arrive.
QUIET_TIMEOUT = 5.0  # seconds

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s  %(levelname)-7s  %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger(__name__)

# ── DB ────────────────────────────────────────────────────────────────────────

def db_init(path: str) -> sqlite3.Connection:
    con = sqlite3.connect(path, check_same_thread=False)
    con.execute("""
        CREATE TABLE IF NOT EXISTS readings (
            date_time   TEXT    PRIMARY KEY,
            reading     INTEGER NOT NULL,
            received_at TEXT    DEFAULT (datetime('now'))
        )
    """)
    con.commit()
    log.info("DB ready: %s", path)
    return con


def _timestamps_for_block(start_dt: str, interval_sec: int, count: int) -> list[str]:
    """Expand a delta-encoded block back into its individual timestamps."""
    fmt = "%Y%m%d%H%M%S"
    t0  = datetime.strptime(start_dt, fmt)
    return [
        (t0 + timedelta(seconds=i * interval_sec)).strftime(fmt)
        for i in range(count)
    ]


def db_insert_block(
    con: sqlite3.Connection,
    start_dt: str,
    interval_sec: int,
    readings: list[int],
) -> int:
    """
    Insert one decoded block into the DB using INSERT OR IGNORE (idempotent).
    Returns how many of the block's timestamps are now present in the DB.
    """
    timestamps = _timestamps_for_block(start_dt, interval_sec, len(readings))
    con.executemany(
        "INSERT OR IGNORE INTO readings (date_time, reading) VALUES (?, ?)",
        zip(timestamps, readings),
    )
    con.commit()

    placeholders = ",".join("?" * len(timestamps))
    cur = con.execute(
        f"SELECT COUNT(*) FROM readings WHERE date_time IN ({placeholders})",
        timestamps,
    )
    return cur.fetchone()[0]


def db_verify_block(
    con: sqlite3.Connection,
    start_dt: str,
    interval_sec: int,
    readings: list[int],
) -> bool:
    """Return True only if every timestamp in the block exists in the DB."""
    return db_insert_block(con, start_dt, interval_sec, readings) == len(readings)


# ── MQTT server ───────────────────────────────────────────────────────────────

class SoilServer:
    def __init__(self, con: sqlite3.Connection) -> None:
        self._con   = con
        self._lock  = threading.Lock()

        # Accumulates (start_dt, interval_sec, readings) tuples between batches
        self._pending: list[tuple[str, int, list[int]]] = []

        self._quiet_timer: threading.Timer | None = None
        self._send_timer:  threading.Timer | None = None

        self.client = mqtt.Client(
            mqtt.CallbackAPIVersion.VERSION2,
            client_id=CLIENT_ID,
            clean_session=True,
        )
        self.client.username_pw_set(MQTT_USER, MQTT_PASS)
        self.client.tls_set(tls_version=ssl.PROTOCOL_TLS_CLIENT)
        self.client.on_connect    = self._on_connect
        self.client.on_message    = self._on_message
        self.client.on_disconnect = self._on_disconnect

    # ── lifecycle ─────────────────────────────────────────────────────────────

    def start(self) -> None:
        log.info(
            "Connecting to %s:%d  (send interval: %ds, quiet timeout: %.1fs)",
            BROKER, PORT, SEND_INTERVAL, QUIET_TIMEOUT,
        )
        self.client.connect(BROKER, PORT, keepalive=60)
        self.client.loop_forever(retry_first_connection=True)

    def stop(self) -> None:
        self._cancel_timer(self._quiet_timer)
        self._cancel_timer(self._send_timer)
        self.client.disconnect()

    # ── connection callbacks ──────────────────────────────────────────────────

    def _on_connect(self, client, userdata, flags, rc, props) -> None:
        if rc != 0:
            log.error("Connection failed (rc=%s)", rc)
            return
        log.info("Connected to %s:%d", BROKER, PORT)
        client.subscribe(TOPIC_DATA, qos=1)
        log.info("Subscribed to %s", TOPIC_DATA)
        # Send the first "send" immediately, then on the regular schedule
        self._arm_send(delay=0)

    def _on_disconnect(self, client, userdata, flags, rc, props) -> None:
        log.warning("Disconnected (rc=%s) — paho will retry", rc)
        self._cancel_timer(self._quiet_timer)

    # ── periodic "send" command ───────────────────────────────────────────────

    def _arm_send(self, delay: float = SEND_INTERVAL) -> None:
        self._cancel_timer(self._send_timer)
        self._send_timer = threading.Timer(delay, self._do_send)
        self._send_timer.daemon = True
        self._send_timer.start()

    def _do_send(self) -> None:
        log.info("→ [%s]  Publishing 'send' to %s", _now(), TOPIC_COMMAND)
        self.client.publish(TOPIC_COMMAND, "send", qos=1)
        self._arm_send()   # schedule the next cycle

    # ── incoming data ─────────────────────────────────────────────────────────

    def _on_message(self, client, userdata, msg) -> None:
        raw = msg.payload.decode().strip()
        log.debug("← %s : %s", msg.topic, raw[:100])

        parts = raw.split("|", 2)
        if len(parts) != 3:
            log.warning("Malformed payload (need 3 pipe-fields): %r", raw[:100])
            return

        start_dt, interval_str, readings_str = parts
        try:
            interval_sec = int(interval_str)
            readings     = [int(v) for v in readings_str.split(",") if v]
        except ValueError:
            log.warning("Could not parse values in payload: %r", raw[:100])
            return

        log.info(
            "← Block  start=%s  interval=%ds  readings=%d",
            start_dt, interval_sec, len(readings),
        )
        with self._lock:
            self._pending.append((start_dt, interval_sec, readings))

        # Reset the quiet timer; process the batch once traffic goes silent
        self._cancel_timer(self._quiet_timer)
        self._quiet_timer = threading.Timer(QUIET_TIMEOUT, self._process_batch)
        self._quiet_timer.daemon = True
        self._quiet_timer.start()

    # ── insert → verify → ack ─────────────────────────────────────────────────

    def _process_batch(self) -> None:
        with self._lock:
            batch, self._pending = self._pending, []

        if not batch:
            return

        log.info("Processing batch: %d block(s)", len(batch))
        acks_to_send: list[str] = []

        for start_dt, interval_sec, readings in batch:
            timestamps = _timestamps_for_block(start_dt, interval_sec, len(readings))
            end_dt     = timestamps[-1]

            # 1. Insert
            found = db_insert_block(self._con, start_dt, interval_sec, readings)
            log.info(
                "  Insert  %s|%d|%s  expected=%d  in_db=%d",
                start_dt, interval_sec, end_dt, len(readings), found,
            )

            # 2. Verify
            if found != len(readings):
                log.error(
                    "  Verification FAILED for block %s — only %d/%d rows present, skipping ACK",
                    start_dt, found, len(readings),
                )
                continue

            # 3. Queue ACK
            acks_to_send.append(f"{start_dt}|{interval_sec}|{end_dt}")

        # Publish all ACKs after the loop so we don't interleave with processing
        for ack_payload in acks_to_send:
            cmd = f"ack {ack_payload}"
            self.client.publish(TOPIC_COMMAND, cmd, qos=1)
            log.info("→ ACK  %s", ack_payload)

    # ── helpers ───────────────────────────────────────────────────────────────

    @staticmethod
    def _cancel_timer(t: threading.Timer | None) -> None:
        if t is not None:
            t.cancel()


def _now() -> str:
    return datetime.now().strftime("%H:%M:%S")


# ── Entry point ───────────────────────────────────────────────────────────────

if __name__ == "__main__":
    con    = db_init(DB_PATH)
    server = SoilServer(con)
    try:
        server.start()
    except KeyboardInterrupt:
        log.info("Stopped by user.")
    finally:
        server.stop()
        con.close()
        log.info("DB closed.")
