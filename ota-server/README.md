# ESP32 OTA Update Server

A Flask-based server for managing Over-The-Air (OTA) updates for ESP32 devices.

## Features

- Secure device authentication
- Version comparison for update eligibility
- Configuration via JSON files and environment variables
- Admin API for device management
- Admin command-line tools
- Firmware checksum verification
- Detailed logging

## Directory Structure

```
ota-server/
├── app.py                  # Main Flask application
├── config.py               # Configuration management
├── utils.py                # Utility functions
├── admin_tools.py          # CLI for device management
├── config.json             # Server configuration
├── devices.json            # Device database
└── firmware/               # Firmware binary files
```

## Configuration

### Server Configuration

The server can be configured through:

1. The `config.json` file
2. Environment variables (prefixed with `OTA_SERVER_`)

Example `config.json`:

```json
{
  "shared_secret_key": "your-device-secret-key",
  "server_port": 5000,
  "server_host": "0.0.0.0",
  "debug_mode": false,
  "log_level": "INFO",
  "devices_file": "devices.json",
  "firmware_directory": "firmware",
  "admin_api_key": "change-admin-api-key-in-production"
}
```

Environment variable overrides:

```bash
export OTA_SERVER_SHARED_SECRET_KEY="my-secure-key"
export OTA_SERVER_SERVER_PORT=8080
export OTA_SERVER_ADMIN_API_KEY="admin-api-key"
```

### Device Management

Devices are stored in `devices.json` with the following format:

```json
{
  "AA:BB:CC:DD:EE:FF": {
    "device_id": "panic_button_01",
    "hardware_version": "1.0",
    "target_version": "1.2.1",
    "firmware_url": "http://example.com/firmware/PanicButton_v1.2.1.bin",
    "checksum": "5f4dcc3b5aa765d61d8327deb882cf99",
    "last_check": null,
    "last_update": null
  }
}
```

## Setup & Running

### Standard Installation

```bash
# Clone the repository
git clone https://github.com/yourusername/ota-update-server.git
cd ota-update-server

# Install dependencies
pip install -r requirements.txt
```

### Starting the Server (Standard)

```bash
python app.py
```

### Docker Installation

This project includes Docker and Docker Compose files for easy deployment.

```bash
# Clone the repository
git clone https://github.com/yourusername/ota-update-server.git
cd ota-update-server

# Create .env file from example
cp .env.example .env

# Edit the .env file with your configuration
nano .env

# Start the server
./start.sh
```

### Using Docker Compose Manually

```bash
# Start the server
docker-compose up -d

# View logs
docker-compose logs -f

# Stop the server
docker-compose down
```

## Device Integration

For your ESP32 device to use this OTA update server, it should:

1. Periodically check for updates using the `/api/firmware` endpoint
2. Include device information in the request (MAC, version, etc.)
3. Include an authentication token in the `X-Device-Auth` header
4. Download and install the new firmware if an update is available

Example ESP32 code snippet for authentication:

```cpp
String generateAuthToken() {
  // Remove colons and ensure uppercase
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

## Admin Tools

The `admin_tools.py` script provides a command-line interface for managing devices.

### Standard Usage

```bash
# List all registered devices
python admin_tools.py list

# Add a new device
python admin_tools.py add AA:BB:CC:DD:EE:FF --version 1.2.1 --firmware-file firmware/PanicButton_v1.2.1.bin

# Update a device
python admin_tools.py update AA:BB:CC:DD:EE:FF --version 1.3.0 --firmware-file firmware/PanicButton_v1.3.0.bin

# View device details
python admin_tools.py get AA:BB:CC:DD:EE:FF

# Calculate firmware checksum
python admin_tools.py checksum firmware/PanicButton_v1.2.1.bin

# Delete a device
python admin_tools.py delete AA:BB:CC:DD:EE:FF
```

### Docker Usage

Use the included `manage-devices.sh` script to manage devices when running in Docker:

```bash
# Make the script executable
chmod +x manage-devices.sh

# List all registered devices
./manage-devices.sh list

# Add a new device
./manage-devices.sh add AA:BB:CC:DD:EE:FF --version 1.2.1 --firmware-file firmware/PanicButton_v1.2.1.bin

# Other commands work the same way as in standard usage
./manage-devices.sh get AA:BB:CC:DD:EE:FF
```

## API Endpoints

### Device API

- `GET /api/firmware` - Check for firmware updates
  - Query parameters: `device_id`, `hardware`, `version`, `mac`
  - Header: `X-Device-Auth`

- `GET /firmware/<filename>` - Download firmware binary

### Admin API

All admin API endpoints require the `X-Admin-API-Key` header.

- `GET /admin/devices` - List all devices
- `GET /admin/devices/<mac_address>` - Get device information
- `POST /admin/devices` - Add a new device
- `PUT /admin/devices/<mac_address>` - Update device information
- `DELETE /admin/devices/<mac_address>` - Delete a device

## Security Considerations

- Always use HTTPS in production
- Use a strong, unique `shared_secret_key` for device authentication
- Set a secure `admin_api_key` for admin API access
- Consider using a reverse proxy (nginx, etc.) for production deployments
- Store `devices.json` in a secure location

## License

This project is licensed under the AGPL-3.0 License. See the [LICENSE](LICENSE) file for details.