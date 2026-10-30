"""
Configuration Module — Central Source of Truth
CPE3207L Thesis: Integrated Automated Feeding and Watering System

All numeric thresholds, hardware constants, and MODBUS addresses are
defined here. Values must stay in sync with:
  - Section7.tex (thesis methodology)
  - WebsitePrototype/config.js (dashboard frontend)
  - Test_Firmware/ (ESP32 register maps)
"""

import os

# ──────────────────────────────────────────────────────────────────────
# System Layout
# ──────────────────────────────────────────────────────────────────────
NUM_PENS = 3

# ──────────────────────────────────────────────────────────────────────
# MODBUS RTU Addresses & Serial
# ──────────────────────────────────────────────────────────────────────
FEEDER_SLAVE_IDS   = [1, 2, 3]        # Feeder sensor nodes
ACTUATOR_SLAVE_ID  = 10               # Actuation node (DRV8825 + solenoids)

# Feeder node registers (FC 0x03 read, 3 × uint16)
FEEDER_REG_START   = 0                # Starting register offset
FEEDER_REG_COUNT   = 3                # weight_g, nh3_x10, water_x10

# Actuation node holding registers (FC 0x06 write)
ACT_REG_FEED_GATE   = 0              # 0 = closed, 1 = open
ACT_REG_WATER_VALVE  = 1             # 0 = closed, 1 = open
ACT_REG_DISPENSE_G   = 2             # Target grams (>0 triggers dispense)
ACT_REG_STATUS_FLAGS = 3             # Read-only status bitmask
ACT_REG_ESTOP        = 4             # 0 = normal, 1 = trigger, 2 = reset

# Serial port detection order
SERIAL_PORTS_USB  = ['/dev/ttyUSB0', '/dev/ttyACM0', '/dev/ttyUSB1', '/dev/ttyACM1']
SERIAL_PORTS_UART = ['/dev/ttyAMA0', '/dev/serial0', '/dev/ttyS0']
SERIAL_PORTS_WIN  = ['COM3', 'COM4', 'COM5']

FEEDER_BAUDRATE   = 115200            # Feeder nodes run at 115200
ACTUATOR_BAUDRATE = 9600              # Actuation node runs at 9600

# ──────────────────────────────────────────────────────────────────────
# Water Thresholds (Section7.tex, TissisChanges.md)
# ──────────────────────────────────────────────────────────────────────
MAX_WATER_L               = 5.0      # Trough full capacity (liters)
DEFAULT_REFILL_THRESHOLD  = 40       # Default auto-refill at 40% (2.0 L)
MIN_REFILL_THRESHOLD      = 10       # Dashboard slider minimum
MAX_REFILL_THRESHOLD      = 80       # Dashboard slider maximum
CRITICAL_WATER_PCT        = 30       # Critical alert below 30% (1.5 L)
WATER_REFILL_TIMEOUT_S    = 300      # 5 min timeout after refill command

# ──────────────────────────────────────────────────────────────────────
# Air Quality / NH3 Thresholds (Section7.tex — ammonia bands)
# ──────────────────────────────────────────────────────────────────────
NH3_NORMAL_PPM   = 10                # < 10 ppm = normal
NH3_CAUTION_PPM  = 20                # 10–19 ppm = caution (logged, no alert)
NH3_WARNING_PPM  = 20                # 20–24 ppm = warning alert
NH3_CRITICAL_PPM = 25                # ≥ 25 ppm  = critical (immediate LoRa)

# ──────────────────────────────────────────────────────────────────────
# Anomaly Detection (Section7.tex — rolling-window baseline)
# ──────────────────────────────────────────────────────────────────────
BASELINE_WINDOW_N        = 14        # N = 14 prior feeding sessions
ANOMALY_K                = 2.0       # k = 2 standard deviations
CONSECUTIVE_DROP_PCT     = 20        # Warning if ≥20% drop for 2 consecutive
CRITICAL_DROP_PCT        = 30        # Critical if ≥30% single-session drop
MIN_SESSIONS_FOR_ANOMALY = 14        # Silent monitoring until 14 sessions logged

# ──────────────────────────────────────────────────────────────────────
# Feed Dispensing
# ──────────────────────────────────────────────────────────────────────
FDA_PASS_THRESHOLD   = 95.0          # Feed dispensing accuracy ≥ 95%
FDA_TOLERANCE_PCT    = 5.0           # ±5% tolerance for gravimetric check
FEED_DISPATCH_MIN_KG = 0.5           # Minimum manual dispatch (kg)
FEED_DISPATCH_STEP   = 0.5           # Increment step (kg)

# ──────────────────────────────────────────────────────────────────────
# Timing & Intervals
# ──────────────────────────────────────────────────────────────────────
POLL_INTERVAL_S        = 1.0         # MODBUS polling cycle (seconds)
DB_COMMIT_INTERVAL_S   = 5.0         # Batch DB writes to reduce SD wear
HEARTBEAT_INTERVAL_S   = 10.0        # Watchdog heartbeat interval
HEARTBEAT_TIMEOUT_S    = 10.0        # Gap > 10 s counts as downtime
SCHEDULE_CHECK_S       = 30.0        # How often to check feeding schedule
ALERT_QUEUE_INTERVAL_S = 60.0        # Bundle non-critical alerts for LoRa

# ──────────────────────────────────────────────────────────────────────
# Performance Targets (Section7.tex — evaluation metrics)
# ──────────────────────────────────────────────────────────────────────
TARGET_LATENCY_MEAN_S   = 2.0        # Mean data latency ≤ 2 s
TARGET_LATENCY_MAX_S    = 3.0        # Max individual latency ≤ 3 s
TARGET_UPTIME_PCT       = 95.0       # System uptime ≥ 95% over 72 h
TARGET_ERC_PCT          = 99.0       # Event-record completeness ≥ 99%

# ──────────────────────────────────────────────────────────────────────
# Paths
# ──────────────────────────────────────────────────────────────────────
BASE_DIR    = os.path.dirname(os.path.abspath(__file__))
DB_PATH     = os.path.join(BASE_DIR, 'piggery_monitor.db')
WEBSITE_DIR = os.path.abspath(os.path.join(BASE_DIR, '..', 'WebsitePrototype'))

# ──────────────────────────────────────────────────────────────────────
# Flask Server
# ──────────────────────────────────────────────────────────────────────
SERVER_HOST = '0.0.0.0'
SERVER_PORT = 5000

# ──────────────────────────────────────────────────────────────────────
# Default Feeding Schedule (seeded on first run)
# ──────────────────────────────────────────────────────────────────────
DEFAULT_SCHEDULES = [
    {'label': 'Morning Cycle', 'time_hour': 6,  'time_minute': 0, 'amount_kg': 2.0},
    {'label': 'Midday Cycle',  'time_hour': 12, 'time_minute': 0, 'amount_kg': 1.5},
]

# ──────────────────────────────────────────────────────────────────────
# GPIO (Raspberry Pi only)
# ──────────────────────────────────────────────────────────────────────
GPIO_DE_RE_PIN = 18                   # BCM GPIO 18 (Physical Pin 12)
