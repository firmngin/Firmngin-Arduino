/*
 * FirmnginKit Push State Example
 *
 * Example using pushState() and VPin to send data to server
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

// Method 1: Direct pushState (manual)
unsigned long lastManualPush = 0;

// Method 2: VPin with smart conditions
VPin sensorA = VPin(10).onChange();       // Push only when value changes
VPin sensorB = VPin(20).interval(5000);   // Push every 5 seconds
VPin sensorC = VPin(30).threshold(10);    // Push when change >= 10

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
  fngin.pushState(10, "READY");
  Serial.println("Initial status sent: READY");
}

void loop()
{
  fngin.loop();

  // Simulate sensor readings
  int valueA = analogRead(A0);
  int valueB = random(0, 100);
  int valueC = random(0, 255);

  // Method 2: VPin auto handles conditions
  if (sensorA.push(valueA)) {
    Serial.print("sensorA pushed (onChange): ");
    Serial.println(valueA);
  }

  if (sensorB.push(valueB)) {
    Serial.print("sensorB pushed (interval): ");
    Serial.println(valueB);
  }

  if (sensorC.push(valueC)) {
    Serial.print("sensorC pushed (threshold): ");
    Serial.println(valueC);
  }

  // Method 1: Direct pushState (every 10 seconds)
  if (millis() - lastManualPush >= 10000) {
    lastManualPush = millis();
    fngin.pushState("uptime", String(millis() / 1000));
    Serial.println("Manual push: uptime");
  }

  delay(500);
}
