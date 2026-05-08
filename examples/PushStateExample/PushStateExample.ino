/*
 * Firmngin Push State Example
 *
 * Example using pushEntity() and pushBatchEntities() to send data to server
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

// WiFi credentials
const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

#if defined(ESP8266)
Firmngin fngin(DEVICE_ID, DEVICE_KEY, CLIENT_CERT, PRIVATE_KEY, SERVER_FINGERPRINT_BYTES);
#elif defined(ESP32)
Firmngin fngin(DEVICE_ID, DEVICE_KEY, SERVER_FINGERPRINT_BYTES, CLIENT_CERT, PRIVATE_KEY);
#endif

// Method 1: Direct pushEntity (manual)
unsigned long lastManualPush = 0;

// Method 2: Batch builder
unsigned long lastBatchPush = 0;

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== Push State Example ===");

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

  fngin.setDebug(true);
  fngin.begin();

  // Send initial status
  fngin.pushEntity("status", "READY");
  Serial.println("Initial status sent: READY");
}

void loop()
{
  fngin.loop();

  // Simulate sensor readings
  int valueA = analogRead(A0);
  int valueB = random(0, 100);
  int valueC = random(0, 255);

  // Method 1: Direct pushEntity (every 10 seconds)
  if (millis() - lastManualPush >= 10000) {
    lastManualPush = millis();
    String uptime = String(millis() / 1000);
    fngin.pushEntity("uptime", uptime.c_str());
    Serial.println("Manual push: uptime");
  }

  // Method 2: Batch push (every 5 seconds)
  if (millis() - lastBatchPush >= 5000) {
    lastBatchPush = millis();
    bool sent = fngin.pushBatchEntities()
      .add("sensor_a", String(valueA))
      .add("sensor_b", String(valueB))
      .add("sensor_c", String(valueC))
      .send();

    if (sent) {
      Serial.println("Batch pushed: sensor_a, sensor_b, sensor_c");
    }
  }

  delay(500);
}
