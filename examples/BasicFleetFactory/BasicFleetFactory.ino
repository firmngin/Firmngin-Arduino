/*
 * Firmngin Basic Fleet Factory Example
 *
 * Generic firmware for mass production (fleet / factory line).
 * One shared .merged.bin for every unit — no keys.h at compile time.
 * Device identity, service trust, and API/OTA endpoints are injected into
 * flash through the firmngin-factory provisioning flow.
 *
 * Flow:
 *   1. Bulk create batch in dashboard
 *   2. Export a complete .merged.bin (FIRMNGIN_FACTORY_SERIAL=1)
 *   3. Upload as Master Fleet Firmware
 *   4. Flash + provision each unit via firmngin-factory CLI/TUI
 *   5. After online, fleet OTA from Firmware Management (not master fleet)
 *
 * website: https://firmngin.dev
 * author: (Arif) Firmngin.dev
 */

#include <Arduino.h>
#define FIRMNGIN_FACTORY_SERIAL 1
#include <firmngin.h>
#include "firmngin_identity.h"

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

const char *ssid     = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

// Empty device ID and device key for factory mode
Firmngin fngin("", "");

void setup()
{
  Serial.begin(115200);

#if defined(FIRMNGIN_FACTORY_SERIAL) && (FIRMNGIN_FACTORY_SERIAL != 0)
  // Not provisioned yet — skip WiFi/cloud; fngin.loop() listens for serial provision
  if (!FirmnginIdentity::isProvisioned())
  {
    Serial.println();
    Serial.println("Factory mode: waiting for serial provision");
    Serial.println("Run: firmngin-factory CLI to provision the unit");
    Serial.println();
    return;
  }
#endif

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
    delay(500);
  Serial.println("WiFi connected");

  fngin.setTimezone(7);

#if defined(ESP8266)
  fngin.setFirmwareInfo("0.0.0", "ESP8266", "esp8266:esp8266:generic");
#elif defined(ESP32)
  fngin.setFirmwareInfo("0.0.0", "ESP32", "esp32:esp32:esp32");
#endif

  fngin.onOTAStatus([](const char *status, const char *message) {
    Serial.print("[OTA] ");
    Serial.print(status);
    Serial.print(": ");
    Serial.println(message);
  });

  fngin.begin();
}

void loop()
{
  fngin.loop();
}
