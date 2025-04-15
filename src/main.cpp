#include <Arduino.h>
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

// Function declarations
void checkButton();
void triggerAlarm();

#if defined(ESP32_C6) && HAS_BATTERY_MONITORING
void goToDeepSleep();
#endif

void setup() {
  Serial.begin(115200);
  
  // Initialize pins
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  initLED();

  #if HAS_BATTERY_MONITORING
  initBattery();
  #endif
  
  // Initialize EEPROM
  initStorage();
  
  //#if defined(ESP32_C6)
  //// Configure deep sleep wakeup for FireBeetle
  //uint64_t bitmask = 1ULL << BUTTON_PIN;
  //esp_sleep_enable_ext1_wakeup(bitmask, ESP_EXT1_WAKEUP_ANY_LOW);
  //#endif
  
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
    
    #if HAS_BATTERY_MONITORING
    checkBatteryStatus();
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

//#if defined(ESP32_C6) && HAS_BATTERY_MONITORING
//// Enter deep sleep mode to save battery (FireBeetle specific)
//void goToDeepSleep() {
//  Serial.println("Going to deep sleep...");
//  delay(100);
//  esp_deep_sleep_start();
//}
//#endif

extern "C" void lwip_hook_ip6_input(void) {
  // No-op implementation to satisfy linker
}