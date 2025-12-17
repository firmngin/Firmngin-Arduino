/*
 * FirmnginKit Sensor Example (DHT22)
 *
 * Example using VPin with DHT22 sensor
 * Demonstrates: onChange(), interval(), threshold()
 *
 * website: https://firmngin.dev
 * author: Firmngin.dev
 */

#include "keys.h"
#include "firmnginKit.h"
#include <DHT.h>

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

// DHT22 sensor
#define DHT_PIN 4
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

#if defined(ESP8266)
FirmnginKit fngin(DEVICE_ID, DEVICE_KEY, CLIENT_CERT, PRIVATE_KEY, SERVER_FINGERPRINT_BYTES);
#elif defined(ESP32)
FirmnginKit fngin(DEVICE_ID, DEVICE_KEY, SERVER_FINGERPRINT_BYTES, CLIENT_CERT, PRIVATE_KEY);
#endif

// VPin with push conditions (no GPIO, sensor data only)
VPin temperature = VPin(10).onChange().threshold(0.5);  // Push when changed AND delta >= 0.5C
VPin humidity = VPin(20).interval(10000);               // Push every 10 seconds
VPin heatIndex = VPin(30).onChange().interval(5000);    // Push when changed AND every 5 seconds

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

  // Push with conditions (returns true if actually pushed)
  if (temperature.push(temp)) {
    Serial.print("Pushed temperature: ");
    Serial.println(temp);
  }

  if (humidity.push(hum)) {
    Serial.print("Pushed humidity: ");
    Serial.println(hum);
  }

  if (heatIndex.push(hi)) {
    Serial.print("Pushed heatIndex: ");
    Serial.println(hi);
  }

  delay(2000);  // Read sensor every 2 seconds
}
