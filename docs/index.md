# 🔴 ESP32 Wi-Fi Panic Button

A standalone ESP32-based panic button with Wi-Fi config and email alerts via SMTP.

## 🚀 Features
- Captive portal setup
- SMTP email alerts
- EEPROM config storage
- Web UI for config/test/reset
- Status LED + debounce logic

## 📦 Hardware
- ESP32 Dev Board
- GPIO 13: Panic Button (active LOW)
- GPIO 2: Status LED

## Web Interface
- `/` – Status page
- `/config` – Update config
- `/test` – Send test email
- `/reset` – Factory reset

## Setup
1. Flash firmware to ESP32
2. Connect to `PanicAlarm_Setup` AP
3. Configure Wi-Fi + SMTP
4. Device reboots into normal mode

## SMTP Notes
- Use a valid SMTP server and login
- Gmail requires app password

## License
GPLv3 – see [GitHub Repo](https://github.com/RAHB-REALTORS-Association/panic-button-esp)