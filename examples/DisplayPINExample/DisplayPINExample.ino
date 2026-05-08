/*
 * Firmngin Minimal PIN Example
 *
 * Demonstrates:
 * - Receiving and displaying a PIN (auto-parsed via Verifications)
 * - Receiving verification result (true / false)
 */

#include <Arduino.h>
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

Firmngin fngin(DEVICE_ID, DEVICE_KEY);

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Verification flow: single callback handles both dpin and vr
  fngin.on(VERIFICATIONS, [](Verifications &v) {
    if (v.hasPinNumber()) {
      Serial.println("========== PIN RECEIVED ==========");
      Serial.print("PIN:        ");
      Serial.println(v.pin());
      Serial.print("Session ID: ");
      Serial.println(v.sessionId());
      Serial.print("TTL:        ");
      Serial.print(v.ttl());
      Serial.println("s");
      Serial.println("==================================");
    }

    if (v.hasResult()) {
      Serial.println("---------- VERIFICATION ----------");
      Serial.print("PIN met:          ");
      Serial.println(v.pinMet() ? "true" : "false");
      Serial.print("Precondition met: ");
      Serial.println(v.preconditionMet() ? "true" : "false");
      Serial.println("----------------------------------");
    }
  });

  fngin.setDebug(true);
  fngin.setTimezone(7);
  fngin.begin();
}

void loop() {
  fngin.loop();
}
