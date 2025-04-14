#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>

// Network function declarations
void startConfigMode();
bool connectToWiFi();
bool isInConfigMode();
void handleConfigMode();
void handleWebRequests();
void setupConfigWebRoutes();
void setupNormalWebRoutes();

// Web handler function declarations
void handleRoot();
void handleSetup();
void handleNormalRoot();
void handleConfigPage();
void handleUpdate();
void handleTestEmail();
void handleReset();
void handleCss();

// External access to global variables
extern String wifi_ssid;
extern String wifi_password;

#endif // NETWORK_H