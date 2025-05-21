"""
Provides essential endpoints for OTA update checking and firmware downloading.
"""
from fastapi import APIRouter, Request, Depends, HTTPException, status
from fastapi.responses import JSONResponse, FileResponse
from datetime import datetime
import os
from typing import Optional

from app.models.device import UpdateCheckResponse
from app.utils import (
    generate_auth_token, compare_versions, validate_mac_address, 
    validate_version, get_device, update_device
)
from app.config import get_settings
from app.auth import verify_device_auth

router = APIRouter()

@router.get("/api/firmware", response_model=UpdateCheckResponse)
async def check_firmware_update(
    request: Request,
    device_id: str,
    hardware: str,
    version: str,
    mac: str,
    authenticated: bool = Depends(verify_device_auth)
):
    """
    Handles firmware update check requests from ESP32 devices.
    """
    # Ensure MAC is uppercase
    mac_address = mac.upper()
    timestamp = datetime.now().isoformat()
    
    # Basic validation
    if not all([device_id, hardware, version, mac_address]):
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Missing required parameters"
        )
    
    if not validate_mac_address(mac_address):
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Invalid MAC address format"
        )
    
    if not validate_version(version):
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Invalid version format"
        )
    
    # Get device info
    device_info = get_device(mac_address)
    if not device_info:
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail="Device not authorized"
        )
    
    # Get target firmware info
    target_version = device_info["target_version"]
    
    # Compare versions
    try:
        version_comparison = compare_versions(target_version, version)
    except Exception as e:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail=f"Version comparison error: {str(e)}"
        )
    
    # Update device's last check timestamp and current version
    device_info["last_check"] = timestamp
    device_info["current_version"] = version
    device_info["ip_address"] = request.client.host
    update_device(mac_address, device_info)
    
    # Prepare response
    if version_comparison > 0:
        # Update is available
        return UpdateCheckResponse(
            update_available=True,
            firmware_version=target_version,
            firmware_url=device_info["firmware_url"],
            checksum=device_info["checksum"]
        )
    else:
        # No update needed
        return UpdateCheckResponse(update_available=False)

@router.get("/firmware/{filename}")
async def download_firmware(filename: str):
    """
    Serves firmware binary files.
    """
    settings = get_settings()
    firmware_dir = settings.firmware_directory
    
    # Security check: prevent path traversal
    if "/" in filename or "\\" in filename:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Invalid filename"
        )
    
    file_path = os.path.join(firmware_dir, filename)
    
    if not os.path.exists(file_path):
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail="Firmware file not found"
        )
    
    return FileResponse(
        file_path, 
        media_type="application/octet-stream",
        filename=filename
    )

@router.get("/status")
async def status():
    """Simple status endpoint"""
    return JSONResponse({
        "status": "ok",
        "timestamp": datetime.now().isoformat(),
        "version": "1.0.0"
    })