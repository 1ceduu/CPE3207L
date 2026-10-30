"""
Command Service — Feeding Scheduler and Manual Override Dispatcher
CPE3207L Thesis: Integrated Automated Feeding and Watering System

As per Section7.tex, this module runs as a background daemon thread. It is responsible for:
1. Monitoring configured feeding schedules and triggering them when due.
2. Handling manual dispensing overrides from the dashboard.
3. Actively monitoring water refill operations, ensuring valves close when the target
   water level is reached or upon timeout.
"""

import time
import logging
import threading
from datetime import datetime

import config
import database

logger = logging.getLogger(__name__)

# State management
_state_lock = threading.Lock()
_fired_today = {}    # Key: (pen_id, label), Value: datetime.date
_active_refills = {} # Key: pen_id, Value: {'start_time': float, 'target_pct': float}


def _check_schedules():
    """
    Query all enabled feeding schedules from database. Compare each schedule's 
    (time_hour, time_minute) against current time. If within a 1-minute window 
    AND not already fired today, dispatch feed.
    """
    global _fired_today
    now = datetime.now()
    current_date = now.date()
    current_hour = now.hour
    current_minute = now.minute

    # Cleanup _fired_today if date changes (midnight reset)
    with _state_lock:
        keys_to_remove = [k for k, v in _fired_today.items() if v != current_date]
        for k in keys_to_remove:
            del _fired_today[k]

    schedules_by_pen = database.get_all_feeding_schedules()
    
    for pen_id, schedules in schedules_by_pen.items():
        for sched in schedules:
            if not sched.get('enabled'):
                continue
                
            label = sched['label']
            sched_hour = sched['time_hour']
            sched_minute = sched['time_minute']
            amount_kg = sched['amount_kg']
            
            # Check if within 1-minute window
            if sched_hour == current_hour and sched_minute == current_minute:
                key = (pen_id, label)
                with _state_lock:
                    if key not in _fired_today or _fired_today[key] != current_date:
                        _fired_today[key] = current_date
                        logger.info(f"Schedule '{label}' due for Pen {pen_id}. Dispatching {amount_kg} kg.")
                        dispatch_feed(pen_id, amount_kg, trigger_type='scheduled')


def dispatch_feed(pen_id, target_kg, trigger_type='scheduled'):
    """
    Trigger feed dispensing sequence.
    Converts target_kg to grams, sends commands to the actuator, and logs the event.
    Actual weight will be updated later when load cell confirms delivery.
    """
    import poller # Lazy import to avoid circular dependency
    
    target_grams = int(target_kg * 1000)
    
    # Send commands to actuator node
    poller.write_actuator_register(config.ACT_REG_FEED_GATE, 1)
    poller.write_actuator_register(config.ACT_REG_DISPENSE_G, target_grams)
    
    # Log the event (actual_g will be updated later)
    database.log_feeding_event(pen_id, target_grams, None, trigger_type)
    
    logger.info(f"Dispatched feed for Pen {pen_id}: {target_grams}g ({trigger_type})")
    
    return {
        'pen_id': pen_id,
        'target_grams': target_grams,
        'trigger_type': trigger_type,
        'status': 'dispatched'
    }


def dispatch_water_refill(pen_id):
    """
    Start water refill and begin monitoring.
    Opens the water valve and starts a timeout timer.
    """
    import poller # Lazy import to avoid circular dependency
    
    pen_config = database.get_pen_config(pen_id)
    target_pct = pen_config['water_refill_threshold'] if pen_config else config.DEFAULT_REFILL_THRESHOLD
    
    # Open water valve
    poller.write_actuator_register(config.ACT_REG_WATER_VALVE, 1)
    
    with _state_lock:
        _active_refills[pen_id] = {
            'start_time': time.time(),
            'target_pct': target_pct
        }
        
    logger.info(f"Dispatched water refill for Pen {pen_id}. Target: {target_pct}%")


def _monitor_refills():
    """
    Monitor ongoing water refills.
    Close valves if target threshold reached or timeout exceeded.
    """
    import poller # Lazy import to avoid circular dependency
    
    current_time = time.time()
    
    with _state_lock:
        pens_to_check = list(_active_refills.keys())
        
    for pen_id in pens_to_check:
        with _state_lock:
            if pen_id not in _active_refills:
                continue
            refill_info = _active_refills[pen_id]
            
        elapsed = current_time - refill_info['start_time']
        
        # Check timeout first
        if elapsed >= config.WATER_REFILL_TIMEOUT_S:
            poller.write_actuator_register(config.ACT_REG_WATER_VALVE, 0)
            logger.error(f"Water refill timeout for Pen {pen_id} ({config.WATER_REFILL_TIMEOUT_S}s). Closing valve.")
            database.log_alert(
                pen_id=pen_id,
                alert_type="WATER_TIMEOUT",
                severity="CRITICAL",
                message=f"Water refill timed out after {config.WATER_REFILL_TIMEOUT_S}s."
            )
            with _state_lock:
                del _active_refills[pen_id]
            continue
            
        # Get latest telemetry
        latest_data = poller.get_latest(pen_id)
        if latest_data:
            water_pct = latest_data.get('water_pct', 0)
            if water_pct >= refill_info['target_pct']:
                poller.write_actuator_register(config.ACT_REG_WATER_VALVE, 0)
                logger.info(f"Water refill complete for Pen {pen_id}. Reached {water_pct}%.")
                with _state_lock:
                    del _active_refills[pen_id]


def dispatch_manual_override(pen_id, override_type, amount=None):
    """
    Handle manual overrides for feeding or water refill.
    """
    result = {'pen_id': pen_id, 'override_type': override_type, 'status': 'failed'}
    
    if override_type == 'feed':
        if amount is None or amount <= 0:
            logger.warning("Invalid feed amount for manual override.")
            return result
        
        res = dispatch_feed(pen_id, amount, trigger_type='manual_override')
        database.log_manual_override(pen_id, 'feed', amount, None)
        result['status'] = 'success'
        result['details'] = res
        
    elif override_type == 'water':
        dispatch_water_refill(pen_id)
        # Log manual override for water (target_value=1 to represent ON)
        database.log_manual_override(pen_id, 'water', 1, None)
        result['status'] = 'success'
        
    else:
        logger.warning(f"Unknown override type: {override_type}")
        
    return result


def start(stop_event):
    """
    Main loop for Command Service to be run as a daemon thread.
    Periodically checks schedules and monitors water refills.
    """
    logger.info("Command Service started.")
    
    last_schedule_check = 0.0
    
    while not stop_event.is_set():
        current_time = time.time()
        
        # Check schedules every SCHEDULE_CHECK_S
        if current_time - last_schedule_check >= config.SCHEDULE_CHECK_S:
            try:
                _check_schedules()
            except Exception as e:
                logger.error(f"Error checking schedules: {e}")
            last_schedule_check = current_time
            
        # Monitor refills frequently
        try:
            _monitor_refills()
        except Exception as e:
            logger.error(f"Error monitoring refills: {e}")
            
        # Check refills more frequently (every 5 seconds)
        # Use wait on the event so it can be interrupted gracefully during shutdown
        stop_event.wait(5.0)
        
    logger.info("Command Service stopped.")
