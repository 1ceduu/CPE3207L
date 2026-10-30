"""
Alert Service Module — LoRa-SMS Alert Pipeline
CPE3207L Thesis: Integrated Automated Feeding and Watering System

Handles encoding and transmission of system alerts via LoRa to a remote SMS gateway.
As per Section7.tex, hardware transmission is currently stubbed until the physical
LoRa modules (SX1278/Ra-02) and SMS gateway are fully integrated.
"""

import logging
import struct
import time
from collections import deque
from datetime import datetime

import config
import database

logger = logging.getLogger(__name__)

LORA_ENABLED = False  # TODO: Set to True when hardware is connected

# Encodings for LoRa transmission
ALERT_TYPE_MAP = {
    'nh3_warning': 1,
    'nh3_critical': 2,
    'water_low': 3,
    'feed_anomaly': 4,
    'system_fault': 5
}

SEVERITY_MAP = {
    'info': 1,
    'warning': 2,
    'critical': 3
}

# Queue for bundling non-critical alerts
_alert_queue = deque()
_last_flush_time = 0.0

def encode_alert_payload(alert):
    """
    Encodes an alert dictionary into a compact binary payload for LoRa transmission.
    Format: [alert_id (4 bytes), pen_id (1 byte), alert_type_code (1 byte), severity_code (1 byte), timestamp_unix (4 bytes)]
    """
    alert_id = alert.get('id', 0)
    pen_id = alert.get('pen_id', 0)
    
    alert_type_str = alert.get('alert_type', 'unknown')
    alert_type_code = ALERT_TYPE_MAP.get(alert_type_str, 0)
    
    severity_str = alert.get('severity', 'info')
    severity_code = SEVERITY_MAP.get(severity_str, 1)
    
    # Parse timestamp to unix epoch
    timestamp_str = alert.get('timestamp')
    try:
        dt = datetime.strptime(timestamp_str, "%Y-%m-%d %H:%M:%S")
        timestamp_unix = int(dt.timestamp())
    except (ValueError, TypeError):
        timestamp_unix = int(time.time())

    # Pack into 11 bytes: I (4 bytes), B (1), B (1), B (1), I (4)
    # Using big-endian for network transmission
    payload = struct.pack(">IBBBI", alert_id, pen_id, alert_type_code, severity_code, timestamp_unix)
    return payload

def format_sms_message(alert):
    """
    Formats an alert into a human-readable SMS text.
    Max ~160 chars for SMS.
    """
    alert_id = alert.get('id', 0)
    pen_id = alert.get('pen_id', 0)
    severity = alert.get('severity', 'UNKNOWN').upper()
    message = alert.get('message', 'No message')
    timestamp = alert.get('timestamp', 'Unknown time')
    
    sms_text = f"[ALERT #{alert_id}] Pen {pen_id} | {severity} | {message} | {timestamp}"
    
    if len(sms_text) > 160:
        sms_text = sms_text[:157] + "..."
        
    return sms_text

def _transmit_lora(payload_bytes):
    """
    STUB: Transmit payload via LoRa.
    """
    hex_payload = payload_bytes.hex().upper()
    logger.info(f"LoRa Payload: {hex_payload}")
    if LORA_ENABLED:
        # TODO: Implement SPI/UART to send to SX1278/Ra-02 module
        pass
    else:
        logger.info("[STUB] LoRa transmission skipped — hardware not connected")
    return True

def _send_sms_via_gateway(sms_text):
    """
    STUB: Send SMS via gateway.
    """
    logger.info(f"SMS Text: {sms_text}")
    if LORA_ENABLED:
        # TODO: Implement actual SMS gateway transmission
        pass
    else:
        logger.info("[STUB] SMS gateway not available")
    return True

def process_alert(alert_dict):
    """
    Process an alert. Critical alerts are sent immediately, others are queued.
    """
    severity = alert_dict.get('severity', '').lower()
    alert_type = alert_dict.get('alert_type', '').lower()
    
    is_critical = severity == 'critical' or alert_type == 'nh3_critical'
    
    if is_critical:
        # Immediate transmission
        payload = encode_alert_payload(alert_dict)
        _transmit_lora(payload)
        
        sms_text = format_sms_message(alert_dict)
        _send_sms_via_gateway(sms_text)
        
        database.mark_alert_sms_sent(alert_dict['id'])
    else:
        # Add to non-critical queue
        _alert_queue.append(alert_dict)

def start(stop_event):
    """
    Main loop for the Alert Service.
    """
    global _last_flush_time
    logger.info("Starting Alert Service loop...")
    _last_flush_time = time.time()
    
    while not stop_event.is_set():
        try:
            # Check database for unsent alerts
            unsent_alerts = database.get_unsent_alerts()
            for alert in unsent_alerts:
                process_alert(alert)
                
            # Flush non-critical queue if interval elapsed
            current_time = time.time()
            if current_time - _last_flush_time >= config.ALERT_QUEUE_INTERVAL_S:
                while _alert_queue:
                    queued_alert = _alert_queue.popleft()
                    payload = encode_alert_payload(queued_alert)
                    _transmit_lora(payload)
                    
                    sms_text = format_sms_message(queued_alert)
                    _send_sms_via_gateway(sms_text)
                    
                    database.mark_alert_sms_sent(queued_alert['id'])
                _last_flush_time = current_time
                
        except Exception as e:
            logger.error(f"Error in alert service loop: {e}")
            
        # Sleep to prevent high CPU usage, check stop_event often
        stop_event.wait(5.0)

    logger.info("Alert Service loop stopped.")
