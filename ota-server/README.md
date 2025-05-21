# ESP32 OTA Update Server

A modern FastAPI-based server for managing Over-The-Air (OTA) updates for ESP32 devices, with a Svelte 5 frontend.

## Features

- **Intuitive Device Management**: Add, update, and monitor your fleet of ESP32 devices
- **Firmware Updates**: Upload and manage firmware versions with automatic versioning
- **Secure Authentication**: Token-based device authentication and API key protection
- **Real-time Dashboard**: Monitor device status, update progress, and firmware inventory
- **Modern Architecture**: FastAPI backend with Svelte 5 frontend for optimal performance
- **Docker Deployment**: Easy setup with Docker Compose for both development and production

## Quick Start

### Prerequisites

- [Docker](https://docs.docker.com/get-docker/)
- [Docker Compose](https://docs.docker.com/compose/install/)

### Setup and Run

1. **Clone the repository**

```bash
git clone https://github.com/RAHB-REALTORS-Association/panic-button-esp.git
cd ota-server
```

2. **Configure environment variables**

```bash
cp .env.example .env
```

Edit `.env` with your preferred settings:

```
OTA_SERVER_SHARED_SECRET_KEY=your-device-secret-key
OTA_SERVER_ADMIN_API_KEY=your-admin-api-key
OTA_SERVER_SERVER_PORT=5000
OTA_SERVER_UI_PORT=80
```

3. **Start the server**

```bash
# For production
docker-compose up -d

# For development with hot-reload
docker-compose -f docker-compose.dev.yml up -d
```

4. **Access the Dashboard**

- Production: http://localhost
- Development: http://localhost:3000

## ESP32 Integration

To use this OTA server with your ESP32 devices, configure your firmware to:

1. **Check for updates** by making periodic requests to `/api/firmware`

```cpp
HTTPClient http;
http.begin("http://your-server-ip:5000/api/firmware?device_id=device1&hardware=esp32&version=1.0.0&mac=" + WiFi.macAddress());
http.addHeader("X-Device-Auth", generateAuthToken());
int httpCode = http.GET();
```

2. **Generate authentication token** for secure communication

```cpp
String generateAuthToken() {
  // Remove colons and convert to uppercase
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  mac.toUpperCase();
  
  // Combine with secret key
  String tokenStr = mac + SHARED_SECRET_KEY;
  
  // Simple hash (djb2 variant)
  uint32_t hash = 5381;
  for (int i = 0; i < tokenStr.length(); i++) {
    hash = ((hash << 5) + hash) + tokenStr.charAt(i);
  }
  
  // Convert to hex string
  char hexHash[9];
  sprintf(hexHash, "%08X", hash);
  return String(hexHash);
}
```

3. **Download and install** firmware when updates are available

```cpp
if (httpCode == HTTP_CODE_OK) {
  String payload = http.getString();
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, payload);

  bool update_available = doc["update_available"];
  if (update_available) {
    String firmware_url = doc["firmware_url"];
    String firmware_version = doc["firmware_version"];
    String firmware_checksum = doc["checksum"];
    
    // Download and apply the update
    if (downloadAndUpdate(firmware_url, firmware_checksum)) {
      Serial.println("Update successful!");
    }
  }
}
```

## API Endpoints

### Device API

- `GET /api/firmware` - Check for firmware updates
  - Query parameters: `device_id`, `hardware`, `version`, `mac`
  - Header: `X-Device-Auth`

- `GET /firmware/<filename>` - Download firmware binary

### Admin API

All admin API endpoints require the `X-Admin-API-Key` header.

- `GET /admin/api/devices` - List all devices
- `POST /admin/api/devices` - Add a new device
- `GET /admin/api/devices/:mac` - Get device info
- `PUT /admin/api/devices/:mac` - Update device
- `DELETE /admin/api/devices/:mac` - Delete device

- `GET /admin/api/firmware` - List firmware files
- `POST /admin/api/firmware` - Upload firmware
- `DELETE /admin/api/firmware/:filename` - Delete firmware

## Docker Management

### View Logs

```bash
# View backend logs
docker logs -f ota-server-backend

# View frontend logs
docker logs -f ota-server-frontend
```

### Stop Server

```bash
# Stop production server
docker-compose down

# Stop development server
docker-compose -f docker-compose.dev.yml down
```

### Update Server

```bash
# Pull latest changes
git pull

# Rebuild and restart containers
docker-compose down
docker-compose up -d --build
```

## Development

### Running in Development Mode

Development mode includes hot-reload for both the backend and frontend:

```bash
docker-compose -f docker-compose.dev.yml up -d
```

### Backend API Documentation

- Swagger UI: http://localhost:5000/admin/docs
- ReDoc: http://localhost:5000/admin/redoc

## Directory Structure

```
ota-server/
├── backend/              # FastAPI backend
├── frontend/             # Svelte frontend
├── docker/               # Docker configuration
├── firmware/             # Firmware storage
├── .env.example          # Environment variables template
├── docker-compose.yml    # Production configuration
└── docker-compose.dev.yml # Development configuration
```

## Security Considerations

- Change default keys in `.env` before deploying to production
- Use HTTPS in production environments
- Regularly update dependencies and Docker images
- Configure a proper backup system for the `devices.json` and firmware files

## License

This project is licensed under the AGPL-3.0 License. See the [LICENSE](LICENSE) file for details.