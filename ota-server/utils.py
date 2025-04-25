"""
Utility functions for the OTA update server.
"""
import os
import logging
import hashlib

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
        logging.error("Invalid MAC address format: %s", mac_address)
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
        logging.error("File not found: %s", filepath)
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
        
    # Remove colons and convert to uppercase
    mac_clean = mac.replace(":", "").upper()
    
    # Check if it's 12 hex characters
    if len(mac_clean) != 12:
        return False
        
    # Check if all characters are hex
    try:
        int(mac_clean, 16)
        return True
    except ValueError:
        return False

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
        
    # Remove any existing colons and convert to uppercase
    mac_clean = mac.replace(":", "").upper()
    
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
        
    parts = version.split('.')
    if len(parts) < 1 or len(parts) > 3:
        return False
        
    # Check if all parts are integers
    try:
        for part in parts:
            int(part)
        return True
    except ValueError:
        return False