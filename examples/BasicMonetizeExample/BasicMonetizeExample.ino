/*
 * Firmngin Basic Monetize Example
 * 
 * website: https://firmngin.dev
 * author: (Arif) Firmngin.dev
 */

#include "keys.h" 
#include <firmngin.h>
#include <ArduinoJson.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

#define DEVICE_ID "YOUR_DEVICE_ID"
#define DEVICE_KEY "YOUR_DEVICE_SECRET_KEY"

// WiFi credentials
const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

Firmngin fngin(DEVICE_ID, DEVICE_KEY);

void setupStates()
{
  // Payment flow: handles both pending and success
  fngin.on(PAYMENTS, [](Payments &p) {
    if (p.isPending()) {
      Serial.println("Payment pending...");
    }
    if (p.isSuccess()) {
      Serial.println("Payment success!");
    }
    Serial.print("Item:  "); Serial.println(p.itemTitle());
    Serial.print("Price: "); Serial.println(p.price());
    Serial.print("Order: "); Serial.println(p.orderId());
  });

  // Device status handler
  fngin.on(DEVICE_STATUS, [](DeviceState state) {
    Serial.println("Device status received");
    Serial.println(state.getPayload());
  });

  // Pending payment handler (raw)
  fngin.on(PENDING_PAYMENT, [](DeviceState state) {
    Serial.println("Payment pending received");
    Serial.println(state.getPayload());
  });

  // Metadata on pending payments (raw JSON from menu_items.on_pending_payments)
  fngin.on(METADATA_ON_PENDING, [](DeviceState state) {
    Serial.println("On pending payments received");
    Serial.println(state.getPayload());
  });

  // Metadata on expired payments (raw JSON from menu_items.on_expired_payments)
  fngin.on(METADATA_ON_EXPIRED, [](DeviceState state) {
    Serial.println("On expired payments received");
    Serial.println(state.getPayload());
  });

  // Metadata on success payments (raw JSON from menu_items.on_success_payments)
  fngin.on(METADATA_ON_SUCCESS, [](DeviceState state) {
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
