#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <ESP_Mail_Client.h>

// Constants
#define EEPROM_SIZE 512
#define CONFIG_FLAG_ADDR 0
#define SSID_ADDR 10
#define PASS_ADDR 80
#define EMAIL_SERVER_ADDR 150
#define EMAIL_PORT_ADDR 220
#define EMAIL_USER_ADDR 224
#define EMAIL_PASS_ADDR 294
#define EMAIL_RECIPIENT_ADDR 364
#define BUTTON_PIN 13
#define LED_PIN 2
#define DNS_PORT 53
#define WEBSERVER_PORT 80
#define CONFIG_FLAG 0xAA

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
bool alarmTriggered = false;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
int buttonState = HIGH;
int lastButtonState = HIGH;

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
  
  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  
  // Check if device is configured
  if (EEPROM.read(CONFIG_FLAG_ADDR) != CONFIG_FLAG) {
    Serial.println("No configuration found. Starting setup mode...");
    startConfigMode();
  } else {
    // Load saved configuration
    loadConfig();
    
    // Try to connect to WiFi
    if (connectToWiFi()) {
      Serial.println("Connected to WiFi. Starting normal operation.");
      blinkLED(3, 200); // Success indicator
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
  }
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
    
    // Set up web server routes for normal operation
    server.on("/", HTTP_GET, handleNormalRoot);
    server.on("/config", HTTP_GET, handleConfigPage);
    server.on("/update", HTTP_POST, handleUpdate);
    server.on("/test", HTTP_GET, handleTestEmail);
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
  server.handleClient(); // Continue handling web requests
  
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
  
  if (sendEmailAlert()) {
    Serial.println("Alarm notification sent successfully");
    // Blink to indicate success
    for (int i = 0; i < 5; i++) {
      digitalWrite(LED_PIN, LOW);
      delay(100);
      digitalWrite(LED_PIN, HIGH);
      delay(100);
    }
  } else {
    Serial.println("Failed to send alarm notification");
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

// Send email alert
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
  
  String htmlMsg = "<div style='color:red;'><h1>PANIC ALARM TRIGGERED</h1><p>The panic button has been activated.</p>"
                   "<p>Time: " + String(millis() / 1000) + " seconds since device boot</p>"
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

// Load configuration from EEPROM
void loadConfig() {
  // Read WiFi SSID
  char buffer[70];
  for (int i = 0; i < 70; i++) {
    buffer[i] = EEPROM.read(SSID_ADDR + i);
  }
  wifi_ssid = String(buffer);
  
  // Read WiFi password
  memset(buffer, 0, 70);
  for (int i = 0; i < 70; i++) {
    buffer[i] = EEPROM.read(PASS_ADDR + i);
  }
  wifi_password = String(buffer);
  
  // Read email server
  memset(buffer, 0, 70);
  for (int i = 0; i < 70; i++) {
    buffer[i] = EEPROM.read(EMAIL_SERVER_ADDR + i);
  }
  email_server = String(buffer);
  
  // Read email port
  email_port = EEPROM.read(EMAIL_PORT_ADDR) + (EEPROM.read(EMAIL_PORT_ADDR + 1) << 8) + 
               (EEPROM.read(EMAIL_PORT_ADDR + 2) << 16) + (EEPROM.read(EMAIL_PORT_ADDR + 3) << 24);
  
  // Read email username
  memset(buffer, 0, 70);
  for (int i = 0; i < 70; i++) {
    buffer[i] = EEPROM.read(EMAIL_USER_ADDR + i);
  }
  email_username = String(buffer);
  
  // Read email password
  memset(buffer, 0, 70);
  for (int i = 0; i < 70; i++) {
    buffer[i] = EEPROM.read(EMAIL_PASS_ADDR + i);
  }
  email_password = String(buffer);
  
  // Read email recipient
  memset(buffer, 0, 70);
  for (int i = 0; i < 70; i++) {
    buffer[i] = EEPROM.read(EMAIL_RECIPIENT_ADDR + i);
  }
  email_recipient = String(buffer);
  
  // Print loaded configuration
  Serial.println("Loaded configuration:");
  Serial.println("WiFi SSID: " + wifi_ssid);
  Serial.println("Email server: " + email_server);
  Serial.println("Email port: " + String(email_port));
  Serial.println("Email username: " + email_username);
  Serial.println("Email recipient: " + email_recipient);
}

// Save configuration to EEPROM
void saveConfig(String ssid, String password, String server, int port, 
                String username, String emailpass, String recipient) {
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
                "<h2>Email Settings</h2>"
                "<label for='email_server'>SMTP Server:</label>"
                "<input type='text' id='email_server' name='email_server' required><br>"
                "<label for='email_port'>SMTP Port:</label>"
                "<input type='number' id='email_port' name='email_port' value='587' required><br>"
                "<label for='email_username'>Email Username:</label>"
                "<input type='text' id='email_username' name='email_username' required><br>"
                "<label for='email_password'>Email Password:</label>"
                "<input type='password' id='email_password' name='email_password' required><br>"
                "<label for='email_recipient'>Recipient Email:</label>"
                "<input type='email' id='email_recipient' name='email_recipient' required><br>"
                "</div>"
                "<input type='submit' value='Save Configuration'>"
                "</form>"
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
  
  // Save configuration
  saveConfig(ssid, password, email_server, email_port, email_username, email_password, email_recipient);
  
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
                "<p><strong>IP Address:</strong> " + WiFi.localIP().toString() + "</p>"
                "<p><strong>Email Recipient:</strong> " + email_recipient + "</p>"
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
                "<h2>Email Settings</h2>"
                "<label for='email_server'>SMTP Server:</label>"
                "<input type='text' id='email_server' name='email_server' value='" + email_server + "' required><br>"
                "<label for='email_port'>SMTP Port:</label>"
                "<input type='number' id='email_port' name='email_port' value='" + String(email_port) + "' required><br>"
                "<label for='email_username'>Email Username:</label>"
                "<input type='text' id='email_username' name='email_username' value='" + email_username + "' required><br>"
                "<label for='email_password'>Email Password:</label>"
                "<input type='password' id='email_password' name='email_password' value='" + email_password + "' required><br>"
                "<label for='email_recipient'>Recipient Email:</label>"
                "<input type='email' id='email_recipient' name='email_recipient' value='" + email_recipient + "' required><br>"
                "</div>"
                "<input type='submit' value='Update Configuration'>"
                "</form>"
                "<div class='back'><a href='/' class='button'>Back to Home</a></div>"
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
  
  // Save configuration
  saveConfig(ssid, password, email_server, email_port, email_username, email_password, email_recipient);
  
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
  if (sendEmailAlert()) {
    result = "Test alert sent successfully!";
  } else {
    result = "Failed to send test alert. Please check your email settings.";
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
               "h1, h2 {"
               "  color: #333;"
               "}"
               ".section {"
               "  background-color: #fff;"
               "  border-radius: 5px;"
               "  padding: 15px;"
               "  margin-bottom: 20px;"
               "  box-shadow: 0 2px 5px rgba(0,0,0,0.1);"
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