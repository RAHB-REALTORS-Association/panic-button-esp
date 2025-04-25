"""
Configuration management for the OTA update server.
"""
import os
import json
import logging
from typing import Dict, Any, Optional

# Default config values
DEFAULT_CONFIG = {
    "shared_secret_key": "change-this-key-in-production",
    "server_port": 5000,
    "server_host": "0.0.0.0",
    "debug_mode": False,
    "log_level": "INFO",
    "devices_file": "devices.json"
}

# Global config dictionary
_config: Dict[str, Any] = {}
# Global devices dictionary
_devices: Dict[str, Dict[str, Any]] = {}

def load_config(config_file: str = "config.json") -> Dict[str, Any]:
    """
    Load configuration from file with environment variable overrides.
    
    Args:
        config_file: Path to the JSON config file
        
    Returns:
        Dict containing the merged configuration
    """
    global _config
    
    # Start with defaults
    config = DEFAULT_CONFIG.copy()
    
    # Try to load from file
    try:
        if os.path.exists(config_file):
            with open(config_file, 'r', encoding='utf-8') as f:
                file_config = json.load(f)
                config.update(file_config)
            logging.info("Loaded configuration from %s", config_file)
        else:
            logging.warning("Config file %s not found, using defaults", config_file)
    except Exception as e:
        logging.error("Error loading config file: %s", e)
    
    # Override with environment variables
    # Format: OTA_SERVER_KEY_NAME (e.g., OTA_SERVER_SHARED_SECRET_KEY)
    for key, value in DEFAULT_CONFIG.items():
        env_key = f"OTA_SERVER_{key.upper()}"
        if env_key in os.environ:
            # Handle type conversion
            if isinstance(value, bool):
                config[key] = os.environ[env_key].strip().lower() in ('true', 'yes', '1')
            elif isinstance(value, int):
                config[key] = int(os.environ[env_key].strip())
            else:
                config[key] = os.environ[env_key].strip()
            logging.info("Overriding %s from environment variable", key)
    # Store in module-level variable
    _config = config
    
    # Set log level
    logging.basicConfig(
        level=getattr(logging, config["log_level"]), 
        format='%(asctime)s - %(levelname)s - %(message)s'
    )
    
    return config

def get_config() -> Dict[str, Any]:
    """Get the current configuration"""
    global _config
    if not _config:
        return load_config()
    return _config

def load_devices() -> Dict[str, Dict[str, Any]]:
    """
    Load device information from the devices file
    
    Returns:
        Dict of device configurations indexed by MAC address
    """
    global _devices
    
    config = get_config()
    devices_file = config["devices_file"]
    
    if not os.path.exists(devices_file):
        logging.warning("Devices file %s not found", devices_file)
        _devices = {}
        return _devices
    
    try:
        with open(devices_file, 'r', encoding='utf-8') as f:
            devices = json.load(f)
        
        # Convert all MAC addresses to uppercase for consistency
        _devices = {mac.upper(): device_info for mac, device_info in devices.items()}
        
        logging.info("Loaded %d devices from %s", len(_devices), devices_file)
        return _devices
    except Exception as e:
        logging.error("Error loading devices file: %s", e)
        _devices = {}
        return _devices

def get_devices() -> Dict[str, Dict[str, Any]]:
    """Get the current device configurations"""
    global _devices
    if not _devices:
        return load_devices()
    return _devices

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

def save_devices() -> bool:
    """
    Save the current device configurations to the devices file
    
    Returns:
        True if successful, False otherwise
    """
    global _devices
    config = get_config()
    devices_file = config["devices_file"]
    
    try:
        with open(devices_file, 'w', encoding='utf-8') as f:
            json.dump(_devices, f, indent=2)
        logging.info("Saved %d devices to %s", len(_devices), devices_file)
        return True
    except Exception as e:
        logging.error("Error saving devices file: %s", e)
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
    global _devices
    _devices[mac_address.upper()] = device_info
    return save_devices()

def delete_device(mac_address: str) -> bool:
    """
    Delete a device from the configuration
    
    Args:
        mac_address: MAC address of the device (case insensitive)
        
    Returns:
        True if successful, False otherwise
    """
    global _devices
    mac_upper = mac_address.upper()
    if mac_upper in _devices:
        del _devices[mac_upper]
        return save_devices()
    return False