#ifndef ESP32_CONFIG_H
#define ESP32_CONFIG_H

// Pin definitions for standard ESP32
#define BUTTON_PIN 0
#define LED_PIN 2

// Standard ESP32 doesn't have battery monitoring in this application
#ifndef HAS_BATTERY_MONITORING
#define HAS_BATTERY_MONITORING 0
#endif

#endif // ESP32_CONFIG_H