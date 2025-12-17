/*
 * FirmnginKit Batch State Example
 *
 * Example using pushBatchState() to send multiple states at once
 *
 * website: https://firmngin.dev
 * author: (Arif) Firmngin.dev
 */

#include "keys.h"
#include "firmnginKit.h"

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

#if defined(ESP8266)
FirmnginKit fngin(DEVICE_ID, DEVICE_KEY, CLIENT_CERT, PRIVATE_KEY, SERVER_FINGERPRINT_BYTES);
#elif defined(ESP32)
FirmnginKit fngin(DEVICE_ID, DEVICE_KEY, SERVER_FINGERPRINT_BYTES, CLIENT_CERT, PRIVATE_KEY);
#endif

unsigned long lastBatchPush = 0;
const unsigned long batchInterval = 5000; // Push batch every 5 seconds

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

  // Push batch state every 5 seconds
  if (millis() - lastBatchPush >= batchInterval) {
    lastBatchPush = millis();

    // Simulate sensor readings
    int temperature = random(20, 30);
    int humidity = random(40, 80);
    int pressure = random(980, 1020);
    int light = analogRead(A0);
    String status = "ONLINE";

    bool sent = fngin.pushBatchState()
      .add(10, String(temperature))      // int key, string value
      .add(20, String(humidity))         // int key, string value
      .add("pressure", String(pressure)) // string key, string value
      .add("light", String(light))       // string key, string value
      .add("status", status)             // string key, string value
      .send();
    
    if (sent) {
      Serial.println("Batch state sent successfully!");
    }

    delay(1000);

    fngin.pushBatchState()
      .add(10, "ON")           // vpin 10
      .add(20, "25.5")          // vpin 20
      .add(30, "OFF")           // vpin 30
      .add(40, "100")           // vpin 40
      .send();

    delay(1000);

    fngin.pushBatchState()
      .add("gpio_1", "1")       // string key, string value
      .add("gpio_2", "0")       // string key, string value
      .add(50, 128)             // int key, int value (auto convert)
      .add(60, (double)25.5)     // int key, double value (explicit cast)
      .add("temp", temperature) // string key, int value (auto convert)
      .send();

    Serial.println("\nWaiting for next batch push...");
  }

  delay(100);
}
