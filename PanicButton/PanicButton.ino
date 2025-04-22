#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <ESP_Mail_Client.h>
#include <HTTPClient.h>

// Constants
#define EEPROM_SIZE 710
#define CONFIG_FLAG_ADDR 0
#define SSID_ADDR 10
#define PASS_ADDR 80
#define EMAIL_SERVER_ADDR 150
#define EMAIL_PORT_ADDR 220
#define EMAIL_USER_ADDR 224
#define EMAIL_PASS_ADDR 294
#define EMAIL_RECIPIENT_ADDR 364
#define LOCATION_ADDR 434
#define WEBHOOK_URL_ADDR 504
#define WEBHOOK_ENABLED_ADDR 674
#define EMAIL_ENABLED_ADDR 675
#define BUTTON_PIN 4    // FireBeetle suitable GPIO pin
#define LED_PIN 15      // FireBeetle's onboard LED
#define BATTERY_PIN 0   // A0 on FireBeetle ESP32-C6
#define DNS_PORT 53
#define WEBSERVER_PORT 80
#define CONFIG_FLAG 0xAA
#define uS_TO_S_FACTOR 1000000  // Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP  60       // Time ESP32 will sleep (in seconds)
#define BATT_SAMPLES 10         // Battery read averaging
#define BATT_MAX_VOLTAGE 4.2    // Maximum battery voltage (fully charged)
#define BATT_MIN_VOLTAGE 3.2    // Minimum battery voltage (depleted)
#define WIFI_CHECK_INTERVAL 30000 // Check WiFi signal every 30 seconds

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
String webhook_url = "";
bool webhook_enabled = false;
bool email_enabled = true; // Flag to enable email (default to true for backward compatibility)
bool alarmTriggered = false;
bool lowBatteryAlertSent = false; // Flag to track if low battery alert was sent
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
int buttonState = HIGH;
int lastButtonState = HIGH;
float batteryVoltage = 0.0;
int batteryPercentage = 0;
unsigned long lastBatteryCheck = 0;
String hardwarePlatform = ""; // Store hardware platform info
int rssi = 0;                 // WiFi signal strength in dBm
String signalQuality = "";    // Signal quality rating
unsigned long lastWifiCheck = 0; // Last time WiFi signal was checked
String firmwareVersion = "1.2.0"; // Firmware version

// Forward declarations
void loadConfig();
void saveConfig(String ssid, String password, String server, int port, 
                String username, String emailpass, String recipient, String location,
                String webhook, bool webhook_en, bool email_en);
void blinkLED(int times, int delayms);
void goToDeepSleep();
void startConfigMode();
bool connectToWiFi();
void checkButton();
void triggerAlarm();
bool sendWebhook();
bool sendEmailAlert();
bool sendLowBatteryAlert();
bool sendLowBatteryWebhook();
bool sendLowBatteryEmail();
float getBatteryVoltage();
bool isLowBattery();
void checkBatteryStatus();
void checkWiFiSignal();
String getSignalQuality(int rssi);
String generateUniqueSSID();
int calculateBatteryPercentage(float voltage);

// Web handlers
void handleRoot();
void handleSetup();
void handleNormalRoot();
void handleConfigPage();
void handleUpdate();
void handleTestEmail();
void handleTestWebhook();
void handleReset();
void handleCss();

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

  // Determine hardware platform
  #if defined(CONFIG_IDF_TARGET_ESP32C6)
    hardwarePlatform = "FireBeetle 2 ESP32-C6";
  #elif defined(CONFIG_IDF_TARGET_ESP32C3)
    hardwarePlatform = "FireBeetle 2 ESP32-C3";
  #elif defined(CONFIG_IDF_TARGET_ESP32E)
    hardwarePlatform = "FireBeetle 2 ESP32-E";
  #else
    hardwarePlatform = "ESP32 Dev Kit";
  #endif

  Serial.print("Hardware Platform: ");
  Serial.println(hardwarePlatform);

  // Generate unique SSID based on MAC
  configSSID = generateUniqueSSID();
  Serial.print("Generated unique SSID: ");
  Serial.println(configSSID);

  // Read initial battery voltage using onboard divider (×2)
  int mV = analogReadMilliVolts(BATTERY_PIN);
  batteryVoltage = (mV * 2) / 1000.0;
  batteryPercentage = calculateBatteryPercentage(batteryVoltage);
  Serial.print("Initial battery voltage: ");
  Serial.print(batteryVoltage);
  Serial.print(" V (");
  Serial.print(batteryPercentage);
  Serial.println("%)");

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
      checkWiFiSignal(); // Get initial signal strength
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

// Calculate battery percentage from voltage
int calculateBatteryPercentage(float voltage) {
  if (voltage >= BATT_MAX_VOLTAGE) {
    return 100;
  } else if (voltage <= BATT_MIN_VOLTAGE) {
    return 0;
  } else {
    // Linear approximation between min and max voltage
    float percentage = ((voltage - BATT_MIN_VOLTAGE) / (BATT_MAX_VOLTAGE - BATT_MIN_VOLTAGE)) * 100.0;
    return (int)percentage;
  }
}

// Evaluate WiFi signal strength and return quality rating
String getSignalQuality(int rssi) {
  if (rssi > -55) {
    return "Excellent";
  } else if (rssi > -70) {
    return "Good";
  } else if (rssi > -80) {
    return "Okay";
  } else if (rssi > -90) {
    return "Poor";
  } else {
    return "Very Poor";
  }
}

// Check WiFi signal strength
void checkWiFiSignal() {
  if (WiFi.status() == WL_CONNECTED) {
    rssi = WiFi.RSSI();
    signalQuality = getSignalQuality(rssi);
    Serial.print("WiFi signal strength: ");
    Serial.print(rssi);
    Serial.print(" dBm (");
    Serial.print(signalQuality);
    Serial.println(")");
  } else {
    rssi = -100; // Set to minimum value if not connected
    signalQuality = "Disconnected";
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
    
    // Check battery status periodically
    checkBatteryStatus();
    
    // Check WiFi signal periodically
    if (millis() - lastWifiCheck > WIFI_CHECK_INTERVAL) {
      lastWifiCheck = millis();
      checkWiFiSignal();
    }
    
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
    batteryPercentage = calculateBatteryPercentage(batteryVoltage);
    Serial.print("Battery voltage: ");
    Serial.print(batteryVoltage);
    Serial.print(" V (");
    Serial.print(batteryPercentage);
    Serial.println("%)");
    
    if (isLowBattery() && !lowBatteryAlertSent) {
      blinkLED(5, 50); // Fast blink to indicate low battery
      // Send low battery notification once per boot
      if (WiFi.status() == WL_CONNECTED) {
        if (sendLowBatteryAlert()) {
          lowBatteryAlertSent = true; // Set flag to prevent repeated alerts
          Serial.println("Low battery alert sent. No more alerts will be sent until reboot.");
        }
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
  return batteryPercentage < 50;
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
    
    // Get initial signal strength
    rssi = WiFi.RSSI();
    signalQuality = getSignalQuality(rssi);
    Serial.print("WiFi signal strength: ");
    Serial.print(rssi);
    Serial.print(" dBm (");
    Serial.print(signalQuality);
    Serial.println(")");
    
    // Enable WiFi power saving for battery life
    WiFi.setSleep(true);
    
    // Set up web server routes for normal operation
    server.on("/", HTTP_GET, handleNormalRoot);
    server.on("/config", HTTP_GET, handleConfigPage);
    server.on("/update", HTTP_POST, handleUpdate);
    server.on("/test", HTTP_GET, handleTestEmail);
    server.on("/test-webhook", HTTP_GET, handleTestWebhook);
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
  
  // Add debugging information
  Serial.println("Sending webhook to URL: " + webhook_url);
  
  // Begin HTTP connection - verify the URL has http:// or https:// prefix
  if (webhook_url.startsWith("http://") || webhook_url.startsWith("https://")) {
    http.begin(webhook_url);
  } else {
    String correctedUrl = "https://" + webhook_url;
    Serial.println("URL didn't have protocol, using: " + correctedUrl);
    http.begin(correctedUrl);
  }
  
  // Set headers
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "ESP32PanicAlarm/1.0");
  
  // Escape special characters in strings
  String deviceId = configSSID;
  deviceId.replace("\"", "\\\"");
  
  String location = device_location.length() > 0 ? device_location : "Not specified";
  location.replace("\"", "\\\"");
  location.replace("\\", "\\\\");
  
  String ipAddr = WiFi.localIP().toString();
  String macAddr = WiFi.macAddress();
  String timeStr = String(millis() / 1000);
  
  // Create message body with escaped newlines
  String msgBody = String("Device ID: ") + deviceId + "\\n" +
                   "Hardware: " + hardwarePlatform + "\\n" +
                   "Location: " + location + "\\n" +
                   "IP Address: " + ipAddr + "\\n" +
                   "MAC Address: " + macAddr + "\\n" +
                   "WiFi Signal: " + String(rssi) + " dBm (" + signalQuality + ")\\n" +
                   "Battery: " + String(batteryPercentage) + "% (" + String(batteryVoltage) + "V)\\n" +
                   "Time: " + timeStr + " seconds since boot";
  
  String titleText = "PANIC ALARM TRIGGERED";
  String jsonPayload;
  
  // Determine service type from URL
  if (webhook_url.indexOf("discord.com") > 0) {
    // Discord webhook format
    jsonPayload = "{\"content\":\"⚠️ **" + titleText + "** ⚠️\\n" + msgBody + 
                  "\",\"embeds\":[{\"title\":\"Panic Alarm Alert\",\"color\":16711680,\"description\":\"A panic button has been activated at " + 
                  location + "\"}]}";
  }
  else if (webhook_url.indexOf("chat.googleapis.com") > 0) {
    // Google Chat webhook format
    jsonPayload = "{\"text\":\"⚠️ *" + titleText + "* ⚠️\\n" + msgBody + "\"}";
  }
  else if (webhook_url.indexOf("hooks.slack.com") > 0) {
    // Slack webhook format
    jsonPayload = "{\"text\":\"⚠️ *" + titleText + "* ⚠️\",\"attachments\":[{\"color\":\"#FF0000\",\"fields\":[" +
                  "{\"title\":\"Device ID\",\"value\":\"" + deviceId + "\",\"short\":true}," +
                  "{\"title\":\"Hardware\",\"value\":\"" + hardwarePlatform + "\",\"short\":true}," +
                  "{\"title\":\"Location\",\"value\":\"" + location + "\",\"short\":true}," +
                  "{\"title\":\"IP Address\",\"value\":\"" + ipAddr + "\",\"short\":true}," +
                  "{\"title\":\"MAC Address\",\"value\":\"" + macAddr + "\",\"short\":true}," +
                  "{\"title\":\"WiFi Signal\",\"value\":\"" + String(rssi) + " dBm (" + signalQuality + ")\",\"short\":true}," +
                  "{\"title\":\"Battery\",\"value\":\"" + String(batteryPercentage) + "% (" + String(batteryVoltage) + "V)\",\"short\":true}," +
                  "{\"title\":\"Time\",\"value\":\"" + timeStr + " seconds since boot\",\"short\":false}" +
                  "]}]}";
  }
  else if (webhook_url.indexOf("webhook.office.com") > 0) {
    // Microsoft Teams webhook format
    jsonPayload = "{";
    jsonPayload += "\"@type\":\"MessageCard\",";
    jsonPayload += "\"@context\":\"http://schema.org/extensions\",";
    jsonPayload += "\"themeColor\":\"FF0000\",";
    jsonPayload += "\"summary\":\"" + titleText + "\",";
    jsonPayload += "\"sections\":[{";
    jsonPayload += "\"activityTitle\":\"⚠️ " + titleText + "\",";
    jsonPayload += "\"facts\":[";
    jsonPayload += "{\"name\":\"Device ID\",\"value\":\"" + deviceId + "\"},";
    jsonPayload += "{\"name\":\"Hardware\",\"value\":\"" + hardwarePlatform + "\"},";
    jsonPayload += "{\"name\":\"Location\",\"value\":\"" + location + "\"},";
    jsonPayload += "{\"name\":\"IP Address\",\"value\":\"" + ipAddr + "\"},";
    jsonPayload += "{\"name\":\"MAC Address\",\"value\":\"" + macAddr + "\"},";
    jsonPayload += "{\"name\":\"WiFi Signal\",\"value\":\"" + String(rssi) + " dBm (" + signalQuality + ")\"},";
    jsonPayload += "{\"name\":\"Battery\",\"value\":\"" + String(batteryPercentage) + "% (" + String(batteryVoltage) + "V)\"},";
    jsonPayload += "{\"name\":\"Time\",\"value\":\"" + timeStr + " seconds since boot\"}";
    jsonPayload += "],\"markdown\":true}]}";
  }
  else {
    // Generic webhook format for other services
    jsonPayload = "{\"event\":\"" + titleText + "\",";
    jsonPayload += "\"device_id\":\"" + deviceId + "\",";
    jsonPayload += "\"hardware\":\"" + hardwarePlatform + "\",";
    jsonPayload += "\"location\":\"" + location + "\",";
    jsonPayload += "\"ip_address\":\"" + ipAddr + "\",";
    jsonPayload += "\"mac_address\":\"" + macAddr + "\",";
    jsonPayload += "\"wifi_signal\":" + String(rssi) + ",";
    jsonPayload += "\"signal_quality\":\"" + signalQuality + "\",";
    jsonPayload += "\"battery_percentage\":" + String(batteryPercentage) + ",";
    jsonPayload += "\"battery_voltage\":" + String(batteryVoltage) + ",";
    jsonPayload += "\"triggered_at\":" + timeStr + "}";
  }
  
  // Log the payload for debugging
  Serial.println("Sending payload: " + jsonPayload);
  
  // Send the request
  int httpCode = http.POST(jsonPayload);
  
  if (httpCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpCode);
    
    // Get response for debugging
    String payload = http.getString();
    Serial.println("Response: " + payload);
    
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
                   "<p><strong>Device ID:</strong> " + configSSID + "</p>"
                   "<p><strong>Hardware:</strong> " + hardwarePlatform + "</p>"
                   + locationInfo +
                   "<p><strong>Device IP:</strong> " + WiFi.localIP().toString() + "</p>"
                   "<p><strong>MAC Address:</strong> " + WiFi.macAddress() + "</p>"
                   "<p><strong>WiFi Signal:</strong> " + String(rssi) + " dBm (" + signalQuality + ")</p>"
                   "<p><strong>" + webhookStatus + "</strong></p>"
                   "<p><strong>Battery:</strong> " + String(batteryPercentage) + "% (" + String(batteryVoltage) + "V)</p>"
                   "<p><strong>Time:</strong> " + String(millis() / 1000) + " seconds since device boot</p>"
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
  
  // Ensure URL has protocol
  if (webhook_url.startsWith("http://") || webhook_url.startsWith("https://")) {
    http.begin(webhook_url);
  } else {
    String correctedUrl = "https://" + webhook_url;
    http.begin(correctedUrl);
  }
  
  http.addHeader("Content-Type", "application/json");
  
  // Construct JSON payload
  String locationInfo = device_location.length() > 0 ? 
                        "\"location\": \"" + device_location + "\"," : 
                        "\"location\": \"Not specified\",";
  
  String jsonPayload = "{"
                      "\"event\": \"LOW_BATTERY_WARNING\","
                      "\"device_id\": \"" + configSSID + "\","
                      "\"hardware\": \"" + hardwarePlatform + "\","
                      + locationInfo +
                      "\"ip_address\": \"" + WiFi.localIP().toString() + "\","
                      "\"mac_address\": \"" + WiFi.macAddress() + "\","
                      "\"wifi_signal\": " + String(rssi) + ","
                      "\"signal_quality\": \"" + signalQuality + "\","
                      "\"battery_voltage\": " + String(batteryVoltage) + ","
                      "\"battery_percentage\": " + String(batteryPercentage) + ","
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
                   "<p><strong>Device ID:</strong> " + configSSID + "</p>"
                   "<p><strong>Hardware:</strong> " + hardwarePlatform + "</p>"
                   + locationInfo +
                   "<p><strong>Device IP:</strong> " + WiFi.localIP().toString() + "</p>"
                   "<p><strong>MAC Address:</strong> " + WiFi.macAddress() + "</p>"
                   "<p><strong>WiFi Signal:</strong> " + String(rssi) + " dBm (" + signalQuality + ")</p>"
                   "<p><strong>Battery level:</strong> " + String(batteryPercentage) + "% (" + String(batteryVoltage) + "V)</p>"
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
  char buffer[170];
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
  
  // Read webhook URL
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
  Serial.println("Hardware platform: " + hardwarePlatform);
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
                "<div class='header-container'>"
                "<div class='logo-title'>"
                "<svg width='40' height='40' viewBox='0 0 200 200' xmlns='http://www.w3.org/2000/svg' class='logo'>"
                "  <circle cx='100' cy='100' r='95' fill='#2f2f2f'/>"
                "  <circle cx='100' cy='100' r='75' fill='#444'/>"
                "  <circle cx='100' cy='100' r='55' fill='#d32f2f'/>"
                "  <ellipse cx='85' cy='80' rx='20' ry='12' fill='white' opacity='0.3'/>"
                "</svg>"
                "<div>"
                "<h1>Panic Alarm Setup</h1>"
                "<p class='version'>Version " + firmwareVersion + "</p>"
                "</div>"
                "</div>"
                "</div>"
                "<p><strong>Device ID: </strong>" + configSSID + "</p>"
                "<p><strong>Hardware: </strong>" + hardwarePlatform + "</p>"
                "<form action='/setup' method='post' onsubmit='return validateForm()'>"

                "<div class='section'>"
                "<h2>Device Settings</h2>"
                "<label for='location'>Location Description:</label>"
                "<input type='text' id='location' name='location' placeholder='e.g. Living Room, Front Door, etc.'><br>"
                "</div>"

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

// Web handlers for normal operation mode
void handleNormalRoot() {
  // Determine network signal icon based on signal quality
  String wifiSvgPaths = "";
  String wifiColor1 = "#e0e0e0"; // default light gray for weak/no signal
  String wifiColor2 = "#e0e0e0";
  String wifiColor3 = "#e0e0e0";
  
  if (WiFi.status() == WL_CONNECTED) {
    if (rssi > -55) { // Excellent
      wifiColor1 = "#4CAF50"; // Green
      wifiColor2 = "#4CAF50";
      wifiColor3 = "#4CAF50";
    } else if (rssi > -70) { // Good
      wifiColor1 = "#e0e0e0";
      wifiColor2 = "#4CAF50";
      wifiColor3 = "#4CAF50";
    } else if (rssi > -80) { // Okay
      wifiColor1 = "#e0e0e0";
      wifiColor2 = "#e0e0e0";
      wifiColor3 = "#4CAF50";
    } else { // Poor
      wifiColor1 = "#e0e0e0";
      wifiColor2 = "#e0e0e0";
      wifiColor3 = "#FF9800"; // Orange
    }
  }
  
  // Create WiFi SVG
  String wifiSvg = "<svg class=\"wifi-icon\" xmlns=\"http://www.w3.org/2000/svg\" width=\"24\" height=\"24\" viewBox=\"0 0 24 24\">"
                   "<path d=\"M1,9 L3,11 C7.97,6.03 16.03,6.03 21,11 L23,9 C16.93,2.93 7.08,2.93 1,9 Z\" fill=\"" + wifiColor1 + "\"/>"
                   "<path d=\"M5,13 L7,15 C9.76,12.24 14.24,12.24 17,15 L19,13 C15.14,9.14 8.87,9.14 5,13 Z\" fill=\"" + wifiColor2 + "\"/>"
                   "<path d=\"M9,17 L12,20 L15,17 C13.35,15.34 10.66,15.34 9,17 Z\" fill=\"" + wifiColor3 + "\"/>"
                   "</svg>";
  
  // Determine battery color based on percentage
  String batteryColor = "#4CAF50"; // Green for good battery
  if (batteryPercentage < 25) {
    batteryColor = "#f44336"; // Red for very low
  } else if (batteryPercentage < 50) {
    batteryColor = "#ff9800"; // Orange/yellow for low
  }
  
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
                "<div class='header-container'>"
                "<div class='logo-title'>"
                "<svg width='40' height='40' viewBox='0 0 200 200' xmlns='http://www.w3.org/2000/svg' class='logo'>"
                "  <circle cx='100' cy='100' r='95' fill='#2f2f2f'/>"
                "  <circle cx='100' cy='100' r='75' fill='#444'/>"
                "  <circle cx='100' cy='100' r='55' fill='#d32f2f'/>"
                "  <ellipse cx='85' cy='80' rx='20' ry='12' fill='white' opacity='0.3'/>"
                "</svg>"
                "<div>"
                "<h1>Panic Alarm Control</h1>"
                "<p class='version'>Version " + firmwareVersion + "</p>"
                "</div>"
                "</div>"
                "<div class='status-indicators'>"
                "<div class='tooltip'>"
                + wifiSvg +
                "<span class=\"tooltip-text\">Signal: " + signalQuality + " (" + String(rssi) + " dBm)</span>"
                "</div>"
                "<div class='tooltip'>"
                "<div class='battery-icon'>"
                "<div class='battery-level' style='width:" + String(batteryPercentage) + "%; background-color:" + batteryColor + ";'></div>"
                "</div>"
                "<span class=\"tooltip-text\">Battery: " + String(batteryPercentage) + "% (" + String(batteryVoltage) + "V)</span>"
                "</div>"
                "</div>"
                "</div>"
                "<p>Device is operational and monitoring for panic button presses.</p>"
                "<div class='status'>"
                "<p><strong>Device ID:</strong> " + configSSID + "</p>"
                "<p><strong>Hardware:</strong> " + hardwarePlatform + "</p>"
                "<p><strong>Location:</strong> " + (device_location.length() > 0 ? device_location : "Not specified") + "</p>"
                "<p><strong>WiFi SSID:</strong> " + wifi_ssid + "</p>"
                "<p><strong>IP Address:</strong> " + WiFi.localIP().toString() + "</p>"
                "<p><strong>MAC Address:</strong> " + WiFi.macAddress() + "</p>"
                "<p><strong>Notification:</strong> " + notificationStatus + "</p>"
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
                "<div class='header-container'>"
                "<div class='logo-title'>"
                "<svg width='40' height='40' viewBox='0 0 200 200' xmlns='http://www.w3.org/2000/svg' class='logo'>"
                "  <circle cx='100' cy='100' r='95' fill='#2f2f2f'/>"
                "  <circle cx='100' cy='100' r='75' fill='#444'/>"
                "  <circle cx='100' cy='100' r='55' fill='#d32f2f'/>"
                "  <ellipse cx='85' cy='80' rx='20' ry='12' fill='white' opacity='0.3'/>"
                "</svg>"
                "<div>"
                "<h1>Update Configuration</h1>"
                "<p class='version'>Version " + firmwareVersion + "</p>"
                "</div>"
                "</div>"
                "</div>"
                "<p><strong>Device ID: </strong>" + configSSID + "</p>"
                "<p><strong>Hardware: </strong>" + hardwarePlatform + "</p>"
                "<form action='/update' method='post' onsubmit='return validateForm()'>"

                "<div class='section'>"
                "<h2>Device Settings</h2>"
                "<label for='location'>Location Description:</label>"
                "<input type='text' id='location' name='location' value='" + device_location + "' placeholder='e.g. Living Room, Front Door, etc.'><br>"
                "</div>"

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
                "<div class='header-container'>"
                "<div class='logo-title'>"
                "<svg width='40' height='40' viewBox='0 0 200 200' xmlns='http://www.w3.org/2000/svg' class='logo'>"
                "  <circle cx='100' cy='100' r='95' fill='#2f2f2f'/>"
                "  <circle cx='100' cy='100' r='75' fill='#444'/>"
                "  <circle cx='100' cy='100' r='55' fill='#d32f2f'/>"
                "  <ellipse cx='85' cy='80' rx='20' ry='12' fill='white' opacity='0.3'/>"
                "</svg>"
                "<div>"
                "<h1>Factory Reset</h1>"
                "<p class='version'>Version " + firmwareVersion + "</p>"
                "</div>"
                "</div>"
                "</div>"
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
               "  box-sizing: border-box;"
               "}"
               "h1, h2, h3 {"
               "  color: #333;"
               "  margin: 0 0 5px 0;"
               "}"
               "/* Section elements */"
               ".section, .toggle-section, .status {"
               "  background-color: #fff;"
               "  border-radius: 5px;"
               "  padding: 15px;"
               "  margin-bottom: 20px;"
               "  box-shadow: 0 2px 5px rgba(0,0,0,0.1);"
               "  width: 100%;"
               "  box-sizing: border-box;"
               "}"
               "/* Header layout */"
               ".header-container {"
               "  display: flex;"
               "  flex-wrap: wrap;"
               "  align-items: flex-start;"
               "  margin-bottom: 20px;"
               "}"
               ".logo-title {"
               "  display: flex;"
               "  align-items: center;"
               "  flex: 1;"
               "  min-width: 250px;"
               "  margin-bottom: 10px;"
               "}"
               ".logo {"
               "  margin-right: 15px;"
               "  display: flex;"
               "  align-items: center;"
               "}"
               ".version {"
               "  color: #666;"
               "  font-size: 14px;"
               "  margin-top: 5px;"
               "}"
               ".status-indicators {"
               "  display: flex;"
               "  align-items: center;"
               "  gap: 15px;"
               "  margin-top: 10px;"
               "  margin-left: auto;"
               "}"
               "/* Status indicators */"
               ".wifi-icon {"
               "  display: inline-flex;"
               "  align-items: center;"
               "}"
               ".battery-icon {"
               "  position: relative;"
               "  width: 22px;"
               "  height: 12px;"
               "  border: 2px solid #4CAF50;"
               "  border-radius: 2px;"
               "  padding: 1px;"
               "  box-sizing: content-box;"
               "  display: inline-block;"
               "  vertical-align: middle;"
               "}"
               ".battery-icon:after {"
               "  content: \"\";"
               "  position: absolute;"
               "  right: -4px;"
               "  top: 50%;"
               "  transform: translateY(-50%);"
               "  width: 2px;"
               "  height: 6px;"
               "  background-color: #4CAF50;"
               "  border-radius: 0 2px 2px 0;"
               "}"
               ".battery-level {"
               "  height: 100%;"
               "  border-radius: 1px;"
               "  transition: width 0.5s ease-in-out;"
               "}"
               "/* Tooltips */"
               ".tooltip {"
               "  position: relative;"
               "  display: inline-block;"
               "}"
               ".tooltip .tooltip-text {"
               "  visibility: hidden;"
               "  background-color: rgba(0, 0, 0, 0.7);"
               "  color: #fff;"
               "  text-align: center;"
               "  padding: 5px 10px;"
               "  border-radius: 4px;"
               "  white-space: nowrap;"
               "  position: absolute;"
               "  z-index: 1;"
               "  bottom: 125%;"
               "  left: 50%;"
               "  transform: translateX(-50%);"
               "  opacity: 0;"
               "  transition: opacity 0.3s;"
               "  pointer-events: none;"
               "}"
               ".tooltip:hover .tooltip-text {"
               "  visibility: visible;"
               "  opacity: 1;"
               "}"
               "/* Buttons */"
               ".buttons {"
               "  margin-top: 20px;"
               "}"
               ".button, input[type='submit'] {"
               "  background-color: #4CAF50;"
               "  color: white;"
               "  padding: 10px 15px;"
               "  border: none;"
               "  border-radius: 4px;"
               "  cursor: pointer;"
               "  font-size: 16px;"
               "  margin-top: 10px;"
               "  display: block;"
               "  width: 100%;"
               "  text-align: center;"
               "  text-decoration: none;"
               "  box-sizing: border-box;"
               "}"
               ".test {"
               "  background-color: #2196F3;"
               "}"
               ".reset {"
               "  background-color: #f44336;"
               "}"
               "input[type='submit']:disabled {"
               "  background-color: #cccccc;"
               "  cursor: not-allowed;"
               "}"
               "/* Form elements */"
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
               "/* Toggle switches */"
               ".switch {"
               "  position: relative;"
               "  display: inline-block;"
               "  width: 50px;"
               "  height: 24px;"
               "  margin-right: 10px;"
               "  vertical-align: middle;"
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
               "  margin-bottom: 15px;"
               "  padding: 5px 0;"
               "}"
               ".toggle-label {"
               "  font-weight: bold;"
               "  margin-left: 5px;"
               "}"
               ".small-note {"
               "  font-size: 0.85em;"
               "  color: #777;"
               "  margin-top: 5px;"
               "}"
               "/* Responsive adjustments */"
               "@media (max-width: 480px) {"
               "  .container {"
               "    padding: 10px;"
               "  }"
               "  .header-container {"
               "    flex-direction: column;"
               "  }"
               "  .status-indicators {"
               "    margin-left: 0;"
               "    margin-top: 15px;"
               "  }"
               "}";
               
  server.send(200, "text/css", css);
}