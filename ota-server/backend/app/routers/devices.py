"""
Device management API endpoints.
"""
from fastapi import APIRouter, Depends, HTTPException, status, Body, Query, Path
from typing import List, Dict, Any, Optional

from app.models.device import DeviceInfo, DeviceCreate, DeviceUpdate
from app.utils import (
    get_devices, get_device, update_device, delete_device,
    validate_mac_address, format_mac_address, calculate_file_md5
)
from app.auth import verify_admin_api_key

router = APIRouter()

@router.get("/devices", response_model=Dict[str, Dict[str, Any]])
async def list_devices(_: bool = Depends(verify_admin_api_key)):
    """List all registered devices"""
    return get_devices()

@router.get("/devices/{mac_address}", response_model=Dict[str, Any])
async def get_device_info(
    mac_address: str = Path(..., description="Device MAC address"),
    _: bool = Depends(verify_admin_api_key)
):
    """Get information about a specific device"""
    # Format MAC to ensure consistent format
    mac = format_mac_address(mac_address)
    if not mac:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Invalid MAC address format"
        )
    
    device_info = get_device(mac)
    if not device_info:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail="Device not found"
        )
    
    return device_info

@router.put("/devices/{mac_address}", response_model=Dict[str, Any])
async def update_device_info(
    mac_address: str = Path(..., description="Device MAC address"),
    device_data: Dict[str, Any] = Body(..., description="Device data to update"),
    _: bool = Depends(verify_admin_api_key)
):
    """Update device information"""
    # Format MAC to ensure consistent format
    mac = format_mac_address(mac_address)
    if not mac:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Invalid MAC address format"
        )
    
    # Validate required fields
    if "target_version" in device_data and not validate_version_format(device_data["target_version"]):
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Invalid target version format"
        )
    
    # Check if device exists
    existing_device = get_device(mac)
    if not existing_device:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail="Device not found"
        )
    
    # Update the device
    existing_device.update(device_data)
    success = update_device(mac, existing_device)
    
    if not success:
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail="Failed to update device"
        )
    
    return {"success": True, "device": existing_device}

@router.delete("/devices/{mac_address}", response_model=Dict[str, bool])
async def delete_device_info(
    mac_address: str = Path(..., description="Device MAC address"),
    _: bool = Depends(verify_admin_api_key)
):
    """Delete a device"""
    # Format MAC to ensure consistent format
    mac = format_mac_address(mac_address)
    if not mac:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Invalid MAC address format"
        )
    
    success = delete_device(mac)
    if not success:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail="Device not found or could not be deleted"
        )
    
    return {"success": True}

@router.post("/devices", response_model=Dict[str, Any])
async def add_device(
    device_data: DeviceCreate,
    _: bool = Depends(verify_admin_api_key)
):
    """Add a new device"""
    mac_address = format_mac_address(device_data.mac_address)
    if not mac_address:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Invalid MAC address format"
        )
    
    # Check if device already exists
    existing_device = get_device(mac_address)
    if existing_device:
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail="Device already exists"
        )
    
    # Convert model to dict and remove the mac_address field
    device_dict = device_data.dict(exclude={"mac_address"})
    
    # Add the device
    success = update_device(mac_address, device_dict)
    if not success:
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail="Failed to add device"
        )
    
    return {"success": True, "mac_address": mac_address, "device": device_dict}

# Helper function for version validation
def validate_version_format(version: str) -> bool:
    """Validate semantic version format"""
    from app.utils import validate_version
    return validate_version(version)