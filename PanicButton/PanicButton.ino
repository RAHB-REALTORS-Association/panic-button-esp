#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <ESP_Mail_Client.h>
#include <HTTPClient.h>

// Constants
#define EEPROM_SIZE 612  // Increased to accommodate webhook URL
#define CONFIG_FLAG_ADDR 0
#define SSID_ADDR 10
#define PASS_ADDR 80
#define EMAIL_SERVER_ADDR 150
#define EMAIL_PORT_ADDR 220
#define EMAIL_USER_ADDR 224
#define EMAIL_PASS_ADDR 294
#define EMAIL_RECIPIENT_ADDR 364
#define WEBHOOK_URL_ADDR 434  // New address for webhook URL
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
String configSSID = "PanicAlarm_Setup";
const char* configPassword = "setupalarm"; // Optional: Set a password for the AP mode
String wifi_ssid = "";
String wifi_password = "";
String email_server = "";
int email_port = 0;
String email_username = "";
String email_password = "";
String email_recipient = "";
String webhook_url = "";  // New variable for webhook URL
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
    server.on("/test", HTTP_GET, handleTestAlert);
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
  
  // Try to send email if configured
  if (email_recipient.length() > 0) {
    if (sendEmailAlert()) {
      Serial.println("Email notification sent successfully");
      notificationSent = true;
    } else {
      Serial.println("Failed to send email notification");
    }
  }
  
  // Try to send webhook if configured
  if (webhook_url.length() > 0) {
    if (sendWebhookAlert()) {
      Serial.println("Webhook notification sent successfully");
      notificationSent = true;
    } else {
      Serial.println("Failed to send webhook notification");
    }
  }
  
  // Indicate success or failure
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

// Send email alert for panic button press
bool sendEmailAlert() {
  if (email_server.length() == 0 || email_recipient.length() == 0) {
    Serial.println("Email not configured");
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
  
  String htmlMsg = "<div style='color:red;'><h1>PANIC ALARM TRIGGERED</h1>"
                   "<p>The panic button has been activated.</p>"
                   "<p>Time: " + String(millis() / 1000) + " seconds since device boot</p>"
                   "<p>Device IP: " + WiFi.localIP().toString() + "</p>"
                   "<p>Battery voltage: " + String(batteryVoltage) + "V</p>";
  
  // Add webhook status without showing the actual URL
  if (webhook_url.length() > 0) {
    htmlMsg += "<p>Webhook alerts: Enabled</p>";
  } else {
    htmlMsg += "<p>Webhook alerts: Disabled</p>";
  }
  
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

// Send webhook alert for panic button press
bool sendWebhookAlert() {
  if (webhook_url.length() == 0) {
    Serial.println("Webhook not configured");
    return false;
  }
  
  HTTPClient http;
  http.begin(webhook_url);
  http.addHeader("Content-Type", "application/json");
  
  // Create JSON payload
  String payload = "{";
  payload += "\"event\":\"panic_alarm\",";
  payload += "\"status\":\"triggered\",";
  payload += "\"device_ip\":\"" + WiFi.localIP().toString() + "\",";
  payload += "\"battery\":\"" + String(batteryVoltage) + "\",";
  payload += "\"uptime\":\"" + String(millis() / 1000) + "\"";
  payload += "}";
  
  int httpResponseCode = http.POST(payload);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("HTTP Response code: " + String(httpResponseCode));
    Serial.println("Response: " + response);
    http.end();
    return true;
  } else {
    Serial.print("Error on sending webhook. Error code: ");
    Serial.println(httpResponseCode);
    http.end();
    return false;
  }
}

// Send low battery alert 
bool sendLowBatteryAlert() {
  bool notificationSent = false;
  
  // Try email if configured
  if (email_server.length() > 0 && email_recipient.length() > 0) {
    if (sendLowBatteryEmail()) {
      notificationSent = true;
    }
  }
  
  // Try webhook if configured
  if (webhook_url.length() > 0) {
    if (sendLowBatteryWebhook()) {
      notificationSent = true;
    }
  }
  
  return notificationSent;
}

// Send low battery alert email
bool sendLowBatteryEmail() {
  if (email_server.length() == 0 || email_recipient.length() == 0) {
    Serial.println("Email not configured");
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
  
  String htmlMsg = "<div style='color:orange;'><h1>LOW BATTERY WARNING</h1>"
                   "<p>Your panic alarm device is running low on battery.</p>"
                   "<p>Current battery voltage: " + String(batteryVoltage) + "V</p>"
                   "<p>Please replace or recharge the battery soon.</p>"
                   "<p>Device IP: " + WiFi.localIP().toString() + "</p>";
  
  // Add webhook status without showing the actual URL
  if (webhook_url.length() > 0) {
    htmlMsg += "<p>Webhook alerts: Enabled</p>";
  } else {
    htmlMsg += "<p>Webhook alerts: Disabled</p>";
  }
  
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

// Send low battery webhook alert
bool sendLowBatteryWebhook() {
  if (webhook_url.length() == 0) {
    Serial.println("Webhook not configured");
    return false;
  }
  
  HTTPClient http;
  http.begin(webhook_url);
  http.addHeader("Content-Type", "application/json");
  
  // Create JSON payload
  String payload = "{";
  payload += "\"event\":\"low_battery\",";
  payload += "\"status\":\"warning\",";
  payload += "\"device_ip\":\"" + WiFi.localIP().toString() + "\",";
  payload += "\"battery\":\"" + String(batteryVoltage) + "\",";
  payload += "\"uptime\":\"" + String(millis() / 1000) + "\"";
  payload += "}";
  
  int httpResponseCode = http.POST(payload);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("HTTP Response code: " + String(httpResponseCode));
    Serial.println("Response: " + response);
    http.end();
    return true;
  } else {
    Serial.print("Error on sending webhook. Error code: ");
    Serial.println(httpResponseCode);
    http.end();
    return false;
  }
}

// Load configuration from EEPROM
void loadConfig() {
  // Read WiFi SSID
  char buffer[100];  // Increased buffer size to handle longer webhook URLs
  for (int i = 0; i < 70; i++) {
    buffer[i] = EEPROM.read(SSID_ADDR + i);
  }
  wifi_ssid = String(buffer);
  
  // Read WiFi password
  memset(buffer, 0, 100);
  for (int i = 0; i < 70; i++) {
    buffer[i] = EEPROM.read(PASS_ADDR + i);
  }
  wifi_password = String(buffer);
  
  // Read email server
  memset(buffer, 0, 100);
  for (int i = 0; i < 70; i++) {
    buffer[i] = EEPROM.read(EMAIL_SERVER_ADDR + i);
  }
  email_server = String(buffer);
  
  // Read email port
  email_port = EEPROM.read(EMAIL_PORT_ADDR) + (EEPROM.read(EMAIL_PORT_ADDR + 1) << 8) + 
               (EEPROM.read(EMAIL_PORT_ADDR + 2) << 16) + (EEPROM.read(EMAIL_PORT_ADDR + 3) << 24);
  
  // Read email username
  memset(buffer, 0, 100);
  for (int i = 0; i < 70; i++) {
    buffer[i] = EEPROM.read(EMAIL_USER_ADDR + i);
  }
  email_username = String(buffer);
  
  // Read email password
  memset(buffer, 0, 100);
  for (int i = 0; i < 70; i++) {
    buffer[i] = EEPROM.read(EMAIL_PASS_ADDR + i);
  }
  email_password = String(buffer);
  
  // Read email recipient
  memset(buffer, 0, 100);
  for (int i = 0; i < 70; i++) {
    buffer[i] = EEPROM.read(EMAIL_RECIPIENT_ADDR + i);
  }
  email_recipient = String(buffer);
  
  // Read webhook URL
  memset(buffer, 0, 100);
  for (int i = 0; i < 100; i++) {  // Allow longer webhook URLs
    buffer[i] = EEPROM.read(WEBHOOK_URL_ADDR + i);
  }
  webhook_url = String(buffer);
  
  // Print loaded configuration
  Serial.println("Loaded configuration:");
  Serial.println("WiFi SSID: " + wifi_ssid);
  Serial.println("Email server: " + email_server);
  Serial.println("Email port: " + String(email_port));
  Serial.println("Email username: " + email_username);
  Serial.println("Email recipient: " + email_recipient);
  Serial.println("Webhook URL configured: " + (webhook_url.length() > 0 ? "Yes" : "No"));
}

// Save configuration to EEPROM
void saveConfig(String ssid, String password, String server, int port, 
                String username, String emailpass, String recipient, String webhook) {
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
  
  // Save webhook URL
  for (unsigned int i = 0; i < webhook.length(); i++) {
    EEPROM.write(WEBHOOK_URL_ADDR + i, webhook[i]);
  }
  EEPROM.write(WEBHOOK_URL_ADDR + webhook.length(), 0); // Null terminator
  
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
                "</head><body>"
                "<div class='container'>"
                "<h1>Panic Alarm Setup</h1>"
                "<form action='/setup' method='post'>"
                "<div class='section'>"
                "<h2>WiFi Settings</h2>"
                "<label for='ssid'>WiFi SSID:</label>"
                "<input type='text' id='ssid' name='ssid' required><br>"
                "<label for='password'>WiFi Password:</label>"
                "<input type='password' id='password' name='password'><br>"
                "</div>"
                "<div class='section'>"
                "<h2>Notification Settings</h2>"
                "<p class='note'>Configure at least one notification method (email or webhook)</p>"
                "<div class='subsection'>"
                "<h3>Email Settings</h3>"
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
                "<div class='subsection'>"
                "<h3>Webhook Settings</h3>"
                "<label for='webhook_url'>Webhook URL:</label>"
                "<input type='text' id='webhook_url' name='webhook_url' placeholder='https://example.com/webhook'><br>"
                "<p class='hint'>The device will send a JSON payload with event details to this URL</p>"
                "</div>"
                "</div>"
                "<input type='submit' value='Save Configuration' id='submit-btn'>"
                "</form>"
                "<script>"
                "document.querySelector('form').addEventListener('submit', function(e) {"
                "  var email = document.getElementById('email_recipient').value;"
                "  var webhook = document.getElementById('webhook_url').value;"
                "  if (!email && !webhook) {"
                "    e.preventDefault();"
                "    alert('You must configure at least one notification method (email or webhook)');"
                "  }"
                "});"
                "</script>"
                "</div></body></html>";
  
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
  String webhook_url = server.arg("webhook_url");
  
  // Validate that at least one notification method is configured
  if (email_recipient.length() == 0 && webhook_url.length() == 0) {
    String html = "<!DOCTYPE html><html><head>"
                  "<title>Setup Error</title>"
                  "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                  "<link rel='stylesheet' href='style.css'>"
                  "<meta http-equiv='refresh' content='5;url=/'>"
                  "</head><body>"
                  "<div class='container'>"
                  "<h1>Setup Error</h1>"
                  "<p>You must configure at least one notification method (email or webhook).</p>"
                  "<p>Redirecting back to setup page in 5 seconds...</p>"
                  "</div></body></html>";
    
    server.send(200, "text/html", html);
    return;
  }
  
  // Save configuration
  saveConfig(ssid, password, email_server, email_port, email_username, email_password, email_recipient, webhook_url);
  
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
String html = "<!DOCTYPE html><html><head>"
              "<title>Panic Alarm Control</title>"
              "<meta name='viewport' content='width=device-width, initial-scale=1'>"
              "<link rel='stylesheet' href='style.css'>"
              "</head><body>"
              "<div class='container'>"
              "<h1>Panic Alarm Control Panel</h1>"
              "<p>Device is operational and monitoring for panic button presses.</p>"
              "<div class='status'>"
              "<p><strong>WiFi SSID:</strong> " + wifi_ssid + "</p>"
              "<p><strong>IP Address:</strong> " + WiFi.localIP().toString() + "</p>";

if (email_recipient.length() > 0) {
  html += "<p><strong>Email Alerts:</strong> Enabled (" + email_recipient + ")</p>";
} else {
  html += "<p><strong>Email Alerts:</strong> Disabled</p>";
}

if (webhook_url.length() > 0) {
  html += "<p><strong>Webhook Alerts:</strong> Enabled</p>";
} else {
  html += "<p><strong>Webhook Alerts:</strong> Disabled</p>";
}

html += "<p><strong>Battery Voltage:</strong> " + String(batteryVoltage) + "V</p>"
        "</div>"
        "<div class='buttons'>"
        "<a href='/config' class='button'>Update Configuration</a>"
        "<a href='/test' class='button test'>Test Alarm</a>"
        "<a href='/reset' class='button reset'>Factory Reset</a>"
        "</div>"
        "</div></body></html>";

server.send(200, "text/html", html);
}

void handleConfigPage() {
  String html = "<!DOCTYPE html><html><head>"
                "<title>Update Configuration</title>"
                "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                "<link rel='stylesheet' href='style.css'>"
                "</head><body>"
                "<div class='container'>"
                "<h1>Update Configuration</h1>"
                "<form action='/update' method='post'>"
                "<div class='section'>"
                "<h2>WiFi Settings</h2>"
                "<label for='ssid'>WiFi SSID:</label>"
                "<input type='text' id='ssid' name='ssid' value='" + wifi_ssid + "' required><br>"
                "<label for='password'>WiFi Password:</label>"
                "<input type='password' id='password' name='password' value='" + wifi_password + "'><br>"
                "</div>"
                "<div class='section'>"
                "<h2>Notification Settings</h2>"
                "<p class='note'>Configure at least one notification method (email or webhook)</p>"
                "<div class='subsection'>"
                "<h3>Email Settings</h3>"
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
                "<div class='subsection'>"
                "<h3>Webhook Settings</h3>"
                "<label for='webhook_url'>Webhook URL:</label>"
                "<input type='text' id='webhook_url' name='webhook_url' value='" + webhook_url + "' placeholder='https://example.com/webhook'><br>"
                "<p class='hint'>The device will send a JSON payload with event details to this URL</p>"
                "</div>"
                "</div>"
                "<input type='submit' value='Update Configuration' id='submit-btn'>"
                "</form>"
                "<div class='back'><a href='/' class='button'>Back to Home</a></div>"
                "<script>"
                "document.querySelector('form').addEventListener('submit', function(e) {"
                "  var email = document.getElementById('email_recipient').value;"
                "  var webhook = document.getElementById('webhook_url').value;"
                "  if (!email && !webhook) {"
                "    e.preventDefault();"
                "    alert('You must configure at least one notification method (email or webhook)');"
                "  }"
                "});"
                "</script>"
                "</div></body></html>";
  
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
  String webhook_url = server.arg("webhook_url");

  // Validate that at least one notification method is configured
  if (email_recipient.length() == 0 && webhook_url.length() == 0) {
    String html = "<!DOCTYPE html><html><head>"
                  "<title>Update Error</title>"
                  "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                  "<link rel='stylesheet' href='style.css'>"
                  "<meta http-equiv='refresh' content='5;url=/config'>"
                  "</head><body>"
                  "<div class='container'>"
                  "<h1>Update Error</h1>"
                  "<p>You must configure at least one notification method (email or webhook).</p>"
                  "<p>Redirecting back to configuration page in 5 seconds...</p>"
                  "</div></body></html>";
    
    server.send(200, "text/html", html);
  return;
}
  
  // Save configuration
  saveConfig(ssid, password, email_server, email_port, email_username, email_password, email_recipient, webhook_url);
  
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

void handleTestAlert() {
  bool emailSuccess = false;
  bool webhookSuccess = false;
  String result = "";

  // Test email if configured
  if (email_recipient.length() > 0) {
    if (sendEmailAlert()) {
      emailSuccess = true;
      result += "Email alert sent successfully!<br>";
    } else {
      result += "Failed to send email alert. Please check your email settings.<br>";
    }
  }

  // Test webhook if configured
  if (webhook_url.length() > 0) {
    if (sendWebhookAlert()) {
      webhookSuccess = true;
      result += "Webhook alert sent successfully!<br>";
    } else {
      result += "Failed to send webhook alert. Please check your webhook URL.<br>";
    }
  }

  // If no notification method is configured
  if (email_recipient.length() == 0 && webhook_url.length() == 0) {
    result = "No notification method is configured. Please configure at least one.";
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
              "h3 {"
              "  margin-top: 15px;"
              "  margin-bottom: 10px;"
              "}"
              ".section {"
              "  background-color: #fff;"
              "  border-radius: 5px;"
              "  padding: 15px;"
              "  margin-bottom: 20px;"
              "  box-shadow: 0 2px 5px rgba(0,0,0,0.1);"
              "}"
              ".subsection {"
              "  border-left: 3px solid #ddd;"
              "  padding-left: 15px;"
              "  margin-bottom: 15px;"
              "}"
              "label {"
              "  display: block;"
              "  margin-top: 10px;"
              "  font-weight: bold;"
              "}"
              "input[type='text'], input[type='password'], input[type='email'], input[type='number'] {"
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
              "  color: #e67e22;"
              "  font-weight: bold;"
              "  margin: 10px 0;"
              "}"
              ".hint {"
              "  color: #7f8c8d;"
              "  font-size: 0.9em;"
              "  margin-top: 5px;"
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