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

#include <WiFi.h>

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
  fngin.setFirmwareInfo("1.0.0", "ESP32", "esp32:esp32:esp32");

  // You can also use the OTA status callback to get the status of the OTA update.
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

  //You can check for updates every 5 minutes or you can use the fngin.checkOTA() function to check for updates.
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 300000) {
    lastCheck = millis();
    fngin.checkOTA();
  }
}
