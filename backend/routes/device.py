from flask import Blueprint, request, jsonify
from app import db, mail
from datetime import datetime, timedelta
from flask_mail import Message
from config import Config
from utils import check_api_key

device_bp = Blueprint('device', __name__, url_prefix='/api')

@device_bp.route('/device/data', methods=['POST'])
def post_device_data():
    """Receive sensor data from device"""
    if not check_api_key():
        return jsonify({'error': 'Unauthorized'}), 401
    
    data = request.json
    if not data:
        return jsonify({'error': 'No data'}), 400
    
    device_id = data.get('device_id', Config.DEVICE_ID)
    
    # Import here to avoid circular imports
    from app import DeviceReading, AlertLog
    
    # Create reading
    reading = DeviceReading(
        device_id=device_id,
        water_level_pct=float(data.get('water_level_pct', 0)),
        water_height_cm=float(data.get('water_height_cm', 0)),
        battery_v=float(data.get('battery_v', 0)),
        battery_pct=int(data.get('battery_pct', 0))
    )
    
    db.session.add(reading)
    db.session.commit()
    
    # Check thresholds and send alerts
    check_thresholds(reading)
    
    return jsonify({'status': 'ok', 'reading_id': reading.id}), 201

@device_bp.route('/device/status', methods=['GET'])
def get_device_status():
    """Get latest device status"""
    if not check_api_key():
        return jsonify({'error': 'Unauthorized'}), 401

    from app import DeviceReading

    # Get latest reading
    latest = DeviceReading.query.order_by(DeviceReading.timestamp.desc()).first()

    if not latest:
        return jsonify({'error': 'No data'}), 404

    return jsonify({
        'water_level': latest.water_level_pct,
        'water_level_pct': latest.water_level_pct,
        'water_height_cm': latest.water_height_cm,
        'battery': latest.battery_pct,
        'battery_pct': latest.battery_pct,
        'battery_v': latest.battery_v,
        'timestamp': latest.timestamp.isoformat(),
    }), 200

@device_bp.route('/device/history', methods=['GET'])
def get_device_history():
    """Get historical sensor data"""
    if not check_api_key():
        return jsonify({'error': 'Unauthorized'}), 401

    from app import DeviceReading

    days = request.args.get('days', 7, type=int)
    start_date = datetime.utcnow() - timedelta(days=days)

    readings = DeviceReading.query.filter(
        DeviceReading.timestamp >= start_date
    ).order_by(DeviceReading.timestamp).all()

    return jsonify([r.to_dict() for r in readings]), 200

@device_bp.route('/device/alerts', methods=['GET'])
def get_alerts():
    """Get alert history"""
    if not check_api_key():
        return jsonify({'error': 'Unauthorized'}), 401

    from app import AlertLog

    limit = request.args.get('limit', 20, type=int)
    alerts = AlertLog.query.order_by(AlertLog.timestamp.desc()).limit(limit).all()

    return jsonify([{
        'id': a.id,
        'timestamp': a.timestamp.isoformat(),
        'alert_type': a.alert_type,
        'message': a.message,
        'sent': a.sent
    } for a in alerts]), 200

def should_send_alert(alert_type):
    """Check if we should send an alert (cooldown period)"""
    from app import AlertLog

    cooldown = timedelta(hours=Config.ALERT_COOLDOWN_HOURS)
    recent = AlertLog.query.filter(
        AlertLog.alert_type == alert_type,
        AlertLog.timestamp >= datetime.utcnow() - cooldown
    ).first()
    return recent is None

def check_thresholds(reading):
    """Check if reading exceeds thresholds and send alerts"""
    from app import AlertLog

    alerts = []

    # Battery low
    if reading.battery_pct < Config.BATTERY_LOW_PCT:
        alerts.append({
            'type': 'battery_low',
            'message': f'⚠️ ALERT: Battery Low: {reading.battery_pct}%',
            'subject': 'Water Monitor - Battery Low'
        })

    # Water full
    if reading.water_level_pct >= Config.WATER_FULL_PCT:
        alerts.append({
            'type': 'water_full',
            'message': f'🚨 ALERT: Water Tank FULL: {reading.water_level_pct}%',
            'subject': 'Water Monitor - Tank Full'
        })
    # Water low 30%
    elif reading.water_level_pct <= Config.WATER_LOW_30_PCT:
        alerts.append({
            'type': 'water_low_30',
            'message': f'⚠️ WARNING: Water Level Low (30%): {reading.water_level_pct}%',
            'subject': 'Water Monitor - Water Low'
        })
    # Water low 20%
    if reading.water_level_pct <= Config.WATER_LOW_20_PCT:
        alerts.append({
            'type': 'water_low_20',
            'message': f'⚠️ WARNING: Water Level Low (20%): {reading.water_level_pct}%',
            'subject': 'Water Monitor - Water Low'
        })
    # Water critical
    if reading.water_level_pct <= Config.WATER_CRITICAL_PCT:
        alerts.append({
            'type': 'water_critical',
            'message': f'🚨 ALERT: Water Level CRITICAL: {reading.water_level_pct}%',
            'subject': 'Water Monitor - Critical Level'
        })

    # Send alerts with cooldown check
    for alert in alerts:
        if should_send_alert(alert['type']):
            log = AlertLog(
                alert_type=alert['type'],
                message=alert['message']
            )
            db.session.add(log)

            # Send email
            try:
                send_alert_email(alert['subject'], alert['message'], reading)
            except Exception as e:
                print(f"Error sending email: {e}")

    if alerts:
        db.session.commit()

def send_alert_email(subject, message, reading):
    """Send alert email via Gmail"""
    try:
        body = f"""{message}

Device Details:
- Water Level: {reading.water_level_pct:.1f}%
- Water Height: {reading.water_height_cm:.1f} cm
- Battery: {reading.battery_pct}%
- Battery Voltage: {reading.battery_v:.2f}V
- Timestamp: {reading.timestamp.isoformat()}

Check your dashboard for more details.
"""
        msg = Message(
            subject=subject,
            recipients=[Config.NOTIFICATION_EMAIL],
            body=body
        )
        mail.send(msg)
    except Exception as e:
        print(f"Error sending email: {e}")
