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
    #define BUTTON_PIN 4
    #define LED_PIN 15
    #define BATTERY_PIN 0  // A0 on FireBeetle ESP32-C6
    #define HAS_BATTERY_MONITORING true
    #define BATT_SAMPLES 10
    #define uS_TO_S_FACTOR 1000000
    #define TIME_TO_SLEEP 60
    // For ESP32-C6 deep sleep
    #define ESP_EXT1_WAKEUP_ANY_LOW ESP_EXT1_WAKEUP_ALL_LOW
#else
    // Standard ESP32 config
    #define BUTTON_PIN 13
    #define LED_PIN 2
    #define HAS_BATTERY_MONITORING false
#endif

// Default values
#define DEFAULT_CONFIG_SSID "PanicAlarm_Setup"
#define DEFAULT_CONFIG_PASSWORD "setupalarm"

#endif // CONFIG_H