#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Common constants for all board variants
#define EEPROM_SIZE 512
#define CONFIG_FLAG_ADDR 0
#define SSID_ADDR 10
#define PASS_ADDR 80
#define EMAIL_SERVER_ADDR 150
#define EMAIL_PORT_ADDR 220
#define EMAIL_USER_ADDR 224
#define EMAIL_PASS_ADDR 294
#define EMAIL_RECIPIENT_ADDR 364
#define DNS_PORT 53
#define WEBSERVER_PORT 80
#define CONFIG_FLAG 0xAA

// Board-specific configurations
#if defined(ESP32_C6)
    // FireBeetle ESP32-C6 config
    #include "board_config/firebeetle_config.h"
#else
    // Standard ESP32 config
    #include "board_config/esp32_config.h"
#endif

// Default values
#define DEFAULT_CONFIG_SSID "PanicAlarm_Setup"
#define DEFAULT_CONFIG_PASSWORD "setupalarm"

#endif // CONFIG_H