/*
 * Firmngin Sensor Example (DHT22)
 *
 * Example using pushEntity() and pushBatchEntities() with DHT22 sensor
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

#if defined(ESP8266)
Firmngin fngin(DEVICE_ID, DEVICE_KEY, CLIENT_CERT, PRIVATE_KEY, SERVER_FINGERPRINT_BYTES);
#elif defined(ESP32)
Firmngin fngin(DEVICE_ID, DEVICE_KEY, SERVER_FINGERPRINT_BYTES, CLIENT_CERT, PRIVATE_KEY);
#endif

unsigned long lastBatchPush = 0;

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

  // Read DHT22 sensor
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  // Check if read failed
  if (isnan(temp) || isnan(hum)) {
    Serial.println("Failed to read from DHT sensor!");
    delay(2000);
    return;
  }

  // Calculate heat index
  float hi = dht.computeHeatIndex(temp, hum, false);

  // Push as a batch every 5 seconds
  if (millis() - lastBatchPush >= 5000) {
    lastBatchPush = millis();
    bool sent = fngin.pushBatchEntities()
      .add("temperature", String(temp))
      .add("humidity", String(hum))
      .add("heat_index", String(hi))
      .send();

    if (sent) {
      Serial.println("Batch pushed: temperature, humidity, heat_index");
    }
  }

  delay(2000);  // Read sensor every 2 seconds
}
