"""
Main Entry Point — Raspberry Pi Backend Orchestrator
CPE3207L Thesis: Integrated Automated Feeding and Watering System

Launches all backend services as daemon threads:
  1. Database initialization (tables, seed data)
  2. Heartbeat thread (uptime tracking)
  3. MODBUS Poller Service (sensor data acquisition)
  4. Command Service (feeding scheduler + override dispatcher)
  5. Alert Service (LoRa-SMS pipeline)
  6. Flask REST API Server (dashboard + API endpoints)

Usage:
  python main.py            # Production mode (requires RS-485 hardware)
  python main.py --dev      # Development mode (mock serial data)
"""

import sys
import os
import time
import signal
import logging
import threading
import argparse

# Ensure the RPi_Backend directory is on the Python path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import config
import database
import poller
import analytical_engine
import command_service
import alert_service
from server import create_app, start_server


# ──────────────────────────────────────────────────────────────────────
# Logging Configuration
# ──────────────────────────────────────────────────────────────────────
def setup_logging(dev_mode=False):
    """Configure structured logging for all modules."""
    level = logging.DEBUG if dev_mode else logging.INFO
    fmt = '%(asctime)s [%(name)-20s] %(levelname)-7s %(message)s'
    datefmt = '%Y-%m-%d %H:%M:%S'

    logging.basicConfig(level=level, format=fmt, datefmt=datefmt)

    # Suppress noisy Flask/werkzeug logs in production
    if not dev_mode:
        logging.getLogger('werkzeug').setLevel(logging.WARNING)


logger = logging.getLogger('main')


# ──────────────────────────────────────────────────────────────────────
# Heartbeat Thread
# ──────────────────────────────────────────────────────────────────────
def heartbeat_thread(stop_event):
    """Write periodic heartbeat timestamps for uptime calculation (Usys metric)."""
    logger.info("Heartbeat service started (interval: %ds)", config.HEARTBEAT_INTERVAL_S)
    while not stop_event.is_set():
        try:
            database.log_heartbeat()
        except Exception as e:
            logger.error("Heartbeat write failed: %s", e)
        stop_event.wait(config.HEARTBEAT_INTERVAL_S)
    logger.info("Heartbeat service stopped")


# ──────────────────────────────────────────────────────────────────────
# Analytical Engine Callback (wired into poller)
# ──────────────────────────────────────────────────────────────────────
def on_telemetry_reading(pen_id, weight_g, water_pct, nh3_ppm):
    """Called by the poller after each successful sensor read."""
    try:
        alerts = analytical_engine.process_reading(pen_id, weight_g, water_pct, nh3_ppm)
        for alert in alerts:
            alert_service.process_alert(alert)
    except Exception as e:
        logger.error("Analytical engine error for Pen %d: %s", pen_id, e)


# ──────────────────────────────────────────────────────────────────────
# Main
# ──────────────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description='Piggery Monitor Backend Server')
    parser.add_argument('--dev', action='store_true',
                        help='Run in development mode with mock serial data')
    parser.add_argument('--port', type=int, default=config.SERVER_PORT,
                        help=f'Flask server port (default: {config.SERVER_PORT})')
    args = parser.parse_args()

    dev_mode = args.dev
    config.SERVER_PORT = args.port

    setup_logging(dev_mode)

    # Shared stop event for graceful shutdown
    stop_event = threading.Event()

    def signal_handler(signum, frame):
        logger.info("Shutdown signal received (signal %d)", signum)
        stop_event.set()

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    # ── Startup Banner ──
    print()
    print("=" * 60)
    print("  Piggery Monitor — Raspberry Pi Backend Server")
    print("  CPE3207L Thesis: Group V, University of San Carlos")
    print("=" * 60)
    print(f"  Mode:           {'DEVELOPMENT (mock data)' if dev_mode else 'PRODUCTION'}")
    print(f"  Database:       {config.DB_PATH}")
    print(f"  Dashboard:      http://localhost:{config.SERVER_PORT}")
    print(f"  Telemetry API:  http://localhost:{config.SERVER_PORT}/api/telemetry")
    print(f"  Pens:           {config.NUM_PENS}")
    print(f"  Poll Interval:  {config.POLL_INTERVAL_S}s")
    print("=" * 60)
    print()

    # ── Step 1: Initialize Database ──
    logger.info("Initializing database...")
    database.init_db()

    # ── Step 2: Wire analytical engine callback into poller ──
    poller.set_on_reading_callback(on_telemetry_reading)

    # ── Step 3: Start service threads ──
    threads = []

    # Heartbeat
    t_heartbeat = threading.Thread(
        target=heartbeat_thread, args=(stop_event,),
        name='heartbeat', daemon=True
    )
    threads.append(('Heartbeat', t_heartbeat))

    # MODBUS Poller
    t_poller = threading.Thread(
        target=poller.start, args=(stop_event,),
        name='poller', daemon=True
    )
    threads.append(('Poller', t_poller))

    # Command Service (Feeding Scheduler)
    t_commands = threading.Thread(
        target=command_service.start, args=(stop_event,),
        name='command_service', daemon=True
    )
    threads.append(('Command Service', t_commands))

    # Alert Service (LoRa-SMS Pipeline)
    t_alerts = threading.Thread(
        target=alert_service.start, args=(stop_event,),
        name='alert_service', daemon=True
    )
    threads.append(('Alert Service', t_alerts))

    for name, t in threads:
        t.start()
        logger.info("Started: %s", name)

    # ── Step 4: Start Flask Server (blocks main thread) ──
    logger.info("Starting Flask server...")
    app = create_app()

    try:
        start_server(app)
    except KeyboardInterrupt:
        pass
    finally:
        logger.info("Shutting down...")
        stop_event.set()

        # Wait for threads to finish (with timeout)
        for name, t in threads:
            t.join(timeout=5.0)
            if t.is_alive():
                logger.warning("%s did not stop cleanly", name)
            else:
                logger.info("Stopped: %s", name)

        logger.info("All services stopped. Goodbye.")


if __name__ == '__main__':
    main()
