/*
 * PanicAlarm.ino
 * 
 * Main sketch file for the ESP32-based Panic Alarm system
 * Designed for ESP32 and ESP32-C6 boards
 */

#include <Arduino.h>
#include <WiFi.h>  // Added WiFi library include
#include "config.h"
#include "network.h"
#include "storage.h"
#include "notifications.h"

// Global variables for button handling
bool alarmTriggered = false;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
int buttonState = HIGH;
int lastButtonState = HIGH;

// Sleep control variables - device will not sleep for the first 5 minutes after boot
unsigned long startupTime = 0;
// Minimum runtime before allowing sleep (in milliseconds)
const unsigned long MIN_RUNTIME_BEFORE_SLEEP = 300000; // 300 seconds = 5 minutes

// Function declarations
void checkButton();
void triggerAlarm();

#if defined(ESP32_C6) && HAS_BATTERY_MONITORING && ENABLE_DEEP_SLEEP
void goToDeepSleep();
#endif

void setup() {
  Serial.begin(115200);
  
  // Record startup time
  startupTime = millis();
  Serial.println("Device starting up. Will not sleep for the first 300 seconds.");
  
  // Initialize pins
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  initLED();

  #if HAS_BATTERY_MONITORING
  initBattery();
  #endif
  
  // Initialize EEPROM
  initStorage();
  
  #if defined(ESP32_C6) && ENABLE_DEEP_SLEEP
  // Configure deep sleep wakeup for FireBeetle
  uint64_t bitmask = 1ULL << BUTTON_PIN;
  esp_sleep_enable_ext1_wakeup(bitmask, ESP_EXT1_WAKEUP_ANY_LOW);
  #endif
  
  // Check if device is configured
  if (!isConfigured()) {
    Serial.println("No configuration found. Starting setup mode...");
    startConfigMode();
  } else {
    loadConfig();
    if (connectToWiFi()) {
      Serial.println("Connected to WiFi. Starting normal operation.");
      blinkLED(3, 200); // Success indicator
    } else {
      Serial.println("Failed to connect to WiFi. Starting setup mode...");
      startConfigMode();
    }
  }
}

void loop() {
  if (isInConfigMode()) {
    // Handle DNS and web server in config mode
    handleConfigMode();
  } else {
    // Normal operation mode
    checkButton();
    
    #if HAS_BATTERY_MONITORING && ENABLE_DEEP_SLEEP
    checkBatteryStatus();
    
    // Check if we can go to sleep yet
    unsigned long currentTime = millis();
    if (currentTime - startupTime > MIN_RUNTIME_BEFORE_SLEEP) {
      // After the minimum runtime, we can consider going to sleep
      // Check if it's time for a regular sleep cycle (every 60 seconds by default)
      static unsigned long lastSleepCheck = 0;
      if (currentTime - lastSleepCheck > TIME_TO_SLEEP * 1000) {
        lastSleepCheck = currentTime;
        Serial.println("Regular sleep cycle - going to sleep");
        goToDeepSleep();
      }
    } else {
      // Log remaining time until sleep is allowed (every 30 seconds)
      if (currentTime % 30000 == 0) { 
        Serial.print("Sleep disabled for ");
        Serial.print((MIN_RUNTIME_BEFORE_SLEEP - (currentTime - startupTime)) / 1000);
        Serial.println(" more seconds");
      }
    }
    #endif
    
    // Continue handling web requests
    handleWebRequests();
    
    // Add short delay to prevent watchdog reset
    delay(10);
  }
}

// Check button state and send alarm if pressed
void checkButton() {
  int reading = digitalRead(BUTTON_PIN);
  
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      
      if (buttonState == LOW) { // Button pressed (active LOW with pull-up)
        Serial.println("Panic button pressed!");
        triggerAlarm();
      }
    }
  }
  
  lastButtonState = reading;
}

// Trigger the alarm and send notification
void triggerAlarm() {
  if (alarmTriggered) return; // Prevent multiple triggers in rapid succession
  
  alarmTriggered = true;
  setLED(true); // Turn on LED to indicate alarm
  
  if (sendEmailAlert()) {
    Serial.println("Alarm notification sent successfully");
    // Blink to indicate success
    for (int i = 0; i < 5; i++) {
      setLED(false);
      delay(100);
      setLED(true);
      delay(100);
    }
  } else {
    Serial.println("Failed to send alarm notification");
    // Different blink pattern for failure
    for (int i = 0; i < 2; i++) {
      setLED(false);
      delay(500);
      setLED(true);
      delay(500);
    }
  }
  
  // Reset alarm state after delay
  delay(5000);
  setLED(false);
  alarmTriggered = false;
}

#if defined(ESP32_C6) && HAS_BATTERY_MONITORING && ENABLE_DEEP_SLEEP
// Enter deep sleep mode to save battery (FireBeetle specific)
void goToDeepSleep() {
  // Check if we've been running long enough to allow sleep
  if (millis() - startupTime <= MIN_RUNTIME_BEFORE_SLEEP) {
    Serial.println("Not sleeping yet, minimum runtime not reached");
    return;
  }
  
  // Ensure any pending operations are completed
  if (WiFi.status() == WL_CONNECTED) {
    // Disconnect WiFi cleanly before sleep
    WiFi.disconnect(true);
    delay(100);
  }
  
  // Configure wake timer (TIME_TO_SLEEP is defined in firebeetle_config.h)
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  
  Serial.println("Going to deep sleep for " + String(TIME_TO_SLEEP) + " seconds");
  Serial.flush(); // Make sure serial output completes
  delay(100);
  esp_deep_sleep_start();
}
#endif

extern "C" void lwip_hook_ip6_input(void) {
  // No-op implementation to satisfy linker
}