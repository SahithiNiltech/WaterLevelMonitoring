from flask import Flask, jsonify
from flask_sqlalchemy import SQLAlchemy
from flask_mail import Mail
from flask_cors import CORS
from config import Config
from datetime import datetime
import os

db = SQLAlchemy()
mail = Mail()

def create_app():
    app = Flask(__name__)
    app.config.from_object(Config)

    db.init_app(app)
    mail.init_app(app)
    CORS(app, resources={r"/api/*": {"origins": "*"}})

    # Register blueprints
    from routes.device import device_bp
    from routes.firmware import firmware_bp

    app.register_blueprint(device_bp)
    app.register_blueprint(firmware_bp)

    # Health check endpoint (unauthenticated)
    @app.route('/health')
    def health():
        return jsonify({'status': 'ok'}), 200

    with app.app_context():
        db.create_all()
        # Create firmware folder if not exists
        os.makedirs(Config.FIRMWARE_FOLDER, exist_ok=True)

    return app

app = create_app()

# ─────────────────────────────────────────────────────────────
#  Database Models
# ─────────────────────────────────────────────────────────────

class DeviceReading(db.Model):
    """Stores sensor readings from device"""
    id = db.Column(db.Integer, primary_key=True)
    timestamp = db.Column(db.DateTime, default=datetime.utcnow)
    device_id = db.Column(db.String(100), nullable=False)
    water_level_pct = db.Column(db.Float, nullable=False)
    water_height_cm = db.Column(db.Float, nullable=False)
    battery_v = db.Column(db.Float, nullable=False)
    battery_pct = db.Column(db.Integer, nullable=False)
    
    def to_dict(self):
        return {
            'id': self.id,
            'timestamp': self.timestamp.isoformat(),
            'water_level_pct': self.water_level_pct,
            'water_height_cm': self.water_height_cm,
            'battery_pct': self.battery_pct,
        }

class AlertLog(db.Model):
    """Stores alert history"""
    id = db.Column(db.Integer, primary_key=True)
    timestamp = db.Column(db.DateTime, default=datetime.utcnow)
    alert_type = db.Column(db.String(50), nullable=False)  # battery_low, water_full, etc
    message = db.Column(db.String(255), nullable=False)
    sent = db.Column(db.Boolean, default=False)
    last_alerted_at = db.Column(db.DateTime, default=datetime.utcnow)

class FirmwareVersion(db.Model):
    """Stores firmware versions"""
    id = db.Column(db.Integer, primary_key=True)
    version = db.Column(db.String(20), unique=True, nullable=False)
    filename = db.Column(db.String(255), nullable=False)
    md5 = db.Column(db.String(32), nullable=False)
    size = db.Column(db.Integer, nullable=False)
    date = db.Column(db.DateTime, default=datetime.utcnow)
    is_current = db.Column(db.Boolean, default=False)

    def to_dict(self):
        return {
            'version': self.version,
            'size': self.size,
            'md5': self.md5,
            'date': self.date.isoformat(),
            'is_current': self.is_current,
        }

class FirmwareUpdateLog(db.Model):
    """Logs each time device applies a firmware update"""
    id = db.Column(db.Integer, primary_key=True)
    timestamp = db.Column(db.DateTime, default=datetime.utcnow)
    device_id = db.Column(db.String(100), nullable=False)
    from_version = db.Column(db.String(20))
    to_version = db.Column(db.String(20), nullable=False)
    status = db.Column(db.String(20), nullable=False)  # 'applied' or 'failed'
    message = db.Column(db.String(255))

    def to_dict(self):
        return {
            'id': self.id,
            'timestamp': self.timestamp.isoformat(),
            'device_id': self.device_id,
            'from_version': self.from_version,
            'to_version': self.to_version,
            'status': self.status,
            'message': self.message,
        }

if __name__ == '__main__':
    app.run(debug=True)
