/*
 * Firmngin GPS Tracker Example
 *
 * Minimal GPS tracker sending lat/lon via pushLocation()
 *
 * Hardware: ESP32 + GPS module (NEO-6M, NEO-8M, etc.) on Serial2
 * Dependencies: TinyGPS++ by Mikal Hart (PlatformIO: mikalhart/TinyGPSPlus)
 *
 * website: https://firmngin.dev
 * author: Firmngin.dev
 */

#include "keys.h"
#include <firmngin.h>

#include <WiFi.h>
#define GPS_BAUD 9600
#define gpsSerial Serial2

#include <TinyGPS++.h>

// WiFi credentials
const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

TinyGPSPlus gps;

Firmngin fngin(DEVICE_ID, DEVICE_KEY);

unsigned long lastPublish = 0;
const unsigned long publishInterval = 10000;  // 10 seconds

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(GPS_BAUD);
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

  fngin.setDebug(true);
  fngin.setTimezone(7);

  fngin.setFirmwareInfo("0.0.0", "ESP32", "esp32:esp32:esp32");
  fngin.begin();

  Serial.println("Waiting for GPS...");
}

void loop() {
  fngin.loop();

  // Feed GPS data
  while (gpsSerial.available())
    gps.encode(gpsSerial.read());

  // Publish location every publishInterval
  if (millis() - lastPublish >= publishInterval) {
    lastPublish = millis();

    if (gps.location.isValid()) {
      fngin.pushLocation()
        .lat(gps.location.lat())
        .lon(gps.location.lng());

      Serial.print("Location sent: ");
      Serial.print(gps.location.lat(), 6);
      Serial.print(", ");
      Serial.println(gps.location.lng(), 6);
    } else {
      Serial.print("No GPS fix. Satellites: ");
      Serial.println(gps.satellites.value());
    }
  }
}
