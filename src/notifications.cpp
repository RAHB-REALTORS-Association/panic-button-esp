#include "notifications.h"
#include "config.h"
#include "storage.h"
#include <WiFi.h>
#include <ESP_Mail_Client.h>

//---------- LED FUNCTIONS ----------//

// Initialize LED pin
void initLED() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
}

// Set LED state
void setLED(bool state) {
  digitalWrite(LED_PIN, state ? HIGH : LOW);
}

// Blink LED
void blinkLED(int times, int delayms) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(delayms);
    digitalWrite(LED_PIN, LOW);
    delay(delayms);
  }
}

//---------- EMAIL FUNCTIONS ----------//

// Global SMTP client
SMTPSession smtp;

// Send email alert for panic button press
bool sendEmailAlert() {
  String server = getEmailServer();
  String recipient = getEmailRecipient();
  
  if (server.length() == 0 || recipient.length() == 0) {
    Serial.println("Email not configured");
    return false;
  }
  
  ESP_Mail_Session session;
  session.server.host_name = server.c_str();
  session.server.port = getEmailPort();
  session.login.email = getEmailUsername().c_str();
  session.login.password = getEmailPassword().c_str();
  
  SMTP_Message message;
  message.sender.name = "Panic Alarm";
  message.sender.email = getEmailUsername().c_str();
  message.subject = "PANIC ALARM TRIGGERED";
  message.addRecipient("User", recipient.c_str());
  
  String htmlMsg = "<div style='color:red;'><h1>PANIC ALARM TRIGGERED</h1>"
                   "<p>The panic button has been activated.</p>"
                   "<p>Time: " + String(millis() / 1000) + " seconds since device boot</p>"
                   "<p>Device IP: " + WiFi.localIP().toString() + "</p>";
  
  #if HAS_BATTERY_MONITORING
  htmlMsg += "<p>Battery voltage: " + String(getBatteryVoltage()) + "V</p>";
  #endif
  
  htmlMsg += "</div>";
  message.html.content = htmlMsg.c_str();
  
  if (!smtp.connect(&session)) {
    Serial.println("Error connecting to SMTP server");
    return false;
  }
  
  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.println("Error sending email: " + smtp.errorReason());
    return false;
  }
  
  return true;
}

//---------- BATTERY FUNCTIONS ----------//

#if HAS_BATTERY_MONITORING
// Battery global variables
float batteryVoltage = 0.0;
unsigned long lastBatteryCheck = 0;

// Initialize battery monitoring
void initBattery() {
  pinMode(BATTERY_PIN, INPUT);
  analogReadResolution(12); // 12-bit ADC resolution
  
  // Read initial battery voltage using onboard divider (×2)
  int mV = analogReadMilliVolts(BATTERY_PIN);
  batteryVoltage = (mV * 2) / 1000.0;
  Serial.print("Initial battery voltage: ");
  Serial.print(batteryVoltage);
  Serial.println(" V");
}

// Check battery status periodically
void checkBatteryStatus() {
  if (millis() - lastBatteryCheck > 60000) { // Check every minute
    lastBatteryCheck = millis();
    batteryVoltage = getBatteryVoltage();
    Serial.print("Battery voltage: ");
    Serial.println(batteryVoltage);
    
    if (isLowBattery()) {
      blinkLED(5, 50); // Fast blink to indicate low battery
      // Optionally send low battery notification email
      if (WiFi.status() == WL_CONNECTED) {
        sendLowBatteryAlert();
      }
    }
  }
}

// Get battery voltage
float getBatteryVoltage() {
  long total_mV = 0;
  for (int i = 0; i < BATT_SAMPLES; i++) {
    total_mV += analogReadMilliVolts(BATTERY_PIN);
    delay(5);
  }
  float avg_mV = total_mV / BATT_SAMPLES;
  return (avg_mV * 2) / 1000.0;  // Compensate for onboard voltage divider
}

// Check if battery is low
bool isLowBattery() {
  return batteryVoltage < 3.3; // Adjust threshold if needed
}

// Send low battery alert email (FireBeetle specific)
bool sendLowBatteryAlert() {
  String server = getEmailServer();
  String recipient = getEmailRecipient();
  
  if (server.length() == 0 || recipient.length() == 0) {
    Serial.println("Email not configured");
    return false;
  }
  
  ESP_Mail_Session session;
  session.server.host_name = server.c_str();
  session.server.port = getEmailPort();
  session.login.email = getEmailUsername().c_str();
  session.login.password = getEmailPassword().c_str();
  
  SMTP_Message message;
  message.sender.name = "Panic Alarm";
  message.sender.email = getEmailUsername().c_str();
  message.subject = "Panic Alarm - LOW BATTERY WARNING";
  message.addRecipient("User", recipient.c_str());
  
  String htmlMsg = "<div style='color:orange;'><h1>LOW BATTERY WARNING</h1>"
                   "<p>Your panic alarm device is running low on battery.</p>"
                   "<p>Current battery voltage: " + String(batteryVoltage) + "V</p>"
                   "<p>Please replace or recharge the battery soon.</p>"
                   "<p>Device IP: " + WiFi.localIP().toString() + "</p></div>";
  message.html.content = htmlMsg.c_str();
  
  if (!smtp.connect(&session)) {
    Serial.println("Error connecting to SMTP server");
    return false;
  }
  
  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.println("Error sending email: " + smtp.errorReason());
    return false;
  }
  
  return true;
}
#endif