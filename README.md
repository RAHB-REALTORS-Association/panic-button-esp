# 🔴 ESP32 Wi-Fi Panic Button

A standalone ESP32-based Wi-Fi panic button that sends an email alert when pressed. Designed for quick deployment with a built-in captive portal for configuration.

## ✨ Features

- Wi-Fi configuration via captive portal
- SMTP email alerts when panic button is pressed
- Web-based control panel and config update page
- EEPROM-stored settings (WiFi and SMTP)
- Factory reset and test email functions
- Mobile-friendly responsive UI

## 🛠️ Hardware

- ESP32 (any dev board)
- Tactile button (connected to GPIO 13)
- Status LED (connected to GPIO 2)

## 📦 Libraries Used

- [ESP Mail Client](https://github.com/mobizt/ESP-Mail-Client)
- WiFi
- WebServer
- DNSServer
- EEPROM

## 🔌 Pinout

| Function     | GPIO |
|--------------|------|
| Panic Button | 13   |
| Status LED   | 2    |

## 🚀 Getting Started

1. Flash the firmware to your ESP32.
2. If no config is stored, the ESP32 starts in **Setup Mode**:
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

## 🔁 Resetting

Visit `/reset`, confirm, and the device will wipe config and restart in setup mode.

## 📄 License

GPLv3 – see [LICENSE](LICENSE) for details.
