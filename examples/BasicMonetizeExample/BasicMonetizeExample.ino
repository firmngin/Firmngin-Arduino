/*
 * FirmnginKit Basic Example
 * 
 * website: https://firmngin.dev
 * author: (Arif) Firmngin.dev
 */

#include "keys.h" 
#include "firmnginKit.h"
#include <ArduinoJson.h>

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
// Choose one of these options:
// Option 1: CA Certificate verification (recommended for production)
// FirmnginKit fngin(DEVICE_ID, DEVICE_KEY, CA_CERT, CLIENT_CERT, PRIVATE_KEY);

// Option 2: Fingerprint verification (ESP8266-style, limited support on ESP32)
// FirmnginKit fngin(DEVICE_ID, DEVICE_KEY, SERVER_FINGERPRINT_BYTES, CLIENT_CERT, PRIVATE_KEY);

// For now, using fingerprint option since CA certificate is causing issues
FirmnginKit fngin(DEVICE_ID, DEVICE_KEY, SERVER_FINGERPRINT_BYTES, CLIENT_CERT, PRIVATE_KEY);
#endif

void setupStates()
{
  // Payment success handler
  fngin.onStateMonetize(PAYMENT_SUCCESS, [](DeviceState state) {
    Serial.println("Payment success received");
    String payload = state.getPayload();
    Serial.println(payload);
  });

  // Device status handler
  fngin.onStateMonetize(DEVICE_STATUS, [](DeviceState state) {
    Serial.println("Device status received");
    Serial.println(state.getPayload());
  });

  // Payment pending handler
  fngin.onStateMonetize(PAYMENT_PENDING, [](DeviceState state) {
    Serial.println("Payment pending received");
    Serial.println(state.getPayload());
  });

  // On pending payments handler
  fngin.onStateMonetize(CUSTOM_ON_PENDING_PAYMENTS, [](DeviceState state) {
    Serial.println("On pending payments received");
    Serial.println(state.getPayload());
  });

  // On expired payments handler
  fngin.onStateMonetize(CUSTOM_ON_EXPIRED_PAYMENTS, [](DeviceState state) {
    Serial.println("On expired payments received");
    Serial.println(state.getPayload());
  });

  // On success payments handler
  fngin.onStateMonetize(CUSTOM_ON_SUCCESS_PAYMENTS, [](DeviceState state) {
    Serial.println("On success payments received");
    Serial.println(state.getPayload());
  });
}

void setup()
{
  Serial.begin(115200);

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

  // Setup state handlers BEFORE begin()
  setupStates();

  // Enable debug output
  fngin.setDebug(true);

  // Set timezone (GMT+7 for Indonesia)
  fngin.setTimezone(7);
  
  // Initialize connection
  fngin.begin();
}

void loop()
{
  fngin.loop();
}
