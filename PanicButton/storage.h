#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>

// Storage function declarations
void initStorage();
bool isConfigured();
void loadConfig();
void saveConfig(String ssid, String password, String server, int port, 
                String username, String emailpass, String recipient);

// Email configuration getter functions
String getEmailServer();
int getEmailPort();
String getEmailUsername();
String getEmailPassword();
String getEmailRecipient();

#endif // STORAGE_H