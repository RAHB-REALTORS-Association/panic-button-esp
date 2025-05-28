"""
Pydantic models for firmware files.
"""
from typing import Optional, List
from pydantic import BaseModel, Field, validator
import os

class FirmwareFile(BaseModel):
    """Firmware file information."""
    filename: str
    checksum: str
    size: int
    uploaded_at: Optional[str] = None
    
    @validator('filename')
    def validate_filename(cls, v):
        """Validate filename is safe."""
        if os.path.sep in v:
            raise ValueError("Invalid filename")
        return v

class FirmwareFileList(BaseModel):
    """List of firmware files."""
    files: List[FirmwareFile]

class FirmwareUploadResponse(BaseModel):
    """Response after firmware upload."""
    filename: str
    checksum: str
    size: int
    success: bool = True
    url: Optional[str] = None
    
    class Config:
        """Pydantic config for the model."""
        schema_extra = {
            "example": {
                "filename": "firmware_v1.2.0.bin",
                "checksum": "a1b2c3d4e5f6",
                "size": 1024567,
                "success": True,
                "url": "/firmware/firmware_v1.2.0.bin"
            }
        }