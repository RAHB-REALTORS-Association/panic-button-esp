#include <ESP8266WiFi.h>
#include <WiFiManager.h>         
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <FS.h> // Include the SPIFFS library

// Constants
const int buttonPin = 2; // choose the input pin (for a pushbutton)
const int resetButtonPin = 3; // choose the input pin (for a reset button)
bool resetEnabled = true; // Set to false to disable reset functionality

// Message details
const char* message = "Your message goes here";
const char* emailSubject = "Your email subject goes here";
const char* senderEmail = "sender@example.com";
const char* sendingPhoneNumber = "0987654321"; // Your Twilio phone number

// Recipient details
const char* recipientEmails[] = {"recipient1@example.com", "recipient2@example.com"};
const char* recipientPhoneNumbers[] = {"1234567890", "0987654321"};

// API details
const char* twilioAPIKey = "TWILIO_API_KEY"; // Replace with your actual Twilio API key
const char* sendGridAPIKey = "SENDGRID_API_KEY"; // Replace with your actual SendGrid API key
const char* twilioAPIEndpoint = "https://api.twilio.com/2010-04-01/Accounts/{Account_SID}/Messages.json";
const char* sendGridAPIEndpoint = "https://api.sendgrid.com/v3/mail/send";

// Button debouncing
const unsigned long debounceTime = 50; // Debounce time in ms
unsigned long lastDebounceTime = 0;  // the last time the button pin was toggled
int buttonState;  // the current reading from the input pin
int lastButtonState = LOW;  // the previous reading from the input pin

// Reset button
unsigned long resetButtonPressTime = 0; // When the reset button was first pressed
const unsigned long resetPressDuration = 5000; // Must hold button for 5 seconds to reset

WiFiClientSecure client;

void setup() {
  pinMode(buttonPin, INPUT);
  if (resetEnabled) {
    pinMode(resetButtonPin, INPUT_PULLUP); // Use internal pullup resistor
  }
  Serial.begin(115200);

  // Connect to Wifi network or start access point if not configured
  WiFiManager wifiManager;
  if(!wifiManager.autoConnect("AutoConnectAP")){
    Serial.println("failed to connect, we should reset as see if it connects");
    delay(3000);
    ESP.reset();
    delay(5000);
  }
  
  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  
  // Setup secure client
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  // Load certificate file for Twilio
  File twilioCert = SPIFFS.open("/twilio.crt", "r");
  if (!twilioCert) {
    Serial.println("Failed to open cert file for Twilio");
  } else if (client.loadCertificate(twilioCert)) {
    Serial.println("Loaded cert file for Twilio");
  }
  twilioCert.close();

  // Load certificate file for SendGrid
  File sendGridCert = SPIFFS.open("/sendgrid.crt", "r");
  if (!sendGridCert) {
    Serial.println("Failed to open cert file for SendGrid");
  } else if (client.loadCertificate(sendGridCert)) {
    Serial.println("Loaded cert file for SendGrid");
  }
  sendGridCert.close();
}

void loop() {
  int reading = digitalRead(buttonPin);
  
  // Debounce button
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceTime) {
    if (reading != buttonState) {
      buttonState = reading;
      
      if (buttonState == HIGH) {
        // The button is pressed, send API requests
        if(WiFi.status()== WL_CONNECTED){
           HTTPClient http;

           for(int i = 0; i < sizeof(recipientPhoneNumbers)/sizeof(char*); i++) {
             http.begin(client, twilioAPIEndpoint); 
             http.addHeader("Content-Type", "application/x-www-form-urlencoded");
             http.addHeader("Authorization", "Basic " + String(twilioAPIKey));
             String twilioPayload = "From=" + String(sendingPhoneNumber) + "&To=" + String(recipientPhoneNumbers[i]) + "&Body=" + String(message);
             int httpCodeTwilio = http.POST(twilioPayload);
             if (httpCodeTwilio > 0) {
                if (httpCodeTwilio != HTTP_CODE_OK) {
                   Serial.println("Twilio request failed, HTTP code: " + String(httpCodeTwilio));
                }
             } else {
                Serial.println("Twilio request failed, error: " + http.errorToString(httpCodeTwilio));
             }
             http.end();
           }

           for(int i = 0; i < sizeof(recipientEmails)/sizeof(char*); i++) {
             http.begin(client, sendGridAPIEndpoint); 
             http.addHeader("Content-Type", "application/json");
             http.addHeader("Authorization", "Bearer " + String(sendGridAPIKey));
             String sendGridPayload = "{\"personalizations\": [{\"to\": [{\"email\": \"" + String(recipientEmails[i]) + 
             "\"}]}], \"from\": {\"email\": \"" + String(senderEmail) + 
             "\"}, \"subject\": \"" + String(emailSubject) + 
             "\", \"content\": [{\"type\": \"text/plain\", \"value\": \"" + String(message) + "\"}]}";
             int httpCodeSendGrid = http.POST(sendGridPayload);
             if (httpCodeSendGrid > 0) {
                if (httpCodeSendGrid != HTTP_CODE_OK) {
                   Serial.println("SendGrid request failed, HTTP code: " + String(httpCodeSendGrid));
                }
             } else {
                Serial.println("SendGrid request failed, error: " + http.errorToString(httpCodeSendGrid));
             }
             http.end();
           }
        }
      }
    }
  }
  
  lastButtonState = reading;

  // Check reset button, if reset is enabled
  if (resetEnabled) {
    int resetButtonState = digitalRead(resetButtonPin);

    // If the reset button is currently being pressed
    if (resetButtonState == LOW) {
      // If the reset button was just pressed, record the start time
      if (resetButtonPressTime == 0) {
        resetButtonPressTime = millis();
      }

      // If the reset button has been held for long enough, reset the settings
      else if ((millis() - resetButtonPressTime) > resetPressDuration) {
        WiFiManager wifiManager;
        wifiManager.resetSettings();
        ESP.reset(); // Reset ESP to trigger WiFi reconnection on startup
      }
    }

    // If the reset button was released before the time limit, do not reset the settings
    else {
      resetButtonPressTime = 0;
    }
  }
}
