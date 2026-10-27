/*
 * Firmngin Batch State Example
 *
 * Example using updateEntities() to batch only changed entity values
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

#if defined(ESP8266)
Firmngin fngin(DEVICE_ID, DEVICE_KEY, CLIENT_CERT, PRIVATE_KEY, SERVER_FINGERPRINT_BYTES);
#elif defined(ESP32)
Firmngin fngin(DEVICE_ID, DEVICE_KEY, SERVER_FINGERPRINT_BYTES, CLIENT_CERT, PRIVATE_KEY);
#endif

const char *entityKeys[] = {"10", "20", "pressure", "light", "status"};
String lastPushed[5];
bool initReceived = false;
bool initialPush = true;

ON_INIT(init) {
  if (!init.isValid()) return;
  initReceived = true;
  initialPush = true;
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

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

  if (!initReceived) return;

  // Replace these stable demo values with real sensor readings.
  String values[] = {"25", "60", "1013", String(analogRead(A0)), "ONLINE"};
  char buffer[512];
  firmngin_json::ArrayBuilder batch(buffer, sizeof(buffer));
  batch.reset();

  // The backend keeps the last entity state. Periodic heartbeats cost usage.
  for (int i = 0; i < 5; ++i) {
    if (initialPush || values[i] != lastPushed[i]) {
      if (!batch.add(entityKeys[i], values[i].c_str())) return;
    }
  }

  // Use the bool-returning API so failed sends do not advance the cache.
  if (batch.count() > 0 && fngin.updateEntities(batch.build())) {
    for (int i = 0; i < 5; ++i) lastPushed[i] = values[i];
    initialPush = false;
    Serial.println("Changed entity states sent!");
  }

  delay(100);
}
