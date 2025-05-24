#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <ESP_Mail_Client.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>

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

#define ENABLE_BATTERY_MONITORING 1  // Set to 0 to disable battery monitoring
#define ENABLE_DEEP_SLEEP 1          // Set to 0 to disable deep sleep functionality
//#define BUTTON_PIN 0                 // ESP32 Dev Module onboard button
//#define BUTTON_PIN 13                // ESP32 Dev Module suitable GPIO pin
#define BUTTON_PIN 4                 // FireBeetle suitable GPIO pin (prod)
//#define BUTTON_PIN 5                 // FireBeetle suitable GPIO pin (proto 2)
//#define LED_PIN 2                    // ESP32 Dev Module onboard LED
//#define LED_PIN 15                   // FireBeetle onboard LED
#define LED_PIN 5                    // FireBeetle suitable GPIO pin (prod)
//#define LED_PIN 23                   // FireBeetle suitable GPIO pin (proto 2)
#define BATTERY_PIN 0                // A0 on FireBeetle ESP32-C6
#define DNS_PORT 53
#define WEBSERVER_PORT 80
#define CONFIG_FLAG 0xAA
#define uS_TO_S_FACTOR 1000000       // Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP  60            // Time ESP32 will sleep (in seconds)
#define BATT_SAMPLES 10              // Battery read averaging
#define BATT_MAX_VOLTAGE 4.2         // Maximum battery voltage (fully charged)
#define BATT_MIN_VOLTAGE 3.2         // Minimum battery voltage (depleted)
#define WIFI_CHECK_INTERVAL 30000    // Check WiFi signal every 30 seconds

// OTA Update configuration
#define OTA_CHECK_INTERVAL 86400000    // Check for updates once per day (in ms)
#define OTA_SERVER_URL "https://your-update-server.com/api" // <<<--- Needs real URL
#define OTA_UPDATE_KEY "your-device-secret-key" // <<<--- Needs real shared secret

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
String firmwareVersion = "1.3.7"; // Firmware version - <<<--- MAKE SURE THIS IS UPDATED WITH EACH RELEASE

// OTA update variables
unsigned long lastOtaCheck = 0;
bool updateInProgress = false;
String newFirmwareVersion = ""; // Stores version found by check, even if update fails

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
void sendSetupCompleteNotification(); // Added forward declaration
float getBatteryVoltage();
bool isLowBattery();
void checkBatteryStatus();
void checkWiFiSignal();
String getSignalQuality(int rssi);
String generateUniqueSSID();
int calculateBatteryPercentage(float voltage);

// OTA Function declarations
bool checkForUpdates();
String generateAuthToken();
bool downloadAndUpdate(String firmwareUrl, String expectedChecksum);
String urlEncode(String str);
int compareVersions(String v1, String v2); // Although defined, not used in client logic from plan

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
void handleCheckUpdate(); // Added for OTA

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

  #if ENABLE_BATTERY_MONITORING
  pinMode(BATTERY_PIN, INPUT);
  analogReadResolution(12); // 12-bit ADC resolution
  #endif

  // Determine hardware platform
  #if defined(CONFIG_IDF_TARGET_ESP32C6)
    hardwarePlatform = "FireBeetle 2 ESP32-C6";
  #elif defined(CONFIG_IDF_TARGET_ESP32C3)
    hardwarePlatform = "FireBeetle 2 ESP32-C3";
  #elif defined(CONFIG_IDF_TARGET_ESP32E)
    hardwarePlatform = "FireBeetle 2 ESP32-E";
  #else
    hardwarePlatform = "ESP32 Dev Module";
  #endif

  Serial.print("Hardware Platform: ");
  Serial.println(hardwarePlatform);

  // Generate unique SSID based on MAC
  configSSID = generateUniqueSSID();
  Serial.print("Generated unique SSID: ");
  Serial.println(configSSID);

  #if ENABLE_BATTERY_MONITORING
  // Read initial battery voltage using onboard divider (×2)
  int mV = analogReadMilliVolts(BATTERY_PIN);
  batteryVoltage = (mV * 2) / 1000.0;
  batteryPercentage = calculateBatteryPercentage(batteryVoltage);
  Serial.print("Initial battery voltage: ");
  Serial.print(batteryVoltage);
  Serial.print(" V (");
  Serial.print(batteryPercentage);
  Serial.println("%)");
  #else
  // Set default values when battery monitoring is disabled
  batteryVoltage = 4.0;
  batteryPercentage = 100;
  #endif

  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);

  #if ENABLE_DEEP_SLEEP
  // Configure deep sleep wakeup
  uint64_t bitmask = 1ULL << BUTTON_PIN;
  esp_sleep_enable_ext1_wakeup(bitmask, ESP_EXT1_WAKEUP_ANY_LOW);
  #endif

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
    
    #if ENABLE_BATTERY_MONITORING
    // Check battery status periodically
    checkBatteryStatus();
    #endif
    
    // Check WiFi signal periodically
    if (millis() - lastWifiCheck > WIFI_CHECK_INTERVAL) {
      lastWifiCheck = millis();
      checkWiFiSignal();
    }
    
    // Check for OTA updates periodically
    if (millis() - lastOtaCheck > OTA_CHECK_INTERVAL && !updateInProgress && WiFi.status() == WL_CONNECTED) {
      lastOtaCheck = millis();
      Serial.println("Checking for firmware updates...");
      checkForUpdates();
    }

    server.handleClient(); // Continue handling web requests
    
    // Add short delay to prevent watchdog reset
    delay(10);
  }
}

// Check battery status periodically
void checkBatteryStatus() {
  #if ENABLE_BATTERY_MONITORING
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
  #endif
}

// Get battery voltage
float getBatteryVoltage() {
  #if ENABLE_BATTERY_MONITORING
  long total_mV = 0;
  for (int i = 0; i < BATT_SAMPLES; i++) {
    total_mV += analogReadMilliVolts(BATTERY_PIN);
    delay(5);
  }
  float avg_mV = total_mV / BATT_SAMPLES;
  return (avg_mV * 2) / 1000.0;  // Compensate for onboard voltage divider
  #else
  return 4.0; // Return a default value when disabled
  #endif
}

// Check if battery is low
bool isLowBattery() {
  #if ENABLE_BATTERY_MONITORING
  return batteryPercentage < 50;
  #else
  return false; // Never report low battery when monitoring is disabled
  #endif
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
    server.on("/check-update", HTTP_GET, handleCheckUpdate); // Added for OTA
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
                   #if ENABLE_BATTERY_MONITORING
                   "Battery: " + String(batteryPercentage) + "% (" + String(batteryVoltage) + "V)\\n" +
                   #else
                   "Battery: Not installed\\n" +
                   #endif
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
                  #if ENABLE_BATTERY_MONITORING
                  "{\"title\":\"Battery\",\"value\":\"" + String(batteryPercentage) + "% (" + String(batteryVoltage) + "V)\",\"short\":true}," +
                  #else
                  "{\"title\":\"Battery\",\"value\":\"Not installed\",\"short\":true}," +
                  #endif
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
    #if ENABLE_BATTERY_MONITORING
    jsonPayload += "{\"name\":\"Battery\",\"value\":\"" + String(batteryPercentage) + "% (" + String(batteryVoltage) + "V)\"},";
    #else
    jsonPayload += "{\"name\":\"Battery\",\"value\":\"Not installed\"},";
    #endif
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
    #if ENABLE_BATTERY_MONITORING
    jsonPayload += "\"battery_percentage\":" + String(batteryPercentage) + ",";
    jsonPayload += "\"battery_voltage\":" + String(batteryVoltage) + ",";
    #else
    jsonPayload += "\"battery_percentage\":null,";
    jsonPayload += "\"battery_voltage\":null,";
    #endif
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
  // Default sender email
  String senderEmail = email_username.c_str();

  // SendGrid uses "apikey" as username and the API key as password.
  // The actual sender email is set in message.sender.email (senderEmail variable).
  if (email_server == "smtp.sendgrid.net") {
    session.login.email = "apikey"; // For SendGrid, the login username is literally "apikey"
    Serial.println("Using SendGrid: login email set to 'apikey'. Sender email will be original username.");
  } else {
    session.login.email = email_username.c_str(); // For other SMTP servers, use the provided username
  }
  session.login.password = email_password.c_str(); // Password is used as is (for SendGrid, this is the API Key)
  
  SMTP_Message message;
  message.sender.name = "Panic Alarm";
  message.sender.email = senderEmail.c_str(); // The 'From' address visible to the recipient
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
  #if ENABLE_BATTERY_MONITORING
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
  #else
  return false; // Battery monitoring disabled
  #endif
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
                      #if ENABLE_BATTERY_MONITORING
                      "\"battery_percentage\": " + String(batteryPercentage) + ","
                      #else
                      "\"battery_percentage\": null,"
                      #endif
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

// Sends notifications (email and/or webhook) after the device setup is completed.
// This function is called from handleSetup() after new settings are saved and before the device restarts.
// It attempts to connect to WiFi using the new credentials first.
void sendSetupCompleteNotification() {
  Serial.println("Attempting to send setup complete notifications...");
  bool notificationSent = false;

  // Attempt to connect to WiFi with new credentials before sending notifications.
  // This is important because the WiFi credentials might have just been updated.
  // Global wifi_ssid and wifi_password (updated by saveConfig) are used by connectToWiFi().
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected for setup notification. Attempting to connect with new settings...");
    // Temporarily disconnect if in AP mode or connected to an old/different network
    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
        WiFi.softAPdisconnect(true); // Disconnect from AP mode
        WiFi.disconnect(true);       // Disconnect from any STA connection
        delay(100);
    } else if (WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect(true);       // Disconnect from current STA connection
        delay(100);
    }
    
    if (!connectToWiFi()) { 
      Serial.println("Failed to connect to WiFi with new settings. Cannot send setup complete notifications.");
      return; // Exit if cannot connect, as notifications cannot be sent.
    }
    Serial.println("Successfully connected to WiFi with new settings for setup notification.");
  }

  // Try to send "Setup Complete" email if enabled
  if (email_enabled && email_server.length() > 0 && email_recipient.length() > 0) {
    Serial.println("Sending setup complete email notification...");
    ESP_Mail_Session session;
    session.server.host_name = email_server.c_str();
    session.server.port = email_port;
    String senderEmail = email_username.c_str(); // This will be the 'From' address in the email

    // Handle SendGrid specific login
    if (email_server == "smtp.sendgrid.net") {
      session.login.email = "apikey"; // SendGrid uses "apikey" as the login username
      Serial.println("Using SendGrid for setup complete email: login email set to 'apikey'.");
    } else {
      session.login.email = email_username.c_str(); // Standard SMTP login
    }
    session.login.password = email_password.c_str(); // SMTP password (or API Key for SendGrid)

    SMTP_Message message;
    message.sender.name = "Panic Alarm";
    message.sender.email = senderEmail.c_str();
    message.subject = "Panic Alarm - Setup Complete";
    message.addRecipient("User", email_recipient.c_str());
    String htmlMsg = "<div style='color:green;'><h1>Setup Complete!</h1>"
                     "<p>Your Panic Alarm device has been successfully configured.</p>"
                     "<p><strong>Device ID:</strong> " + configSSID + "</p>"
                     "<p><strong>Hardware:</strong> " + hardwarePlatform + "</p>"
                     "<p><strong>Location:</strong> " + (device_location.length() > 0 ? device_location : "Not specified") + "</p>"
                     "<p><strong>WiFi SSID:</strong> " + wifi_ssid + "</p>"
                     "<p><strong>Device IP:</strong> " + WiFi.localIP().toString() + "</p>"
                     "<p>It will now restart and operate with these settings.</p></div>";
    message.html.content = htmlMsg.c_str();

    if (!smtp.connect(&session)) {
      Serial.println("Failed to connect to SMTP server for setup complete email.");
    } else {
      if (!MailClient.sendMail(&smtp, &message)) {
        Serial.println("Failed to send setup complete email: " + smtp.errorReason());
      } else {
        Serial.println("Setup complete email sent successfully.");
        notificationSent = true;
      }
    }
  }

  // Try to send webhook if enabled
  if (webhook_enabled && webhook_url.length() > 0) {
    Serial.println("Sending setup complete webhook notification...");
    HTTPClient http;
    String targetUrl = webhook_url;
    if (!targetUrl.startsWith("http://") && !targetUrl.startsWith("https://")) {
        targetUrl = "https://" + targetUrl;
    }
    http.begin(targetUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("User-Agent", "ESP32PanicAlarm/1.0");

    String location = device_location.length() > 0 ? device_location : "Not specified";
    location.replace("\"", "\\\"");
    location.replace("\\", "\\\\");
    String deviceId = configSSID;
    deviceId.replace("\"", "\\\"");

    String titleText = "PANIC ALARM - SETUP COMPLETE";
    String msgBody = "Device ID: " + deviceId + "\\n" +
                     "Hardware: " + hardwarePlatform + "\\n" +
                     "Location: " + location + "\\n" +
                     "WiFi SSID: " + wifi_ssid + "\\n" +
                     "IP Address: " + WiFi.localIP().toString();
    String jsonPayload;

    // Determine service type from URL
    if (webhook_url.indexOf("discord.com") > 0) {
        jsonPayload = "{\"content\":\"✅ **" + titleText + "** ✅\\n" + msgBody + "\",\"embeds\":[{\"title\":\"Setup Successful\",\"color\":3066993,\"description\":\"The Panic Alarm device has been configured successfully.\"}]}";
    } else if (webhook_url.indexOf("chat.googleapis.com") > 0) {
        jsonPayload = "{\"text\":\"✅ *" + titleText + "* ✅\\n" + msgBody + "\"}";
    } else if (webhook_url.indexOf("hooks.slack.com") > 0) {
        jsonPayload = "{\"text\":\"✅ *" + titleText + "* ✅\",\"attachments\":[{\"color\":\"#2ECC71\",\"fields\":[" +
                      "{\"title\":\"Device ID\",\"value\":\"" + deviceId + "\",\"short\":true}," +
                      "{\"title\":\"Hardware\",\"value\":\"" + hardwarePlatform + "\",\"short\":true}," +
                      "{\"title\":\"Location\",\"value\":\"" + location + "\",\"short\":true}," +
                      "{\"title\":\"WiFi SSID\",\"value\":\"" + wifi_ssid + "\",\"short\":true}," +
                      "{\"title\":\"IP Address\",\"value\":\"" + WiFi.localIP().toString() + "\",\"short\":true}" +
                      "]}]}";
    } else if (webhook_url.indexOf("webhook.office.com") > 0) {
        jsonPayload = "{";
        jsonPayload += "\"@type\":\"MessageCard\",";
        jsonPayload += "\"@context\":\"http://schema.org/extensions\",";
        jsonPayload += "\"themeColor\":\"2ECC71\","; // Green
        jsonPayload += "\"summary\":\"" + titleText + "\",";
        jsonPayload += "\"sections\":[{";
        jsonPayload += "\"activityTitle\":\"✅ " + titleText + "\",";
        jsonPayload += "\"facts\":[";
        jsonPayload += "{\"name\":\"Device ID\",\"value\":\"" + deviceId + "\"},";
        jsonPayload += "{\"name\":\"Hardware\",\"value\":\"" + hardwarePlatform + "\"},";
        jsonPayload += "{\"name\":\"Location\",\"value\":\"" + location + "\"},";
        jsonPayload += "{\"name\":\"WiFi SSID\",\"value\":\"" + wifi_ssid + "\"},";
        jsonPayload += "{\"name\":\"IP Address\",\"value\":\"" + WiFi.localIP().toString() + "\"}";
        jsonPayload += "],\"markdown\":true}]}";
    } else { // Generic
        jsonPayload = "{\"event\":\"" + titleText + "\",";
        jsonPayload += "\"device_id\":\"" + deviceId + "\",";
        jsonPayload += "\"hardware\":\"" + hardwarePlatform + "\",";
        jsonPayload += "\"location\":\"" + location + "\",";
        jsonPayload += "\"wifi_ssid\":\"" + wifi_ssid + "\",";
        jsonPayload += "\"ip_address\":\"" + WiFi.localIP().toString() + "\",";
        jsonPayload += "\"message\":\"Device configured successfully.\"}";
    }
    
    Serial.println("Sending setup complete webhook payload: " + jsonPayload);
    int httpCode = http.POST(jsonPayload);
    if (httpCode > 0) {
      Serial.printf("Setup complete webhook sent. HTTP Code: %d\n", httpCode);
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED || httpCode == HTTP_CODE_ACCEPTED || httpCode == HTTP_CODE_NO_CONTENT) {
        notificationSent = true;
      }
    } else {
      Serial.printf("Failed to send setup complete webhook. Error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }

  if (notificationSent) {
    Serial.println("Setup complete notification(s) sent.");
  } else {
    Serial.println("No setup complete notifications were sent (either disabled or failed).");
  }
}

// Check for firmware updates
bool checkForUpdates() {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  newFirmwareVersion = ""; // Clear previous check result

  // Prepare URL with device information
  String url = String(OTA_SERVER_URL) + "/firmware";
  url += "?device_id=" + urlEncode(configSSID); // Use urlEncode
  url += "&hardware=" + urlEncode(hardwarePlatform);
  url += "&version=" + urlEncode(firmwareVersion);
  url += "&mac=" + WiFi.macAddress();

  Serial.println("Checking for updates at: " + url);

  http.begin(url);
  // Add a validation header using device MAC and shared secret
  http.addHeader("X-Device-Auth", generateAuthToken());

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    DynamicJsonDocument doc(1024); // Adjust size if needed
    DeserializationError error = deserializeJson(doc, response);

    if (!error) {
      if (doc.containsKey("update_available") && doc["update_available"].as<bool>()) {
        if (doc.containsKey("firmware_url") && doc.containsKey("checksum")) {
          String firmwareUrl = doc["firmware_url"].as<String>();
          String checksum = doc["checksum"].as<String>();

          if (doc.containsKey("firmware_version")) {
            newFirmwareVersion = doc["firmware_version"].as<String>();
          } else {
            newFirmwareVersion = "unknown"; // Mark as unknown if not provided
          }

          Serial.printf("Found new firmware: %s, Version: %s\n",
                     firmwareUrl.c_str(), newFirmwareVersion.c_str());

          http.end(); // End the connection before starting download

          // Download and apply the update
          return downloadAndUpdate(firmwareUrl, checksum);
        } else {
           Serial.println("Update available but missing firmware_url or checksum in response.");
        }
      } else {
        Serial.println("No update available.");
      }
    } else {
      Serial.print("Error parsing update response JSON: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.printf("Update check failed, HTTP code: %d\n", httpCode);
    String responseBody = http.getString(); // Get response body for debugging
    Serial.println("Response body: " + responseBody);
  }

  http.end();
  return false;
}

// Simple auth token generation (MAC + shared key)
String generateAuthToken() {
  String mac = WiFi.macAddress();
  mac.replace(":", ""); // Remove colons

  // Simple one-way token - in production you'd want a better algorithm
  String token = mac + OTA_UPDATE_KEY;

  // Create a simple hash - this is not cryptographically secure but better than plaintext
  uint32_t hashValue = 0;
  for (unsigned int i = 0; i < token.length(); i++) {
    hashValue = ((hashValue << 5) + hashValue) + token.charAt(i); // djb2 hash variation
  }

  return String(hashValue, HEX); // Return as Hex String
}

// Download and apply firmware update
bool downloadAndUpdate(String firmwareUrl, String expectedChecksum) {
  if (updateInProgress) {
      Serial.println("Update already in progress. Aborting new attempt.");
      return false;
  }
  updateInProgress = true;

  HTTPClient http;
  http.begin(firmwareUrl);

  // Add auth header here too if your binary server requires it
  // http.addHeader("X-Device-Auth", generateAuthToken());

  Serial.println("Downloading firmware from: " + firmwareUrl);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    // Get total content length
    int contentLength = http.getSize();

    if (contentLength <= 0) {
      Serial.println("Invalid content length received from server.");
      http.end();
      updateInProgress = false;
      return false;
    }

    Serial.printf("Firmware size: %d bytes\n", contentLength);

    // Hash calculation for simple validation (Additive Checksum)
    uint32_t calculatedChecksum = 0;

    // Use Update library to write firmware
    if (Update.begin(contentLength)) {
      Update.setMD5(expectedChecksum.c_str()); // Use MD5 if checksum is MD5, otherwise use custom checksum below

      // Read and update in chunks
      uint8_t buffer[1024];
      WiFiClient *stream = http.getStreamPtr();

      size_t written = 0;
      int lastProgress = -1; // To avoid printing 0% multiple times

      while (http.connected() && written < (size_t)contentLength) {
        // Check how much data is available
        size_t available = stream->available();

        if (available) {
          // Read up to buffer size or available bytes
          size_t readSize = min(available, sizeof(buffer));
          size_t bytesRead = stream->readBytes(buffer, readSize);

          if (bytesRead > 0) {
             // -- Simple Additive Checksum Calculation (Alternative) --
             // If your server provides a simple additive checksum instead of MD5, use this block.
             // Otherwise, rely on Update.setMD5() and Update.end() for validation.
             // for (size_t i = 0; i < bytesRead; i++) {
             //   calculatedChecksum += buffer[i];
             // }
             // -- End Simple Checksum --

             // Write buffer to flash
             size_t written_now = Update.write(buffer, bytesRead);

             if (written_now != bytesRead) {
               Serial.printf("Flash write error: wrote %d/%d bytes\n", written_now, bytesRead);
               Update.abort();
               http.end();
               updateInProgress = false;
               return false;
             }

             written += bytesRead;

             // Show progress
             int progress = (written * 100) / contentLength;
             if (progress != lastProgress) { // Only print if percentage changes
                 Serial.printf("Update progress: %d%%\n", progress);
                 lastProgress = progress;
             }
          } else {
              Serial.println("Stream read 0 bytes.");
          }
        } else {
            // Wait a bit if no data is available yet
            delay(10);
        }
      }

      // Check if download completed fully
      if (written != (size_t)contentLength) {
          Serial.printf("Download incomplete: Received %d/%d bytes\n", written, contentLength);
          Update.abort();
          http.end();
          updateInProgress = false;
          return false;
      }

      Serial.println("\nUpdate download complete.");

      // --- Checksum Verification ---
      // If using the simple additive checksum:
      // uint32_t expectedChecksumInt = strtoul(expectedChecksum.c_str(), NULL, 16);
      // if (calculatedChecksum != expectedChecksumInt) {
      //   Serial.printf("Checksum mismatch! Expected: %u (0x%X), Got: %u (0x%X)\n",
      //               expectedChecksumInt, expectedChecksumInt, calculatedChecksum, calculatedChecksum);
      //   Update.abort();
      //   http.end();
      //   updateInProgress = false;
      //   return false;
      // } else {
      //    Serial.println("Checksum verified successfully.");
      // }

      // --- End Checksum Verification ---

      // Finalize the update (true = success, flash will be committed)
      // If using Update.setMD5(), Update.end() will perform the check internally.
      if (Update.end(true)) {
        Serial.printf("Update Success: %u bytes written\n", written);
        http.end();

        // Restart device to apply update
        Serial.println("Rebooting to apply update...");
        delay(2000); // Give time for serial message to send
        ESP.restart();
        // Code execution stops here after restart
        return true; // Technically won't be reached
      } else {
        Serial.println("Update failed: Error #" + String(Update.getError()));
        Update.printError(Serial); // Print detailed error
      }
    } else {
      Serial.printf("Not enough space for update: %d required, Error #%d\n", contentLength, Update.getError());
       Update.printError(Serial); // Print detailed error
    }
  } else {
    Serial.printf("Firmware download failed, HTTP code: %d\n", httpCode);
    String responseBody = http.getString(); // Get response body for debugging
    Serial.println("Response body: " + responseBody);
  }

  http.end();
  updateInProgress = false;
  return false;
}


// URL encode a string for HTTP requests
String urlEncode(String str) {
  String encodedString = "";
  char c;
  char code0;
  char code1;

  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ') {
      encodedString += '+';
    } else if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') { // RFC3986 Unreserved chars
      encodedString += c;
    } else {
      code1 = (c & 0xf);
      code0 = (c >> 4) & 0xf;

      encodedString += '%';
      encodedString += (code0 < 10 ? code0 + '0' : code0 - 10 + 'A');
      encodedString += (code1 < 10 ? code1 + '0' : code1 - 10 + 'A');
    }
  }
  return encodedString;
}

// Compare two semantic version strings (returns >0 if v1>v2, <0 if v1<v2, 0 if equal)
// NOTE: This function is provided but not used in the client-side OTA check logic from the plan.
// The server is assumed to handle the version comparison.
int compareVersions(String v1, String v2) {
  int v1_parts[3] = {0, 0, 0};
  int v2_parts[3] = {0, 0, 0};

  sscanf(v1.c_str(), "%d.%d.%d", &v1_parts[0], &v1_parts[1], &v1_parts[2]);
  sscanf(v2.c_str(), "%d.%d.%d", &v2_parts[0], &v2_parts[1], &v2_parts[2]);

  // Compare major version
  if (v1_parts[0] != v2_parts[0]) {
    return v1_parts[0] - v2_parts[0];
  }

  // Compare minor version
  if (v1_parts[1] != v2_parts[1]) {
    return v1_parts[1] - v2_parts[1];
  }

  // Compare patch version
  return v1_parts[2] - v2_parts[2];
}

// --- End OTA Functions ---


// Load configuration from EEPROM
void loadConfig() {
  // Read WiFi SSID
  char buffer[170]; // Increased buffer size for potentially longer strings
  memset(buffer, 0, 170); // Clear buffer before reading
  for (int i = 0; i < 70; i++) { // Read up to 70 bytes
    byte val = EEPROM.read(SSID_ADDR + i);
    if (val == 0) break; // Stop at null terminator
    buffer[i] = (char)val;
  }
  wifi_ssid = String(buffer);
  
  // Read WiFi password
  memset(buffer, 0, 170);
  for (int i = 0; i < 70; i++) {
     byte val = EEPROM.read(PASS_ADDR + i);
     if (val == 0) break;
     buffer[i] = (char)val;
  }
  wifi_password = String(buffer);
  
  // Read email server
  memset(buffer, 0, 170);
  for (int i = 0; i < 70; i++) {
     byte val = EEPROM.read(EMAIL_SERVER_ADDR + i);
     if (val == 0) break;
     buffer[i] = (char)val;
  }
  email_server = String(buffer);
  
  // Read email port
  EEPROM.get(EMAIL_PORT_ADDR, email_port);
  
  // Read email username
  memset(buffer, 0, 170);
  for (int i = 0; i < 70; i++) {
     byte val = EEPROM.read(EMAIL_USER_ADDR + i);
     if (val == 0) break;
     buffer[i] = (char)val;
  }
  email_username = String(buffer);
  
  // Read email password
  memset(buffer, 0, 170);
  for (int i = 0; i < 70; i++) {
     byte val = EEPROM.read(EMAIL_PASS_ADDR + i);
     if (val == 0) break;
     buffer[i] = (char)val;
  }
  email_password = String(buffer);
  
  // Read email recipient
  memset(buffer, 0, 170);
  for (int i = 0; i < 70; i++) {
     byte val = EEPROM.read(EMAIL_RECIPIENT_ADDR + i);
     if (val == 0) break;
     buffer[i] = (char)val;
  }
  email_recipient = String(buffer);
  
  // Read location
  memset(buffer, 0, 170);
  for (int i = 0; i < 70; i++) {
     byte val = EEPROM.read(LOCATION_ADDR + i);
     if (val == 0) break;
     buffer[i] = (char)val;
  }
  device_location = String(buffer);
  
  // Read webhook URL
  memset(buffer, 0, 170);
  for (int i = 0; i < 170; i++) { // Read up to 170 bytes
     byte val = EEPROM.read(WEBHOOK_URL_ADDR + i);
     if (val == 0) break;
     buffer[i] = (char)val;
  }
  webhook_url = String(buffer);
  
  // Read webhook enabled flag
  webhook_enabled = EEPROM.read(WEBHOOK_ENABLED_ADDR) == 1;
  
  // Read email enabled flag (default to true if not set or invalid value)
  byte emailFlag = EEPROM.read(EMAIL_ENABLED_ADDR);
  email_enabled = (emailFlag == 1); // Only enable if explicitly set to 1
  
  // Print loaded configuration
  Serial.println("Loaded configuration:");
  Serial.println("Hardware platform: " + hardwarePlatform);
  Serial.println("Firmware Version: " + firmwareVersion); // Added Firmware Version
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

  // Clear EEPROM area before writing new data (optional, but good practice)
  // for (int i = SSID_ADDR; i < EMAIL_ENABLED_ADDR + 1; i++) {
  //   EEPROM.write(i, 0);
  // }

  // Save WiFi SSID
  for (unsigned int i = 0; i < 70; i++) {
    if (i < ssid.length()) {
      EEPROM.write(SSID_ADDR + i, ssid[i]);
    } else {
      EEPROM.write(SSID_ADDR + i, 0); // Null terminate or fill rest with 0
      if (i == ssid.length()) break; // Exit after writing terminator
    }
  }

  // Save WiFi password
  for (unsigned int i = 0; i < 70; i++) {
     if (i < password.length()) {
        EEPROM.write(PASS_ADDR + i, password[i]);
     } else {
        EEPROM.write(PASS_ADDR + i, 0);
        if (i == password.length()) break;
     }
  }

  // Save email server
  for (unsigned int i = 0; i < 70; i++) {
     if (i < server.length()) {
        EEPROM.write(EMAIL_SERVER_ADDR + i, server[i]);
     } else {
        EEPROM.write(EMAIL_SERVER_ADDR + i, 0);
        if (i == server.length()) break;
     }
  }

  // Save email port
  EEPROM.put(EMAIL_PORT_ADDR, port);

  // Save email username (limit length)
  for (unsigned int i = 0; i < 70; i++) {
     if (i < username.length()) {
        EEPROM.write(EMAIL_USER_ADDR + i, username[i]);
     } else {
        EEPROM.write(EMAIL_USER_ADDR + i, 0);
        if (i == username.length()) break;
     }
  }

  // Save email password
  for (unsigned int i = 0; i < 70; i++) {
     if (i < emailpass.length()) {
        EEPROM.write(EMAIL_PASS_ADDR + i, emailpass[i]);
     } else {
        EEPROM.write(EMAIL_PASS_ADDR + i, 0);
        if (i == emailpass.length()) break;
     }
  }

  // Save email recipient
  for (unsigned int i = 0; i < 70; i++) {
     if (i < recipient.length()) {
        EEPROM.write(EMAIL_RECIPIENT_ADDR + i, recipient[i]);
     } else {
        EEPROM.write(EMAIL_RECIPIENT_ADDR + i, 0);
        if (i == recipient.length()) break;
     }
  }

  // Save location
  for (unsigned int i = 0; i < 70; i++) {
     if (i < location.length()) {
        EEPROM.write(LOCATION_ADDR + i, location[i]);
     } else {
        EEPROM.write(LOCATION_ADDR + i, 0);
        if (i == location.length()) break;
     }
  }

  // Save webhook URL
  for (unsigned int i = 0; i < 170; i++) {
     if (i < webhook.length()) {
        EEPROM.write(WEBHOOK_URL_ADDR + i, webhook[i]);
     } else {
        EEPROM.write(WEBHOOK_URL_ADDR + i, 0);
        if (i == webhook.length()) break;
     }
  }
  
  // Save webhook enabled flag
  EEPROM.write(WEBHOOK_ENABLED_ADDR, webhook_en ? 1 : 0);
  
  // Save email enabled flag
  EEPROM.write(EMAIL_ENABLED_ADDR, email_en ? 1 : 0);
  
  // Set configuration flag
  EEPROM.write(CONFIG_FLAG_ADDR, CONFIG_FLAG);
  
  // Commit changes to EEPROM
  if (EEPROM.commit()) {
      Serial.println("Configuration saved successfully.");
  } else {
      Serial.println("EEPROM commit failed!");
  }
}

void blinkLED(int times, int delayms) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(delayms);
    digitalWrite(LED_PIN, LOW);
    if (i < times - 1) delay(delayms); // Prevent delay after last blink
  }
}

// Enter deep sleep mode to save battery
void goToDeepSleep() {
  #if ENABLE_DEEP_SLEEP
  Serial.println("Going to deep sleep...");
  delay(100);
  esp_deep_sleep_start();
  #else
  Serial.println("Deep sleep is disabled. Continuing normal operation.");
  #endif
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
                "  const validationMsg = document.getElementById('validation-msg');"
                "  if (!emailEnabled && !webhookEnabled) {"
                "    submitBtn.disabled = true;"
                "    validationMsg.style.display = 'block';"
                "    return false; // Prevent form submission"
                "  } else {"
                "    submitBtn.disabled = false;"
                "    validationMsg.style.display = 'none';"
                "    return true; // Allow form submission"
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
                "</div>"
                "</div>"
                "</div>"
                "<p><strong>Device ID: </strong>" + configSSID + "</p>"
                "<p><strong>Hardware: </strong>" + hardwarePlatform + "</p>"
                "<form action='/setup' method='post' onsubmit='return validateForm()'>"

                "<div class='section'>"
                "<h2>Device Settings</h2>"
                "<label for='location'>Location Description:</label>"
                "<input type='text' id='location' name='location' placeholder='e.g. Living Room, Front Door, etc.' maxlength='69'><br>"
                "</div>"

                "<div class='section'>"
                "<h2>WiFi Settings</h2>"
                "<label for='ssid'>WiFi SSID:</label>"
                "<input type='text' id='ssid' name='ssid' required maxlength='69'><br>"
                "<label for='password'>WiFi Password:</label>"
                "<input type='password' id='password' name='password' maxlength='69'><br>"
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
                "<span class='toggle-label'>Enable</span>"
                "</div>"
                "<div id='email-fields'>"
                "<label for='email_server'>SMTP Server:</label>"
                "<input type='text' id='email_server' name='email_server' maxlength='69'><br>"
                "<label for='email_port'>SMTP Port:</label>"
                "<input type='number' id='email_port' name='email_port' value='587'><br>"
                "<label for='email_username'>Email Username:</label>"
                "<input type='text' id='email_username' name='email_username' maxlength='69'><br>"
                "<label for='email_password'>Email Password:</label>"
                "<input type='password' id='email_password' name='email_password' maxlength='69'><br>"
                "<label for='email_recipient'>Recipient Email:</label>"
                "<input type='email' id='email_recipient' name='email_recipient' maxlength='69'><br>"
                "</div>"
                "</div>"
                
                "<div class='toggle-section'>"
                "<h3>Webhook Notifications</h3>"
                "<div class='toggle-container'>"
                "<label class='switch'>"
                "<input type='checkbox' id='webhook_enabled' name='webhook_enabled' onchange='toggleWebhookFields()'>"
                "<span class='slider'></span>"
                "</label>"
                "<span class='toggle-label'>Enable</span>"
                "</div>"
                "<div id='webhook-fields' style='display:none;'>"
                "<label for='webhook_url'>Webhook URL:</label>"
                "<input type='url' id='webhook_url' name='webhook_url' placeholder='https://example.com/webhook' maxlength='169'><br>"
                "<p class='info'>The device will send a JSON payload to this URL when the alarm is triggered.</p>"
                "</div>"
                "</div>"
                "</div>"
                
                "<input type='submit' id='submit-btn' value='Save Configuration'>"
                "</form>"
                "</div>"
                "<script>"
                "// Initialize toggle states and validation on page load"
                "document.addEventListener('DOMContentLoaded', function() {"
                "  toggleEmailFields();"
                "  toggleWebhookFields();"
                "  validateForm(); // Initial validation check"
                "});"
                "</script>"
                "</body></html>";
  
  server.send(200, "text/html", html);
}

// Handle saving the initial setup configuration
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
                "<p>Your configuration has been saved. The device will now restart and attempt to connect to your WiFi network.</p>"
                "<p>If the connection is successful, you can access the device control panel at the IP address assigned by your router.</p>"
                "<p>Restarting in 10 seconds...</p>"
                "</div></body></html>";
  
  server.send(200, "text/html", html);

  // Send setup complete notification before restarting
  Serial.println("Calling sendSetupCompleteNotification from handleSetup...");
  sendSetupCompleteNotification();
  
  // Wait a bit and then restart
  Serial.println("Restarting device after setup...");
  delay(2000); // Delay to allow notifications to be sent and page to load
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
    notificationStatus = "<span style='color:red;'>No notifications enabled!</span>";
  }

  // --- OTA Status Display ---
  String otaStatus = "";
  if (updateInProgress) {
    otaStatus = "<p><strong>Update:</strong> <span style='color:orange;'>Firmware update in progress...</span></p>";
  } else {
    otaStatus = "<p><strong>Firmware:</strong> " + firmwareVersion + "</p>";
  }
  // --- End OTA Status ---
  
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
                // Removed version here, added to status below
                "</div>"
                "</div>"
                "<div class='status-indicators'>"
                "<div class='tooltip'>"
                + wifiSvg +
                "<span class=\"tooltip-text\">Signal: " + signalQuality + " (" + String(rssi) + " dBm)</span>"
                "</div>"
                #if ENABLE_BATTERY_MONITORING
                "<div class='tooltip'>"
                "<div class='battery-icon'>"
                "<div class='battery-level' style='width:" + String(batteryPercentage) + "%; background-color:" + batteryColor + ";'></div>"
                "</div>"
                "<span class=\"tooltip-text\">Battery: " + String(batteryPercentage) + "% (" + String(batteryVoltage) + "V)</span>"
                "</div>"
                #endif
                "</div>"
                "</div>"
                "<p>Device is operational and monitoring for panic button presses.</p>"
                "<div class='status'>"
                "<p><strong>Device ID:</strong> " + configSSID + "</p>"
                "<p><strong>Hardware:</strong> " + hardwarePlatform + "</p>"
                 + otaStatus + // Added OTA status/firmware version here
                "<p><strong>Location:</strong> " + (device_location.length() > 0 ? device_location : "Not specified") + "</p>"
                "<p><strong>WiFi SSID:</strong> " + wifi_ssid + "</p>"
                "<p><strong>IP Address:</strong> " + WiFi.localIP().toString() + "</p>"
                "<p><strong>MAC Address:</strong> " + WiFi.macAddress() + "</p>"
                "<p><strong>Notification:</strong> " + notificationStatus + "</p>"
                "</div>"
                "<div class='buttons'>"
                "<a href='/config' class='button'>Update Configuration</a>";
  
  // Add OTA Check Button only if an update is not already in progress
  if (!updateInProgress) {
    html += "<a href='/check-update' class='button update'>Check for Updates</a>";
  }

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

// Handle displaying the configuration update page
void handleConfigPage() {
  String html = "<!DOCTYPE html><html><head>"
                "<title>Update Configuration</title>"
                // ... (Rest of the config page head - no changes needed) ...
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
                "  const validationMsg = document.getElementById('validation-msg');"
                "  if (!emailEnabled && !webhookEnabled) {"
                "    submitBtn.disabled = true;"
                "    validationMsg.style.display = 'block';"
                "    return false;"
                "  } else {"
                "    submitBtn.disabled = false;"
                "    validationMsg.style.display = 'none';"
                "    return true;"
                "  }"
                "}"
                "</script>"
                "</head><body>"
                "<div class='container'>"
                "<div class='header-container'>"
                "<div class='logo-title'>"
                // ... (logo SVG) ...
                "<svg width='40' height='40' viewBox='0 0 200 200' xmlns='http://www.w3.org/2000/svg' class='logo'>"
                "  <circle cx='100' cy='100' r='95' fill='#2f2f2f'/>"
                "  <circle cx='100' cy='100' r='75' fill='#444'/>"
                "  <circle cx='100' cy='100' r='55' fill='#d32f2f'/>"
                "  <ellipse cx='85' cy='80' rx='20' ry='12' fill='white' opacity='0.3'/>"
                "</svg>"
                "<div>"
                "<h1>Update Configuration</h1>"
                "</div>"
                "</div>"
                "</div>"
                "<p><strong>Device ID: </strong>" + configSSID + "</p>"
                "<p><strong>Hardware: </strong>" + hardwarePlatform + "</p>"
                "<form action='/update' method='post' onsubmit='return validateForm()'>"

                "<div class='section'>"
                "<h2>Device Settings</h2>"
                "<label for='location'>Location Description:</label>"
                "<input type='text' id='location' name='location' value='" + device_location + "' placeholder='e.g. Living Room, Front Door, etc.' maxlength='69'><br>"
                "</div>"

                "<div class='section'>"
                "<h2>WiFi Settings</h2>"
                "<label for='ssid'>WiFi SSID:</label>"
                "<input type='text' id='ssid' name='ssid' value='" + wifi_ssid + "' required maxlength='69'><br>"
                "<label for='password'>WiFi Password:</label>"
                "<input type='password' id='password' name='password' value='" + wifi_password + "' maxlength='69'><br>"
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
                "<span class='toggle-label'>Enable</span>"
                "</div>"
                "<div id='email-fields' style='" + (email_enabled ? "display:block" : "display:none") + "'>"
                "<label for='email_server'>SMTP Server:</label>"
                "<input type='text' id='email_server' name='email_server' value='" + email_server + "' maxlength='69'><br>"
                "<label for='email_port'>SMTP Port:</label>"
                "<input type='number' id='email_port' name='email_port' value='" + String(email_port) + "'><br>"
                "<label for='email_username'>Email Username:</label>"
                "<input type='text' id='email_username' name='email_username' value='" + email_username + "' maxlength='69'><br>"
                "<label for='email_password'>Email Password:</label>"
                "<input type='password' id='email_password' name='email_password' value='" + email_password + "' maxlength='69'><br>"
                "<label for='email_recipient'>Recipient Email:</label>"
                "<input type='email' id='email_recipient' name='email_recipient' value='" + email_recipient + "' maxlength='69'><br>"
                "</div>"
                "</div>"

                "<div class='toggle-section'>"
                "<h3>Webhook Notifications</h3>"
                "<div class='toggle-container'>"
                "<label class='switch'>"
                "<input type='checkbox' id='webhook_enabled' name='webhook_enabled' " + (webhook_enabled ? "checked" : "") + " onchange='toggleWebhookFields()'>"
                "<span class='slider'></span>"
                "</label>"
                "<span class='toggle-label'>Enable</span>"
                "</div>"
                "<div id='webhook-fields' style='" + (webhook_enabled ? "display:block" : "display:none") + "'>"
                "<label for='webhook_url'>Webhook URL:</label>"
                "<input type='url' id='webhook_url' name='webhook_url' value='" + webhook_url + "' placeholder='https://example.com/webhook' maxlength='169'><br>"
                "<p class='info'>The device will send a JSON payload to this URL when the alarm is triggered.</p>"
                "</div>"
                "</div>"
                "</div>"

                "<input type='submit' id='submit-btn' value='Update Configuration'>"
                "</form>"
                "<div class='back'><a href='/' class='button back-button'>Back to Home</a></div>" // Added class for styling
                "</div>"
                "<script>"
                "// Initialize toggle states and validation on page load"
                "document.addEventListener('DOMContentLoaded', function() {"
                "  toggleEmailFields();"
                "  toggleWebhookFields();"
                "  validateForm();"
                "});"
                "</script>"
                "</body></html>";
  
  server.send(200, "text/html", html);
}

// Handle saving the updated configuration
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
                "<p>Your settings have been saved successfully.</p>"
                "<p>Redirecting to the home page in 5 seconds...</p>"
                "</div></body></html>";
  
  server.send(200, "text/html", html);
  
  // Load the new configuration
  loadConfig();
}

// Handle sending a test email
void handleTestEmail() {
  String result;
  if (email_enabled) {
    Serial.println("Sending test email...");
    // Use a slightly different subject/body for test emails
    ESP_Mail_Session session;
    session.server.host_name = email_server.c_str();
    session.server.port = email_port;
    // Default sender email
    String senderEmail = email_username.c_str();

    // SendGrid uses "apikey" as username and the API key as password.
    // SendGrid uses "apikey" as username and the API key as password.
    // The actual sender email ('From' address) is set in message.sender.email.
    if (email_server == "smtp.sendgrid.net") {
      session.login.email = "apikey"; // For SendGrid, the login username is "apikey"
      Serial.println("Using SendGrid for test email: login email set to 'apikey'. Sender email will be original username.");
    } else {
      session.login.email = email_username.c_str(); // For other SMTPs, use the configured username
    }
    session.login.password = email_password.c_str(); // Password or API Key for SendGrid

    SMTP_Message message;
    message.sender.name = "Panic Alarm (Test)";
    message.sender.email = senderEmail.c_str(); // The 'From' address visible to the recipient
    message.subject = "Panic Alarm - TEST EMAIL"; // Clearly mark as a test
    message.addRecipient("User", email_recipient.c_str());

    // Construct detailed HTML content for the test email, mirroring the actual alert.
    // This ensures all dynamic fields are correctly populated and displayed.
    String locationInfo = device_location.length() > 0 ? 
                        "<p><strong>Location:</strong> " + device_location + "</p>" : 
                        "<p><strong>Location:</strong> Not specified</p>";
    String webhookStatus = webhook_enabled ? "<p>Webhook notifications: Enabled</p>" : "<p>Webhook notifications: Disabled</p>";
    String batteryInfo = "";
    #if ENABLE_BATTERY_MONITORING
    batteryInfo = "<p><strong>Battery:</strong> " + String(batteryPercentage) + "% (" + String(batteryVoltage) + "V)</p>";
    #else
    batteryInfo = "<p><strong>Battery:</strong> Monitoring disabled</p>";
    #endif

    String htmlMsg = "<div style='color:blue;'><h1>PANIC ALARM - TEST EMAIL</h1>" // Styled blue to visually distinguish as a test
                     "<p>This is a test email from your Panic Alarm device. If you received this, your email settings are likely correct.</p>"
                     "<p><strong>Device ID:</strong> " + configSSID + "</p>"
                     "<p><strong>Hardware:</strong> " + hardwarePlatform + "</p>"
                     + locationInfo +
                     "<p><strong>Device IP:</strong> " + WiFi.localIP().toString() + "</p>"
                     "<p><strong>MAC Address:</strong> " + WiFi.macAddress() + "</p>"
                     "<p><strong>WiFi Signal:</strong> " + String(rssi) + " dBm (" + signalQuality + ")</p>"
                     + batteryInfo +
                     "<p><strong>" + webhookStatus + "</strong></p>"
                     "<p><strong>Time:</strong> " + String(millis() / 1000) + " seconds since device boot</p>"
                     "</div>";
    message.html.content = htmlMsg.c_str();

    if (!smtp.connect(&session)) {
       result = "Failed to connect to SMTP server. Check server/port/credentials.";
       Serial.println("SMTP Connect Error: " + smtp.errorReason());
    } else {
        if (!MailClient.sendMail(&smtp, &message)) {
            result = "Failed to send test email. Error: " + smtp.errorReason();
            Serial.println("Email Send Error: " + smtp.errorReason());
        } else {
            result = "Test email sent successfully!";
            Serial.println("Test email sent.");
        }
    }
  } else {
    result = "Email notifications are disabled. Please enable them in settings first.";
    Serial.println("Test email skipped: Email disabled.");
  }
  
  String html = "<!DOCTYPE html><html><head>"
                "<title>Test Result</title>"
                "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                "<link rel='stylesheet' href='style.css'>"
                "<meta http-equiv='refresh' content='5;url=/'>"
                "</head><body>"
                "<div class='container'>"
                "<h1>Email Test Result</h1>"
                "<p>" + result + "</p>"
                "<p>Redirecting to home page in 5 seconds...</p>"
                "</div></body></html>";
  
  server.send(200, "text/html", html);
}

// Handle sending a test webhook
void handleTestWebhook() {
  String result;
  if (webhook_enabled) {
    Serial.println("Sending test webhook...");
    
    HTTPClient http;
    
    // Ensure URL has protocol
    if (webhook_url.startsWith("http://") || webhook_url.startsWith("https://")) {
        http.begin(webhook_url);
    } else {
        String correctedUrl = "https://" + webhook_url;
        http.begin(correctedUrl);
    }
    http.addHeader("Content-Type", "application/json");

    // Determine service type from URL and format accordingly
    String jsonPayload;
    String deviceId = configSSID;
    deviceId.replace("\"", "\\\""); // Escape quotes for JSON
    String location = device_location.length() > 0 ? device_location : "Not specified";
    location.replace("\"", "\\\"");
    location.replace("\\", "\\\\");
    String ipAddr = WiFi.localIP().toString();
    String macAddr = WiFi.macAddress();
    String timeStr = String(millis() / 1000);

    // Create detailed message body, similar to actual alert.
    // This ensures all dynamic fields are correctly populated for thorough testing.
    String msgBody = "Device ID: " + deviceId + "\\n" +
                     "Hardware: " + hardwarePlatform + "\\n" +
                     "Location: " + location + "\\n" +
                     "IP Address: " + ipAddr + "\\n" +
                     "MAC Address: " + macAddr + "\\n" +
                     "WiFi Signal: " + String(rssi) + " dBm (" + signalQuality + ")\\n" +
                     #if ENABLE_BATTERY_MONITORING
                     "Battery: " + String(batteryPercentage) + "% (" + String(batteryVoltage) + "V)\\n" +
                     #else
                     "Battery: Not installed\\n" + // Indicate if battery monitoring is off
                     #endif
                     "Time: " + timeStr + " seconds since boot";
    
    String titleText = "PANIC ALARM - TEST WEBHOOK"; // Clearly identifies this as a test webhook

    // Format payload based on the webhook service type
    if (webhook_url.indexOf("discord.com") > 0) {
        jsonPayload = "{\"content\":\"📢 **" + titleText + "** 📢\\n" + msgBody + 
                      "\",\"embeds\":[{\"title\":\"Panic Alarm Test Notification\",\"color\":3447003,\"description\":\"This is a detailed test notification. All systems nominal.\"}]}";
    }
    else if (webhook_url.indexOf("chat.googleapis.com") > 0) {
        jsonPayload = "{\"text\":\"📢 *" + titleText + "* 📢\\n" + msgBody + "\"}";
    }
    else if (webhook_url.indexOf("hooks.slack.com") > 0) {
        jsonPayload = "{\"text\":\"📢 *" + titleText + "* 📢\",\"attachments\":[{\"color\":\"#3AA3E3\",\"fields\":[" +
                      "{\"title\":\"Device ID\",\"value\":\"" + deviceId + "\",\"short\":true}," +
                      "{\"title\":\"Hardware\",\"value\":\"" + hardwarePlatform + "\",\"short\":true}," +
                      "{\"title\":\"Location\",\"value\":\"" + location + "\",\"short\":true}," +
                      "{\"title\":\"IP Address\",\"value\":\"" + ipAddr + "\",\"short\":true}," +
                      "{\"title\":\"MAC Address\",\"value\":\"" + macAddr + "\",\"short\":true}," +
                      "{\"title\":\"WiFi Signal\",\"value\":\"" + String(rssi) + " dBm (" + signalQuality + ")\",\"short\":true}," +
                      #if ENABLE_BATTERY_MONITORING
                      "{\"title\":\"Battery\",\"value\":\"" + String(batteryPercentage) + "% (" + String(batteryVoltage) + "V)\",\"short\":true}," +
                      #else
                      "{\"title\":\"Battery\",\"value\":\"Not installed\",\"short\":true}," +
                      #endif
                      "{\"title\":\"Time\",\"value\":\"" + timeStr + " seconds since boot\",\"short\":false}" +
                      "]}]}";
    }
    else if (webhook_url.indexOf("webhook.office.com") > 0) {
        jsonPayload = "{";
        jsonPayload += "\"@type\":\"MessageCard\",";
        jsonPayload += "\"@context\":\"http://schema.org/extensions\",";
        jsonPayload += "\"themeColor\":\"3AA3E3\","; // Blue for test
        jsonPayload += "\"summary\":\"" + titleText + "\",";
        jsonPayload += "\"sections\":[{";
        jsonPayload += "\"activityTitle\":\"📢 " + titleText + "\",";
        jsonPayload += "\"facts\":[";
        jsonPayload += "{\"name\":\"Device ID\",\"value\":\"" + deviceId + "\"},";
        jsonPayload += "{\"name\":\"Hardware\",\"value\":\"" + hardwarePlatform + "\"},";
        jsonPayload += "{\"name\":\"Location\",\"value\":\"" + location + "\"},";
        jsonPayload += "{\"name\":\"IP Address\",\"value\":\"" + ipAddr + "\"},";
        jsonPayload += "{\"name\":\"MAC Address\",\"value\":\"" + macAddr + "\"},";
        jsonPayload += "{\"name\":\"WiFi Signal\",\"value\":\"" + String(rssi) + " dBm (" + signalQuality + ")\"},";
        #if ENABLE_BATTERY_MONITORING
        jsonPayload += "{\"name\":\"Battery\",\"value\":\"" + String(batteryPercentage) + "% (" + String(batteryVoltage) + "V)\"},";
        #else
        jsonPayload += "{\"name\":\"Battery\",\"value\":\"Not installed\"},";
        #endif
        jsonPayload += "{\"name\":\"Time\",\"value\":\"" + timeStr + " seconds since boot\"}";
        jsonPayload += "],\"markdown\":true}]}";
    }
    else { // Generic webhook
        jsonPayload = "{\"event\":\"TEST_NOTIFICATION\","; // Clearly a test event
        jsonPayload += "\"device_id\":\"" + deviceId + "\",";
        jsonPayload += "\"hardware\":\"" + hardwarePlatform + "\",";
        jsonPayload += "\"location\":\"" + location + "\",";
        jsonPayload += "\"ip_address\":\"" + ipAddr + "\",";
        jsonPayload += "\"mac_address\":\"" + macAddr + "\",";
        jsonPayload += "\"wifi_signal\":" + String(rssi) + ",";
        jsonPayload += "\"signal_quality\":\"" + signalQuality + "\",";
        #if ENABLE_BATTERY_MONITORING
        jsonPayload += "\"battery_percentage\":" + String(batteryPercentage) + ",";
        jsonPayload += "\"battery_voltage\":" + String(batteryVoltage) + ",";
        #else
        jsonPayload += "\"battery_percentage\":null,"; // Keep fields consistent
        jsonPayload += "\"battery_voltage\":null,";
        #endif
        jsonPayload += "\"time\":" + timeStr + ","; // Changed from triggered_at for clarity, but value is the same
        jsonPayload += "\"message\":\"This is a detailed test notification from your Panic Alarm device.\"}";
    }

    Serial.println("Sending test webhook payload: " + jsonPayload);
    int httpCode = http.POST(jsonPayload);

    if (httpCode > 0) {
        String response = http.getString();
        Serial.printf("Test webhook sent. HTTP Code: %d, Response: %s\n", httpCode, response.c_str());
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED || httpCode == HTTP_CODE_ACCEPTED || httpCode == HTTP_CODE_NO_CONTENT) {
            result = "Test webhook sent successfully! (HTTP Code: " + String(httpCode) + ")";
        } else {
            result = "Test webhook sent, but received unexpected HTTP code: " + String(httpCode) + ". Check webhook receiver.";
        }
    } else {
        result = "Failed to send test webhook. Error: " + String(http.errorToString(httpCode));
        Serial.println("Test webhook failed: " + String(http.errorToString(httpCode)));
    }
    http.end();

  } else {
    result = "Webhook notifications are disabled. Please enable them in settings first.";
    Serial.println("Test webhook skipped: Webhook disabled.");
  }

  String html = "<!DOCTYPE html><html><head>";
  html += "<title>Test Result</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<link rel='stylesheet' href='style.css'>";
  html += "<meta http-equiv='refresh' content='5;url=/'>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>Webhook Test Result</h1>";
  html += "<p>" + result + "</p>";
  html += "<p>Redirecting to home page in 5 seconds...</p>";
  html += "</div></body></html>";

  server.send(200, "text/html", html);
}

// Handle device factory reset request
void handleReset() {
   // Confirmation step
  if (!server.hasArg("confirm") || server.arg("confirm") != "true") {
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
                "<h1>Factory Reset Confirmation</h1>"
                "</div>"
                "</div>"
                "</div>"
                "<p style='color:red; font-weight:bold;'>WARNING:</p>"
                "<p>Are you absolutely sure you want to reset all settings?</p>"
                "<p>This action will erase your WiFi credentials, notification settings, and location information. The device will restart in setup mode, requiring you to reconnect to the '" + configSSID + "' WiFi network and configure it again.</p>"
                "<p>This action cannot be undone.</p>"
                "<div class='buttons'>"
                // Use buttons instead of links for actions
                "<form action='/reset' method='GET' style='display: inline-block; width: 48%; margin-right: 2%;'>"
                "<input type='hidden' name='confirm' value='true'>"
                "<button type='submit' class='button reset'>Yes, Reset Everything</button>"
                "</form>"
                "<form action='/' method='GET' style='display: inline-block; width: 48%;'>"
                "<button type='submit' class='button back-button'>Cancel</button>"
                "</form>"
                "</div>"
                "</div></body></html>";
      server.send(200, "text/html", html);
      return; // Stop processing here until confirmed
  }

  // --- Confirmed Reset ---
  Serial.println("Factory reset initiated...");

  // Erase configuration flag and potentially other settings
  EEPROM.write(CONFIG_FLAG_ADDR, 0); // Erase config flag
  // Optionally clear other EEPROM areas
  for(int i = SSID_ADDR; i < EEPROM_SIZE; i++){
      EEPROM.write(i, 0);
  }

  if (EEPROM.commit()) {
      Serial.println("EEPROM cleared for reset.");
  } else {
      Serial.println("EEPROM commit failed during reset!");
      // Still proceed with restart
  }

  String resetMsgHtml = "<!DOCTYPE html><html><head>"
                        "<title>Resetting...</title>"
                        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                        "<link rel='stylesheet' href='style.css'>"
                        // No refresh needed here, device will restart
                        "</head><body>"
                        "<div class='container'>"
                        "<h1>Factory Resetting</h1>"
                        "<p>Device is resetting and erasing configuration...</p>"
                        "<p>After restart, please connect to the WiFi network named \"" + configSSID + "\" to begin setup again.</p>"
                        "</div></body></html>";

  server.send(200, "text/html", resetMsgHtml);

  delay(2000); // Allow time for the response to be sent
  ESP.restart();
}


// --- OTA Web Handler ---
void handleCheckUpdate() {
  String result;
  bool updateStarted = false; // Track if update process began

  if (WiFi.status() != WL_CONNECTED) {
    result = "WiFi not connected. Cannot check for updates.";
  } else if (updateInProgress) {
    result = "An update is already in progress. Please wait for the device to restart.";
  } else {
    Serial.println("Manual update check triggered via web UI...");
    if (checkForUpdates()) {
      // If checkForUpdates returns true, it means downloadAndUpdate was called and *should* restart the device upon success.
      // The user might see this page briefly before the device restarts.
      result = "Update found and download initiated. The device will restart automatically if the update is successful.";
      updateStarted = true;
    } else {
      // Check if checkForUpdates found a version but failed to start/complete
      if (newFirmwareVersion.length() > 0) {
         result = "Found new version " + newFirmwareVersion + ", but the update process failed to start or complete. Check Serial Monitor for details.";
      } else {
         result = "No new firmware updates available. Your device is up to date (Version: " + firmwareVersion + ").";
      }
    }
  }

  // Display a status page. If update started, it might only show briefly.
  String html = "<!DOCTYPE html><html><head>"
                "<title>Check for Updates</title>"
                "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                "<link rel='stylesheet' href='style.css'>";
        // Only refresh if update *didn't* start, otherwise let restart handle it
        if (!updateStarted) {
          html += "<meta http-equiv='refresh' content='7;url=/'>";
        }
        html += "</head><body>"
                "<div class='container'>"
                "<h1>Update Check Result</h1>"
                "<p>" + result + "</p>";
        if (updateStarted) {
          html += "<p>If the update is successful, the device will restart shortly.</p>";
        } else {
          html += "<p>Redirecting back to the home page in 7 seconds...</p>";
        }
        html += "</div></body></html>";

  server.send(200, "text/html", html);
}

// Handle serving the CSS file
void handleCss() {
  String css = "body {"
               "  font-family: Arial, sans-serif;"
               "  margin: 0;"
               "  padding: 0;"
               "  background-color: #f4f4f4;" // Light gray background
               "}"
               ".container {"
               "  max-width: 700px;" // Slightly wider
               "  margin: 20px auto;" // Add margin top/bottom
               "  padding: 25px;"
               "  background-color: #fff;" // White background for container
               "  border-radius: 8px;" // Rounded corners
               "  box-shadow: 0 4px 8px rgba(0,0,0,0.1);" // Subtle shadow
               "  box-sizing: border-box;"
               "}"
               "h1, h2, h3 {"
               "  color: #333;"
               "  margin-top: 0;"
               "  margin-bottom: 0;"
               "}"
               "h1 { font-size: 1.8em; }"
               "h2 { font-size: 1.4em; color: #555; border-bottom: 1px solid #eee; padding-bottom: 5px; margin-bottom: 15px;}"
               "h3 { font-size: 1.1em; color: #666; margin-bottom: 10px; }"
               "/* Section elements */"
               ".section, .toggle-section, .status {"
               "  background-color: #f9f9f9;" // Very light gray for sections
               "  border: 1px solid #eee;" // Light border
               "  border-radius: 5px;"
               "  padding: 20px;" // More padding
               "  margin-bottom: 25px;" // More space between sections
               "  width: 100%;"
               "  box-sizing: border-box;"
               "}"
               ".status p { margin: 8px 0; }" // Spacing for status lines
               ".status strong { color: #444; }"
               "/* Header layout */"
               ".header-container {"
               "  display: flex;"
               "  flex-wrap: wrap;"
               "  align-items: center;" // Align items vertically
               "  justify-content: space-between;" // Space out logo/title and indicators
               "  margin-bottom: 25px;"
               "  padding-bottom: 15px;"
               "  border-bottom: 1px solid #ddd;"
               "}"
               ".logo-title {"
               "  display: flex;"
               "  align-items: center;"
               "  /* flex: 1; */" // Allow shrinking if needed
               "  margin-right: 20px;" // Space before indicators
               "}"
               ".logo {"
               "  margin-right: 15px;"
               "}"
               ".version {"
               "  color: #777;" // Darker gray for version
               "  font-size: 0.9em;" // Slightly smaller
               "  margin-top: 3px;"
               "  margin-bottom: 0;"
               "}"
               ".status-indicators {"
               "  display: flex;"
               "  align-items: center;"
               "  gap: 20px;" // More gap
               "  /* margin-left: auto; */" // Removed, handled by justify-content
               "}"
               "/* Status indicators */"
               ".wifi-icon {"
               "  display: inline-flex;"
               "  align-items: center;"
               "}"
               ".battery-icon {"
               "  position: relative;"
               "  width: 24px;" // Slightly larger
               "  height: 12px;"
               "  border: 2px solid #555;" // Darker border
               "  border-radius: 3px;" // More rounded
               "  padding: 1px;"
               "  box-sizing: content-box;"
               "  display: inline-block;"
               "  vertical-align: middle;"
               "}"
               ".battery-icon:after {"
               "  content: \"\";"
               "  position: absolute;"
               "  right: -5px;" // Adjusted position
               "  top: 50%;"
               "  transform: translateY(-50%);"
               "  width: 3px;" // Thicker terminal
               "  height: 6px;"
               "  background-color: #555;" // Darker terminal
               "  border-radius: 0 2px 2px 0;"
               "}"
               ".battery-level {"
               "  height: 100%;"
               "  border-radius: 1px;"
               "  transition: width 0.5s ease-in-out, background-color 0.5s ease-in-out;" // Animate color too
               "}"
               "/* Tooltips */"
               ".tooltip {"
               "  position: relative;"
               "  display: inline-block;"
               "}"
               ".tooltip .tooltip-text {"
               "  visibility: hidden;"
               "  background-color: rgba(0, 0, 0, 0.8);" // Darker tooltip background
               "  color: #fff;"
               "  text-align: center;"
               "  padding: 6px 12px;" // More padding
               "  border-radius: 5px;" // Rounded tooltip
               "  white-space: nowrap;"
               "  position: absolute;"
               "  z-index: 10;" // Ensure tooltip is on top
               "  bottom: 130%;" // Position above the icon
               "  left: 50%;"
               "  transform: translateX(-50%);"
               "  opacity: 0;"
               "  transition: opacity 0.3s ease-in-out;"
               "  pointer-events: none;" // Don't interfere with mouse
               "  font-size: 0.85em;"
               "}"
               ".tooltip:hover .tooltip-text {"
               "  visibility: visible;"
               "  opacity: 1;"
               "}"
               "/* Buttons */"
               ".buttons {"
               "  margin-top: 25px;"
               "  display: flex;" // Use flexbox for buttons
               "  flex-wrap: wrap;" // Allow wrapping on small screens
               "  gap: 10px;" // Space between buttons
               "}"
               ".button, input[type='submit'], button.button {" // Style button element too
               "  background-color: #5c6bc0;" // Indigo primary color
               "  color: white;"
               "  padding: 12px 18px;" // Larger padding
               "  border: none;"
               "  border-radius: 5px;"
               "  cursor: pointer;"
               "  font-size: 1em;" // Relative font size
               "  text-align: center;"
               "  text-decoration: none;"
               "  transition: background-color 0.2s ease-in-out, box-shadow 0.2s ease-in-out;"
               "  flex: 1 1 auto;" // Allow buttons to grow/shrink
               "  min-width: 150px;" // Minimum width before wrapping
               "  box-sizing: border-box;"
               "  box-shadow: 0 2px 4px rgba(0,0,0,0.1);"
               "}"
               ".button:hover, input[type='submit']:hover, button.button:hover {"
               "  background-color: #3f51b5;" // Darker indigo on hover
               "  box-shadow: 0 4px 8px rgba(0,0,0,0.15);"
               "}"
                // Specific button colors
               ".update { background-color: #66bb6a; } .update:hover { background-color: #4caf50; }" // Green for update check
               ".test { background-color: #29b6f6; } .test:hover { background-color: #03a9f4; }" // Light blue for test
               ".reset { background-color: #ef5350; } .reset:hover { background-color: #f44336; }" // Red for reset
               ".back-button { background-color: #78909c; } .back-button:hover { background-color: #546e7a; }" // Gray for back/cancel
               "input[type='submit']:disabled {"
               "  background-color: #cccccc;"
               "  cursor: not-allowed;"
               "  box-shadow: none;"
               "}"
               "/* Form elements */"
               "label {"
               "  display: block;"
               "  margin-top: 15px;" // More space above label
               "  margin-bottom: 5px;" // Space below label
               "  font-weight: bold;"
               "  color: #555;"
               "}"
               "input[type='text'], input[type='password'], input[type='email'], input[type='number'], input[type='url'] {"
               "  width: 100%;"
               "  padding: 10px;" // More padding in inputs
               "  margin-top: 0;" // Remove top margin, use label margin
               "  border: 1px solid #ccc;" // Slightly darker border
               "  border-radius: 4px;"
               "  box-sizing: border-box;"
               "  font-size: 1em;"
               "}"
               "input:focus { outline-color: #5c6bc0; }" // Outline matches button color
               ".back {"
               "  margin-top: 25px;"
               "}"
               ".note {"
               "  color: #666;"
               "  font-style: italic;"
               "  margin-bottom: 15px;"
               "  font-size: 0.9em;"
               "}"
               ".info {"
               "  color: #03a9f4;" // Match test button color
               "  font-size: 0.9em;"
               "  margin-top: 8px;"
               "}"
               ".validation-error {"
               "  color: #d32f2f;" // Darker red
               "  font-weight: bold;"
               "  margin: 15px 0;"
               "  padding: 12px;" // More padding
               "  background-color: #ffebee;" // Light red background
               "  border: 1px solid #ef9a9a;" // Red border
               "  border-radius: 4px;"
               "}"
               ".error {"
               "  color: #d32f2f;"
               "  font-weight: bold;"
               "}"
               "/* Toggle switches */"
               ".switch {"
               "  position: relative;"
               "  display: inline-block;"
               "  width: 50px;" // Standard size
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
               "  border-radius: 24px;" // Fully rounded
               "}"
               ".slider:before {"
               "  position: absolute;"
               "  content: \"\";"
               "  height: 18px;" // Larger knob
               "  width: 18px;"
               "  left: 3px;" // Adjusted position
               "  bottom: 3px;"
               "  background-color: white;"
               "  transition: .4s;"
               "  border-radius: 50%;"
               "  box-shadow: 0 1px 3px rgba(0,0,0,0.2);" // Add shadow to knob
               "}"
               "input:checked + .slider {"
               "  background-color: #5c6bc0;" // Match primary button color
               "}"
               "input:checked + .slider:before {"
               "  transform: translateX(26px);"
               "}"
               ".toggle-container {"
               "  display: flex;"
               "  align-items: center;"
               "  margin-bottom: 10px;" // Less margin below toggle line
               "  /* padding: 5px 0; */" // Remove padding here
               "}"
               ".toggle-label {"
               "  font-weight: bold;"
               "  color: #444;"
               "  margin-left: 5px;"
               "}"
               ".toggle-section > div:not(.toggle-container) {" // Indent fields under toggle
               "  padding-left: 20px;"
               "  border-left: 2px solid #eee;"
               "  margin-left: 10px;"
               "}"
               ".small-note {"
               "  font-size: 0.85em;"
               "  color: #777;"
               "  margin-top: 5px;"
               "}"
               "/* Responsive adjustments */"
               "@media (max-width: 600px) {"
               "  .container {"
               "    margin: 10px;"
               "    padding: 15px;"
               "  }"
               "  .header-container {"
               "    flex-direction: column;"
               "    align-items: flex-start;" // Align left on mobile
               "  }"
               "  .status-indicators {"
               "    margin-top: 15px;"
               "    width: 100%;" // Take full width
               "    justify-content: flex-start;" // Align left
               "  }"
               "  .buttons {"
               "     flex-direction: column;" // Stack buttons vertically
               "  }"
               "  .button, input[type='submit'], button.button {"
               "     width: 100%;" // Full width buttons
               "  }"
               "  .logo-title h1 { font-size: 1.5em; }"
               "}";
  server.send(200, "text/css", css);
}