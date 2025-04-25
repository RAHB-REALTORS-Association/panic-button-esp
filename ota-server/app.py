"""
OTA Update Server for ESP32 devices - Main Application
"""
import os
import logging
from datetime import datetime

from flask import Flask, request, jsonify, send_from_directory

from config import load_config, get_config, get_devices, get_device, update_device
from utils import (
    generate_auth_token, 
    compare_versions, 
    validate_mac_address,
    validate_version
)

# --- Flask App Setup ---
app = Flask(__name__)

# --- API Endpoints ---

@app.route('/api/firmware', methods=['GET'])
def check_firmware_update():
    """
    Handles firmware update check requests from devices.
    """
    # 1. Get parameters and headers
    device_id = request.args.get('device_id')
    hardware = request.args.get('hardware')
    current_version_str = request.args.get('version')
    mac_address = request.args.get('mac', '').upper()  # Ensure MAC is uppercase
    auth_header = request.headers.get('X-Device-Auth')
    timestamp = datetime.now().isoformat()

    log_prefix = f"[MAC: {mac_address or 'N/A'}]"
    logging.info("%s Received update check: device_id=%s, hardware=%s, version=%s", log_prefix, device_id, hardware, current_version_str)

    # 2. Basic validation
    if not all([device_id, hardware, current_version_str, mac_address, auth_header]):
        logging.warning("%s Bad request - Missing parameters or auth header.", log_prefix)
        return jsonify({"error": "Missing required parameters or auth header"}), 400

    if not validate_mac_address(mac_address):
        logging.warning("%s Invalid MAC address format.", log_prefix)
        return jsonify({"error": "Invalid MAC address format"}), 400

    if not validate_version(current_version_str):
        logging.warning("%s Invalid version format: %s", log_prefix, current_version_str)
        return jsonify({"error": "Invalid version format"}), 400

    # 3. Check if MAC is authorized
    device_info = get_device(mac_address)
    if not device_info:
        logging.warning("%s Unauthorized MAC address.", log_prefix)
        return jsonify({"error": "Device not authorized"}), 403  # Forbidden

    # 4. Authenticate device
    config = get_config()
    expected_token = generate_auth_token(mac_address, config["shared_secret_key"])
    if not expected_token or auth_header.upper() != expected_token:
        logging.warning("%s Authentication failed.", log_prefix)
        return jsonify({"error": "Authentication failed"}), 401  # Unauthorized

    logging.info("%s Authentication successful.", log_prefix)

    # 5. Get target firmware info for this device
    target_version_str = device_info["target_version"]

    # 6. Compare versions
    try:
        version_comparison = compare_versions(target_version_str, current_version_str)
    except Exception as e:
        logging.error("%s Error comparing versions: %s", log_prefix, e)
        return jsonify({"error": "Version comparison error"}), 400

    # 7. Update device's last check timestamp
    device_info["last_check"] = timestamp
    update_device(mac_address, device_info)

    # 8. Prepare response
    if version_comparison > 0:
        # Update is available
        logging.info("%s Update available: Current=%s, Target=%s", log_prefix, current_version_str, target_version_str)
        response_data = {
            "update_available": True,
            "firmware_version": target_version_str,
            "firmware_url": device_info["firmware_url"],
            "checksum": device_info["checksum"]
        }
        return jsonify(response_data), 200
    else:
        # No update needed (or device has a newer version somehow)
        logging.info("%s No update needed. Current=%s, Target=%s", log_prefix, current_version_str, target_version_str)
        response_data = {
            "update_available": False
        }
        return jsonify(response_data), 200

@app.route('/firmware/<filename>', methods=['GET'])
def download_firmware(filename):
    """
    Serves firmware binary files from the firmware directory.
    """
    config = get_config()
    firmware_dir = config.get("firmware_directory", "firmware")
    
    # Make sure the directory exists
    if not os.path.exists(firmware_dir):
        os.makedirs(firmware_dir)
        
    return send_from_directory(firmware_dir, filename)

# --- Admin API Routes ---

def verify_admin_api_key() -> bool:
    """Check if the admin API key is valid"""
    config = get_config()
    api_key = request.headers.get('X-Admin-API-Key')
    if not api_key:
        return False
    return api_key == config.get("admin_api_key")

@app.route('/admin/devices', methods=['GET'])
def list_devices():
    """List all registered devices"""
    if not verify_admin_api_key():
        return jsonify({"error": "Unauthorized"}), 401
        
    devices = get_devices()
    return jsonify(devices), 200

@app.route('/admin/devices/<mac_address>', methods=['GET'])
def get_device_info(mac_address):
    """Get information about a specific device"""
    if not verify_admin_api_key():
        return jsonify({"error": "Unauthorized"}), 401
        
    device_info = get_device(mac_address)
    if not device_info:
        return jsonify({"error": "Device not found"}), 404
        
    return jsonify(device_info), 200

@app.route('/admin/devices/<mac_address>', methods=['PUT'])
def update_device_info(mac_address):
    """Update device information"""
    if not verify_admin_api_key():
        return jsonify({"error": "Unauthorized"}), 401
        
    # Format MAC address properly
    if not validate_mac_address(mac_address):
        return jsonify({"error": "Invalid MAC address format"}), 400
        
    device_info = request.json
    if not device_info:
        return jsonify({"error": "No data provided"}), 400
        
    # Validate required fields
    required_fields = ["target_version", "firmware_url", "checksum"]
    for field in required_fields:
        if field not in device_info:
            return jsonify({"error": f"Missing required field: {field}"}), 400
            
    # Validate version format
    if not validate_version(device_info["target_version"]):
        return jsonify({"error": "Invalid target version format"}), 400
        
    # Update the device
    success = update_device(mac_address, device_info)
    if not success:
        return jsonify({"error": "Failed to update device"}), 500
        
    return jsonify({"success": True}), 200

@app.route('/admin/devices/<mac_address>', methods=['DELETE'])
def delete_device_info(mac_address):
    """Delete a device"""
    if not verify_admin_api_key():
        return jsonify({"error": "Unauthorized"}), 401
        
    from config import delete_device
    
    if not validate_mac_address(mac_address):
        return jsonify({"error": "Invalid MAC address format"}), 400
        
    success = delete_device(mac_address)
    if not success:
        return jsonify({"error": "Device not found or could not be deleted"}), 404
        
    return jsonify({"success": True}), 200

@app.route('/admin/devices', methods=['POST'])
def add_device():
    """Add a new device"""
    if not verify_admin_api_key():
        return jsonify({"error": "Unauthorized"}), 401
        
    device_data = request.json
    if not device_data or "mac_address" not in device_data:
        return jsonify({"error": "MAC address required"}), 400
        
    mac_address = device_data.pop("mac_address")
    if not validate_mac_address(mac_address):
        return jsonify({"error": "Invalid MAC address format"}), 400
        
    # Check required fields
    required_fields = ["target_version", "firmware_url", "checksum"]
    for field in required_fields:
        if field not in device_data:
            return jsonify({"error": f"Missing required field: {field}"}), 400
            
    # Check if device already exists
    existing_device = get_device(mac_address)
    if existing_device:
        return jsonify({"error": "Device already exists"}), 409
        
    # Add the device
    success = update_device(mac_address, device_data)
    if not success:
        return jsonify({"error": "Failed to add device"}), 500
        
    return jsonify({"success": True}), 201

# --- Status Endpoint ---
@app.route('/status', methods=['GET'])
def status():
    """Simple status endpoint to check if server is running"""
    return jsonify({
        "status": "ok",
        "timestamp": datetime.now().isoformat(),
        "version": "1.0.0"
    }), 200

# --- Main Execution ---

def main():
    """Main function to start the server"""
    # Load configuration
    config = load_config()
    
    # Configure logging
    logging.basicConfig(
        level=getattr(logging, config["log_level"]), 
        format='%(asctime)s - %(levelname)s - %(message)s'
    )
    
    # Start server
    logging.info("Starting OTA Update Server...")
    app.run(
        host=config["server_host"], 
        port=config["server_port"], 
        debug=config["debug_mode"]
    )

if __name__ == '__main__':
    main()