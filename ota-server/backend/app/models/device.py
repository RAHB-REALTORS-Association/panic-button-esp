"""
Pydantic models for device data.
"""
from typing import Optional, Dict, Any, List
from pydantic import BaseModel, Field, validator
from datetime import datetime

class DeviceBase(BaseModel):
    """Base class for device data."""
    device_id: str
    hardware_version: str = "1.0"
    target_version: str
    firmware_url: str
    checksum: str
    
    @validator('target_version')
    def validate_version(cls, v):
        """Validate semantic version format."""
        from app.utils import validate_version
        if not validate_version(v):
            raise ValueError("Invalid target version format")
        return v

class DeviceCreate(DeviceBase):
    """Model for creating a new device."""
    mac_address: str
    
    @validator('mac_address')
    def validate_mac(cls, v):
        """Validate MAC address format."""
        from app.utils import validate_mac_address, format_mac_address
        if not validate_mac_address(v):
            raise ValueError("Invalid MAC address format")
        return format_mac_address(v)

class DeviceUpdate(BaseModel):
    """Model for updating device attributes."""
    device_id: Optional[str] = None
    hardware_version: Optional[str] = None
    target_version: Optional[str] = None
    firmware_url: Optional[str] = None
    checksum: Optional[str] = None
    
    @validator('target_version')
    def validate_version(cls, v):
        """Validate semantic version format if present."""
        if v is not None:
            from app.utils import validate_version
            if not validate_version(v):
                raise ValueError("Invalid target version format")
        return v

class DeviceInfo(DeviceBase):
    """Full device model including status information."""
    last_check: Optional[datetime] = None
    last_update: Optional[datetime] = None
    current_version: Optional[str] = None
    ip_address: Optional[str] = None
    status: Optional[str] = "unknown"
    
    class Config:
        """Pydantic config for the model."""
        orm_mode = True
        json_encoders = {
            datetime: lambda v: v.isoformat() if v else None
        }

class DeviceList(BaseModel):
    """List of devices."""
    devices: List[DeviceInfo]

class UpdateCheckRequest(BaseModel):
    """Model for update check request params."""
    device_id: str
    hardware: str
    version: str
    mac: str
    
    @validator('mac')
    def validate_mac(cls, v):
        """Validate MAC address."""
        from app.utils import validate_mac_address
        if not validate_mac_address(v):
            raise ValueError("Invalid MAC address format")
        return v.upper()  # Ensure MAC is uppercase
    
    @validator('version')
    def validate_version(cls, v):
        """Validate semantic version."""
        from app.utils import validate_version
        if not validate_version(v):
            raise ValueError("Invalid version format")
        return v

class UpdateCheckResponse(BaseModel):
    """Response for update check request."""
    update_available: bool
    firmware_version: Optional[str] = None
    firmware_url: Optional[str] = None
    checksum: Optional[str] = None