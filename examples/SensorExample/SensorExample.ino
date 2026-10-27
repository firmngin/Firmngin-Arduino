/*
 * Firmngin Sensor Example (DHT22)
 *
 * Example batching changed DHT22 readings with updateEntities()
 *
 * website: https://firmngin.dev
 * author: Firmngin.dev
 */

#include "keys.h"
#include <firmngin.h>
#include <DHT.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

// WiFi credentials
const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

// DHT22 sensor
#define DHT_PIN 4
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

Firmngin fngin(DEVICE_ID, DEVICE_KEY);

unsigned long lastSample = 0;
const unsigned long sampleInterval = 2000; // DHT22 sampling, not a publish timer
const char *entityKeys[] = {"temperature", "humidity", "heat_index"};
String lastPushed[3];
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

  Serial.println("=== Sensor Example (DHT22) ===");

  // Initialize DHT sensor
  dht.begin();

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
  if (millis() - lastSample < sampleInterval) return;
  lastSample = millis();

  // Read DHT22 sensor
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  // Check if read failed
  if (isnan(temp) || isnan(hum)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Calculate heat index
  float hi = dht.computeHeatIndex(temp, hum, false);

  if (isnan(hi)) return;

  String values[] = {String(temp), String(hum), String(hi)};
  char buffer[256];
  firmngin_json::ArrayBuilder batch(buffer, sizeof(buffer));
  batch.reset();

  // The backend keeps the last entity state. Periodic heartbeats cost usage.
  for (int i = 0; i < 3; ++i) {
    if (initialPush || values[i] != lastPushed[i]) {
      if (!batch.add(entityKeys[i], values[i].c_str())) return;
    }
  }

  if (batch.count() > 0 && fngin.updateEntities(batch.build())) {
    for (int i = 0; i < 3; ++i) lastPushed[i] = values[i];
    initialPush = false;
    Serial.println("Changed sensor readings pushed");
  }
}
