#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

#include <Arduino.h>
#include "config.h"

// LED functions
void initLED();
void setLED(bool state);
void blinkLED(int times, int delayms);

// Email functions
bool sendEmailAlert();

// Battery functions (only on FireBeetle)
#if HAS_BATTERY_MONITORING
void initBattery();
void checkBatteryStatus();
float getBatteryVoltage();
bool isLowBattery();
bool sendLowBatteryAlert();
#endif

#endif // NOTIFICATIONS_H