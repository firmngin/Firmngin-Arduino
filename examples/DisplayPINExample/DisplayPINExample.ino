/*
 * Firmngin Minimal PIN Example
 *
 * Demonstrates:
 * - Receiving and displaying a PIN (auto-parsed via Verifications)
 * - Receiving verification result (true / false)
 */

#include <Arduino.h>
#include <firmngin.h>

#include <WiFi.h>

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
