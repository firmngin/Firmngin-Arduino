/*
 * FirmnginKit Basic Example
 *
 * This example shows basic usage of DeviceState API with manual JSON parsing.
 * For mTLS authentication, create your own keys.h file with certificates.
 *
 * See README.md for complete setup instructions.
 */

#include <Arduino.h>
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

FirmnginKit fngin(DEVICE_ID, DEVICE_KEY);

void setupStates()
{
  // Payment received handler
  fngin.onState(PAYMENT, [](DeviceState state) {
    Serial.println("Payment success message received");
    
    // Get raw JSON payload
    String payload = state.getPayload();
    Serial.print("Raw payload: ");
    Serial.println(payload);
    
    // Manual JSON parsing
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
      Serial.print("JSON parse error: ");
      Serial.println(error.c_str());
      return;
    }
    
    // Access JSON fields (shortened keys from backend)
    if (doc.containsKey("it")) {
      Serial.print("Item: ");
      Serial.println(doc["it"].as<String>());
    }
    
    if (doc.containsKey("pc")) {
      Serial.print("Price: ");
      Serial.println(doc["pc"].as<String>());
    }
    
    if (doc.containsKey("oid")) {
      Serial.print("Order ID: ");
      Serial.println(doc["oid"].as<String>());
    }
  });

  // Device status handler
  fngin.onState(DEVICE_STATUS, [](DeviceState state) {
    Serial.println("Device status update received");
    String payload = state.getPayload();
    Serial.println(payload);
  });

  // Pending payment handler
  fngin.onState(PENDING_PAYMENT, [](DeviceState state) {
    Serial.println("Pending payment notification received");
    String payload = state.getPayload();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    Serial.print("  Item:  "); Serial.println(doc["it"].as<String>());
    Serial.print("  Price: "); Serial.println(doc["pc"].as<String>());
    Serial.print("  Order: "); Serial.println(doc["oid"].as<String>());
  });

  // Metadata on pending payments (raw JSON from menu_items.on_pending_payments)
  fngin.onState(METADATA_ON_PENDING, [](DeviceState state) {
    Serial.println("Metadata on pending payments received");
    Serial.println(state.getPayload());
  });

  // Metadata on success payments (raw JSON from menu_items.on_success_payments)
  fngin.onState(METADATA_ON_SUCCESS, [](DeviceState state) {
    Serial.println("Metadata on success payments received");
    Serial.println(state.getPayload());
  });

  // Metadata on expired payments (raw JSON from menu_items.on_expired_payments)
  fngin.onState(METADATA_ON_EXPIRED, [](DeviceState state) {
    Serial.println("Metadata on expired payments received");
    Serial.println(state.getPayload());
  });

  // Init response handler
  fngin.onState(INIT, [](DeviceState state) {
    Serial.println("Init response received");
    String payload = state.getPayload();
    Serial.println(payload);
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      if (doc.containsKey("m")) {
        Serial.print("Merchant status: ");
        Serial.println(doc["m"].as<String>());
      }
      if (doc.containsKey("vf")) {
        Serial.print("Verification flag: ");
        Serial.println(doc["vf"].as<int>());
      }
    }
  });

  // Display PIN handler
  fngin.onState(DISPLAY_PIN, [](DeviceState state) {
    Serial.println("Display PIN command received");
    String payload = state.getPayload();
    
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      if (doc.containsKey("pi")) {
        Serial.print("PIN: ");
        Serial.println(doc["pi"].as<String>());
      }
      if (doc.containsKey("si")) {
        Serial.print("Session ID: ");
        Serial.println(doc["si"].as<String>());
      }
      if (doc.containsKey("ttl")) {
        Serial.print("TTL: ");
        Serial.println(doc["ttl"].as<int>());
      }
    }
  });

  // Verification result handler
  fngin.onState(VERIFICATION_RESULT, [](DeviceState state) {
    Serial.println("Verification result received");
    String payload = state.getPayload();
    
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      bool pinMet = doc["pn"] | false;
      bool precondMet = doc["pr"] | false;
      Serial.print("PIN met: ");
      Serial.println(pinMet ? "true" : "false");
      Serial.print("Precondition met: ");
      Serial.println(precondMet ? "true" : "false");
    }
  });

  // Menu items handler
  fngin.onState(MENU_ITEMS, [](DeviceState state) {
    Serial.println("Menu items received");
    Serial.println(state.getPayload());
  });

  // Usage response handler
  fngin.onState(USAGE_RESPONSE, [](DeviceState state) {
    Serial.println("Usage response received");
    String payload = state.getPayload();
    
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      if (doc.containsKey("u")) {
        Serial.print("Used: ");
        Serial.println(doc["u"].as<int>());
      }
      if (doc.containsKey("l")) {
        Serial.print("Limit: ");
        Serial.println(doc["l"].as<int>());
      }
      if (doc.containsKey("pct")) {
        Serial.print("Percentage: ");
        Serial.println(doc["pct"].as<int>());
      }
    }
  });

  // Limit exceeded handler
  fngin.onState(LIMIT_EXCEEDED, [](DeviceState state) {
    Serial.println("LIMIT EXCEEDED!");
    Serial.println(state.getPayload());
  });

  // Near limit handler
  fngin.onState(NEAR_LIMIT, [](DeviceState state) {
    Serial.println("Near limit warning!");
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
