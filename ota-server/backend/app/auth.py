"""
Authentication and authorization for the OTA update server.
"""
from typing import Optional
from fastapi import Depends, HTTPException, status, Query
from fastapi.security import APIKeyHeader
from app.config import get_settings

# API key header scheme
API_KEY_HEADER = APIKeyHeader(name="X-Admin-API-Key", auto_error=False)
DEVICE_AUTH_HEADER = APIKeyHeader(name="X-Device-Auth", auto_error=False)

async def verify_admin_api_key(api_key: str = Depends(API_KEY_HEADER)) -> bool:
    """
    Verify admin API key for protected routes
    
    Args:
        api_key: API key from request header
        
    Returns:
        True if valid
        
    Raises:
        HTTPException: If API key is invalid
    """
    settings = get_settings()
    if not api_key:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="API key is required",
            headers={"WWW-Authenticate": "APIKey"},
        )
    
    if api_key != settings.admin_api_key:
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail="Invalid API key",
            headers={"WWW-Authenticate": "APIKey"},
        )
    
    return True

async def verify_device_auth(
    auth_token: str = Depends(DEVICE_AUTH_HEADER),
    mac: str = Query(..., description="Device MAC address")
) -> bool:
    """
    Verify device authentication token
    
    Args:
        auth_token: Authentication token from request header
        mac: MAC address from request query parameter
        
    Returns:
        True if valid
        
    Raises:
        HTTPException: If authentication fails
    """
    if not auth_token:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Authentication header required",
            headers={"WWW-Authenticate": "DeviceAuth"},
        )
    
    from app.utils import generate_auth_token, validate_mac_address
    from app.config import get_settings
    
    settings = get_settings()
    
    # Ensure MAC is properly formatted
    mac_address = mac.upper()
    
    if not validate_mac_address(mac_address):
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Invalid MAC address format"
        )
    
    # Generate expected token
    expected_token = generate_auth_token(mac_address, settings.shared_secret_key)
    
    if not expected_token or auth_token.upper() != expected_token:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Authentication failed",
            headers={"WWW-Authenticate": "DeviceAuth"},
        )
    
    return True