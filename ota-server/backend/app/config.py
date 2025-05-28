"""
Configuration management for the OTA update server using Pydantic.
"""
import os
import json
from pathlib import Path
from typing import Dict, Any, Optional
from pydantic import BaseSettings, Field

# Default config values
class Settings(BaseSettings):
    """Configuration settings with environment variable support."""
    shared_secret_key: str = Field("change-this-key-in-production", env="OTA_SERVER_SHARED_SECRET_KEY")
    admin_api_key: str = Field("change-admin-api-key-in-production", env="OTA_SERVER_ADMIN_API_KEY")
    server_port: int = Field(5000, env="OTA_SERVER_SERVER_PORT")
    server_host: str = Field("0.0.0.0", env="OTA_SERVER_SERVER_HOST")
    debug_mode: bool = Field(False, env="OTA_SERVER_DEBUG_MODE")
    log_level: str = Field("INFO", env="OTA_SERVER_LOG_LEVEL")
    devices_file: str = Field("devices.json", env="OTA_SERVER_DEVICES_FILE")
    firmware_directory: str = Field("firmware", env="OTA_SERVER_FIRMWARE_DIR")

    class Config:
        env_file = ".env"
        case_sensitive = False

# Global settings instance
_settings: Optional[Settings] = None

def get_settings() -> Settings:
    """Get the application settings, initializing if needed."""
    global _settings
    if _settings is None:
        _settings = Settings()
    return _settings

def get_config() -> Dict[str, Any]:
    """For backward compatibility: get settings as a dict."""
    return get_settings().dict()

def load_config(config_file: str = "config.json") -> Dict[str, Any]:
    """
    For backward compatibility: load configuration from file with environment variable overrides.
    
    Args:
        config_file: Path to the JSON config file
        
    Returns:
        Dict containing the merged configuration
    """
    settings = get_settings()
    
    # Try to load from file and update settings if file exists
    try:
        if os.path.exists(config_file):
            with open(config_file, 'r', encoding='utf-8') as f:
                file_config = json.load(f)
                
                # Only override settings that exist in the model
                for key, value in file_config.items():
                    if hasattr(settings, key):
                        setattr(settings, key, value)
    except Exception as e:
        print(f"Error loading config file: {e}")
    
    # Ensure firmware directory exists
    firmware_dir = Path(settings.firmware_directory)
    firmware_dir.mkdir(exist_ok=True)
    
    return settings.dict()