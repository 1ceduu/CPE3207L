#!/usr/bin/env python3
"""
===================================================================================
CPE3207L Laptop Kiosk Bridge & Dashboard Runner (With SQLite & Dynamic Multi-Pen)
===================================================================================
Connects your ESP32 directly to your Laptop via USB Serial (115200 baud),
maintains persistent SQLite storage (piggery_monitor.db), and serves
the dynamic Kiosk Dashboard UI locally at http://localhost:8000.

Features:
  - SQLite persistent database for telemetry, schedules, thresholds, alerts, overrides
  - Dynamic discovery & real-time tracking of connected Feeder Nodes (Pens)
  - Real-time Screen 1 and Screen 2 synchronization
  - Direct interactive terminal command prompt
===================================================================================
"""

import os
import sys
import json
import time
import sqlite3
import threading
import webbrowser
from http.server import HTTPServer, SimpleHTTPRequestHandler
import serial
import serial.tools.list_ports

PORT = 8000
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
WEBSITE_DIR = os.path.join(BASE_DIR, "WebsitePrototype")
DB_PATH = os.path.join(BASE_DIR, "piggery_monitor.db")

# ─────────────────────────────────────────────────────────────────────────────
# SQLite Database Layer
# ─────────────────────────────────────────────────────────────────────────────

def get_db():
    conn = sqlite3.connect(DB_PATH, check_same_thread=False)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA journal_mode=WAL")
    return conn

def init_db():
    """Initializes the database schema and seeds default data if empty."""
    with get_db() as conn:
        conn.executescript("""
        CREATE TABLE IF NOT EXISTS pen_config (
            pen_id                   INTEGER PRIMARY KEY,
            name                     TEXT NOT NULL,
            water_refill_threshold   INTEGER NOT NULL DEFAULT 45,
            is_connected             INTEGER NOT NULL DEFAULT 0,
            last_seen                TEXT
        );

        CREATE TABLE IF NOT EXISTS telemetry (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            pen_id        INTEGER NOT NULL,
            weight_g      REAL NOT NULL,
            water_pct     REAL NOT NULL,
            nh3_ppm       REAL NOT NULL,
            timestamp     TEXT NOT NULL DEFAULT (datetime('now', 'localtime'))
        );

        CREATE TABLE IF NOT EXISTS feeding_schedules (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            pen_id        INTEGER NOT NULL,
            label         TEXT NOT NULL,
            time_hour     INTEGER NOT NULL,
            time_minute   INTEGER NOT NULL,
            time_period   TEXT NOT NULL DEFAULT 'AM',
            amount_kg     REAL NOT NULL,
            enabled       INTEGER NOT NULL DEFAULT 1,
            UNIQUE(pen_id, label)
        );

        CREATE TABLE IF NOT EXISTS alerts (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            pen_id        INTEGER NOT NULL,
            alert_type    TEXT NOT NULL,
            severity      TEXT NOT NULL,
            message       TEXT NOT NULL,
            timestamp     TEXT NOT NULL DEFAULT (datetime('now', 'localtime'))
        );

        CREATE TABLE IF NOT EXISTS manual_overrides (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            pen_id        INTEGER NOT NULL,
            override_type TEXT NOT NULL,
            target_value  REAL NOT NULL,
            actual_value  REAL,
            timestamp     TEXT NOT NULL DEFAULT (datetime('now', 'localtime'))
        );
        """)

        # Ensure Pen 1 default configuration exists
        conn.execute("""
            INSERT OR IGNORE INTO pen_config (pen_id, name, water_refill_threshold, is_connected)
            VALUES (1, 'Pen 01', 45, 0)
        """)

        # Seed default feeding schedules for Pen 1 if empty
        conn.execute("""
            INSERT OR IGNORE INTO feeding_schedules (pen_id, label, time_hour, time_minute, time_period, amount_kg)
            VALUES (1, 'Morning Cycle', 6, 0, 'AM', 2.0)
        """)
        conn.execute("""
            INSERT OR IGNORE INTO feeding_schedules (pen_id, label, time_hour, time_minute, time_period, amount_kg)
            VALUES (1, 'Midday Cycle', 12, 0, 'PM', 1.5)
        """)
        conn.commit()

init_db()

# ─────────────────────────────────────────────────────────────────────────────
# In-Memory Real-Time State & Multi-Pen Tracking
# ─────────────────────────────────────────────────────────────────────────────

state_lock = threading.Lock()
serial_conn = None
last_db_log_time = {}

# Active pens dictionary: pen_id -> dict
active_pens = {}

def get_or_create_pen_state(pen_id):
    """Retrieves or loads pen state from SQLite with initial values."""
    if pen_id in active_pens:
        return active_pens[pen_id]

    with get_db() as conn:
        row = conn.execute("SELECT * FROM pen_config WHERE pen_id = ?", (pen_id,)).fetchone()
        if not row:
            name = f"Pen {pen_id:02d}"
            conn.execute("INSERT INTO pen_config (pen_id, name, water_refill_threshold, is_connected) VALUES (?, ?, 45, 1)",
                         (pen_id, name))
            conn.execute("INSERT OR IGNORE INTO feeding_schedules (pen_id, label, time_hour, time_minute, time_period, amount_kg) VALUES (?, 'Morning Cycle', 6, 0, 'AM', 2.0)", (pen_id,))
            conn.execute("INSERT OR IGNORE INTO feeding_schedules (pen_id, label, time_hour, time_minute, time_period, amount_kg) VALUES (?, 'Midday Cycle', 12, 0, 'PM', 1.5)", (pen_id,))
            conn.commit()
            threshold = 45
        else:
            name = row["name"]
            threshold = row["water_refill_threshold"]

        # Fetch schedules
        sched_rows = conn.execute("SELECT * FROM feeding_schedules WHERE pen_id = ?", (pen_id,)).fetchall()
        schedules = []
        settings = {"mHour": 6, "mMin": 0, "mPer": "AM", "mAmount": 2.0, "aHour": 12, "aMin": 0, "aPer": "PM", "aAmount": 1.5}
        for s in sched_rows:
            schedules.append({
                "label": s["label"],
                "time": f"{s['time_hour']:02d}:{s['time_minute']:02d} {s['time_period']}",
                "amount": f"{s['amount_kg']:.1f} kg"
            })
            if "morning" in s["label"].lower():
                settings["mHour"] = s["time_hour"]
                settings["mMin"] = s["time_minute"]
                settings["mPer"] = s["time_period"]
                settings["mAmount"] = s["amount_kg"]
            elif "midday" in s["label"].lower() or "afternoon" in s["label"].lower():
                settings["aHour"] = s["time_hour"]
                settings["aMin"] = s["time_minute"]
                settings["aPer"] = s["time_period"]
                settings["aAmount"] = s["amount_kg"]

        # Fetch alerts
        alert_rows = conn.execute("SELECT * FROM alerts WHERE pen_id = ? ORDER BY id DESC LIMIT 10", (pen_id,)).fetchall()
        alerts = []
        for a in alert_rows:
            alerts.append({
                "time": a["timestamp"].split(" ")[-1] if " " in a["timestamp"] else a["timestamp"],
                "type": a["severity"],
                "text": a["message"]
            })
        if not alerts:
            alerts = [
                {"time": time.strftime("%I:%M %p"), "type": "status", "text": "Feeder Node online — all sensors active"}
            ]

    pen_state = {
        "pen_id": pen_id,
        "name": name,
        "feedRemaining": 0.0,
        "waterLevel": 0,
        "nh3": 0.0,
        "status": "Connecting...",
        "statusClass": "normal",
        "cardClass": "",
        "refillThreshold": threshold,
        "schedule": schedules if schedules else [
            {"label": "Morning Cycle", "time": "06:00 AM", "amount": "2.0 kg"},
            {"label": "Midday Cycle", "time": "12:00 PM", "amount": "1.5 kg"}
        ],
        "settings": settings,
        "alerts": alerts,
        "history": [28, 30, 32, 29, 31, 33, 30],
        "dangerZone": [20, 20, 20, 20, 20, 20, 20],
        "last_seen": time.time(),
        "is_connected": True
    }
    active_pens[pen_id] = pen_state
    return pen_state


def add_alert_to_pen(pen_id, alert_type, text):
    """Adds an alert to in-memory state and persists to SQLite."""
    timestamp_str = time.strftime("%Y-%m-%d %H:%M:%S")
    time_display = time.strftime("%I:%M %p")
    with get_db() as conn:
        conn.execute("INSERT INTO alerts (pen_id, alert_type, severity, message, timestamp) VALUES (?, ?, ?, ?, ?)",
                     (pen_id, alert_type, alert_type, text, timestamp_str))
        conn.commit()

    if pen_id in active_pens:
        pen = active_pens[pen_id]
        # Avoid duplicate consecutive alerts
        if not pen["alerts"] or pen["alerts"][0]["text"] != text:
            pen["alerts"].insert(0, {
                "time": time_display,
                "type": alert_type,
                "text": text
            })
            if len(pen["alerts"]) > 20:
                pen["alerts"].pop()


def auto_detect_esp32_port():
    """Find the most likely ESP32 COM port."""
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        desc = p.description.lower()
        if "cp210" in desc or "ch340" in desc or "ch910" in desc or "usb-serial" in desc or "uart" in desc:
            return p.device
    if ports:
        return ports[-1].device
    return "COM7"


def send_esp32_command(cmd_str):
    """Send a command string to the ESP32 over USB Serial."""
    global serial_conn
    if serial_conn and serial_conn.is_open:
        try:
            payload = (cmd_str.strip() + "\n").encode('utf-8')
            serial_conn.write(payload)
            serial_conn.flush()
            print(f"\n>>> [SENT TO ESP32] {cmd_str.strip()}")

            # Log manual override in SQLite if applicable
            upper = cmd_str.strip().upper()
            if upper.startswith("D "):
                try:
                    grams = float(upper.split(" ")[1])
                    with get_db() as conn:
                        conn.execute("INSERT INTO manual_overrides (pen_id, override_type, target_value) VALUES (1, 'feed', ?)", (grams / 1000.0,))
                        conn.commit()
                except Exception:
                    pass
            elif upper in ["REFILL", "W FILL", "WATER REFILL", "FILL"]:
                with get_db() as conn:
                    conn.execute("INSERT INTO manual_overrides (pen_id, override_type, target_value) VALUES (1, 'water', 100.0)", ())
                    conn.commit()

            return True
        except Exception as e:
            print(f"\n[ERROR] Failed to send command: {e}")
            return False
    else:
        print("\n[ERROR] Serial port is not connected!")
        return False


def serial_reader_thread(com_port):
    """Background thread reading telemetry lines from ESP32 over USB and updating SQLite."""
    global serial_conn
    baud = 115200

    print(f"\n[SERIAL] Connecting to ESP32 on {com_port} at {baud} baud...")
    while True:
        try:
            ser = serial.Serial(com_port, baud, timeout=1.0)
            serial_conn = ser
            print(f"[SERIAL] Connected successfully to {com_port}!")
            print("[INFO] You can type commands here at any time (e.g. 'D 100', 'REFILL', 'THRESH 45', 'STATUS'):\n")

            while True:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if not line:
                    # Check for pen disconnection timeout (12 seconds)
                    now = time.time()
                    with state_lock:
                        for pid, pen in list(active_pens.items()):
                            if now - pen["last_seen"] > 12:
                                pen["is_connected"] = False
                                pen["status"] = "Disconnected"
                                pen["statusClass"] = "critical"
                    continue

                # Parse JSON telemetry from ESP32
                if line.startswith('{"telemetry":') or (line.startswith('{') and 'weight_g' in line):
                    try:
                        data = json.loads(line)
                        t = data.get("telemetry", data)
                        pen_id = int(t.get("pen_id", 1))

                        weight_g = float(t.get("weight_g", 0.0))
                        water_pct = int(float(t.get("water_pct", 0.0)))
                        nh3_ppm = float(t.get("nh3_ppm", 0.0))

                        # Determine status classification
                        status_str = "Normal"
                        status_class = "normal"
                        card_class = ""

                        if water_pct < 30 or nh3_ppm >= 25.0:
                            status_str = "Critical"
                            status_class = "critical"
                            card_class = "critical"
                        elif water_pct < 45 or nh3_ppm >= 20.0:
                            status_str = "Warning"
                            status_class = "warning"
                            card_class = "warning"

                        with state_lock:
                            pen = get_or_create_pen_state(pen_id)
                            pen["feedRemaining"] = round(weight_g / 1000.0, 2)
                            pen["waterLevel"] = water_pct
                            pen["nh3"] = round(nh3_ppm, 1)
                            pen["status"] = status_str
                            pen["statusClass"] = status_class
                            pen["cardClass"] = card_class
                            pen["last_seen"] = time.time()
                            pen["is_connected"] = True

                            # Alert generation logic
                            if water_pct < 30:
                                add_alert_to_pen(pen_id, "critical", f"Critical low water level ({water_pct}%) detected!")
                            elif water_pct < pen["refillThreshold"]:
                                add_alert_to_pen(pen_id, "warning", f"Water level ({water_pct}%) below refill threshold ({pen['refillThreshold']}%)")

                            if nh3_ppm >= 25.0:
                                add_alert_to_pen(pen_id, "critical", f"High Ammonia ({nh3_ppm:.1f} ppm) exceeded safe threshold (25 ppm)!")
                            elif nh3_ppm >= 20.0:
                                add_alert_to_pen(pen_id, "warning", f"Ammonia level ({nh3_ppm:.1f} ppm) approaching critical limit")

                            # Periodic SQLite telemetry persistence (every 3 seconds per pen)
                            now_sec = time.time()
                            if now_sec - last_db_log_time.get(pen_id, 0) >= 3.0:
                                last_db_log_time[pen_id] = now_sec
                                try:
                                    with get_db() as conn:
                                        conn.execute(
                                            "INSERT INTO telemetry (pen_id, weight_g, water_pct, nh3_ppm) VALUES (?, ?, ?, ?)",
                                            (pen_id, weight_g, water_pct, nh3_ppm)
                                        )
                                        conn.execute(
                                            "UPDATE pen_config SET is_connected = 1, last_seen = datetime('now','localtime') WHERE pen_id = ?",
                                            (pen_id,)
                                        )
                                        conn.commit()
                                except Exception:
                                    pass

                    except Exception:
                        pass
                else:
                    # Print raw hardware messages (actuators, auto-refill triggers, logs)
                    print(f"[ESP32] {line}")

        except serial.SerialException as e:
            print(f"[SERIAL WARN] {com_port} disconnected or unavailable. Retrying in 2s...")
            with state_lock:
                for pid, pen in active_pens.items():
                    pen["is_connected"] = False
                    pen["status"] = "Disconnected"
                    pen["statusClass"] = "critical"
            time.sleep(2.0)
        except Exception as e:
            print(f"[SERIAL ERROR] {e}")
            time.sleep(2.0)


def terminal_input_thread():
    """Allows typing commands directly into the terminal window."""
    time.sleep(1.5)
    while True:
        try:
            cmd = input().strip()
            if cmd:
                send_esp32_command(cmd)
        except (EOFError, KeyboardInterrupt):
            break
        except Exception:
            pass


# ─────────────────────────────────────────────────────────────────────────────
# HTTP REST API & Static File Server
# ─────────────────────────────────────────────────────────────────────────────

class KioskRequestHandler(SimpleHTTPRequestHandler):
    """Serves the WebsitePrototype frontend and full REST API endpoints."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=WEBSITE_DIR, **kwargs)

    def do_GET(self):
        # 1. Full Multi-Pen State & Discovery Endpoint
        if self.path == '/api/pens':
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.send_header('Cache-Control', 'no-cache, no-store, must-revalidate')
            self.end_headers()

            with state_lock:
                # Filter only currently connected pens (or recently seen)
                connected = {str(k): v for k, v in active_pens.items() if v.get("is_connected", False)}
                payload = json.dumps({
                    "pens": connected,
                    "connected_count": len(connected)
                })
            self.wfile.write(payload.encode('utf-8'))

        # 2. Fast 1-second Telemetry Endpoint
        elif self.path == '/api/telemetry':
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.send_header('Cache-Control', 'no-cache, no-store, must-revalidate')
            self.end_headers()

            with state_lock:
                connected = {str(k): v for k, v in active_pens.items() if v.get("is_connected", False)}
                primary = active_pens.get(1, {
                    "pen_id": 1,
                    "feedRemaining": 0.0,
                    "waterLevel": 0,
                    "nh3": 0.0,
                    "status": "Searching for Feeder Nodes...",
                    "statusClass": "normal",
                    "last_updated": time.strftime("%H:%M:%S")
                })
                telemetry_payload = {
                    "pen_id": primary.get("pen_id", 1),
                    "feedRemaining": primary.get("feedRemaining", 0.0),
                    "waterLevel": primary.get("waterLevel", 0),
                    "nh3": primary.get("nh3", 0.0),
                    "status": primary.get("status", "Normal"),
                    "statusClass": primary.get("statusClass", "normal"),
                    "last_updated": time.strftime("%H:%M:%S"),
                    "connected_pens": list(connected.keys()),
                    "pens": connected
                }
                payload = json.dumps(telemetry_payload)
            self.wfile.write(payload.encode('utf-8'))

        # 3. Static Web Assets
        else:
            self.send_response(200)
            self.send_header('Cache-Control', 'no-cache, no-store, must-revalidate')
            super().do_GET()

    def do_POST(self):
        content_len = int(self.headers.get('Content-Length', 0))
        post_body = self.rfile.read(content_len).decode('utf-8') if content_len > 0 else "{}"
        try:
            body = json.loads(post_body)
        except Exception:
            body = {}

        # 1. Update Feeding Program Schedule: POST /api/pens/<id>/schedule
        if '/schedule' in self.path:
            parts = self.path.strip('/').split('/')
            pen_id = int(parts[2]) if len(parts) >= 3 and parts[2].isdigit() else 1
            settings = body.get("settings", {})
            if settings:
                m_hour = settings.get("mHour", 6)
                m_min = settings.get("mMin", 0)
                m_per = settings.get("mPer", "AM")
                m_amt = float(settings.get("mAmount", 2.0))

                a_hour = settings.get("aHour", 12)
                a_min = settings.get("aMin", 0)
                a_per = settings.get("aPer", "PM")
                a_amt = float(settings.get("aAmount", 1.5))

                with get_db() as conn:
                    conn.execute("""
                        INSERT INTO feeding_schedules (pen_id, label, time_hour, time_minute, time_period, amount_kg)
                        VALUES (?, 'Morning Cycle', ?, ?, ?, ?)
                        ON CONFLICT(pen_id, label) DO UPDATE SET
                            time_hour = excluded.time_hour,
                            time_minute = excluded.time_minute,
                            time_period = excluded.time_period,
                            amount_kg = excluded.amount_kg
                    """, (pen_id, m_hour, m_min, m_per, m_amt))

                    conn.execute("""
                        INSERT INTO feeding_schedules (pen_id, label, time_hour, time_minute, time_period, amount_kg)
                        VALUES (?, 'Midday Cycle', ?, ?, ?, ?)
                        ON CONFLICT(pen_id, label) DO UPDATE SET
                            time_hour = excluded.time_hour,
                            time_minute = excluded.time_minute,
                            time_period = excluded.time_period,
                            amount_kg = excluded.amount_kg
                    """, (pen_id, a_hour, a_min, a_per, a_amt))
                    conn.commit()

                with state_lock:
                    if pen_id in active_pens:
                        active_pens[pen_id]["settings"] = settings
                        active_pens[pen_id]["schedule"] = [
                            {"label": "Morning Cycle", "time": f"{m_hour:02d}:{m_min:02d} {m_per}", "amount": f"{m_amt:.1f} kg"},
                            {"label": "Midday Cycle", "time": f"{a_hour:02d}:{a_min:02d} {a_per}", "amount": f"{a_amt:.1f} kg"}
                        ]
                print(f"[DB] Feeding schedule for Pen {pen_id:02d} saved to SQLite: Morning={m_amt}kg, Midday={a_amt}kg")

            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(b'{"status":"saved"}')

        # 2. Update Water Refill Threshold: POST /api/pens/<id>/threshold
        elif '/threshold' in self.path:
            parts = self.path.strip('/').split('/')
            pen_id = int(parts[2]) if len(parts) >= 3 and parts[2].isdigit() else 1
            threshold = int(body.get("threshold", 45))

            with get_db() as conn:
                conn.execute("UPDATE pen_config SET water_refill_threshold = ? WHERE pen_id = ?", (threshold, pen_id))
                conn.commit()

            with state_lock:
                if pen_id in active_pens:
                    active_pens[pen_id]["refillThreshold"] = threshold

            # Dispatch threshold configuration to ESP32 over serial
            send_esp32_command(f"THRESH {threshold}")
            print(f"[DB] Water refill threshold for Pen {pen_id:02d} saved to SQLite: {threshold}%")

            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(b'{"status":"saved"}')

        # 3. Direct Actuator Hardware Command: POST /api/command
        elif self.path.startswith('/api/command'):
            command_str = body.get("command", "")
            if command_str:
                print(f"\n[DASHBOARD BUTTON CLICK] Sending command: {command_str}")
                send_esp32_command(command_str)

            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(b'{"status":"ok"}')
        else:
            self.send_error(404)

    def log_message(self, format, *args):
        # Suppress repetitive GET polling logs from cluttering terminal
        msg = args[0] if args else ""
        if "GET /api/telemetry" in msg or "GET /api/pens" in msg:
            return
        super().log_message(format, *args)


def main():
    if len(sys.argv) > 1:
        target_port = sys.argv[1]
    else:
        target_port = auto_detect_esp32_port()

    print("==================================================================")
    print("   CPE3207L Swine Feeder Kiosk Dashboard (Laptop Runner)         ")
    print("==================================================================")
    print(f" Web Dashboard URL : http://localhost:{PORT}")
    print(f" Target Serial Port: {target_port}")
    print(f" SQLite Database   : {DB_PATH}")
    print("==================================================================")
    print(" QUICK TERMINAL COMMANDS (Type anytime):")
    print("   D 100     -> Dispense 100g Feed (Gate -> Stepper -> Gate Close)")
    print("   REFILL    -> Closed-Loop Refill to 100% (Closes when full)")
    print("   THRESH 45 -> Set auto-refill threshold % (e.g. 45%)")
    print("   AUTO ON   -> Enable automated background refilling")
    print("   AUTO OFF  -> Disable automated background refilling")
    print("   1 / 0     -> Open / Close Feed Gate Solenoid")
    print("   W 1/0     -> Open / Close Water Solenoid Valve")
    print("   S 800     -> Step Auger Stepper 800 steps")
    print("   STATUS    -> Print Hardware States")
    print("==================================================================\n")

    # Start Serial Background Thread
    ser_thread = threading.Thread(target=serial_reader_thread, args=(target_port,), daemon=True)
    ser_thread.start()

    # Start Terminal Input Thread
    input_thread = threading.Thread(target=terminal_input_thread, daemon=True)
    input_thread.start()

    # Automatically open browser after 1 second
    threading.Timer(1.2, lambda: webbrowser.open(f"http://localhost:{PORT}")).start()

    # Start HTTP Server
    server = HTTPServer(('0.0.0.0', PORT), KioskRequestHandler)
    print(f"[SERVER] Kiosk Web Server running at http://localhost:{PORT}/")
    print("[SERVER] Press Ctrl+C to stop.\n")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[SERVER] Stopping Kiosk Bridge Server. Goodbye!")
        server.server_close()


if __name__ == '__main__':
    main()
