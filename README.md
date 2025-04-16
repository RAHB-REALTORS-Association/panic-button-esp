# 🔴 ESP32 Wi-Fi Panic Button

A standalone ESP32-based Wi-Fi panic button that sends email and webhook alerts when pressed. Designed for quick deployment with a built-in captive portal for configuration.

## ✨ Features

- Wi-Fi configuration via captive portal
- Multiple notification options:
  - SMTP email alerts when panic button is pressed
  - Webhook integration for connecting to other systems
- Battery level monitoring with percentage display and visual gauge
- Web-based control panel and config update page
- EEPROM-stored settings (WiFi, SMTP, and webhook)
- Factory reset and test functions
- Mobile-friendly responsive UI
- Support for ESP32-C6 boards (like FireBeetle)

## 🛠️ Hardware

The firmware is optimized for:

### FireBeetle ESP32-C6
- [DFRobot FireBeetle 2 ESP32-C6 board](https://www.dfrobot.com/product-2771.html)
- Tactile button (connected to GPIO 4)
- Onboard LED (GPIO 15)
- Battery monitoring via ADC (GPIO 0)

## 📦 Libraries Used

- [ESP Mail Client](https://github.com/mobizt/ESP-Mail-Client)
- [HTTPClient](https://github.com/espressif/arduino-esp32) (for webhook functionality)
- WiFi
- WebServer
- DNSServer
- EEPROM

## 🔌 Pinout

| Function       | FireBeetle GPIO |
|----------------|----------------|
| Panic Button   | 4              |
| Status LED     | 15             |
| Battery ADC    | 0              |

## 🚀 Getting Started

1. Flash the firmware using Arduino IDE
2. If no config is stored, the device starts in **Setup Mode**:
   - Hosts an AP named `PanicAlarm_XXXX` (where XXXX is derived from the MAC address)
   - Password: `setupalarm`
   - Access the captive portal to configure Wi-Fi and notification settings
3. Once configured, the device will connect to the saved network and begin monitoring.

## 🔧 Web Interface

### Setup Mode (AP)
- Captive portal guides you through initial configuration
- Configure WiFi, notifications, and device location

### Normal Mode (STA)
- Main status page with battery gauge and device information
- Configuration update page for changing settings
- Test features for both email and webhook notifications
- Factory reset option

## 🔋 Battery Monitoring

- Visual battery gauge with color coding:
  - Green: Good battery (above 50%)
  - Yellow: Medium battery (25-50%)
  - Red: Low battery (below 25%)
- Percentage and voltage display
- Low battery notifications via both email and webhook

## 🌐 Webhook Integration

The device can send JSON payloads to a custom webhook URL with the following information:
- Event type (alarm triggered or low battery)
- Device ID and location
- Battery status
- IP and MAC address
- Timestamp

## 🔁 Resetting

Visit the reset page from the control panel, confirm, and the device will wipe configuration and restart in setup mode.

## 📄 License

GPLv3 – see [LICENSE](LICENSE) for details.
