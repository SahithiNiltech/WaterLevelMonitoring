import os
from dotenv import load_dotenv

load_dotenv()

class Config:
    # Flask
    SECRET_KEY = os.getenv('SECRET_KEY', 'dev-secret-key-change-in-production')

    # Database — PostgreSQL via Supabase (or SQLite for local dev)
    database_url = os.getenv('DATABASE_URL')
    if database_url:
        # Fix SQLAlchemy 1.4+ requirement: postgres:// → postgresql://
        if database_url.startswith('postgres://'):
            database_url = database_url.replace('postgres://', 'postgresql://', 1)
        SQLALCHEMY_DATABASE_URI = database_url
    else:
        # Fallback to SQLite for local development
        SQLALCHEMY_DATABASE_URI = 'sqlite:///water_monitor.db'

    SQLALCHEMY_TRACK_MODIFICATIONS = False
    SQLALCHEMY_ENGINE_OPTIONS = {
        'pool_pre_ping': True,        # Test connections before using (handles Render spin-down)
        'pool_recycle': 300,          # Recycle connections after 5 minutes
    }

    # API Auth
    API_KEY = os.getenv('API_KEY', 'your-api-key-12345')
    DEVICE_ID = 'WaterLevelMonitor_001'

    # Email (Gmail)
    MAIL_SERVER = 'smtp.gmail.com'
    MAIL_PORT = 587
    MAIL_USE_TLS = True
    MAIL_USERNAME = os.getenv('MAIL_USERNAME', 'your-email@gmail.com')
    MAIL_PASSWORD = os.getenv('MAIL_PASSWORD', 'your-app-password')
    MAIL_DEFAULT_SENDER = os.getenv('MAIL_USERNAME', 'your-email@gmail.com')
    NOTIFICATION_EMAIL = os.getenv('NOTIFICATION_EMAIL', 'sahithireddy837@gmail.com')

    # Alert Thresholds & Cooldown
    BATTERY_LOW_PCT = 20
    WATER_FULL_PCT = 95
    WATER_LOW_30_PCT = 30
    WATER_LOW_20_PCT = 20
    WATER_CRITICAL_PCT = 10
    ALERT_COOLDOWN_HOURS = 4
    FIRMWARE_APPLIED_NOTIFICATION = True

    # Firmware
    FIRMWARE_FOLDER = os.getenv('FIRMWARE_FOLDER', 'firmware')
    CURRENT_VERSION = '1.0'
