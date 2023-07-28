# Panic Button for ESP8266-WiFi

This is an IoT project that sends alerts to configured recipients using Twilio and SendGrid when a physical button is pressed. The device uses an ESP8266 and Arduino.

## Setup

### Prerequisites

You need to have the following installed:

1. Arduino IDE
2. Python 3
3. OpenSSL
4. ESP8266 board package in Arduino IDE
5. ESP8266FS plugin for Arduino IDE
6. Following Arduino Libraries: ESP8266WiFi, WiFiManager, ESP8266HTTPClient, WiFiClientSecure

### Steps

1. Clone the repository.

```sh
git clone https://github.com/RAHB-REALTORS-Association/panic-button-esp.git
cd panic-button
```

2. Run the Python script that fetches root CA certificates used by Twilio and SendGrid and saves them to the SPIFFS data directory.

```sh
python generate_certificates.py
```

This will generate `twilio.crt` and `sendgrid.crt` files in the `data` directory inside your Arduino project.

3. Modify the `panic-button.ino` file inside the `panic-button` folder to use your actual Twilio and SendGrid API keys and recipient details. Please remember to base64 encode your 
Twilio SID and Auth token and use this as your TWILIO_API_KEY.

4. In the Arduino IDE, select your ESP8266 board and the correct COM port.

5. Go to "Sketch" -> "Show Sketch Folder". 

6. In the opened window, navigate up a level and then into the `data` directory.

7. You should see `twilio.crt` and `sendgrid.crt` files here. These are the root CA certificates fetched by our Python script.

8. Return to Arduino IDE, go to "Tools" -> "ESP8266 Sketch Data Upload". This will upload the contents of the data directory (the CA certificates) to your ESP8266's filesystem.

9. After uploading the certificates, go to "Sketch" -> "Upload" to compile and upload the sketch to the ESP8266.

## Operation

Once the device starts up, it will try to connect to the WiFi network. If it can't connect to a known network, it will start an access point called "AutoConnectAP". You can connect to 
this AP and configure your WiFi credentials.

When you press the panic button, the device will send alerts to all configured recipients via Twilio and SendGrid.

## Reset

If you want to change the WiFi credentials, keep the reset button pressed for 5 seconds. This will reset the stored WiFi credentials and restart the device. After restart, you can 
configure new WiFi credentials as described above.

