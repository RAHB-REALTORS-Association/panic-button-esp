"""
Main FastAPI application for the OTA update server.
"""
import os
import logging
from fastapi import FastAPI, Depends, Request, HTTPException
from fastapi.responses import JSONResponse
from fastapi.staticfiles import StaticFiles
from fastapi.middleware.cors import CORSMiddleware
import uvicorn

from app.config import load_config, get_settings
from app.routers import api, devices, firmware
from app.auth import verify_admin_api_key

# Load configuration
settings = get_settings()
config = load_config()

# Configure logging
logging.basicConfig(
    level=getattr(logging, settings.log_level),
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# Create FastAPI app
def create_app() -> FastAPI:
    """Create and configure the FastAPI application."""
    app = FastAPI(
        title="OTA Update Server",
        description="OTA Update Server for ESP32 devices",
        version="2.0.0",
        docs_url="/admin/docs",
        redoc_url="/admin/redoc",
    )
    
    # Add CORS middleware
    app.add_middleware(
        CORSMiddleware,
        allow_origins=["*"],  # In production, replace with specific origins
        allow_credentials=True,
        allow_methods=["*"],
        allow_headers=["*"],
    )
    
    # Mount static files for the frontend if the directory exists
    frontend_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), "frontend")
    if os.path.exists(frontend_dir):
        app.mount("/ui", StaticFiles(directory=frontend_dir, html=True), name="ui")
    
    # Create firmware directory if it doesn't exist
    os.makedirs(settings.firmware_directory, exist_ok=True)
    
    # Mount firmware directory for direct file access (optional)
    app.mount(
        "/firmware_files", 
        StaticFiles(directory=settings.firmware_directory), 
        name="firmware_files"
    )
    
    # Include routers
    # Legacy API endpoints for backward compatibility
    app.include_router(api.router)
    
    # Admin API endpoints
    admin_api = FastAPI(dependencies=[Depends(verify_admin_api_key)])
    admin_api.include_router(devices.router, tags=["devices"])
    admin_api.include_router(firmware.router, tags=["firmware"])
    app.mount("/admin/api", admin_api)
    
    @app.exception_handler(HTTPException)
    async def http_exception_handler(request: Request, exc: HTTPException):
        """Custom exception handler to ensure consistent error responses."""
        return JSONResponse(
            status_code=exc.status_code,
            content={"error": exc.detail}
        )
    
    @app.exception_handler(Exception)
    async def general_exception_handler(request: Request, exc: Exception):
        """Handler for unexpected exceptions."""
        logger.error(f"Unexpected error: {str(exc)}", exc_info=True)
        return JSONResponse(
            status_code=500,
            content={"error": "Internal server error"}
        )
    
    @app.get("/")
    async def redirect_to_ui():
        """Redirect root to the UI."""
        return {"message": "OTA Update Server is running. Access the UI at /ui"}
    
    logger.info(f"OTA Update Server initialized with config: {config}")
    return app

app = create_app()

if __name__ == "__main__":
    """Start the server when run directly."""
    uvicorn.run(
        "app.main:app",
        host=settings.server_host,
        port=settings.server_port,
        reload=settings.debug_mode,
        log_level=settings.log_level.lower()
    )