/*
 * Firmngin Persistent Queue Example
 *
 * Demonstrates the offline-tolerant publish queue.
 * When the MQTT connection drops (or the publish call fails), pending
 * state updates are stored in LITTLEFS and replayed automatically once
 * the connection is back.
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

#if defined(ESP8266)
Firmngin fngin(DEVICE_ID, DEVICE_KEY, CLIENT_CERT, PRIVATE_KEY, SERVER_FINGERPRINT_BYTES);
#elif defined(ESP32)
Firmngin fngin(DEVICE_ID, DEVICE_KEY, SERVER_FINGERPRINT_BYTES, CLIENT_CERT, PRIVATE_KEY);
#endif

unsigned long lastPush = 0;
unsigned long lastQueueReport = 0;

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== Persistent Queue Example ===");

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  fngin.setDebug(true);

  fngin.setMaxQueueEntries(64);
  fngin.setQueueEnabled(true);

  fngin.begin();

  Serial.print("Queue ready, size=");
  Serial.println(fngin.getQueueSize());
}

void loop()
{
  fngin.loop();

  if (millis() - lastPush >= 3000) {
    lastPush = millis();
    int counter = (int)(millis() / 1000UL);
    fngin.pushEntity("counter", String(counter).c_str());
  }

  if (millis() - lastQueueReport >= 10000) {
    lastQueueReport = millis();
    Serial.print("[monitor] queue size = ");
    Serial.println(fngin.getQueueSize());
  }

  delay(50);
}
