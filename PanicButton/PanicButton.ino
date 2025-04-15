#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <ESP_Mail_Client.h>
#include <HTTPClient.h> // Added for webhook support

// Constants
#define EEPROM_SIZE 710 // Increased for webhook URL
#define CONFIG_FLAG_ADDR 0
#define SSID_ADDR 10
#define PASS_ADDR 80
#define EMAIL_SERVER_ADDR 150
#define EMAIL_PORT_ADDR 220
#define EMAIL_USER_ADDR 224
#define EMAIL_PASS_ADDR 294
#define EMAIL_RECIPIENT_ADDR 364
#define LOCATION_ADDR 434
#define WEBHOOK_URL_ADDR 504 // New address for webhook URL
#define WEBHOOK_ENABLED_ADDR 674 // New address for webhook enabled flag
#define EMAIL_ENABLED_ADDR 675 // New address for email enabled flag
#define BUTTON_PIN 4    // FireBeetle suitable GPIO pin
#define LED_PIN 15      // FireBeetle's onboard LED
#define BATTERY_PIN 0   // A0 on FireBeetle ESP32-C6
#define DNS_PORT 53
#define WEBSERVER_PORT 80
#define CONFIG_FLAG 0xAA
#define uS_TO_S_FACTOR 1000000  // Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP  60       // Time ESP32 will sleep (in seconds)
#define BATT_SAMPLES 10         // Battery read averaging

// Global variables
bool isConfigMode = false;
String configSSID = "PanicAlarm_"; // Will be updated with MAC address
const char* configPassword = "setupalarm"; // Optional: Set a password for the AP mode
String wifi_ssid = "";
String wifi_password = "";
String email_server = "";
int email_port = 0;
String email_username = "";
String email_password = "";
String email_recipient = "";
String device_location = "";
String webhook_url = ""; // New variable for webhook URL
bool webhook_enabled = false; // Flag to enable webhook
bool email_enabled = true; // Flag to enable email (default to true for backward compatibility)
bool alarmTriggered = false;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
int buttonState = HIGH;
int lastButtonState = HIGH;
float batteryVoltage = 0.0;
unsigned long lastBatteryCheck = 0;

WebServer server(WEBSERVER_PORT);
DNSServer dnsServer;
SMTPSession smtp;

// Setup function
void setup() {
  Serial.begin(115200);

  // Initialize pins
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(BATTERY_PIN, INPUT);
  analogReadResolution(12); // 12-bit ADC resolution

  // Generate unique SSID based on MAC
  configSSID = generateUniqueSSID();
  Serial.print("Generated unique SSID: ");
  Serial.println(configSSID);

  // Read initial battery voltage using onboard divider (×2)
  int mV = analogReadMilliVolts(BATTERY_PIN);
  batteryVoltage = (mV * 2) / 1000.0;
  Serial.print("Initial battery voltage: ");
  Serial.print(batteryVoltage);
  Serial.println(" V");

  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);

  // Configure deep sleep wakeup
  uint64_t bitmask = 1ULL << BUTTON_PIN;
  esp_sleep_enable_ext1_wakeup(bitmask, ESP_EXT1_WAKEUP_ANY_LOW);

  // Check if device is configured
  if (EEPROM.read(CONFIG_FLAG_ADDR) != CONFIG_FLAG) {
    Serial.println("No configuration found. Starting setup mode...");
    startConfigMode();
  } else {
    loadConfig();
    if (connectToWiFi()) {
      Serial.println("Connected to WiFi. Starting normal operation.");
      blinkLED(3, 200);
    } else {
      Serial.println("Failed to connect to WiFi. Starting setup mode...");
      startConfigMode();
    }
  }
}

// Generate unique SSID using the last 4 characters of MAC address
String generateUniqueSSID() {
  String baseName = "PanicAlarm_";
  
  // Initialize WiFi to ensure MAC is available
  WiFi.mode(WIFI_STA);
  delay(100); // Short delay to ensure WiFi is initialized
  
  uint8_t mac[6];
  WiFi.macAddress(mac);
  
  // Convert the last two bytes of MAC to HEX string
  char macStr[5];
  sprintf(macStr, "%02X%02X", mac[4], mac[5]);
  
  return baseName + macStr;
}

// Main loop
void loop() {
  if (isConfigMode) {
    // Handle DNS and web server in config mode
    dnsServer.processNextRequest();
    server.handleClient();
  } else {
    // Normal operation mode
    checkButton();
    checkBatteryStatus();
    server.handleClient(); // Continue handling web requests
    
    // Add short delay to prevent watchdog reset
    delay(10);
  }
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
      // Optionally send low battery notification
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

// Start configuration mode with AP and captive portal
void startConfigMode() {
  isConfigMode = true;
  
  // Set up Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(configSSID.c_str(), configPassword);
  
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  
  // Configure DNS server for captive portal
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  
  // Set up web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/setup", HTTP_POST, handleSetup);
  server.on("/style.css", HTTP_GET, handleCss);
  server.onNotFound(handleRoot); // Captive portal redirect
  
  server.begin();
  Serial.println("HTTP server started");
  
  // Blink LED to indicate config mode
  blinkLED(5, 100);
}

// Connect to saved WiFi network
bool connectToWiFi() {
  if (wifi_ssid.length() == 0) return false;
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
  
  Serial.print("Connecting to WiFi");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("Connected to WiFi: ");
    Serial.println(wifi_ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    // Enable WiFi power saving for battery life
    WiFi.setSleep(true);
    
    // Set up web server routes for normal operation
    server.on("/", HTTP_GET, handleNormalRoot);
    server.on("/config", HTTP_GET, handleConfigPage);
    server.on("/update", HTTP_POST, handleUpdate);
    server.on("/test", HTTP_GET, handleTestEmail);
    server.on("/test-webhook", HTTP_GET, handleTestWebhook);  // New handler for testing webhook
    server.on("/reset", HTTP_GET, handleReset);
    server.on("/style.css", HTTP_GET, handleCss);
    
    server.begin();
    return true;
  } else {
    Serial.println("");
    Serial.println("Failed to connect to WiFi");
    return false;
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
  digitalWrite(LED_PIN, HIGH); // Turn on LED to indicate alarm
  
  bool notificationSent = false;
  
  // Try to send webhook if enabled
  if (webhook_enabled && webhook_url.length() > 0) {
    if (sendWebhook()) {
      Serial.println("Webhook notification sent successfully");
      notificationSent = true;
    } else {
      Serial.println("Failed to send webhook notification");
    }
  }
  
  // Try to send email if enabled
  if (email_enabled && email_server.length() > 0 && email_recipient.length() > 0) {
    if (sendEmailAlert()) {
      Serial.println("Email notification sent successfully");
      notificationSent = true;
    } else {
      Serial.println("Failed to send email notification");
    }
  }
  
  if (notificationSent) {
    // Blink to indicate success
    for (int i = 0; i < 5; i++) {
      digitalWrite(LED_PIN, LOW);
      delay(100);
      digitalWrite(LED_PIN, HIGH);
      delay(100);
    }
  } else {
    // Different blink pattern for failure
    for (int i = 0; i < 2; i++) {
      digitalWrite(LED_PIN, LOW);
      delay(500);
      digitalWrite(LED_PIN, HIGH);
      delay(500);
    }
  }
  
  // Reset alarm state after delay
  delay(5000);
  digitalWrite(LED_PIN, LOW);
  alarmTriggered = false;
}

// Send webhook for panic button press
bool sendWebhook() {
  if (webhook_url.length() == 0 || !webhook_enabled) {
    Serial.println("Webhook not configured or disabled");
    return false;
  }
  
  HTTPClient http;
  http.begin(webhook_url);
  http.addHeader("Content-Type", "application/json");
  
  // Construct JSON payload
  String locationInfo = device_location.length() > 0 ? 
                        "\"location\": \"" + device_location + "\"," : 
                        "\"location\": \"Not specified\",";
  
  String jsonPayload = "{"
                      "\"event\": \"PANIC_ALARM_TRIGGERED\","
                      "\"device_id\": \"" + configSSID + "\","
                      "\"mac_address\": \"" + WiFi.macAddress() + "\","
                      + locationInfo +
                      "\"ip_address\": \"" + WiFi.localIP().toString() + "\","
                      "\"battery_voltage\": " + String(batteryVoltage) + ","
                      "\"triggered_at\": " + String(millis() / 1000) +
                      "}";
  
  int httpCode = http.POST(jsonPayload);
  
  if (httpCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpCode);
    http.end();
    return (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED || httpCode == HTTP_CODE_ACCEPTED);
  } else {
    Serial.print("Error sending HTTP request: ");
    Serial.println(http.errorToString(httpCode).c_str());
    http.end();
    return false;
  }
}

// Send email alert for panic button press
bool sendEmailAlert() {
  if (email_server.length() == 0 || email_recipient.length() == 0 || !email_enabled) {
    Serial.println("Email not configured or disabled");
    return false;
  }
  
  ESP_Mail_Session session;
  session.server.host_name = email_server.c_str();
  session.server.port = email_port;
  session.login.email = email_username.c_str();
  session.login.password = email_password.c_str();
  
  SMTP_Message message;
  message.sender.name = "Panic Alarm";
  message.sender.email = email_username.c_str();
  message.subject = "PANIC ALARM TRIGGERED";
  message.addRecipient("User", email_recipient.c_str());
  
  String locationInfo = device_location.length() > 0 ? 
                        "<p><strong>Location:</strong> " + device_location + "</p>" : 
                        "<p>No location specified</p>";
  
  String webhookStatus = webhook_enabled ? "<p>Webhook notifications: Enabled</p>" : "<p>Webhook notifications: Disabled</p>";
  
  String htmlMsg = "<div style='color:red;'><h1>PANIC ALARM TRIGGERED</h1>"
                   "<p>The panic button has been activated.</p>"
                   + locationInfo +
                   "<p><strong>Device ID:</strong> " + configSSID + "</p>"
                   "<p><strong>Time:</strong> " + String(millis() / 1000) + " seconds since device boot</p>"
                   "<p><strong>Device IP:</strong> " + WiFi.localIP().toString() + "</p>"
                   "<p><strong>Battery voltage:</strong> " + String(batteryVoltage) + "V</p>"
                   "<p><strong>MAC Address:</strong> " + WiFi.macAddress() + "</p>"
                   + webhookStatus +
                   "</div>";
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

// Send low battery alert
bool sendLowBatteryAlert() {
  bool notificationSent = false;
  
  // Try to send webhook if enabled
  if (webhook_enabled && webhook_url.length() > 0) {
    if (sendLowBatteryWebhook()) {
      Serial.println("Low battery webhook notification sent successfully");
      notificationSent = true;
    }
  }
  
  // Try to send email if enabled
  if (email_enabled && email_server.length() > 0 && email_recipient.length() > 0) {
    if (sendLowBatteryEmail()) {
      Serial.println("Low battery email notification sent successfully");
      notificationSent = true;
    }
  }
  
  return notificationSent;
}

// Send low battery alert via webhook
bool sendLowBatteryWebhook() {
  if (webhook_url.length() == 0 || !webhook_enabled) {
    Serial.println("Webhook not configured or disabled");
    return false;
  }
  
  HTTPClient http;
  http.begin(webhook_url);
  http.addHeader("Content-Type", "application/json");
  
  // Construct JSON payload
  String locationInfo = device_location.length() > 0 ? 
                        "\"location\": \"" + device_location + "\"," : 
                        "\"location\": \"Not specified\",";
  
  String jsonPayload = "{"
                      "\"event\": \"LOW_BATTERY_WARNING\","
                      "\"device_id\": \"" + configSSID + "\","
                      "\"mac_address\": \"" + WiFi.macAddress() + "\","
                      + locationInfo +
                      "\"ip_address\": \"" + WiFi.localIP().toString() + "\","
                      "\"battery_voltage\": " + String(batteryVoltage) + ","
                      "\"reported_at\": " + String(millis() / 1000) +
                      "}";
  
  int httpCode = http.POST(jsonPayload);
  
  if (httpCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpCode);
    http.end();
    return (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED || httpCode == HTTP_CODE_ACCEPTED);
  } else {
    Serial.print("Error sending HTTP request: ");
    Serial.println(http.errorToString(httpCode).c_str());
    http.end();
    return false;
  }
}

// Send low battery alert via email
bool sendLowBatteryEmail() {
  if (email_server.length() == 0 || email_recipient.length() == 0 || !email_enabled) {
    Serial.println("Email not configured or disabled");
    return false;
  }
  
  ESP_Mail_Session session;
  session.server.host_name = email_server.c_str();
  session.server.port = email_port;
  session.login.email = email_username.c_str();
  session.login.password = email_password.c_str();
  
  SMTP_Message message;
  message.sender.name = "Panic Alarm";
  message.sender.email = email_username.c_str();
  message.subject = "Panic Alarm - LOW BATTERY WARNING";
  message.addRecipient("User", email_recipient.c_str());
  
  String locationInfo = device_location.length() > 0 ? 
                        "<p><strong>Location:</strong> " + device_location + "</p>" : 
                        "<p>No location specified</p>";
  
  String webhookStatus = webhook_enabled ? "<p>Webhook notifications: Enabled</p>" : "<p>Webhook notifications: Disabled</p>";
  
  String htmlMsg = "<div style='color:orange;'><h1>LOW BATTERY WARNING</h1>"
                   "<p>Your panic alarm device is running low on battery.</p>"
                   + locationInfo +
                   "<p><strong>Device ID:</strong> " + configSSID + "</p>"
                   "<p><strong>Current battery voltage:</strong> " + String(batteryVoltage) + "V</p>"
                   "<p><strong>Device IP:</strong> " + WiFi.localIP().toString() + "</p>"
                   "<p><strong>MAC Address:</strong> " + WiFi.macAddress() + "</p>"
                   + webhookStatus +
                   "<p>Please replace or recharge the battery soon.</p></div>";
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

// Load configuration from EEPROM
void loadConfig() {
  // Read WiFi SSID
  char buffer[170]; // Increased buffer size for webhook URL
  for (int i = 0; i < 70; i++) {
    buffer[i] = EEPROM.read(SSID_ADDR + i);
  }
  wifi_ssid = String(buffer);
  
  // Read WiFi password
  memset(buffer, 0, 170);
  for (int i = 0; i < 70; i++) {
    buffer[i] = EEPROM.read(PASS_ADDR + i);
  }
  wifi_password = String(buffer);
  
  // Read email server
  memset(buffer, 0, 170);
  for (int i = 0; i < 70; i++) {
    buffer[i] = EEPROM.read(EMAIL_SERVER_ADDR + i);
  }
  email_server = String(buffer);
  
  // Read email port
  email_port = EEPROM.read(EMAIL_PORT_ADDR) + (EEPROM.read(EMAIL_PORT_ADDR + 1) << 8) + 
               (EEPROM.read(EMAIL_PORT_ADDR + 2) << 16) + (EEPROM.read(EMAIL_PORT_ADDR + 3) << 24);
  
  // Read email username
  memset(buffer, 0, 170);
  for (int i = 0; i < 70; i++) {
    buffer[i] = EEPROM.read(EMAIL_USER_ADDR + i);
  }
  email_username = String(buffer);
  
  // Read email password
  memset(buffer, 0, 170);
  for (int i = 0; i < 70; i++) {
    buffer[i] = EEPROM.read(EMAIL_PASS_ADDR + i);
  }
  email_password = String(buffer);
  
  // Read email recipient
  memset(buffer, 0, 170);
  for (int i = 0; i < 70; i++) {
    buffer[i] = EEPROM.read(EMAIL_RECIPIENT_ADDR + i);
  }
  email_recipient = String(buffer);
  
  // Read location
  memset(buffer, 0, 170);
  for (int i = 0; i < 70; i++) {
    buffer[i] = EEPROM.read(LOCATION_ADDR + i);
  }
  device_location = String(buffer);
  
  // Read webhook URL (longer string up to 170 chars)
  memset(buffer, 0, 170);
  for (int i = 0; i < 170; i++) {
    buffer[i] = EEPROM.read(WEBHOOK_URL_ADDR + i);
  }
  webhook_url = String(buffer);
  
  // Read webhook enabled flag
  webhook_enabled = EEPROM.read(WEBHOOK_ENABLED_ADDR) == 1;
  
  // Read email enabled flag (default to true if not set)
  email_enabled = EEPROM.read(EMAIL_ENABLED_ADDR) != 0;
  
  // Print loaded configuration
  Serial.println("Loaded configuration:");
  Serial.println("WiFi SSID: " + wifi_ssid);
  Serial.println("Email server: " + email_server);
  Serial.println("Email port: " + String(email_port));
  Serial.println("Email username: " + email_username);
  Serial.println("Email recipient: " + email_recipient);
  Serial.println("Email enabled: " + String(email_enabled ? "Yes" : "No"));
  Serial.println("Device location: " + device_location);
  Serial.println("Webhook URL: " + webhook_url);
  Serial.println("Webhook enabled: " + String(webhook_enabled ? "Yes" : "No"));
}

// Save configuration to EEPROM
void saveConfig(String ssid, String password, String server, int port, 
                String username, String emailpass, String recipient, String location,
                String webhook, bool webhook_en, bool email_en) {
  // Save WiFi SSID
  for (unsigned int i = 0; i < ssid.length(); i++) {
    EEPROM.write(SSID_ADDR + i, ssid[i]);
  }
  EEPROM.write(SSID_ADDR + ssid.length(), 0); // Null terminator
  
  // Save WiFi password
  for (unsigned int i = 0; i < password.length(); i++) {
    EEPROM.write(PASS_ADDR + i, password[i]);
  }
  EEPROM.write(PASS_ADDR + password.length(), 0); // Null terminator
  
  // Save email server
  for (unsigned int i = 0; i < server.length(); i++) {
    EEPROM.write(EMAIL_SERVER_ADDR + i, server[i]);
  }
  EEPROM.write(EMAIL_SERVER_ADDR + server.length(), 0); // Null terminator
  
  // Save email port
  EEPROM.write(EMAIL_PORT_ADDR, port & 0xFF);
  EEPROM.write(EMAIL_PORT_ADDR + 1, (port >> 8) & 0xFF);
  EEPROM.write(EMAIL_PORT_ADDR + 2, (port >> 16) & 0xFF);
  EEPROM.write(EMAIL_PORT_ADDR + 3, (port >> 24) & 0xFF);
  
  // Save email username
  for (unsigned int i = 0; i < username.length(); i++) {
    EEPROM.write(EMAIL_USER_ADDR + i, username[i]);
  }
  EEPROM.write(EMAIL_USER_ADDR + username.length(), 0); // Null terminator
  
  // Save email password
  for (unsigned int i = 0; i < emailpass.length(); i++) {
    EEPROM.write(EMAIL_PASS_ADDR + i, emailpass[i]);
  }
  EEPROM.write(EMAIL_PASS_ADDR + emailpass.length(), 0); // Null terminator
  
  // Save email recipient
  for (unsigned int i = 0; i < recipient.length(); i++) {
    EEPROM.write(EMAIL_RECIPIENT_ADDR + i, recipient[i]);
  }
  EEPROM.write(EMAIL_RECIPIENT_ADDR + recipient.length(), 0); // Null terminator
  
  // Save location
  for (unsigned int i = 0; i < location.length(); i++) {
    EEPROM.write(LOCATION_ADDR + i, location[i]);
  }
  EEPROM.write(LOCATION_ADDR + location.length(), 0); // Null terminator
  
  // Save webhook URL (longer string)
  for (unsigned int i = 0; i < webhook.length() && i < 170; i++) {
    EEPROM.write(WEBHOOK_URL_ADDR + i, webhook[i]);
  }
  EEPROM.write(WEBHOOK_URL_ADDR + min(webhook.length(), (unsigned int)170), 0); // Null terminator
  
  // Save webhook enabled flag
  EEPROM.write(WEBHOOK_ENABLED_ADDR, webhook_en ? 1 : 0);
  
  // Save email enabled flag
  EEPROM.write(EMAIL_ENABLED_ADDR, email_en ? 1 : 0);
  
  // Set configuration flag
  EEPROM.write(CONFIG_FLAG_ADDR, CONFIG_FLAG);
  
  // Commit changes to EEPROM
  EEPROM.commit();
  
  Serial.println("Configuration saved");
}

void blinkLED(int times, int delayms) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(delayms);
    digitalWrite(LED_PIN, LOW);
    delay(delayms);
  }
}

// Enter deep sleep mode to save battery
void goToDeepSleep() {
  Serial.println("Going to deep sleep...");
  delay(100);
  esp_deep_sleep_start();
}

// Web handlers
void handleRoot() {
  String html = "<!DOCTYPE html><html><head>"
                "<title>Panic Alarm Setup</title>"
                "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                "<link rel='stylesheet' href='style.css'>"
                "<script>"
                "function toggleEmailFields() {"
                "  const enabled = document.getElementById('email_enabled').checked;"
                "  const fields = document.getElementById('email-fields');"
                "  fields.style.display = enabled ? 'block' : 'none';"
                "  validateForm();"
                "}"
                "function toggleWebhookFields() {"
                "  const enabled = document.getElementById('webhook_enabled').checked;"
                "  const fields = document.getElementById('webhook-fields');"
                "  fields.style.display = enabled ? 'block' : 'none';"
                "  validateForm();"
                "}"
                "function validateForm() {"
                "  const emailEnabled = document.getElementById('email_enabled').checked;"
                "  const webhookEnabled = document.getElementById('webhook_enabled').checked;"
                "  const submitBtn = document.getElementById('submit-btn');"
                "  submitBtn.disabled = !emailEnabled && !webhookEnabled;"
                "  if (!emailEnabled && !webhookEnabled) {"
                "    document.getElementById('validation-msg').style.display = 'block';"
                "  } else {"
                "    document.getElementById('validation-msg').style.display = 'none';"
                "  }"
                "}"
                "</script>"
                "</head><body>"
                "<div class='container'>"
                "<h1>Panic Alarm Setup</h1>"
                "<p><strong>Device ID: </strong>" + configSSID + "</p>"
                "<form action='/setup' method='post' onsubmit='return validateForm()'>"
                "<div class='section'>"
                "<h2>WiFi Settings</h2>"
                "<label for='ssid'>WiFi SSID:</label>"
                "<input type='text' id='ssid' name='ssid' required><br>"
                "<label for='password'>WiFi Password:</label>"
                "<input type='password' id='password' name='password'><br>"
                "</div>"
                "<div class='section'>"
                "<h2>Notification Settings</h2>"
                "<p class='note'>At least one notification method must be enabled.</p>"
                "<div id='validation-msg' class='validation-error' style='display:none;'>"
                "Please enable at least one notification method."
                "</div>"
                
                "<div class='toggle-section'>"
                "<h3>Email Notifications</h3>"
                "<div class='toggle-container'>"
                "<label class='switch'>"
                "<input type='checkbox' id='email_enabled' name='email_enabled' checked onchange='toggleEmailFields()'>"
                "<span class='slider'></span>"
                "</label>"
                "<span class='toggle-label'>Enable Email Notifications</span>"
                "</div>"
                "<div id='email-fields'>"
                "<label for='email_server'>SMTP Server:</label>"
                "<input type='text' id='email_server' name='email_server'><br>"
                "<label for='email_port'>SMTP Port:</label>"
                "<input type='number' id='email_port' name='email_port' value='587'><br>"
                "<label for='email_username'>Email Username:</label>"
                "<input type='text' id='email_username' name='email_username'><br>"
                "<label for='email_password'>Email Password:</label>"
                "<input type='password' id='email_password' name='email_password'><br>"
                "<label for='email_recipient'>Recipient Email:</label>"
                "<input type='email' id='email_recipient' name='email_recipient'><br>"
                "</div>"
                "</div>"
                
                "<div class='toggle-section'>"
                "<h3>Webhook Notifications</h3>"
                "<div class='toggle-container'>"
                "<label class='switch'>"
                "<input type='checkbox' id='webhook_enabled' name='webhook_enabled' onchange='toggleWebhookFields()'>"
                "<span class='slider'></span>"
                "</label>"
                "<span class='toggle-label'>Enable Webhook Notifications</span>"
                "</div>"
                "<div id='webhook-fields' style='display:none;'>"
                "<label for='webhook_url'>Webhook URL:</label>"
                "<input type='url' id='webhook_url' name='webhook_url' placeholder='https://example.com/webhook'><br>"
                "<p class='info'>The device will send a JSON payload to this URL when the alarm is triggered.</p>"
                "</div>"
                "</div>"
                
                "<div class='section'>"
                "<h2>Device Settings</h2>"
                "<label for='location'>Location Description:</label>"
                "<input type='text' id='location' name='location' placeholder='e.g. Living Room, Front Door, etc.'><br>"
                "</div>"
                "<input type='submit' id='submit-btn' value='Save Configuration'>"
                "</form>"
                "</div>"
                "<script>"
                "// Initialize toggle states on page load"
                "document.addEventListener('DOMContentLoaded', function() {"
                "  toggleEmailFields();"
                "  toggleWebhookFields();"
                "  validateForm();"
                "});"
                "</script>"
                "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleSetup() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  String email_server = server.arg("email_server");
  int email_port = server.arg("email_port").toInt();
  String email_username = server.arg("email_username");
  String email_password = server.arg("email_password");
  String email_recipient = server.arg("email_recipient");
  String location = server.arg("location");
  String webhook_url = server.arg("webhook_url");
  bool webhook_enabled = server.hasArg("webhook_enabled");
  bool email_enabled = server.hasArg("email_enabled");
  
  // Validate that at least one notification method is enabled
  if (!webhook_enabled && !email_enabled) {
    String errorHtml = "<!DOCTYPE html><html><head>"
                      "<title>Setup Error</title>"
                      "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                      "<link rel='stylesheet' href='style.css'>"
                      "<meta http-equiv='refresh' content='5;url=/'>"
                      "</head><body>"
                      "<div class='container'>"
                      "<h1>Setup Error</h1>"
                      "<p class='error'>At least one notification method (Email or Webhook) must be enabled.</p>"
                      "<p>Redirecting back to setup page in 5 seconds...</p>"
                      "</div></body></html>";
    
    server.send(400, "text/html", errorHtml);
    return;
  }
  
  // Save configuration
  saveConfig(ssid, password, email_server, email_port, email_username, email_password, email_recipient, location, webhook_url, webhook_enabled, email_enabled);
  
  String html = "<!DOCTYPE html><html><head>"
                "<title>Setup Complete</title>"
                "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                "<link rel='stylesheet' href='style.css'>"
                "<meta http-equiv='refresh' content='10;url=/'>"
                "</head><body>"
                "<div class='container'>"
                "<h1>Setup Complete</h1>"
                "<p>Your configuration has been saved. The device will restart in normal mode.</p>"
                "<p>If the device connects successfully to your WiFi network, it will be available at its assigned IP address.</p>"
                "<p>Restarting in 10 seconds...</p>"
                "</div></body></html>";
  
  server.send(200, "text/html", html);
  
  // Wait a bit and then restart
  delay(2000);
  ESP.restart();
}

void handleNormalRoot() {
  String notificationStatus = "";
  if (email_enabled && webhook_enabled) {
    notificationStatus = "Email and Webhook notifications enabled";
  } else if (email_enabled) {
    notificationStatus = "Email notifications enabled";
  } else if (webhook_enabled) {
    notificationStatus = "Webhook notifications enabled";
  } else {
    notificationStatus = "No notifications enabled!";
  }
  
  String html = "<!DOCTYPE html><html><head>"
                "<title>Panic Alarm Control</title>"
                "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                "<link rel='stylesheet' href='style.css'>"
                "</head><body>"
                "<div class='container'>"
                "<h1>Panic Alarm Control Panel</h1>"
                "<p>Device is operational and monitoring for panic button presses.</p>"
                "<div class='status'>"
                "<p><strong>Device ID:</strong> " + configSSID + "</p>"
                "<p><strong>MAC Address:</strong> " + WiFi.macAddress() + "</p>"
                "<p><strong>Location:</strong> " + (device_location.length() > 0 ? device_location : "Not specified") + "</p>"
                "<p><strong>WiFi SSID:</strong> " + wifi_ssid + "</p>"
                "<p><strong>IP Address:</strong> " + WiFi.localIP().toString() + "</p>"
                "<p><strong>Notification:</strong> " + notificationStatus + "</p>"
                "<p><strong>Battery Voltage:</strong> " + String(batteryVoltage) + "V</p>"
                "</div>"
                "<div class='buttons'>"
                "<a href='/config' class='button'>Update Configuration</a>";
  
  if (email_enabled) {
    html += "<a href='/test' class='button test'>Test Email</a>";
  }
  
  if (webhook_enabled) {
    html += "<a href='/test-webhook' class='button test'>Test Webhook</a>";
  }
  
  html += "<a href='/reset' class='button reset'>Factory Reset</a>"
          "</div>"
          "</div></body></html>";
  
  server.send(200, "text/html", html);
}

void handleConfigPage() {
  String html = "<!DOCTYPE html><html><head>"
                "<title>Update Configuration</title>"
                "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                "<link rel='stylesheet' href='style.css'>"
                "<script>"
                "function toggleEmailFields() {"
                "  const enabled = document.getElementById('email_enabled').checked;"
                "  const fields = document.getElementById('email-fields');"
                "  fields.style.display = enabled ? 'block' : 'none';"
                "  validateForm();"
                "}"
                "function toggleWebhookFields() {"
                "  const enabled = document.getElementById('webhook_enabled').checked;"
                "  const fields = document.getElementById('webhook-fields');"
                "  fields.style.display = enabled ? 'block' : 'none';"
                "  validateForm();"
                "}"
                "function validateForm() {"
                "  const emailEnabled = document.getElementById('email_enabled').checked;"
                "  const webhookEnabled = document.getElementById('webhook_enabled').checked;"
                "  const submitBtn = document.getElementById('submit-btn');"
                "  submitBtn.disabled = !emailEnabled && !webhookEnabled;"
                "  if (!emailEnabled && !webhookEnabled) {"
                "    document.getElementById('validation-msg').style.display = 'block';"
                "  } else {"
                "    document.getElementById('validation-msg').style.display = 'none';"
                "  }"
                "}"
                "</script>"
                "</head><body>"
                "<div class='container'>"
                "<h1>Update Configuration</h1>"
                "<p><strong>Device ID: </strong>" + configSSID + "</p>"
                "<form action='/update' method='post' onsubmit='return validateForm()'>"
                "<div class='section'>"
                "<h2>WiFi Settings</h2>"
                "<label for='ssid'>WiFi SSID:</label>"
                "<input type='text' id='ssid' name='ssid' value='" + wifi_ssid + "' required><br>"
                "<label for='password'>WiFi Password:</label>"
                "<input type='password' id='password' name='password' value='" + wifi_password + "'><br>"
                "</div>"
                "<div class='section'>"
                "<h2>Notification Settings</h2>"
                "<p class='note'>At least one notification method must be enabled.</p>"
                "<div id='validation-msg' class='validation-error' style='display:none;'>"
                "Please enable at least one notification method."
                "</div>"
                
                "<div class='toggle-section'>"
                "<h3>Email Notifications</h3>"
                "<div class='toggle-container'>"
                "<label class='switch'>"
                "<input type='checkbox' id='email_enabled' name='email_enabled' " + (email_enabled ? "checked" : "") + " onchange='toggleEmailFields()'>"
                "<span class='slider'></span>"
                "</label>"
                "<span class='toggle-label'>Enable Email Notifications</span>"
                "</div>"
                "<div id='email-fields' style='" + (email_enabled ? "display:block" : "display:none") + "'>"
                "<label for='email_server'>SMTP Server:</label>"
                "<input type='text' id='email_server' name='email_server' value='" + email_server + "'><br>"
                "<label for='email_port'>SMTP Port:</label>"
                "<input type='number' id='email_port' name='email_port' value='" + String(email_port) + "'><br>"
                "<label for='email_username'>Email Username:</label>"
                "<input type='text' id='email_username' name='email_username' value='" + email_username + "'><br>"
                "<label for='email_password'>Email Password:</label>"
                "<input type='password' id='email_password' name='email_password' value='" + email_password + "'><br>"
                "<label for='email_recipient'>Recipient Email:</label>"
                "<input type='email' id='email_recipient' name='email_recipient' value='" + email_recipient + "'><br>"
                "</div>"
                "</div>"
                
                "<div class='toggle-section'>"
                "<h3>Webhook Notifications</h3>"
                "<div class='toggle-container'>"
                "<label class='switch'>"
                "<input type='checkbox' id='webhook_enabled' name='webhook_enabled' " + (webhook_enabled ? "checked" : "") + " onchange='toggleWebhookFields()'>"
                "<span class='slider'></span>"
                "</label>"
                "<span class='toggle-label'>Enable Webhook Notifications</span>"
                "</div>"
                "<div id='webhook-fields' style='" + (webhook_enabled ? "display:block" : "display:none") + "'>"
                "<label for='webhook_url'>Webhook URL:</label>"
                "<input type='url' id='webhook_url' name='webhook_url' value='" + webhook_url + "' placeholder='https://example.com/webhook'><br>"
                "<p class='info'>The device will send a JSON payload to this URL when the alarm is triggered.</p>"
                "</div>"
                "</div>"
                
                "<div class='section'>"
                "<h2>Device Settings</h2>"
                "<label for='location'>Location Description:</label>"
                "<input type='text' id='location' name='location' value='" + device_location + "' placeholder='e.g. Living Room, Front Door, etc.'><br>"
                "</div>"
                "<input type='submit' id='submit-btn' value='Update Configuration'>"
                "</form>"
                "<div class='back'><a href='/' class='button'>Back to Home</a></div>"
                "</div>"
                "<script>"
                "// Initialize toggle states on page load"
                "document.addEventListener('DOMContentLoaded', function() {"
                "  toggleEmailFields();"
                "  toggleWebhookFields();"
                "  validateForm();"
                "});"
                "</script>"
                "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleUpdate() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  String email_server = server.arg("email_server");
  int email_port = server.arg("email_port").toInt();
  String email_username = server.arg("email_username");
  String email_password = server.arg("email_password");
  String email_recipient = server.arg("email_recipient");
  String location = server.arg("location");
  String webhook_url = server.arg("webhook_url");
  bool webhook_enabled = server.hasArg("webhook_enabled");
  bool email_enabled = server.hasArg("email_enabled");
  
  // Validate that at least one notification method is enabled
  if (!webhook_enabled && !email_enabled) {
    String errorHtml = "<!DOCTYPE html><html><head>"
                      "<title>Update Error</title>"
                      "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                      "<link rel='stylesheet' href='style.css'>"
                      "<meta http-equiv='refresh' content='5;url=/config'>"
                      "</head><body>"
                      "<div class='container'>"
                      "<h1>Update Error</h1>"
                      "<p class='error'>At least one notification method (Email or Webhook) must be enabled.</p>"
                      "<p>Redirecting back to configuration page in 5 seconds...</p>"
                      "</div></body></html>";
    
    server.send(400, "text/html", errorHtml);
    return;
  }
  
  // Save configuration
  saveConfig(ssid, password, email_server, email_port, email_username, email_password, email_recipient, location, webhook_url, webhook_enabled, email_enabled);
  
  String html = "<!DOCTYPE html><html><head>"
                "<title>Configuration Updated</title>"
                "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                "<link rel='stylesheet' href='style.css'>"
                "<meta http-equiv='refresh' content='5;url=/'>"
                "</head><body>"
                "<div class='container'>"
                "<h1>Configuration Updated</h1>"
                "<p>Your settings have been saved.</p>"
                "<p>Redirecting to home page in 5 seconds...</p>"
                "</div></body></html>";
  
  server.send(200, "text/html", html);
  
  // Load the new configuration
  loadConfig();
}

void handleTestEmail() {
  String result;
  if (email_enabled) {
    if (sendEmailAlert()) {
      result = "Test email sent successfully!";
    } else {
      result = "Failed to send test email. Please check your email settings.";
    }
  } else {
    result = "Email notifications are disabled. Please enable them in settings.";
  }
  
  String html = "<!DOCTYPE html><html><head>"
                "<title>Test Result</title>"
                "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                "<link rel='stylesheet' href='style.css'>"
                "<meta http-equiv='refresh' content='5;url=/'>"
                "</head><body>"
                "<div class='container'>"
                "<h1>Test Result</h1>"
                "<p>" + result + "</p>"
                "<p>Redirecting to home page in 5 seconds...</p>"
                "</div></body></html>";
  
  server.send(200, "text/html", html);
}

void handleTestWebhook() {
  String result;
  if (webhook_enabled) {
    if (sendWebhook()) {
      result = "Test webhook sent successfully!";
    } else {
      result = "Failed to send test webhook. Please check your webhook URL and network connection.";
    }
  } else {
    result = "Webhook notifications are disabled. Please enable them in settings.";
  }
  
  String html = "<!DOCTYPE html><html><head>"
                "<title>Test Result</title>"
                "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                "<link rel='stylesheet' href='style.css'>"
                "<meta http-equiv='refresh' content='5;url=/'>"
                "</head><body>"
                "<div class='container'>"
                "<h1>Test Result</h1>"
                "<p>" + result + "</p>"
                "<p>Redirecting to home page in 5 seconds...</p>"
                "</div></body></html>";
  
  server.send(200, "text/html", html);
}

void handleReset() {
  String html = "<!DOCTYPE html><html><head>"
                "<title>Factory Reset</title>"
                "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                "<link rel='stylesheet' href='style.css'>"
                "</head><body>"
                "<div class='container'>"
                "<h1>Factory Reset</h1>"
                "<p>Are you sure you want to reset all settings?</p>"
                "<p>This will erase all configuration and restart the device in setup mode.</p>"
                "<div class='buttons'>"
                "<a href='/' class='button'>Cancel</a>"
                "<a href='javascript:confirmReset()' class='button reset'>Yes, Reset Everything</a>"
                "</div>"
                "<script>"
                "function confirmReset() {"
                "  fetch('/reset?confirm=true')"
                "  .then(response => {"
                "    document.body.innerHTML = '<div class=\"container\"><h1>Factory Reset</h1><p>Device is resetting...</p></div>';"
                "  });"
                "}"
                "</script>"
                "</div></body></html>";
  
  if (server.hasArg("confirm") && server.arg("confirm") == "true") {
    // Erase configuration flag to force setup mode
    EEPROM.write(CONFIG_FLAG_ADDR, 0);
    EEPROM.commit();
    
    server.send(200, "text/html", "<!DOCTYPE html><html><head>"
                "<title>Resetting</title>"
                "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                "<link rel='stylesheet' href='style.css'>"
                "</head><body>"
                "<div class='container'>"
                "<h1>Factory Reset</h1>"
                "<p>Device is resetting. Connect to the WiFi network \"" + configSSID + "\" to set up the device again.</p>"
                "</div></body></html>");
    
    delay(1000);
    ESP.restart();
  } else {
    server.send(200, "text/html", html);
  }
}

void handleCss() {
  String css = "body {"
               "  font-family: Arial, sans-serif;"
               "  margin: 0;"
               "  padding: 0;"
               "  background-color: #f0f0f0;"
               "}"
               ".container {"
               "  max-width: 600px;"
               "  margin: 0 auto;"
               "  padding: 20px;"
               "}"
               "h1, h2, h3 {"
               "  color: #333;"
               "}"
               ".section {"
               "  background-color: #fff;"
               "  border-radius: 5px;"
               "  padding: 15px;"
               "  margin-bottom: 20px;"
               "  box-shadow: 0 2px 5px rgba(0,0,0,0.1);"
               "}"
               ".toggle-section {"
               "  background-color: #fff;"
               "  border-radius: 5px;"
               "  padding: 15px;"
               "  margin-bottom: 15px;"
               "  box-shadow: 0 2px 5px rgba(0,0,0,0.1);"
               "}"
               "label {"
               "  display: block;"
               "  margin-top: 10px;"
               "  font-weight: bold;"
               "}"
               "input[type='text'], input[type='password'], input[type='email'], input[type='number'], input[type='url'] {"
               "  width: 100%;"
               "  padding: 8px;"
               "  margin-top: 5px;"
               "  border: 1px solid #ddd;"
               "  border-radius: 4px;"
               "  box-sizing: border-box;"
               "}"
               "input[type='submit'], .button {"
               "  background-color: #4CAF50;"
               "  color: white;"
               "  padding: 10px 15px;"
               "  border: none;"
               "  border-radius: 4px;"
               "  cursor: pointer;"
               "  font-size: 16px;"
               "  margin-top: 10px;"
               "  display: inline-block;"
               "  text-decoration: none;"
               "}"
               "input[type='submit']:disabled {"
               "  background-color: #cccccc;"
               "  cursor: not-allowed;"
               "}"
               ".button {"
               "  margin-right: 10px;"
               "}"
               ".test {"
               "  background-color: #2196F3;"
               "}"
               ".reset {"
               "  background-color: #f44336;"
               "}"
               ".buttons {"
               "  margin-top: 20px;"
               "}"
               ".status {"
               "  background-color: #fff;"
               "  border-radius: 5px;"
               "  padding: 15px;"
               "  margin-bottom: 20px;"
               "  box-shadow: 0 2px 5px rgba(0,0,0,0.1);"
               "}"
               ".back {"
               "  margin-top: 20px;"
               "}"
               ".note {"
               "  color: #666;"
               "  font-style: italic;"
               "  margin-bottom: 10px;"
               "}"
               ".info {"
               "  color: #2196F3;"
               "  font-size: 0.9em;"
               "  margin-top: 5px;"
               "}"
               ".validation-error {"
               "  color: #f44336;"
               "  font-weight: bold;"
               "  margin: 10px 0;"
               "  padding: 10px;"
               "  background-color: #ffebee;"
               "  border-radius: 4px;"
               "}"
               ".error {"
               "  color: #f44336;"
               "  font-weight: bold;"
               "}"
               ".switch {"
               "  position: relative;"
               "  display: inline-block;"
               "  width: 50px;"
               "  height: 24px;"
               "  margin-right: 10px;"
               "}"
               ".switch input {"
               "  opacity: 0;"
               "  width: 0;"
               "  height: 0;"
               "}"
               ".slider {"
               "  position: absolute;"
               "  cursor: pointer;"
               "  top: 0;"
               "  left: 0;"
               "  right: 0;"
               "  bottom: 0;"
               "  background-color: #ccc;"
               "  transition: .4s;"
               "  border-radius: 24px;"
               "}"
               ".slider:before {"
               "  position: absolute;"
               "  content: \"\";"
               "  height: 16px;"
               "  width: 16px;"
               "  left: 4px;"
               "  bottom: 4px;"
               "  background-color: white;"
               "  transition: .4s;"
               "  border-radius: 50%;"
               "}"
               "input:checked + .slider {"
               "  background-color: #2196F3;"
               "}"
               "input:checked + .slider:before {"
               "  transform: translateX(26px);"
               "}"
               ".toggle-container {"
               "  display: flex;"
               "  align-items: center;"
               "  margin-bottom: 10px;"
               "}"
               ".toggle-label {"
               "  font-weight: bold;"
               "}"
               "@media (max-width: 480px) {"
               "  .container {"
               "    padding: 10px;"
               "  }"
               "  .button {"
               "    display: block;"
               "    margin-bottom: 10px;"
               "    text-align: center;"
               "  }"
               "}";
               
  server.send(200, "text/css", css);
}