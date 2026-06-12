from flask import Blueprint, request, jsonify, send_file
from app import db, mail
from flask_mail import Message
from datetime import datetime
import os
import hashlib
from config import Config
from utils import check_api_key

firmware_bp = Blueprint('firmware', __name__, url_prefix='/api')

@firmware_bp.route('/firmware/check', methods=['GET'])
def check_firmware():
    """Device checks for available firmware update"""
    if not check_api_key():
        return jsonify({'error': 'Unauthorized'}), 401

    from app import FirmwareVersion

    current_version = request.args.get('current_version', '1.0')

    # Get current firmware version
    current_fw = FirmwareVersion.query.filter_by(is_current=True).first()

    if not current_fw:
        return jsonify({'update_available': False, 'current': current_version}), 200

    update_available = current_fw.version != current_version

    return jsonify({
        'current': current_fw.version,
        'update_available': update_available,
        'size': current_fw.size,
        'md5': current_fw.md5,
        'download_url': f'/api/firmware/download/{current_fw.version}?api_key={Config.API_KEY}'
    }), 200

@firmware_bp.route('/firmware/download/<version>', methods=['GET'])
def download_firmware(version):
    """Device downloads firmware binary"""
    if not check_api_key():
        return jsonify({'error': 'Unauthorized'}), 401

    from app import FirmwareVersion

    fw = FirmwareVersion.query.filter_by(version=version).first()
    if not fw:
        return jsonify({'error': 'Version not found'}), 404

    file_path = os.path.join(Config.FIRMWARE_FOLDER, fw.filename)
    if not os.path.exists(file_path):
        return jsonify({'error': 'File not found'}), 404

    return send_file(file_path, mimetype='application/octet-stream', as_attachment=True)

@firmware_bp.route('/firmware/upload', methods=['POST'])
def upload_firmware():
    """Upload new firmware via dashboard"""
    if not check_api_key():
        return jsonify({'error': 'Unauthorized'}), 401

    from app import FirmwareVersion

    if 'file' not in request.files:
        return jsonify({'error': 'No file part'}), 400

    file = request.files['file']
    version = request.form.get('version', '1.1')
    set_current = request.form.get('set_current', 'false').lower() == 'true'

    if file.filename == '':
        return jsonify({'error': 'No selected file'}), 400

    # Save file
    os.makedirs(Config.FIRMWARE_FOLDER, exist_ok=True)
    filename = f'firmware_v{version}.bin'
    filepath = os.path.join(Config.FIRMWARE_FOLDER, filename)
    file.save(filepath)

    # Calculate MD5
    md5 = hashlib.md5(open(filepath, 'rb').read()).hexdigest()
    size = os.path.getsize(filepath)

    # Save to database
    fw = FirmwareVersion(
        version=version,
        filename=filename,
        md5=md5,
        size=size,
        is_current=set_current
    )
    # If setting as current, clear previous current
    if set_current:
        FirmwareVersion.query.update({'is_current': False})
    db.session.add(fw)
    db.session.commit()

    return jsonify({'version': version, 'md5': md5, 'size': size, 'set_current': set_current}), 201

@firmware_bp.route('/firmware/set-current/<version>', methods=['PUT'])
def set_current_firmware(version):
    """Set a firmware version as current"""
    if not check_api_key():
        return jsonify({'error': 'Unauthorized'}), 401
    
    from app import FirmwareVersion
    
    # Clear current
    FirmwareVersion.query.update({'is_current': False})
    
    # Set new current
    fw = FirmwareVersion.query.filter_by(version=version).first()
    if not fw:
        return jsonify({'error': 'Version not found'}), 404
    
    fw.is_current = True
    db.session.commit()
    
    return jsonify({'status': 'ok', 'current': version}), 200

@firmware_bp.route('/firmware/applied', methods=['POST'])
def firmware_applied():
    """Device notifies that firmware update was applied"""
    if not check_api_key():
        return jsonify({'error': 'Unauthorized'}), 401

    from app import FirmwareUpdateLog

    data = request.json or {}

    log = FirmwareUpdateLog(
        device_id=data.get('device_id', Config.DEVICE_ID),
        from_version=data.get('from_version', 'unknown'),
        to_version=data.get('to_version', 'unknown'),
        status=data.get('status', 'applied'),
        message=data.get('message', '')
    )
    db.session.add(log)
    db.session.commit()

    # Send notification email
    if Config.FIRMWARE_APPLIED_NOTIFICATION:
        try:
            body = f"""Firmware Update Applied Successfully

Device: {log.device_id}
From Version: v{log.from_version}
To Version: v{log.to_version}
Status: {log.status}
Time: {log.timestamp.isoformat()}

{f"Message: {log.message}" if log.message else ""}
"""
            msg = Message(
                subject='Water Monitor - Firmware Updated',
                recipients=[Config.NOTIFICATION_EMAIL],
                body=body
            )
            mail.send(msg)
        except Exception as e:
            print(f"Error sending firmware notification email: {e}")

    return jsonify({'status': 'ok', 'log_id': log.id}), 201

@firmware_bp.route('/firmware/update-log', methods=['GET'])
def get_update_log():
    """Get firmware update history"""
    from app import FirmwareUpdateLog

    logs = FirmwareUpdateLog.query.order_by(FirmwareUpdateLog.timestamp.desc()).limit(20).all()
    return jsonify([l.to_dict() for l in logs]), 200

@firmware_bp.route('/firmware/list', methods=['GET'])
def list_firmware():
    """List all available firmware versions"""
    from app import FirmwareVersion

    versions = FirmwareVersion.query.all()
    return jsonify([fw.to_dict() for fw in versions]), 200
