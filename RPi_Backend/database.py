"""
Database Module — SQLite Schema & Data Access Layer
CPE3207L Thesis: Integrated Automated Feeding and Watering System

Manages piggery_monitor.db with 7 tables covering:
  - Live telemetry logging (batched writes)
  - Feeding events & FDA calculation
  - Feeding schedules (per-pen, configurable)
  - Per-pen configuration (water threshold, anomaly toggle)
  - Anomaly alerts (with SMS tracking)
  - System heartbeat (for uptime Usys metric)
  - Manual override events (RA0 traceability)
"""

import sqlite3
import threading
from datetime import datetime
from contextlib import contextmanager

from config import DB_PATH, NUM_PENS, DEFAULT_REFILL_THRESHOLD, DEFAULT_SCHEDULES

# ──────────────────────────────────────────────────────────────────────
# Thread-safe connection management
# ──────────────────────────────────────────────────────────────────────
_local = threading.local()


def _get_conn():
    """Get a thread-local SQLite connection with WAL mode for concurrent reads."""
    if not hasattr(_local, 'conn') or _local.conn is None:
        conn = sqlite3.connect(DB_PATH, check_same_thread=False)
        conn.row_factory = sqlite3.Row
        conn.execute("PRAGMA journal_mode=WAL")
        conn.execute("PRAGMA synchronous=NORMAL")
        _local.conn = conn
    return _local.conn


@contextmanager
def get_db():
    """Context manager yielding a thread-local connection with auto-commit."""
    conn = _get_conn()
    try:
        yield conn
        conn.commit()
    except Exception:
        conn.rollback()
        raise


# ──────────────────────────────────────────────────────────────────────
# Schema Creation
# ──────────────────────────────────────────────────────────────────────
_SCHEMA_SQL = """
-- Live sensor telemetry (logged every poll cycle, batched)
CREATE TABLE IF NOT EXISTS telemetry (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    pen_id        INTEGER NOT NULL,
    weight_g      REAL    NOT NULL,
    water_pct     REAL    NOT NULL,
    nh3_ppm       REAL    NOT NULL,
    timestamp     TEXT    NOT NULL DEFAULT (datetime('now','localtime'))
);

-- Feeding events (each scheduled or manual dispense)
CREATE TABLE IF NOT EXISTS feeding_events (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    pen_id        INTEGER NOT NULL,
    target_g      REAL    NOT NULL,
    actual_g      REAL,
    fda_pct       REAL,
    trigger_type  TEXT    NOT NULL,
    timestamp     TEXT    NOT NULL DEFAULT (datetime('now','localtime'))
);

-- Feeding schedules (per pen, persisted for Command Service)
CREATE TABLE IF NOT EXISTS feeding_schedules (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    pen_id        INTEGER NOT NULL,
    label         TEXT    NOT NULL,
    time_hour     INTEGER NOT NULL,
    time_minute   INTEGER NOT NULL,
    amount_kg     REAL    NOT NULL,
    enabled       INTEGER NOT NULL DEFAULT 1,
    UNIQUE(pen_id, label)
);

-- Per-pen configuration (water threshold, anomaly detection toggle)
CREATE TABLE IF NOT EXISTS pen_config (
    pen_id                   INTEGER PRIMARY KEY,
    water_refill_threshold   INTEGER NOT NULL DEFAULT 40,
    anomaly_detection_active INTEGER NOT NULL DEFAULT 0
);

-- Anomaly alerts
CREATE TABLE IF NOT EXISTS alerts (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    pen_id          INTEGER NOT NULL,
    alert_type      TEXT    NOT NULL,
    severity        TEXT    NOT NULL,
    message         TEXT    NOT NULL,
    baseline_value  REAL,
    threshold_value REAL,
    actual_value    REAL,
    acknowledged    INTEGER NOT NULL DEFAULT 0,
    sms_sent        INTEGER NOT NULL DEFAULT 0,
    timestamp       TEXT    NOT NULL DEFAULT (datetime('now','localtime'))
);

-- System heartbeat for uptime calculation (Usys metric)
CREATE TABLE IF NOT EXISTS heartbeat (
    id        INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);

-- Manual override events (RA0 traceability)
CREATE TABLE IF NOT EXISTS manual_overrides (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    pen_id        INTEGER NOT NULL,
    override_type TEXT    NOT NULL,
    target_value  REAL    NOT NULL,
    actual_value  REAL,
    timestamp     TEXT    NOT NULL DEFAULT (datetime('now','localtime'))
);

-- Performance indexes for common query patterns
CREATE INDEX IF NOT EXISTS idx_telemetry_pen_ts     ON telemetry(pen_id, timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_feeding_pen_ts       ON feeding_events(pen_id, timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_alerts_pen_ts        ON alerts(pen_id, timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_alerts_unacked       ON alerts(acknowledged, sms_sent);
CREATE INDEX IF NOT EXISTS idx_heartbeat_ts         ON heartbeat(timestamp DESC);
"""


def init_db():
    """Create all tables, indexes, and seed default data on first run."""
    with get_db() as conn:
        conn.executescript(_SCHEMA_SQL)

        # Seed pen_config for each pen if not exists
        for pen_id in range(1, NUM_PENS + 1):
            conn.execute(
                "INSERT OR IGNORE INTO pen_config (pen_id, water_refill_threshold, anomaly_detection_active) "
                "VALUES (?, ?, 0)",
                (pen_id, DEFAULT_REFILL_THRESHOLD)
            )

        # Seed default feeding schedules for each pen if not exists
        for pen_id in range(1, NUM_PENS + 1):
            for sched in DEFAULT_SCHEDULES:
                conn.execute(
                    "INSERT OR IGNORE INTO feeding_schedules "
                    "(pen_id, label, time_hour, time_minute, amount_kg) VALUES (?, ?, ?, ?, ?)",
                    (pen_id, sched['label'], sched['time_hour'],
                     sched['time_minute'], sched['amount_kg'])
                )

    print(f"[DB] Database initialized at {DB_PATH}")


# ──────────────────────────────────────────────────────────────────────
# Telemetry
# ──────────────────────────────────────────────────────────────────────
def log_telemetry(pen_id, weight_g, water_pct, nh3_ppm):
    """Insert a telemetry reading for a pen."""
    with get_db() as conn:
        conn.execute(
            "INSERT INTO telemetry (pen_id, weight_g, water_pct, nh3_ppm) VALUES (?, ?, ?, ?)",
            (pen_id, weight_g, water_pct, nh3_ppm)
        )


def log_telemetry_batch(readings):
    """Insert multiple telemetry readings at once. Each item: (pen_id, weight_g, water_pct, nh3_ppm)."""
    with get_db() as conn:
        conn.executemany(
            "INSERT INTO telemetry (pen_id, weight_g, water_pct, nh3_ppm) VALUES (?, ?, ?, ?)",
            readings
        )


def get_latest_telemetry(pen_id):
    """Get the most recent telemetry reading for a pen."""
    with get_db() as conn:
        row = conn.execute(
            "SELECT * FROM telemetry WHERE pen_id = ? ORDER BY timestamp DESC LIMIT 1",
            (pen_id,)
        ).fetchone()
    return dict(row) if row else None


def get_all_latest_telemetry():
    """Get the most recent telemetry for all pens."""
    result = {}
    for pen_id in range(1, NUM_PENS + 1):
        data = get_latest_telemetry(pen_id)
        if data:
            result[pen_id] = data
    return result


def get_telemetry_history(pen_id, hours=24):
    """Get telemetry history for a pen within the last N hours."""
    with get_db() as conn:
        rows = conn.execute(
            "SELECT * FROM telemetry WHERE pen_id = ? "
            "AND timestamp >= datetime('now', 'localtime', ?)"
            "ORDER BY timestamp ASC",
            (pen_id, f'-{hours} hours')
        ).fetchall()
    return [dict(r) for r in rows]


# ──────────────────────────────────────────────────────────────────────
# Feeding Events
# ──────────────────────────────────────────────────────────────────────
def log_feeding_event(pen_id, target_g, actual_g, trigger_type):
    """Log a completed feeding event with FDA calculation."""
    fda_pct = None
    if target_g and target_g > 0 and actual_g is not None:
        fda_pct = round((1.0 - abs(target_g - actual_g) / target_g) * 100.0, 2)

    with get_db() as conn:
        conn.execute(
            "INSERT INTO feeding_events (pen_id, target_g, actual_g, fda_pct, trigger_type) "
            "VALUES (?, ?, ?, ?, ?)",
            (pen_id, target_g, actual_g, fda_pct, trigger_type)
        )
    return fda_pct


def get_feeding_history(pen_id, n=14):
    """Get the last N completed feeding events for anomaly baseline."""
    with get_db() as conn:
        rows = conn.execute(
            "SELECT * FROM feeding_events WHERE pen_id = ? AND actual_g IS NOT NULL "
            "ORDER BY timestamp DESC LIMIT ?",
            (pen_id, n)
        ).fetchall()
    return [dict(r) for r in rows]


def get_feeding_events_range(pen_id, hours=168):
    """Get feeding events within a time range (default 7 days = 168 hours)."""
    with get_db() as conn:
        rows = conn.execute(
            "SELECT * FROM feeding_events WHERE pen_id = ? "
            "AND timestamp >= datetime('now', 'localtime', ?) "
            "ORDER BY timestamp ASC",
            (pen_id, f'-{hours} hours')
        ).fetchall()
    return [dict(r) for r in rows]


# ──────────────────────────────────────────────────────────────────────
# Feeding Schedules
# ──────────────────────────────────────────────────────────────────────
def get_feeding_schedule(pen_id):
    """Get all feeding schedule entries for a pen."""
    with get_db() as conn:
        rows = conn.execute(
            "SELECT * FROM feeding_schedules WHERE pen_id = ? ORDER BY time_hour, time_minute",
            (pen_id,)
        ).fetchall()
    return [dict(r) for r in rows]


def get_all_feeding_schedules():
    """Get feeding schedules for all pens."""
    result = {}
    for pen_id in range(1, NUM_PENS + 1):
        result[pen_id] = get_feeding_schedule(pen_id)
    return result


def update_feeding_schedule(pen_id, label, time_hour, time_minute, amount_kg):
    """Update or insert a feeding schedule entry."""
    with get_db() as conn:
        conn.execute(
            "INSERT INTO feeding_schedules (pen_id, label, time_hour, time_minute, amount_kg) "
            "VALUES (?, ?, ?, ?, ?) "
            "ON CONFLICT(pen_id, label) DO UPDATE SET "
            "time_hour = excluded.time_hour, time_minute = excluded.time_minute, "
            "amount_kg = excluded.amount_kg",
            (pen_id, label, time_hour, time_minute, amount_kg)
        )


def set_schedule_enabled(pen_id, label, enabled):
    """Enable or disable a schedule entry."""
    with get_db() as conn:
        conn.execute(
            "UPDATE feeding_schedules SET enabled = ? WHERE pen_id = ? AND label = ?",
            (1 if enabled else 0, pen_id, label)
        )


# ──────────────────────────────────────────────────────────────────────
# Pen Configuration
# ──────────────────────────────────────────────────────────────────────
def get_pen_config(pen_id):
    """Get configuration for a specific pen."""
    with get_db() as conn:
        row = conn.execute(
            "SELECT * FROM pen_config WHERE pen_id = ?", (pen_id,)
        ).fetchone()
    return dict(row) if row else None


def get_all_pen_configs():
    """Get configuration for all pens."""
    result = {}
    for pen_id in range(1, NUM_PENS + 1):
        cfg = get_pen_config(pen_id)
        if cfg:
            result[pen_id] = cfg
    return result


def update_water_threshold(pen_id, threshold_pct):
    """Update the water refill threshold for a pen (10–80%)."""
    threshold_pct = max(10, min(80, int(threshold_pct)))
    with get_db() as conn:
        conn.execute(
            "UPDATE pen_config SET water_refill_threshold = ? WHERE pen_id = ?",
            (threshold_pct, pen_id)
        )


def set_anomaly_detection_active(pen_id, active):
    """Enable or disable anomaly detection for a pen."""
    with get_db() as conn:
        conn.execute(
            "UPDATE pen_config SET anomaly_detection_active = ? WHERE pen_id = ?",
            (1 if active else 0, pen_id)
        )


# ──────────────────────────────────────────────────────────────────────
# Alerts
# ──────────────────────────────────────────────────────────────────────
def log_alert(pen_id, alert_type, severity, message,
              baseline_value=None, threshold_value=None, actual_value=None):
    """Log an anomaly alert."""
    with get_db() as conn:
        cursor = conn.execute(
            "INSERT INTO alerts (pen_id, alert_type, severity, message, "
            "baseline_value, threshold_value, actual_value) "
            "VALUES (?, ?, ?, ?, ?, ?, ?)",
            (pen_id, alert_type, severity, message,
             baseline_value, threshold_value, actual_value)
        )
    return cursor.lastrowid


def get_recent_alerts(pen_id, limit=20):
    """Get recent alerts for a pen."""
    with get_db() as conn:
        rows = conn.execute(
            "SELECT * FROM alerts WHERE pen_id = ? ORDER BY timestamp DESC LIMIT ?",
            (pen_id, limit)
        ).fetchall()
    return [dict(r) for r in rows]


def get_all_recent_alerts(limit=50):
    """Get recent alerts across all pens."""
    with get_db() as conn:
        rows = conn.execute(
            "SELECT * FROM alerts ORDER BY timestamp DESC LIMIT ?",
            (limit,)
        ).fetchall()
    return [dict(r) for r in rows]


def get_unacknowledged_alerts():
    """Get all unacknowledged alerts."""
    with get_db() as conn:
        rows = conn.execute(
            "SELECT * FROM alerts WHERE acknowledged = 0 ORDER BY timestamp DESC"
        ).fetchall()
    return [dict(r) for r in rows]


def acknowledge_alert(alert_id):
    """Mark an alert as acknowledged."""
    with get_db() as conn:
        conn.execute(
            "UPDATE alerts SET acknowledged = 1 WHERE id = ?", (alert_id,)
        )


def get_unsent_alerts():
    """Get alerts that have not been sent via SMS/LoRa."""
    with get_db() as conn:
        rows = conn.execute(
            "SELECT * FROM alerts WHERE sms_sent = 0 ORDER BY timestamp ASC"
        ).fetchall()
    return [dict(r) for r in rows]


def mark_alert_sms_sent(alert_id):
    """Mark an alert as sent via SMS/LoRa."""
    with get_db() as conn:
        conn.execute(
            "UPDATE alerts SET sms_sent = 1 WHERE id = ?", (alert_id,)
        )


# ──────────────────────────────────────────────────────────────────────
# Heartbeat (System Uptime Tracking)
# ──────────────────────────────────────────────────────────────────────
def log_heartbeat():
    """Write a heartbeat timestamp for uptime calculation."""
    with get_db() as conn:
        conn.execute("INSERT INTO heartbeat DEFAULT VALUES")


def get_heartbeat_history(hours=72):
    """Get heartbeat entries for uptime calculation."""
    with get_db() as conn:
        rows = conn.execute(
            "SELECT timestamp FROM heartbeat "
            "WHERE timestamp >= datetime('now', 'localtime', ?) "
            "ORDER BY timestamp ASC",
            (f'-{hours} hours',)
        ).fetchall()
    return [r['timestamp'] for r in rows]


def calculate_uptime(hours=72):
    """
    Calculate system uptime percentage over the given period.
    Downtime = sum of gaps between consecutive heartbeats that exceed HEARTBEAT_TIMEOUT_S.
    """
    from config import HEARTBEAT_TIMEOUT_S

    timestamps = get_heartbeat_history(hours)
    if len(timestamps) < 2:
        return None  # Not enough data to calculate

    total_downtime_s = 0.0
    fmt = "%Y-%m-%d %H:%M:%S"

    for i in range(1, len(timestamps)):
        t_prev = datetime.strptime(timestamps[i - 1], fmt)
        t_curr = datetime.strptime(timestamps[i], fmt)
        gap_s = (t_curr - t_prev).total_seconds()

        if gap_s > HEARTBEAT_TIMEOUT_S:
            total_downtime_s += gap_s

    total_period_s = hours * 3600.0
    uptime_pct = ((total_period_s - total_downtime_s) / total_period_s) * 100.0
    return round(max(0.0, min(100.0, uptime_pct)), 2)


# ──────────────────────────────────────────────────────────────────────
# Manual Overrides
# ──────────────────────────────────────────────────────────────────────
def log_manual_override(pen_id, override_type, target_value, actual_value=None):
    """Log a manual override event."""
    with get_db() as conn:
        conn.execute(
            "INSERT INTO manual_overrides (pen_id, override_type, target_value, actual_value) "
            "VALUES (?, ?, ?, ?)",
            (pen_id, override_type, target_value, actual_value)
        )


def get_manual_overrides(pen_id, limit=20):
    """Get recent manual overrides for a pen."""
    with get_db() as conn:
        rows = conn.execute(
            "SELECT * FROM manual_overrides WHERE pen_id = ? ORDER BY timestamp DESC LIMIT ?",
            (pen_id, limit)
        ).fetchall()
    return [dict(r) for r in rows]


# ──────────────────────────────────────────────────────────────────────
# Utility: Count feeding sessions (for silent monitoring period check)
# ──────────────────────────────────────────────────────────────────────
def count_feeding_sessions(pen_id):
    """Count total completed feeding sessions for a pen."""
    with get_db() as conn:
        row = conn.execute(
            "SELECT COUNT(*) as cnt FROM feeding_events "
            "WHERE pen_id = ? AND actual_g IS NOT NULL",
            (pen_id,)
        ).fetchone()
    return row['cnt'] if row else 0
