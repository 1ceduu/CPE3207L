"""
MODBUS RTU Multi-Node Poller Service
CPE3207L Thesis: Integrated Automated Feeding and Watering System

This module handles communication with ESP32 feeder nodes and the actuation node
via RS-485 (MODBUS RTU) or direct USB Serial (JSON fallback). It maintains the
latest telemetry readings in memory, triggers real-time callbacks for analytical
processing, and batches data for SQLite database logging.
"""

import os
import sys
import time
import json
import random
import logging
import threading

import serial
import serial.tools.list_ports

from config import (
    FEEDER_SLAVE_IDS, ACTUATOR_SLAVE_ID,
    FEEDER_REG_START, FEEDER_REG_COUNT,
    SERIAL_PORTS_USB, SERIAL_PORTS_UART, SERIAL_PORTS_WIN,
    FEEDER_BAUDRATE, ACTUATOR_BAUDRATE,
    POLL_INTERVAL_S, DB_COMMIT_INTERVAL_S,
    GPIO_DE_RE_PIN
)
import database

logger = logging.getLogger(__name__)

# Try to initialize GPIO for RS-485 DE/RE control
HAS_GPIO = False
try:
    import RPi.GPIO as GPIO
    GPIO.setmode(GPIO.BCM)
    GPIO.setwarnings(False)
    GPIO.setup(GPIO_DE_RE_PIN, GPIO.OUT)
    GPIO.output(GPIO_DE_RE_PIN, GPIO.LOW)  # Default: Receive mode
    HAS_GPIO = True
    logger.info(f"RPi.GPIO initialized. MAX485 DE/RE on GPIO {GPIO_DE_RE_PIN}")
except ImportError:
    logger.warning("RPi.GPIO not found or failed to load. Running without hardware DE/RE control.")

# Thread-safe state
_latest_lock = threading.Lock()
_latest = {}
_on_new_reading_callback = None

_batch_lock = threading.Lock()
_telemetry_batch = []
_last_commit_time = time.time()

# Module-level serial connection (set by start(), used by write_actuator_register)
_serial_conn = None
_serial_lock = threading.Lock()

def set_on_reading_callback(fn):
    """Register a callback function to be called after each successful poll."""
    global _on_new_reading_callback
    _on_new_reading_callback = fn

def get_latest(pen_id=None):
    """Return the latest telemetry for a specific pen, or all pens."""
    with _latest_lock:
        if pen_id is not None:
            return _latest.get(pen_id)
        return dict(_latest)

def detect_serial_ports():
    """
    Scan for available serial ports based on config lists and pySerial tools.
    Returns a dict with 'usb', 'uart', and 'win' lists of available paths.
    """
    ports = {'usb': [], 'uart': [], 'win': []}
    
    # Use pySerial to get all active ports
    available_ports = [p.device for p in serial.tools.list_ports.comports()]
    
    for p in SERIAL_PORTS_USB:
        if p in available_ports or os.path.exists(p):
            ports['usb'].append(p)
            
    for p in SERIAL_PORTS_UART:
        if p in available_ports or os.path.exists(p):
            ports['uart'].append(p)
            
    if sys.platform.startswith('win'):
        for p in SERIAL_PORTS_WIN:
            if p in available_ports:
                ports['win'].append(p)
                
    return ports

def calculate_crc16(data: bytes) -> int:
    """Calculate MODBUS RTU CRC16 checksum."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc

def poll_feeder_node(serial_conn, slave_id):
    """
    Poll a feeder node via MODBUS RTU FC 0x03.
    Extracts 3 registers: weight_g, nh3_x10, water_x10.
    Returns {pen_id, weight_g, nh3_ppm, water_pct} or None on failure.
    """
    try:
        serial_conn.reset_input_buffer()
        serial_conn.reset_output_buffer()
        
        # Build query: [slave_id, 0x03, 0x00, start_reg, 0x00, reg_count, CRC_LO, CRC_HI]
        query = bytes([slave_id, 0x03, 
                       (FEEDER_REG_START >> 8) & 0xFF, FEEDER_REG_START & 0xFF,
                       (FEEDER_REG_COUNT >> 8) & 0xFF, FEEDER_REG_COUNT & 0xFF])
        crc = calculate_crc16(query)
        query += bytes([crc & 0xFF, (crc >> 8) & 0xFF])
        
        if HAS_GPIO:
            GPIO.output(GPIO_DE_RE_PIN, GPIO.HIGH)
        time.sleep(0.002)
        
        serial_conn.write(query)
        serial_conn.flush()
        
        # Transmission delay
        baudrate = serial_conn.baudrate
        tx_delay = 0.010 if baudrate == 9600 else 0.002
        time.sleep(tx_delay)
        
        if HAS_GPIO:
            GPIO.output(GPIO_DE_RE_PIN, GPIO.LOW)
            
        # Read response
        time.sleep(0.1) # Wait for processing and response
        
        # Frame size = slave_id(1) + FC(1) + byte_count(1) + data(6) + crc(2) = 11 bytes
        raw_rx = serial_conn.read(64)
        if not raw_rx:
            return None
            
        header = bytes([slave_id, 0x03, FEEDER_REG_COUNT * 2])
        idx = raw_rx.find(header)
        if idx != -1 and (idx + 11) <= len(raw_rx):
            rx = raw_rx[idx:idx+11]
            calc_crc = calculate_crc16(rx[:9])
            recv_crc = rx[9] | (rx[10] << 8)
            
            if calc_crc == recv_crc:
                weight_g  = (rx[3] << 8) | rx[4]
                nh3_x10   = (rx[5] << 8) | rx[6]
                water_x10 = (rx[7] << 8) | rx[8]
                
                return {
                    'pen_id': slave_id,
                    'weight_g': weight_g,
                    'nh3_ppm': nh3_x10 / 10.0,
                    'water_pct': water_x10 / 10.0
                }
            else:
                logger.warning(f"CRC error from node {slave_id}: calc={calc_crc}, recv={recv_crc}")
    except Exception as e:
        logger.error(f"Error polling feeder node {slave_id}: {e}")
        
    return None

def write_actuator_register(register, value):
    """
    Write to an actuator node register via MODBUS RTU FC 0x06.
    Uses the module-level serial connection.
    Temporarily changes baudrate to ACTUATOR_BAUDRATE, then restores.
    Returns True on success, False on failure.
    """
    serial_conn = _serial_conn
    if serial_conn is None or not serial_conn.is_open:
        logger.warning("Cannot write actuator register: no serial connection")
        return False
    orig_baudrate = serial_conn.baudrate
    success = False
    try:
        serial_conn.baudrate = ACTUATOR_BAUDRATE
        serial_conn.reset_input_buffer()
        serial_conn.reset_output_buffer()
        
        query = bytes([ACTUATOR_SLAVE_ID, 0x06,
                       (register >> 8) & 0xFF, register & 0xFF,
                       (value >> 8) & 0xFF, value & 0xFF])
        crc = calculate_crc16(query)
        query += bytes([crc & 0xFF, (crc >> 8) & 0xFF])
        
        if HAS_GPIO:
            GPIO.output(GPIO_DE_RE_PIN, GPIO.HIGH)
        time.sleep(0.002)
        
        serial_conn.write(query)
        serial_conn.flush()
        time.sleep(0.010) # 9600 baud tx delay
        
        if HAS_GPIO:
            GPIO.output(GPIO_DE_RE_PIN, GPIO.LOW)
            
        time.sleep(0.1)
        raw_rx = serial_conn.read(8) # Echo should be exactly 8 bytes
        
        if raw_rx and len(raw_rx) >= 8:
            idx = raw_rx.find(bytes([ACTUATOR_SLAVE_ID, 0x06]))
            if idx != -1 and (idx + 8) <= len(raw_rx):
                rx = raw_rx[idx:idx+8]
                if rx == query: # Success echo
                    success = True
                else:
                    logger.warning(f"Actuator write echo mismatch. sent={query.hex()}, recv={rx.hex()}")
    except Exception as e:
        logger.error(f"Error writing to actuator register {register}: {e}")
    finally:
        serial_conn.baudrate = orig_baudrate
        
    return success

def try_usb_serial_json(port):
    """
    Fallback mode for direct USB cable testing.
    Reads JSON lines from ESP32 at 115200 baud.
    Returns parsed dict or None.
    """
    try:
        with serial.Serial(port, 115200, timeout=1.0) as ser:
            for _ in range(5):
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line.startswith('{') and line.endswith('}'):
                    data = json.loads(line)
                    pen_id = int(data.get("pen_id", 1))
                    weight_kg = float(data.get("feedRemaining", 0.0))
                    water_pct = float(data.get("waterLevel", 0.0))
                    nh3_ppm = float(data.get("nh3", 0.0))
                    
                    return {
                        'pen_id': pen_id,
                        'weight_g': int(weight_kg * 1000),
                        'nh3_ppm': nh3_ppm,
                        'water_pct': water_pct,
                        'source': f'USB Serial JSON ({port})'
                    }
    except Exception as e:
        logger.debug(f"USB JSON fallback error on {port}: {e}")
    return None

def _process_reading(reading):
    """Process a successful reading, updating memory and scheduling DB batches."""
    global _last_commit_time
    
    pen_id = reading['pen_id']
    reading['last_updated'] = time.strftime('%H:%M:%S')
    reading.setdefault('source', 'MODBUS RTU')
    
    with _latest_lock:
        _latest[pen_id] = reading
        
    if _on_new_reading_callback:
        try:
            _on_new_reading_callback(
                reading['pen_id'],
                reading['weight_g'],
                reading['water_pct'],
                reading['nh3_ppm']
            )
        except Exception as e:
            logger.error(f"Error in on_new_reading callback: {e}")
            
    with _batch_lock:
        _telemetry_batch.append((pen_id, reading['weight_g'], reading['water_pct'], reading['nh3_ppm']))
        now = time.time()
        if (now - _last_commit_time) >= DB_COMMIT_INTERVAL_S and _telemetry_batch:
            try:
                database.log_telemetry_batch(_telemetry_batch)
                _telemetry_batch.clear()
                _last_commit_time = now
            except Exception as e:
                logger.error(f"Error flushing telemetry batch to DB: {e}")

def start(stop_event):
    """
    Main polling loop service.
    Scans for serial ports, polls telemetry, and triggers callbacks.
    """
    logger.info("Starting MODBUS RTU Poller Service...")
    
    # Auto-detect serial ports
    ports = detect_serial_ports()
    all_ports = ports['usb'] + ports['uart'] + ports['win']
    
    if not all_ports:
        logger.warning("No serial ports found. Running in SIMULATION dev mode.")
        
    global _serial_conn
    connected_port = None
    
    # Try to open a port if available
    for p in all_ports:
        try:
            _serial_conn = serial.Serial(
                port=p,
                baudrate=FEEDER_BAUDRATE,
                bytesize=8,
                parity=serial.PARITY_NONE,
                stopbits=1,
                timeout=0.3
            )
            connected_port = p
            logger.info(f"Connected to serial port {p} at {FEEDER_BAUDRATE} baud")
            break
        except Exception as e:
            logger.debug(f"Failed to open port {p}: {e}")
            
    # Main polling loop
    while not stop_event.is_set():
        cycle_start = time.time()
        
        if _serial_conn and _serial_conn.is_open:
            # Poll MODBUS nodes
            success = False
            for slave_id in FEEDER_SLAVE_IDS:
                reading = poll_feeder_node(_serial_conn, slave_id)
                if reading:
                    _process_reading(reading)
                    success = True
                    
            # Fallback to USB JSON if MODBUS failed and it's a USB port
            if not success and connected_port in ports['usb']:
                _serial_conn.close() # Close to allow try_usb_serial_json to open it
                reading = try_usb_serial_json(connected_port)
                if reading:
                    _process_reading(reading)
                # Reopen for next cycle
                try:
                    _serial_conn.open()
                except Exception as e:
                    logger.error(f"Failed to reopen port {connected_port}: {e}")
        else:
            # Simulation mode
            for slave_id in FEEDER_SLAVE_IDS:
                reading = {
                    'pen_id': slave_id,
                    'weight_g': random.uniform(100, 2000),
                    'nh3_ppm': random.uniform(5, 30),
                    'water_pct': random.uniform(20, 100),
                    'source': 'Simulation (dev mode)'
                }
                _process_reading(reading)
                
        # Sleep until next poll interval
        elapsed = time.time() - cycle_start
        sleep_time = max(0, POLL_INTERVAL_S - elapsed)
        if sleep_time > 0:
            stop_event.wait(sleep_time)
            
    # Cleanup
    if _serial_conn and _serial_conn.is_open:
        _serial_conn.close()
        logger.info("Serial port closed.")
        
    # Flush remaining batch
    with _batch_lock:
        if _telemetry_batch:
            try:
                database.log_telemetry_batch(_telemetry_batch)
                _telemetry_batch.clear()
            except Exception as e:
                logger.error(f"Error flushing final telemetry batch to DB: {e}")
                
    logger.info("Poller Service stopped.")
