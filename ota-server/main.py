import hashlib
import logging
from flask import Flask, request, jsonify

# --- Configuration ---

# IMPORTANT: Use the EXACT same secret key as defined in your ESP32 code's OTA_UPDATE_KEY
SHARED_SECRET_KEY = "your-device-secret-key" # <<<--- REPLACE THIS

# Define authorized devices and their target firmware
# Keys are MAC addresses (UPPERCASE, with colons)
# Values contain target version, URL to the firmware binary, and its MD5 checksum
AUTHORIZED_DEVICES = {
    "AA:BB:CC:DD:EE:FF": { # Example MAC 1
        "target_version": "1.2.1",
        "firmware_url": "http://<YOUR_SERVER_IP_OR_DOMAIN>/firmware/PanicButton_v1.2.1.bin", # <<<--- REPLACE
        "checksum": "<MD5_CHECKSUM_FOR_1.2.1_BIN>" # <<<--- REPLACE (e.g., output of 'md5sum PanicButton_v1.2.1.bin')
    },
    "11:22:33:44:55:66": { # Example MAC 2
        "target_version": "1.3.0", # Maybe this device gets a newer version
        "firmware_url": "http://<YOUR_SERVER_IP_OR_DOMAIN>/firmware/PanicButton_v1.3.0.bin", # <<<--- REPLACE
        "checksum": "<MD5_CHECKSUM_FOR_1.3.0_BIN>" # <<<--- REPLACE
    },
    # Add more authorized MAC addresses and their firmware details here
}

# --- Flask App Setup ---
app = Flask(__name__)

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

# --- Helper Functions ---

def generate_auth_token(mac_address: str, secret: str) -> str:
    """
    Generates the expected authentication token based on MAC and secret.
    MUST match the logic in the ESP32's generateAuthToken() function.
    """
    if not mac_address or not secret:
        return ""
    mac_clean = mac_address.replace(":", "").upper() # Remove colons, ensure uppercase
    token_string = mac_clean + secret

    # Simple hash (djb2 variant - matching the C++ code)
    hash_value = 0
    for char in token_string:
        hash_value = (((hash_value << 5) + hash_value) + ord(char)) & 0xFFFFFFFF # Ensure 32-bit unsigned

    return format(hash_value, 'X') # Return as uppercase Hex String

def compare_versions(v1: str, v2: str) -> int:
    """
    Compares two semantic version strings (e.g., "1.2.0").
    Returns > 0 if v1 > v2, < 0 if v1 < v2, 0 if equal.
    Handles missing patch/minor versions by treating them as 0.
    """
    def parse_version(v):
        parts = list(map(int, v.split('.')))
        while len(parts) < 3:
            parts.append(0) # Pad with zeros (e.g., "1.2" becomes [1, 2, 0])
        return parts

    v1_parts = parse_version(v1)
    v2_parts = parse_version(v2)

    if v1_parts[0] != v2_parts[0]:
        return v1_parts[0] - v2_parts[0]
    if v1_parts[1] != v2_parts[1]:
        return v1_parts[1] - v2_parts[1]
    return v1_parts[2] - v2_parts[2]

# --- API Endpoint ---

@app.route('/api/firmware', methods=['GET'])
def check_firmware_update():
    """
    Handles firmware update check requests from devices.
    """
    # 1. Get parameters and headers
    device_id = request.args.get('device_id')
    hardware = request.args.get('hardware')
    current_version_str = request.args.get('version')
    mac_address = request.args.get('mac', '').upper() # Ensure MAC is uppercase
    auth_header = request.headers.get('X-Device-Auth')

    log_prefix = f"[MAC: {mac_address or 'N/A'}]"
    logging.info(f"{log_prefix} Received update check: device_id={device_id}, hardware={hardware}, version={current_version_str}, auth={auth_header is not None}")

    # 2. Basic validation
    if not all([device_id, hardware, current_version_str, mac_address, auth_header]):
        logging.warning(f"{log_prefix} Bad request - Missing parameters or auth header.")
        return jsonify({"error": "Missing required parameters or auth header"}), 400

    # 3. Check if MAC is authorized
    if mac_address not in AUTHORIZED_DEVICES:
        logging.warning(f"{log_prefix} Unauthorized MAC address.")
        return jsonify({"error": "Device MAC not authorized"}), 403 # Forbidden

    # 4. Authenticate device
    expected_token = generate_auth_token(mac_address, SHARED_SECRET_KEY)
    if not expected_token or auth_header.upper() != expected_token: # Compare case-insensitively just in case
        logging.warning(f"{log_prefix} Authentication failed. Received: {auth_header}, Expected: {expected_token}")
        return jsonify({"error": "Authentication failed"}), 401 # Unauthorized

    logging.info(f"{log_prefix} Authentication successful.")

    # 5. Get target firmware info for this device
    target_info = AUTHORIZED_DEVICES[mac_address]
    target_version_str = target_info["target_version"]

    # 6. Compare versions
    try:
        version_comparison = compare_versions(target_version_str, current_version_str)
    except Exception as e:
        logging.error(f"{log_prefix} Error comparing versions '{target_version_str}' vs '{current_version_str}': {e}")
        return jsonify({"error": "Invalid version format"}), 400

    # 7. Prepare response
    if version_comparison > 0:
        # Update is available
        logging.info(f"{log_prefix} Update available: Current={current_version_str}, Target={target_version_str}")
        response_data = {
            "update_available": True,
            "firmware_version": target_version_str,
            "firmware_url": target_info["firmware_url"],
            "checksum": target_info["checksum"] # Provide the checksum (MD5 recommended)
        }
        return jsonify(response_data), 200
    else:
        # No update needed (or device has a newer version somehow)
        logging.info(f"{log_prefix} No update needed. Current={current_version_str}, Target={target_version_str}")
        response_data = {
            "update_available": False
        }
        return jsonify(response_data), 200

# --- Main Execution ---
if __name__ == '__main__':
    logging.info("Starting OTA Update Server...")
    # Run on all available network interfaces (0.0.0.0) and default Flask port 5000
    # Use port=80 or port=443 (with SSL) for production if needed
    app.run(host='0.0.0.0', port=5000, debug=False) # Set debug=False for production