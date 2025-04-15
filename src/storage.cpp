#include "storage.h"
#include "config.h"
#include "network.h"
#include <EEPROM.h>

// Global email variables
String email_server = "";
int email_port = 0;
String email_username = "";
String email_password = "";
String email_recipient = "";

// Getter functions for email configuration
String getEmailServer() { return email_server; }
int getEmailPort() { return email_port; }
String getEmailUsername() { return email_username; }
String getEmailPassword() { return email_password; }
String getEmailRecipient() { return email_recipient; }

// Initialize EEPROM storage
void initStorage() {
  EEPROM.begin(EEPROM_SIZE);
}

// Check if device is configured
bool isConfigured() {
  return (EEPROM.read(CONFIG_FLAG_ADDR) == CONFIG_FLAG);
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