#include "network.h"
#include "config.h"
#include "storage.h"
#include "notifications.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <ESP.h>
#include <EEPROM.h>

// Global network variables
bool configMode = false;
String configSSID = DEFAULT_CONFIG_SSID;
const char* configPassword = DEFAULT_CONFIG_PASSWORD;
String wifi_ssid = "";
String wifi_password = "";
WebServer server(WEBSERVER_PORT);
DNSServer dnsServer;

// Check if in configuration mode
bool isInConfigMode() {
  return configMode;
}

// Start configuration mode with AP and captive portal
void startConfigMode() {
  configMode = true;
  
  // Set up Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(configSSID.c_str(), configPassword);
  
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  
  // Configure DNS server for captive portal
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  
  // Set up web server routes
  setupConfigWebRoutes();
  
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
    setupNormalWebRoutes();
    
    server.begin();
    return true;
  } else {
    Serial.println("");
    Serial.println("Failed to connect to WiFi");
    return false;
  }
}

// Handle DNS and WebServer in config mode
void handleConfigMode() {
  dnsServer.processNextRequest();
  server.handleClient();
}

// Handle WebServer in normal operation mode
void handleWebRequests() {
  server.handleClient();
}

// Set up web routes for configuration mode
void setupConfigWebRoutes() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/setup", HTTP_POST, handleSetup);
  server.on("/style.css", HTTP_GET, handleCss);
  server.onNotFound(handleRoot); // Captive portal redirect
}

// Set up web routes for normal operation
void setupNormalWebRoutes() {
  server.on("/", HTTP_GET, handleNormalRoot);
  server.on("/config", HTTP_GET, handleConfigPage);
  server.on("/update", HTTP_POST, handleUpdate);
  server.on("/test", HTTP_GET, handleTestEmail);
  server.on("/reset", HTTP_GET, handleReset);
  server.on("/style.css", HTTP_GET, handleCss);
}

// Handle config mode root page
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

// Handle setup form submission
void handleSetup() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  String email_server = server.arg("email_server");
  int email_port = server.arg("email_port").toInt();
  String email_username = server.arg("email_username");
  String email_password = server.arg("email_password");
  String email_recipient = server.arg("email_recipient");
  
  // Save configuration
  saveConfig(ssid, password, email_server, email_port, 
            email_username, email_password, email_recipient);
  
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

// Handle normal operation root page
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
                "<p><strong>Email Recipient:</strong> " + getEmailRecipient() + "</p>";
  
  #if HAS_BATTERY_MONITORING
  html += "<p><strong>Battery Voltage:</strong> " + String(getBatteryVoltage()) + "V</p>";
  #endif
  
  html += "</div>"
          "<div class='buttons'>"
          "<a href='/config' class='button'>Update Configuration</a>"
          "<a href='/test' class='button test'>Test Alarm</a>"
          "<a href='/reset' class='button reset'>Factory Reset</a>"
          "</div>"
          "</div></body></html>";
  
  server.send(200, "text/html", html);
}

// Handle configuration page
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
                "<input type='text' id='email_server' name='email_server' value='" + getEmailServer() + "' required><br>"
                "<label for='email_port'>SMTP Port:</label>"
                "<input type='number' id='email_port' name='email_port' value='" + String(getEmailPort()) + "' required><br>"
                "<label for='email_username'>Email Username:</label>"
                "<input type='text' id='email_username' name='email_username' value='" + getEmailUsername() + "' required><br>"
                "<label for='email_password'>Email Password:</label>"
                "<input type='password' id='email_password' name='email_password' value='" + getEmailPassword() + "' required><br>"
                "<label for='email_recipient'>Recipient Email:</label>"
                "<input type='email' id='email_recipient' name='email_recipient' value='" + getEmailRecipient() + "' required><br>"
                "</div>"
                "<input type='submit' value='Update Configuration'>"
                "</form>"
                "<div class='back'><a href='/' class='button'>Back to Home</a></div>"
                "</div></body></html>";
  
  server.send(200, "text/html", html);
}

// Handle configuration update
void handleUpdate() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  String email_server = server.arg("email_server");
  int email_port = server.arg("email_port").toInt();
  String email_username = server.arg("email_username");
  String email_password = server.arg("email_password");
  String email_recipient = server.arg("email_recipient");
  
  // Save configuration
  saveConfig(ssid, password, email_server, email_port, 
            email_username, email_password, email_recipient);
  
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

// Handle test email page
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

// Handle factory reset page
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

// Handle CSS stylesheet
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