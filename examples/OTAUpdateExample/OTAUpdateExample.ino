/*
 * Firmngin OTA Update Example
 *
 * Demonstrates basic OTA firmware update using checkOTA() and performOTA()
 *
 * website: https://firmngin.dev
 * author: (Arif) Firmngin.dev
 */

#include <Arduino.h>
#include <firmngin.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

#define DEVICE_ID  "YOUR_DEVICE_ID"
#define DEVICE_KEY "YOUR_DEVICE_SECRET_KEY"

const char *ssid     = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

Firmngin fngin(DEVICE_ID, DEVICE_KEY);

void setup() {
  Serial.begin(115200);

  // Connect WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  Serial.println("WiFi connected");

  // Set timezone
  fngin.setTimezone(7);

  // Set firmware info (version, board, model)
#if defined(ESP8266)
  fngin.setFirmwareInfo("1.0.0", "ESP8266", "esp8266:esp8266:generic");
#elif defined(ESP32)
  fngin.setFirmwareInfo("1.0.0", "ESP32", "esp32:esp32:esp32");
#endif

  // OTA status callback
  fngin.onOTAStatus([](const char *status, const char *message) {
    Serial.print("[OTA] ");
    Serial.print(status);
    Serial.print(": ");
    Serial.println(message);
  });

  fngin.begin();
}

void loop() {
  fngin.loop();

  // Check for updates every 5 minutes
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 300000) {
    lastCheck = millis();
    fngin.checkOTA();
  }
}
