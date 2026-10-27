/*
 * Firmngin Push State Example
 *
 * Example using pushEntity() to send initial state and subsequent changes
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

// WiFi credentials
const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

Firmngin fngin(DEVICE_ID, DEVICE_KEY);

String status = "READY";
String lastPushedStatus;
int lastPushedSensor = 0;
bool statusPushed = false;
bool sensorPushed = false;
bool initReceived = false;

ON_INIT(init) {
  if (!init.isValid()) return;
  initReceived = true;
  statusPushed = false;
  sensorPushed = false;
}

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
}

void loop()
{
  fngin.loop();

  if (!initReceived) return;

  // The backend keeps the last entity state. Periodic heartbeats cost usage.
  // Send once after INIT, then only when a value differs from its last push.
  if (!statusPushed || status != lastPushedStatus) {
    if (fngin.pushEntity("status", status)) {
      lastPushedStatus = status;
      statusPushed = true;
    }
  }

  int valueA = analogRead(A0);
  if (!sensorPushed || valueA != lastPushedSensor) {
    if (fngin.pushEntity("sensor_a", valueA)) {
      lastPushedSensor = valueA;
      sensorPushed = true;
    }
  }

  delay(500); // Sensor sampling pace; unchanged values are never published.
}
