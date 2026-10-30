"""
Flask REST API Server
CPE3207L Thesis: Integrated Automated Feeding and Watering System

Serves the Kiosk Dashboard and exposes RESTful API endpoints for:
  - Live telemetry (all pens / single pen / historical)
  - Feeding schedules (read / update)
  - Pen configuration (water threshold, anomaly toggle)
  - Alerts (recent / unacknowledged / acknowledge)
  - Manual overrides (trigger feed/water dispatch)
  - Feeding event history (for consumption charts)
  - System status (uptime, connection health)

The dashboard (WebsitePrototype/) is served as static files.
"""

import logging
from flask import Flask, jsonify, request, send_from_directory

from config import WEBSITE_DIR, NUM_PENS

logger = logging.getLogger(__name__)


def create_app():
    """Create and configure the Flask application."""
    app = Flask(__name__, static_folder=WEBSITE_DIR)

    # Lazy import to avoid circular dependencies at module load time
    try:
        from flask_cors import CORS
        CORS(app)
    except ImportError:
        logger.warning("flask-cors not installed — CORS headers disabled")

    # ──────────────────────────────────────────────────────────────
    # Static File Serving (Kiosk Dashboard)
    # ──────────────────────────────────────────────────────────────
    @app.route('/')
    def serve_index():
        """Serve the main dashboard HTML."""
        return send_from_directory(WEBSITE_DIR, 'index.html')

    @app.route('/<path:path>')
    def serve_static(path):
        """Serve static assets (JS, CSS, images)."""
        return send_from_directory(WEBSITE_DIR, path)

    # ──────────────────────────────────────────────────────────────
    # Telemetry Endpoints
    # ──────────────────────────────────────────────────────────────
    @app.route('/api/telemetry', methods=['GET'])
    def get_all_telemetry():
        """
        Get latest telemetry for all pens.
        Returns a dict keyed by pen_id with sensor values and status.
        """
        import poller
        import analytical_engine as ae

        result = {}
        for pen_id in range(1, NUM_PENS + 1):
            data = poller.get_latest(pen_id)
            if data:
                status, status_class = ae.classify_status(
                    data.get('water_pct', 100),
                    data.get('nh3_ppm', 0)
                )
                result[pen_id] = {
                    'pen_id': pen_id,
                    'feedRemaining': round(data.get('weight_g', 0) / 1000.0, 2),
                    'waterLevel': round(data.get('water_pct', 0), 1),
                    'nh3': round(data.get('nh3_ppm', 0), 1),
                    'status': status,
                    'statusClass': status_class,
                    'source': data.get('source', 'unknown'),
                    'last_updated': data.get('last_updated', ''),
                }
            else:
                result[pen_id] = {
                    'pen_id': pen_id,
                    'feedRemaining': 0.0,
                    'waterLevel': 0,
                    'nh3': 0.0,
                    'status': 'Offline',
                    'statusClass': 'critical',
                    'source': 'No data',
                    'last_updated': '',
                }
        return jsonify(result)

    @app.route('/api/telemetry/<int:pen_id>', methods=['GET'])
    def get_pen_telemetry(pen_id):
        """Get latest telemetry for a single pen."""
        if pen_id < 1 or pen_id > NUM_PENS:
            return jsonify({'error': f'Invalid pen_id {pen_id}'}), 400

        import poller
        import analytical_engine as ae

        data = poller.get_latest(pen_id)
        if not data:
            return jsonify({'error': 'No telemetry data available'}), 404

        status, status_class = ae.classify_status(
            data.get('water_pct', 100),
            data.get('nh3_ppm', 0)
        )
        return jsonify({
            'pen_id': pen_id,
            'feedRemaining': round(data.get('weight_g', 0) / 1000.0, 2),
            'waterLevel': round(data.get('water_pct', 0), 1),
            'nh3': round(data.get('nh3_ppm', 0), 1),
            'status': status,
            'statusClass': status_class,
            'source': data.get('source', 'unknown'),
            'last_updated': data.get('last_updated', ''),
        })

    @app.route('/api/telemetry/<int:pen_id>/history', methods=['GET'])
    def get_telemetry_history(pen_id):
        """Get historical telemetry for a pen. Query param: hours (default 24)."""
        if pen_id < 1 or pen_id > NUM_PENS:
            return jsonify({'error': f'Invalid pen_id {pen_id}'}), 400

        import database as db

        hours = request.args.get('hours', 24, type=int)
        hours = max(1, min(720, hours))  # Clamp to 1h–30d
        history = db.get_telemetry_history(pen_id, hours=hours)
        return jsonify(history)

    # ──────────────────────────────────────────────────────────────
    # Feeding Schedule Endpoints
    # ──────────────────────────────────────────────────────────────
    @app.route('/api/schedule/<int:pen_id>', methods=['GET'])
    def get_schedule(pen_id):
        """Get feeding schedule for a pen."""
        if pen_id < 1 or pen_id > NUM_PENS:
            return jsonify({'error': f'Invalid pen_id {pen_id}'}), 400

        import database as db
        schedules = db.get_feeding_schedule(pen_id)
        return jsonify(schedules)

    @app.route('/api/schedule/<int:pen_id>', methods=['PUT'])
    def update_schedule(pen_id):
        """
        Update feeding schedule for a pen.
        Body: { "schedules": [{"label": "Morning Cycle", "time_hour": 6,
                "time_minute": 0, "amount_kg": 2.0}, ...] }
        """
        if pen_id < 1 or pen_id > NUM_PENS:
            return jsonify({'error': f'Invalid pen_id {pen_id}'}), 400

        import database as db

        data = request.get_json()
        if not data or 'schedules' not in data:
            return jsonify({'error': 'Missing schedules array'}), 400

        for sched in data['schedules']:
            label = sched.get('label')
            hour = sched.get('time_hour')
            minute = sched.get('time_minute', 0)
            amount = sched.get('amount_kg')

            if not all([label, hour is not None, amount]):
                return jsonify({'error': f'Invalid schedule entry: {sched}'}), 400

            db.update_feeding_schedule(pen_id, label, int(hour), int(minute), float(amount))

        logger.info(f"Schedule updated for Pen {pen_id}: {data['schedules']}")
        return jsonify({'status': 'ok', 'message': f'Schedule updated for Pen {pen_id}'})

    # ──────────────────────────────────────────────────────────────
    # Pen Configuration Endpoints
    # ──────────────────────────────────────────────────────────────
    @app.route('/api/config/<int:pen_id>', methods=['GET'])
    def get_config(pen_id):
        """Get configuration for a pen (water threshold, anomaly detection status)."""
        if pen_id < 1 or pen_id > NUM_PENS:
            return jsonify({'error': f'Invalid pen_id {pen_id}'}), 400

        import database as db
        config = db.get_pen_config(pen_id)
        if not config:
            return jsonify({'error': 'Pen config not found'}), 404
        return jsonify(config)

    @app.route('/api/config/<int:pen_id>', methods=['PUT'])
    def update_config(pen_id):
        """
        Update pen configuration.
        Body: { "water_refill_threshold": 40 }
        """
        if pen_id < 1 or pen_id > NUM_PENS:
            return jsonify({'error': f'Invalid pen_id {pen_id}'}), 400

        import database as db

        data = request.get_json()
        if not data:
            return jsonify({'error': 'Missing request body'}), 400

        if 'water_refill_threshold' in data:
            threshold = int(data['water_refill_threshold'])
            db.update_water_threshold(pen_id, threshold)
            logger.info(f"Water threshold updated for Pen {pen_id}: {threshold}%")

        return jsonify({'status': 'ok', 'message': f'Config updated for Pen {pen_id}'})

    # ──────────────────────────────────────────────────────────────
    # Alert Endpoints
    # ──────────────────────────────────────────────────────────────
    @app.route('/api/alerts/<int:pen_id>', methods=['GET'])
    def get_alerts(pen_id):
        """Get recent alerts for a pen. Query param: limit (default 20)."""
        if pen_id < 1 or pen_id > NUM_PENS:
            return jsonify({'error': f'Invalid pen_id {pen_id}'}), 400

        import database as db

        limit = request.args.get('limit', 20, type=int)
        limit = max(1, min(100, limit))
        alerts = db.get_recent_alerts(pen_id, limit=limit)
        return jsonify(alerts)

    @app.route('/api/alerts/unacknowledged', methods=['GET'])
    def get_unacknowledged():
        """Get all unacknowledged alerts across all pens."""
        import database as db
        alerts = db.get_unacknowledged_alerts()
        return jsonify(alerts)

    @app.route('/api/alerts/<int:alert_id>/acknowledge', methods=['POST'])
    def ack_alert(alert_id):
        """Acknowledge an alert by ID."""
        import database as db
        db.acknowledge_alert(alert_id)
        return jsonify({'status': 'ok', 'message': f'Alert {alert_id} acknowledged'})

    # ──────────────────────────────────────────────────────────────
    # Manual Override Endpoint
    # ──────────────────────────────────────────────────────────────
    @app.route('/api/override/<int:pen_id>', methods=['POST'])
    def trigger_override(pen_id):
        """
        Trigger a manual override for a pen.
        Body: { "type": "feed"|"water", "amount": 2.0 }
          - For feed: amount in kg
          - For water: amount is target fill level in %
        """
        if pen_id < 1 or pen_id > NUM_PENS:
            return jsonify({'error': f'Invalid pen_id {pen_id}'}), 400

        import command_service as cs

        data = request.get_json()
        if not data:
            return jsonify({'error': 'Missing request body'}), 400

        override_type = data.get('type')
        amount = data.get('amount')

        if override_type not in ('feed', 'water'):
            return jsonify({'error': 'type must be "feed" or "water"'}), 400
        if amount is None or float(amount) <= 0:
            return jsonify({'error': 'amount must be a positive number'}), 400

        result = cs.dispatch_manual_override(pen_id, override_type, float(amount))
        logger.info(f"Manual override dispatched: Pen {pen_id}, {override_type}, {amount}")
        return jsonify({
            'status': 'ok',
            'message': f'Manual override dispatched for Pen {pen_id}',
            'details': result
        })

    # ──────────────────────────────────────────────────────────────
    # Feeding History Endpoint (for Consumption Charts)
    # ──────────────────────────────────────────────────────────────
    @app.route('/api/feeding-history/<int:pen_id>', methods=['GET'])
    def get_feeding_history(pen_id):
        """
        Get feeding event history for consumption charts.
        Query params: n (default 14), hours (default 168 = 7 days)
        """
        if pen_id < 1 or pen_id > NUM_PENS:
            return jsonify({'error': f'Invalid pen_id {pen_id}'}), 400

        import database as db

        n = request.args.get('n', 14, type=int)
        hours = request.args.get('hours', 168, type=int)

        # Return either last N events or events within time range
        if request.args.get('hours'):
            events = db.get_feeding_events_range(pen_id, hours=hours)
        else:
            events = db.get_feeding_history(pen_id, n=n)
        return jsonify(events)

    # ──────────────────────────────────────────────────────────────
    # System Status Endpoint
    # ──────────────────────────────────────────────────────────────
    @app.route('/api/status', methods=['GET'])
    def get_status():
        """Get system health status: uptime, connection info, service states."""
        import database as db
        import poller

        uptime = db.calculate_uptime(hours=72)

        # Connection status for each pen
        connections = {}
        for pen_id in range(1, NUM_PENS + 1):
            data = poller.get_latest(pen_id)
            if data:
                connections[pen_id] = {
                    'connected': True,
                    'source': data.get('source', 'unknown'),
                    'last_updated': data.get('last_updated', ''),
                }
            else:
                connections[pen_id] = {
                    'connected': False,
                    'source': 'No connection',
                    'last_updated': '',
                }

        return jsonify({
            'uptime_pct': uptime,
            'connections': connections,
            'num_pens': NUM_PENS,
        })

    return app


def start_server(app):
    """Start the Flask server. Called from main.py."""
    from config import SERVER_HOST, SERVER_PORT
    logger.info(f"Flask server starting on {SERVER_HOST}:{SERVER_PORT}")
    app.run(host=SERVER_HOST, port=SERVER_PORT, debug=False, use_reloader=False)
