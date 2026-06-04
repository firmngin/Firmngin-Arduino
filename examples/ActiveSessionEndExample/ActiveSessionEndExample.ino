/*
 * Firmngin Active Session End Example
 *
 * Simple device-local session end using fngin.on(ON_ACTIVE_SESSION, ...).
 *
 * website: https://firmngin.dev
 * author: (Arif) Firmngin.dev
 */

#include "keys.h"
#include <firmngin.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

#define DEVICE_ID "YOUR_DEVICE_ID"
#define DEVICE_KEY "YOUR_DEVICE_SECRET_KEY"

const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

Entity energyKwh("energy_kwh");

#if defined(ESP8266)
Firmngin fngin(DEVICE_ID, DEVICE_KEY, CLIENT_CERT, PRIVATE_KEY, SERVER_FINGERPRINT_BYTES);
#elif defined(ESP32)
Firmngin fngin(DEVICE_ID, DEVICE_KEY, SERVER_FINGERPRINT_BYTES, CLIENT_CERT, PRIVATE_KEY);
#endif

void setup()
{
  Serial.begin(115200);
  delay(1000);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");

  fngin.setDebug(true);
  fngin.on(ON_ACTIVE_SESSION, [](ActiveSession &s) {
    if (s.entity(energyKwh).toFloat() >= 10.0) {
      s.endSession();
    }
  });

  fngin.begin();
}

void loop()
{
  fngin.loop();
}
