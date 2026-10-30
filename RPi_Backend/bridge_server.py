#!/usr/bin/env python3
"""
Raspberry Pi Telemetry Bridge Server (MODBUS RTU + USB Serial)
CPE3207L Thesis Hardware & Communication Stack

- Telemetry poller for ESP32 Feeder Node over USB Serial JSON and RS-485 MODBUS RTU.
- Serves HTTP REST API (/api/telemetry) for the Kiosk Dashboard.
"""

import os
import json
import time
import struct
import threading
from flask import Flask, jsonify, send_from_directory
from flask_cors import CORS

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
WEBSITE_DIR = os.path.abspath(os.path.join(BASE_DIR, "..", "WebsitePrototype"))

app = Flask(__name__, static_folder=WEBSITE_DIR)
CORS(app)

# Initialize Raspberry Pi GPIO 18 (Pin 12) for MAX485 DE Control
HAS_GPIO = False
try:
    import RPi.GPIO as GPIO
    GPIO.setmode(GPIO.BCM)
    GPIO.setwarnings(False)
    GPIO.setup(18, GPIO.OUT)
    GPIO.output(18, GPIO.LOW) # Default: Receive mode
    HAS_GPIO = True
    print("[INIT] RPi.GPIO initialized for Pin 12 (GPIO 18) DE/RE direction control.")
except Exception as e:
    print(f"[WARN] RPi.GPIO initialization failed: {e}")

# Filter physical ports
def get_unique_ports():
    usb_ports = []
    uart_ports = []
    # USB Serial Adapters / Cables
    for p in ['/dev/ttyUSB0', '/dev/ttyACM0', '/dev/ttyUSB1', '/dev/ttyACM1']:
        if os.path.exists(p):
            usb_ports.append(p)
    # Hardware PL011 UART
    if os.path.exists('/dev/ttyAMA0'):
        uart_ports.append('/dev/ttyAMA0')
    elif os.path.exists('/dev/serial0'):
        uart_ports.append('/dev/serial0')
    elif os.path.exists('/dev/ttyS0'):
        uart_ports.append('/dev/ttyS0')
    # Windows COM ports
    for p in ['COM3', 'COM4']:
        if os.path.exists(p):
            usb_ports.append(p)
    return usb_ports, uart_ports

latest_telemetry = {
    "pen_id": 1,
    "feedRemaining": 0.0,
    "waterLevel": 0,
    "nh3": 0.0,
    "status": "Normal",
    "statusClass": "normal",
    "source": "Searching for ESP32...",
    "last_updated": time.strftime("%H:%M:%S")
}


def update_status_class(t):
    water = t.get("waterLevel", 100)
    nh3 = t.get("nh3", 0)
    if water < 40:
        t["status"] = "Critical"
        t["statusClass"] = "critical"
    elif nh3 > 25:
        t["status"] = "Warning"
        t["statusClass"] = "warning"
    else:
        t["status"] = "Normal"
        t["statusClass"] = "normal"


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


def try_usb_serial_json(port):
    """Attempt to read 115200 baud USB Serial JSON lines from ESP32."""
    if not os.path.exists(port):
        return False

    try:
        import serial
        ser = serial.Serial(port, 115200, timeout=1.0)
        # Read multiple lines to catch JSON payload frame
        for _ in range(5):
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                print(f"[USB READ] {port}: {line}")
            if line.startswith('{') and line.endswith('}'):
                data = json.loads(line)
                latest_telemetry["feedRemaining"] = float(data.get("feedRemaining", 0.0))
                latest_telemetry["waterLevel"] = int(data.get("waterLevel", 0))
                latest_telemetry["nh3"] = float(data.get("nh3", 0.0))
                latest_telemetry["source"] = f"Live USB Serial ({port})"
                latest_telemetry["last_updated"] = time.strftime("%H:%M:%S")
                update_status_class(latest_telemetry)
                print(f"[SUCCESS] USB Serial JSON ({port}): Feed={latest_telemetry['feedRemaining']}kg, Water={latest_telemetry['waterLevel']}%, NH3={latest_telemetry['nh3']}ppm")
                ser.close()
                return True
        ser.close()
    except Exception as e:
        print(f"[SERIAL INFO] Port {port} USB JSON read error: {e}")
    return False


def try_modbus_polling_baud(port, baudrate):
    """Attempt to poll ESP32 Feeder Node via MODBUS RTU at a specific baud rate."""
    if not os.path.exists(port):
        return False

    try:
        import serial
        ser = serial.Serial(
            port=port,
            baudrate=baudrate,
            bytesize=8,
            parity=serial.PARITY_NONE,
            stopbits=1,
            timeout=0.3
        )

        ser.reset_input_buffer()
        ser.reset_output_buffer()

        query = bytes([0x01, 0x03, 0x00, 0x00, 0x00, 0x03])
        crc = calculate_crc16(query)
        query += bytes([crc & 0xFF, (crc >> 8) & 0xFF])

        if HAS_GPIO:
            GPIO.output(18, GPIO.HIGH)
        time.sleep(0.002)

        ser.write(query)
        ser.flush()
        tx_delay = 0.010 if baudrate == 9600 else 0.002
        time.sleep(tx_delay)

        if HAS_GPIO:
            GPIO.output(18, GPIO.LOW)

        time.sleep(0.12)
        raw_rx = ser.read(64)
        ser.close()

        if not raw_rx:
            print(f"[POLL INFO] {port} @ {baudrate} baud: 0 bytes received (Timeout)")
            return False

        header = bytes([0x01, 0x03, 0x06])
        idx = raw_rx.find(header)

        if idx != -1 and (idx + 11) <= len(raw_rx):
            rx = raw_rx[idx:idx+11]
            calc_crc = calculate_crc16(rx[:9])
            recv_crc = rx[9] | (rx[10] << 8)

            if calc_crc == recv_crc:
                weight_g  = (rx[3] << 8) | rx[4]
                nh3_val   = ((rx[5] << 8) | rx[6]) / 10.0
                water_pct = int(((rx[7] << 8) | rx[8]) / 10.0)

                latest_telemetry["feedRemaining"] = round(weight_g / 1000.0, 2)
                latest_telemetry["waterLevel"] = water_pct
                latest_telemetry["nh3"] = round(nh3_val, 1)
                latest_telemetry["source"] = f"Live MODBUS RTU ({port} @ {baudrate})"
                latest_telemetry["last_updated"] = time.strftime("%H:%M:%S")
                update_status_class(latest_telemetry)
                print(f"[SUCCESS] MODBUS ({port} @ {baudrate} baud): Weight={weight_g}g, NH3={nh3_val}ppm, Water={water_pct}%")
                return True

        print(f"[MODBUS RAW] Raw RX ({port} @ {baudrate} baud): {raw_rx.hex()}")

    except Exception as e:
        print(f"[MODBUS INFO] Port {port} @ {baudrate} baud attempted: {e}")

    return False


def try_modbus_polling(port):
    """Try polling port at 115200 first, then 9600 if no response."""
    if try_modbus_polling_baud(port, 115200):
        return True
    if try_modbus_polling_baud(port, 9600):
        return True
    return False


def telemetry_poller_thread():
    last_detected_ports = None
    while True:
        polled = False
        usb_ports, uart_ports = get_unique_ports()

        all_ports_str = f"USB: {usb_ports} | UART: {uart_ports}"
        if all_ports_str != last_detected_ports:
            print(f"[DETECTED PORTS] {all_ports_str}")
            last_detected_ports = all_ports_str

        # 1. Try USB Serial JSON first for plugged USB cables
        for port in usb_ports:
            if try_usb_serial_json(port):
                polled = True
                time.sleep(1.0)
                break
            elif try_modbus_polling(port):
                polled = True
                time.sleep(1.0)
                break

        # 2. Try Hardware UART MODBUS RTU for RS-485 bus
        if not polled:
            for port in uart_ports:
                if try_modbus_polling(port):
                    polled = True
                    time.sleep(1.0)
                    break

        if not polled:
            latest_telemetry["source"] = "No ESP32 connection detected"
            latest_telemetry["last_updated"] = time.strftime("%H:%M:%S")
            update_status_class(latest_telemetry)
            time.sleep(2.0)


@app.route('/')
def serve_index():
    return send_from_directory(WEBSITE_DIR, 'index.html')


@app.route('/<path:path>')
def serve_static(path):
    return send_from_directory(WEBSITE_DIR, path)


@app.route('/api/telemetry', methods=['GET'])
def get_telemetry():
    return jsonify(latest_telemetry)


if __name__ == '__main__':
    t = threading.Thread(target=telemetry_poller_thread, daemon=True)
    t.start()

    print("==================================================")
    print("  Raspberry Pi MODBUS RTU + Web Server Started    ")
    print("==================================================")
    print(f" Serving UI from: {WEBSITE_DIR}")
    print(" Dashboard URL:  http://localhost:5000")
    print(" Telemetry API:  http://localhost:5000/api/telemetry")
    print(" MODBUS Config:  USB Serial & Auto Baud MODBUS RTU")
    print("==================================================")

    app.run(host='0.0.0.0', port=5000, debug=False)
