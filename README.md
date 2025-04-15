# 🔴 ESP32 Wi-Fi Panic Button

A standalone ESP32-based Wi-Fi panic button that sends an email alert when pressed. Designed for quick deployment with a built-in captive portal for configuration.

## ✨ Features

- Wi-Fi configuration via captive portal
- SMTP email alerts when panic button is pressed
- Web-based control panel and config update page
- EEPROM-stored settings (WiFi and SMTP)
- Factory reset and test email functions
- Mobile-friendly responsive UI
- Modular code structure for maintainability
- Support for both ESP32 and FireBeetle ESP32-C6 boards

## 🛠️ Hardware

The firmware supports two hardware variants:

### Standard ESP32
- ESP32 (any standard dev board)
- Tactile button (connected to GPIO 13)
- Status LED (connected to GPIO 2)

### FireBeetle ESP32-C6
- DFRobot FireBeetle 2 ESP32-C6 board
- Tactile button (connected to GPIO 4)
- Onboard LED (GPIO 15)
- Battery monitoring via ADC (GPIO 0)

## 📦 Libraries Used

- [ESP Mail Client](https://github.com/mobizt/ESP-Mail-Client)
- WiFi
- WebServer
- DNSServer
- EEPROM

## 🔌 Pinout

| Function       | ESP32 GPIO | FireBeetle GPIO |
|----------------|------------|----------------|
| Panic Button   | 13         | 4              |
| Status LED     | 2          | 15             |
| Battery ADC    | N/A        | 0              |

## 📂 Project Structure

The project uses a modular structure to organize functionality:

```
src/
  ├── main.cpp         # Main program entry point
  ├── config.h         # Common and board-specific configuration
  ├── network.cpp/h    # WiFi and web server functionality
  ├── storage.cpp/h    # EEPROM storage functions
  └── notifications.cpp/h  # Email and LED notifications
```

## 🔨 Building and Uploading

This project uses PlatformIO for building and uploading. The firmware supports two board variants:

### For Standard ESP32
```bash
pio run -e esp32
pio run -e esp32 -t upload
pio run -e esp32 -t monitor
```

### For FireBeetle ESP32-C6
```bash
pio run -e esp32-c6
pio run -e esp32-c6 -t upload
pio run -e esp32-c6 -t monitor
```

## 🚀 Getting Started

1. Choose the correct board environment and flash the firmware.
2. If no config is stored, the device starts in **Setup Mode**:
   - Hosts an AP named `PanicAlarm_Setup`
   - Access the captive portal to configure Wi-Fi and email
3. Once configured, the device will connect to the saved network and begin monitoring.

## 🔧 Web Interface

### Setup Mode (AP)

- `GET /` — Captive portal for config
- `POST /setup` — Submit config

### Normal Mode (STA)

- `GET /` — Main status page
- `GET /config` — Config update form
- `POST /update` — Save updated config
- `GET /test` — Send a test email
- `GET /reset` — Factory reset interface

## 🧪 Testing

- Press the hardware button to trigger an alarm.
- Use `/test` from the browser to simulate an alert.

## 🔔 FireBeetle-Specific Features

When built for the FireBeetle ESP32-C6:
- Battery voltage monitoring
- Low battery email alerts
- Deep sleep capabilities
- Optimized for battery operation

## 🔁 Resetting

Visit `/reset`, confirm, and the device will wipe config and restart in setup mode.

## 📄 License

GPLv3 – see [LICENSE](LICENSE) for details.
