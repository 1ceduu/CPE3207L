"""
Analytical Engine Module — Rolling-Window Anomaly Detection
CPE3207L Thesis: Integrated Automated Feeding and Watering System

This module implements the anomaly detection algorithms defined in Section7.tex.
It uses a rolling-window baseline over the last N feeding sessions to detect
deviations in feed consumption, and applies static thresholds for water level
and ammonia concentration.
"""

import math
import logging

import config
import database

logger = logging.getLogger(__name__)


def check_feed_anomaly(pen_id, current_consumption_g):
    """
    Check if the current feed consumption is anomalous based on a rolling-window baseline.
    Computes mean (μ) and standard deviation (σ) from the last BASELINE_WINDOW_N sessions.
    Alerts:
      - Critical drop: current < μ * (1 - CRITICAL_DROP_PCT/100)
      - Deviation drop: current < μ - k*σ
      - Overconsumption: current > μ + k*σ
    Returns an alert dict or None.
    """
    if database.count_feeding_sessions(pen_id) < config.MIN_SESSIONS_FOR_ANOMALY:
        return None

    history = database.get_feeding_history(pen_id, n=config.BASELINE_WINDOW_N)
    
    # We only consider records with actual_g
    actuals = [record['actual_g'] for record in history if record.get('actual_g') is not None]
    
    if len(actuals) < config.MIN_SESSIONS_FOR_ANOMALY:
        return None

    mean_val = sum(actuals) / len(actuals)
    variance = sum((x - mean_val) ** 2 for x in actuals) / len(actuals)
    std_dev = math.sqrt(variance)

    critical_drop_threshold = mean_val * (1.0 - config.CRITICAL_DROP_PCT / 100.0)
    lower_bound = mean_val - (config.ANOMALY_K * std_dev)
    upper_bound = mean_val + (config.ANOMALY_K * std_dev)

    if current_consumption_g < critical_drop_threshold:
        return {
            'type': 'feed_deviation',
            'severity': 'critical',
            'message': f'Feed consumption critically dropped by {config.CRITICAL_DROP_PCT}% or more from baseline.',
            'baseline_value': mean_val,
            'threshold_value': critical_drop_threshold,
            'actual_value': current_consumption_g
        }
    elif current_consumption_g < lower_bound:
        return {
            'type': 'feed_deviation',
            'severity': 'warning',
            'message': 'Feed consumption is below normal limits.',
            'baseline_value': mean_val,
            'threshold_value': lower_bound,
            'actual_value': current_consumption_g
        }
    elif current_consumption_g > upper_bound:
        return {
            'type': 'overconsumption',
            'severity': 'warning',
            'message': 'Feed consumption is above normal limits.',
            'baseline_value': mean_val,
            'threshold_value': upper_bound,
            'actual_value': current_consumption_g
        }

    return None


def check_consecutive_drop(pen_id):
    """
    Check if the last 2 feeding sessions both dropped by >= CONSECUTIVE_DROP_PCT
    below the rolling mean.
    Returns a warning alert dict or None.
    """
    # Need at least N sessions to calculate mean, plus 2 recent ones to evaluate.
    # We will get N sessions, calculate mean, and check the 2 most recent from history.
    if database.count_feeding_sessions(pen_id) < config.MIN_SESSIONS_FOR_ANOMALY:
        return None

    history = database.get_feeding_history(pen_id, n=config.BASELINE_WINDOW_N)
    actuals = [record['actual_g'] for record in history if record.get('actual_g') is not None]
    
    if len(actuals) < 2:
        return None

    mean_val = sum(actuals) / len(actuals)
    drop_threshold = mean_val * (1.0 - config.CONSECUTIVE_DROP_PCT / 100.0)

    last_two = actuals[:2]
    if all(val <= drop_threshold for val in last_two):
        return {
            'type': 'feed_deviation',
            'severity': 'warning',
            'message': f'Feed consumption dropped by >= {config.CONSECUTIVE_DROP_PCT}% for 2 consecutive sessions.',
            'baseline_value': mean_val,
            'threshold_value': drop_threshold,
            'actual_value': last_two[0]  # Return the most recent as actual
        }

    return None


def check_water_level(pen_id, water_pct):
    """
    Check the current water level percentage against thresholds.
    Returns a tuple: (alert_dict_or_None, needs_refill_boolean)
    """
    pen_cfg = database.get_pen_config(pen_id)
    refill_threshold = pen_cfg.get('water_refill_threshold', config.DEFAULT_REFILL_THRESHOLD) if pen_cfg else config.DEFAULT_REFILL_THRESHOLD

    needs_refill = water_pct < refill_threshold

    if water_pct < config.CRITICAL_WATER_PCT:
        alert = {
            'type': 'water_level',
            'severity': 'critical',
            'message': 'Water level is critically low.',
            'baseline_value': None,
            'threshold_value': config.CRITICAL_WATER_PCT,
            'actual_value': water_pct
        }
        return (alert, needs_refill)
    elif needs_refill:
        alert = {
            'type': 'water_level',
            'severity': 'warning',
            'message': f'Water level dropped below refill threshold ({refill_threshold}%).',
            'baseline_value': None,
            'threshold_value': refill_threshold,
            'actual_value': water_pct
        }
        return (alert, needs_refill)

    return (None, False)


def check_air_quality(pen_id, nh3_ppm):
    """
    Check the current ammonia (NH3) concentration.
    Returns a critical or warning alert dict, or None if normal/caution.
    """
    if nh3_ppm >= config.NH3_CRITICAL_PPM:
        return {
            'type': 'air_quality',
            'severity': 'critical',
            'message': 'Ammonia levels are critically high.',
            'baseline_value': None,
            'threshold_value': config.NH3_CRITICAL_PPM,
            'actual_value': nh3_ppm,
            'immediate_lora': True
        }
    elif nh3_ppm >= config.NH3_WARNING_PPM:
        return {
            'type': 'air_quality',
            'severity': 'warning',
            'message': 'Ammonia levels are elevated.',
            'baseline_value': None,
            'threshold_value': config.NH3_WARNING_PPM,
            'actual_value': nh3_ppm
        }
    elif nh3_ppm >= config.NH3_NORMAL_PPM:
        # Caution level (10-19 ppm). Logged but no alert.
        logger.info(f"Pen {pen_id} NH3 caution: {nh3_ppm} ppm")
        return None

    return None


def classify_status(water_pct, nh3_ppm):
    """
    Classify the overall status of the pen for UI visualization.
    Returns a tuple (status_text, status_class).
    """
    if water_pct < config.CRITICAL_WATER_PCT or nh3_ppm >= config.NH3_CRITICAL_PPM:
        return ('Critical', 'critical')
    elif nh3_ppm >= config.NH3_WARNING_PPM:
        return ('Warning', 'warning')
    else:
        return ('Normal', 'normal')


def process_reading(pen_id, weight_g, water_pct, nh3_ppm):
    """
    Main entry point for evaluating live telemetry from a pen.
    Runs water and air quality checks, logs alerts, and handles anomaly toggles.
    Returns a list of generated alerts.
    """
    alerts_generated = []
    
    pen_cfg = database.get_pen_config(pen_id)
    anomaly_active = False
    if pen_cfg:
        anomaly_active = bool(pen_cfg.get('anomaly_detection_active', 0))
    
    # Auto-enable anomaly detection if minimum sessions met
    if not anomaly_active and database.count_feeding_sessions(pen_id) >= config.MIN_SESSIONS_FOR_ANOMALY:
        database.set_anomaly_detection_active(pen_id, True)
        anomaly_active = True
        logger.info(f"Auto-enabled anomaly detection for Pen {pen_id}")

    # Check water
    water_alert, needs_refill = check_water_level(pen_id, water_pct)
    if water_alert:
        alerts_generated.append(water_alert)
        
    # Check air quality
    air_alert = check_air_quality(pen_id, nh3_ppm)
    if air_alert:
        alerts_generated.append(air_alert)
        
    # Anomaly detection for continuous metrics could be placed here if any,
    # but feed consumption checks run in process_feeding_complete.

    # Log generated alerts
    for alert in alerts_generated:
        database.log_alert(
            pen_id=pen_id,
            alert_type=alert['type'],
            severity=alert['severity'],
            message=alert['message'],
            baseline_value=alert['baseline_value'],
            threshold_value=alert['threshold_value'],
            actual_value=alert['actual_value']
        )
        
    return alerts_generated


def process_feeding_complete(pen_id, actual_g):
    """
    Evaluate alerts immediately after a feeding event completes.
    Checks feed consumption anomalies and consecutive drops.
    Returns a list of generated alerts.
    """
    alerts_generated = []
    
    pen_cfg = database.get_pen_config(pen_id)
    if pen_cfg and pen_cfg.get('anomaly_detection_active', 0):
        feed_alert = check_feed_anomaly(pen_id, actual_g)
        if feed_alert:
            alerts_generated.append(feed_alert)
            
        consecutive_alert = check_consecutive_drop(pen_id)
        if consecutive_alert:
            alerts_generated.append(consecutive_alert)

    # Log generated alerts
    for alert in alerts_generated:
        database.log_alert(
            pen_id=pen_id,
            alert_type=alert['type'],
            severity=alert['severity'],
            message=alert['message'],
            baseline_value=alert['baseline_value'],
            threshold_value=alert['threshold_value'],
            actual_value=alert['actual_value']
        )

    return alerts_generated
