"""
Firmware management API endpoints.
"""
import os
import shutil
from datetime import datetime
from typing import List, Dict, Any, Optional
from fastapi import (
    APIRouter, Depends, HTTPException, status, 
    UploadFile, File, Form, Query, Path
)
from fastapi.responses import FileResponse

from app.models.firmware import FirmwareFile, FirmwareFileList, FirmwareUploadResponse
from app.utils import calculate_file_md5, list_firmware_files
from app.auth import verify_admin_api_key
from app.config import get_settings

router = APIRouter()

@router.get("/firmware", response_model=List[FirmwareFile])
async def list_files(_: bool = Depends(verify_admin_api_key)):
    """List all firmware files"""
    return list_firmware_files()

@router.post("/firmware", response_model=FirmwareUploadResponse)
async def upload_firmware(
    file: UploadFile = File(...),
    _: bool = Depends(verify_admin_api_key)
):
    """Upload a new firmware file"""
    settings = get_settings()
    firmware_dir = settings.firmware_directory
    
    # Ensure firmware directory exists
    if not os.path.exists(firmware_dir):
        os.makedirs(firmware_dir)
    
    # Security check: validate filename
    filename = file.filename
    if not filename or "/" in filename or "\\" in filename:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Invalid filename"
        )
    
    # Save the file
    file_path = os.path.join(firmware_dir, filename)
    
    try:
        with open(file_path, "wb") as f:
            shutil.copyfileobj(file.file, f)
    except Exception as e:
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Error saving file: {str(e)}"
        )
    finally:
        file.file.close()
    
    # Calculate checksum
    checksum = calculate_file_md5(file_path)
    file_size = os.path.getsize(file_path)
    
    # Generate URL for the firmware
    base_url = f"http://{settings.server_host}"
    if settings.server_port != 80:
        base_url += f":{settings.server_port}"
    firmware_url = f"{base_url}/firmware/{filename}"
    
    return FirmwareUploadResponse(
        filename=filename,
        checksum=checksum,
        size=file_size,
        success=True,
        url=firmware_url
    )

@router.delete("/firmware/{filename}", response_model=Dict[str, bool])
async def delete_firmware(
    filename: str = Path(..., description="Firmware filename"),
    _: bool = Depends(verify_admin_api_key)
):
    """Delete a firmware file"""
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
    
    try:
        os.remove(file_path)
        return {"success": True}
    except Exception as e:
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Error deleting file: {str(e)}"
        )

@router.get("/firmware/{filename}/info", response_model=FirmwareFile)
async def get_firmware_info(
    filename: str = Path(..., description="Firmware filename"),
    _: bool = Depends(verify_admin_api_key)
):
    """Get information about a firmware file"""
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
    
    checksum = calculate_file_md5(file_path)
    size = os.path.getsize(file_path)
    stat = os.stat(file_path)
    
    return FirmwareFile(
        filename=filename,
        checksum=checksum,
        size=size,
        uploaded_at=datetime.fromtimestamp(stat.st_mtime).isoformat()
    )