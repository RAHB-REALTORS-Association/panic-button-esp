"""
Utility functions for the OTA update server.
"""
import os
import logging
import hashlib
import re
from typing import Dict, Any, Optional, List, Tuple
import json

from app.config import get_settings, get_config

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

def generate_auth_token(mac_address: str, secret: str) -> str:
    """
    Generates the expected authentication token based on MAC and secret.
    MUST match the logic in the ESP32's generateAuthToken() function.
    
    Args:
        mac_address: The MAC address of the device
        secret: The shared secret key
        
    Returns:
        Uppercase hex string of the hash
    """
    if not mac_address or not secret:
        logger.error("Invalid MAC address format: %s", mac_address)
        return ""
    mac_clean = mac_address.replace(":", "").upper()  # Remove colons, ensure uppercase
    token_string = mac_clean + secret

    # Simple hash (djb2 variant - matching the C++ code)
    hash_value = 0
    for char in token_string:
        hash_value = (((hash_value << 5) + hash_value) + ord(char)) & 0xFFFFFFFF  # Ensure 32-bit unsigned

    return format(hash_value, 'X')  # Return as uppercase Hex String

def compare_versions(v1: str, v2: str) -> int:
    """
    Compares two semantic version strings (e.g., "1.2.0").
    
    Args:
        v1: First version string
        v2: Second version string
        
    Returns:
        > 0 if v1 > v2, < 0 if v1 < v2, 0 if equal
    """
    def parse_version(v):
        parts = list(map(int, v.split('.')))
        while len(parts) < 3:
            parts.append(0)  # Pad with zeros (e.g., "1.2" becomes [1, 2, 0])
        return parts

    v1_parts = parse_version(v1)
    v2_parts = parse_version(v2)

    if v1_parts[0] != v2_parts[0]:
        return v1_parts[0] - v2_parts[0]
    if v1_parts[1] != v2_parts[1]:
        return v1_parts[1] - v2_parts[1]
    return v1_parts[2] - v2_parts[2]

def calculate_file_md5(filepath: str) -> str:
    """
    Calculate MD5 hash of a file
    
    Args:
        filepath: Path to the file
        
    Returns:
        MD5 hash as a hexadecimal string
    """
    if not os.path.exists(filepath):
        logger.error("File not found: %s", filepath)
        return ""
        
    hash_md5 = hashlib.md5()
    with open(filepath, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            hash_md5.update(chunk)
    return hash_md5.hexdigest()

def validate_mac_address(mac: str) -> bool:
    """
    Validate MAC address format
    
    Args:
        mac: MAC address string
        
    Returns:
        True if valid, False otherwise
    """
    if not mac:
        return False
        
    # Check if it's a valid MAC address format (with or without colons)
    pattern = re.compile(r'^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$|^([0-9A-Fa-f]{12})$')
    return bool(pattern.match(mac))

def format_mac_address(mac: str) -> str:
    """
    Format a MAC address with colons (AA:BB:CC:DD:EE:FF)
    
    Args:
        mac: Raw MAC address (with or without colons)
        
    Returns:
        Formatted MAC address or empty string if invalid
    """
    if not validate_mac_address(mac):
        return ""
        
    # Remove any existing colons/hyphens and convert to uppercase
    mac_clean = re.sub(r'[:-]', '', mac).upper()
    
    # Insert colons
    return ':'.join(mac_clean[i:i+2] for i in range(0, 12, 2))

def validate_version(version: str) -> bool:
    """
    Validate semantic version format (e.g., "1.2.3")
    
    Args:
        version: Version string
        
    Returns:
        True if valid, False otherwise
    """
    if not version:
        return False
        
    # Check format: x.y.z where x, y, z are non-negative integers
    pattern = re.compile(r'^\d+(\.\d+){0,2}$')
    return bool(pattern.match(version))

# Device management functions
def get_devices() -> Dict[str, Dict[str, Any]]:
    """
    Load device information from the devices file
    
    Returns:
        Dict of device configurations indexed by MAC address
    """
    settings = get_settings()
    devices_file = settings.devices_file
    
    if not os.path.exists(devices_file):
        logger.warning("Devices file %s not found, creating empty file", devices_file)
        with open(devices_file, 'w', encoding='utf-8') as f:
            json.dump({}, f)
        return {}
    
    try:
        with open(devices_file, 'r', encoding='utf-8') as f:
            devices = json.load(f)
        
        # Convert all MAC addresses to uppercase for consistency
        devices = {mac.upper(): device_info for mac, device_info in devices.items()}
        
        logger.info("Loaded %d devices from %s", len(devices), devices_file)
        return devices
    except Exception as e:
        logger.error("Error loading devices file: %s", e)
        return {}

def get_device(mac_address: str) -> Optional[Dict[str, Any]]:
    """
    Get a specific device's configuration
    
    Args:
        mac_address: MAC address of the device (case insensitive)
        
    Returns:
        Device configuration dict or None if not found
    """
    devices = get_devices()
    return devices.get(mac_address.upper())

def save_devices(devices: Dict[str, Dict[str, Any]]) -> bool:
    """
    Save the device configurations to the devices file
    
    Args:
        devices: Dict of device configurations indexed by MAC address
        
    Returns:
        True if successful, False otherwise
    """
    settings = get_settings()
    devices_file = settings.devices_file
    
    try:
        with open(devices_file, 'w', encoding='utf-8') as f:
            json.dump(devices, f, indent=2)
        logger.info("Saved %d devices to %s", len(devices), devices_file)
        return True
    except Exception as e:
        logger.error("Error saving devices file: %s", e)
        return False

def update_device(mac_address: str, device_info: Dict[str, Any]) -> bool:
    """
    Update a device's configuration
    
    Args:
        mac_address: MAC address of the device (case insensitive)
        device_info: New device configuration
        
    Returns:
        True if successful, False otherwise
    """
    devices = get_devices()
    devices[mac_address.upper()] = device_info
    return save_devices(devices)

def delete_device(mac_address: str) -> bool:
    """
    Delete a device from the configuration
    
    Args:
        mac_address: MAC address of the device (case insensitive)
        
    Returns:
        True if successful, False otherwise
    """
    devices = get_devices()
    mac_upper = mac_address.upper()
    if mac_upper in devices:
        del devices[mac_upper]
        return save_devices(devices)
    return False

def list_firmware_files() -> List[Dict[str, str]]:
    """
    List all firmware files in the firmware directory
    
    Returns:
        List of dicts with filename and path
    """
    settings = get_settings()
    firmware_dir = settings.firmware_directory
    
    if not os.path.exists(firmware_dir):
        os.makedirs(firmware_dir)
        
    files = []
    for filename in os.listdir(firmware_dir):
        if os.path.isfile(os.path.join(firmware_dir, filename)):
            filepath = os.path.join(firmware_dir, filename)
            files.append({
                "filename": filename,
                "path": filepath,
                "checksum": calculate_file_md5(filepath)
            })
    
    return files