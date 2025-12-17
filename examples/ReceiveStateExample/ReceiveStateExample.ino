/*
 * FirmnginKit Receive State Example
 *
 * Example using VPin to receive commands from server
 *
 * website: https://firmngin.dev
 * author: (Arif) Firmngin.dev
 */

#include "keys.h"
#include "firmnginKit.h"

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

#define DEVICE_ID "FNG_YOUR_DEVICE_ID"
#define DEVICE_KEY "FNG_YOUR_DEVICE_KEY"

// WiFi credentials
const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

#if defined(ESP8266)
FirmnginKit fngin(DEVICE_ID, DEVICE_KEY, CLIENT_CERT, PRIVATE_KEY, SERVER_FINGERPRINT_BYTES);
#elif defined(ESP32)
FirmnginKit fngin(DEVICE_ID, DEVICE_KEY, SERVER_FINGERPRINT_BYTES, CLIENT_CERT, PRIVATE_KEY);
#endif

// VPin with GPIO: receive commands from server
VPin relay1(10, 2);              // vpin 10 -> GPIO 2 (digital)
VPin relay2(20, 4, ACTIVE_LOW);  // vpin 20 -> GPIO 4 (active low)
VPin led(30, 5, PWM);            // vpin 30 -> GPIO 5 (PWM 0-255)

// Register to receive server commands
ON_VPIN(relay1);
ON_VPIN(relay2);
ON_VPIN(led);

// Custom callback for complex logic
ON_VPIN(40, [](String payload) {
  Serial.print("Virtual Pin 40 received: ");
  Serial.println(payload);
});

void setup()
{
  Serial.begin(115200);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  fngin.begin();
}

void loop()
{
  fngin.loop();
  delay(100);
}
