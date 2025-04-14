#ifndef FIREBEETLE_CONFIG_H
#define FIREBEETLE_CONFIG_H

// Pin definitions for FireBeetle ESP32-C6
#define BUTTON_PIN 4
#define LED_PIN 15
#define BATTERY_PIN 0  // A0 on FireBeetle ESP32-C6

// FireBeetle has battery monitoring
#ifndef HAS_BATTERY_MONITORING
#define HAS_BATTERY_MONITORING true
#endif

// Battery monitoring constants
#define BATT_SAMPLES 10         // Battery read averaging
#define uS_TO_S_FACTOR 1000000  // Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP  60       // Time ESP32 will sleep (in seconds)

#endif // FIREBEETLE_CONFIG_H