/*
 * Firmngin Basic Example
 *
 * This example shows basic usage of DeviceState API with manual JSON parsing.
 * For mTLS authentication, create your own keys.h file with certificates.
 *
 * Setup:
 * 1. Generate keys.h from firmngin dashboard or cert-gen API
 * 2. Copy keys.h to this sketch folder and set DEVICE_ID / DEVICE_KEY
 * 3. Choose validation mode in keys.h:
 *    - #define USE_CA_CERT     (recommended for ESP32)
 *    - #define USE_FINGERPRINT (recommended for ESP8266)
 *
 * See README.md for complete setup instructions.
 */

#include <Arduino.h>
#include <firmngin.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

// WiFi credentials
const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

Firmngin fngin(DEVICE_ID, DEVICE_KEY);

// Entity objects as key references
Entity relay1(1);

const uint8_t RELAY1_PIN = 2;

// Register callbacks for entity objects
ON_ENTITY(relay1, [](EntityCommand &cmd) {
  digitalWrite(RELAY1_PIN, cmd.value() == "1" ? HIGH : LOW);
});

void setup()
{
  Serial.begin(115200);

  pinMode(RELAY1_PIN, OUTPUT);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Enable debug output
  fngin.setDebug(true);

#if defined(ESP8266)
  fngin.setFirmwareInfo("1.0.0", "ESP8266", "esp8266:esp8266:generic");
#else
  fngin.setFirmwareInfo("1.0.0", "ESP32", "esp32:esp32:esp32");
#endif

  // Set timezone (GMT+7 for Indonesia)
  fngin.setTimezone(7);
  
  // Initialize connection
  fngin.begin();
}

void loop()
{
  fngin.loop();
}
