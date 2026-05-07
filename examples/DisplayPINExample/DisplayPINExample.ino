/*
 * FirmnginKit Display PIN & Verification Example
 *
 * This example demonstrates:
 * - Handling PIN display from backend (/c/{id}/dpin)
 * - Verification result handling (/c/{id}/vr)
 * - Device initialization (/c/{id}/init)
 * - Countdown timer for PIN TTL
 *
 * Hardware requirements:
 * - ESP8266 or ESP32
 * - OLED/LCD display (optional, output also sent to Serial)
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

const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

FirmnginKit fngin(DEVICE_ID, DEVICE_KEY);

// PIN display state
struct PinDisplay {
  bool active = false;
  String pin = "";
  String sessionId = "";
  unsigned long expiresAt = 0;
  unsigned long ttlSeconds = 0;
} pinDisplay;

// Verification state
struct VerificationState {
  bool pinMet = false;
  bool preconditionMet = false;
} verification;

void clearPinDisplay() {
  pinDisplay.active = false;
  pinDisplay.pin = "";
  pinDisplay.sessionId = "";
  pinDisplay.expiresAt = 0;
  pinDisplay.ttlSeconds = 0;
  Serial.println("[PIN] Cleared / Expired");
  // TODO: Clear your physical display here
}

void setupHandlers() {
  // Init response — get initial configuration
  fngin.onState(INIT, [](DeviceState state) {
    Serial.println("[INIT] Device initialized");
    String payload = state.getPayload();

    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      Serial.print("[INIT] JSON error: ");
      Serial.println(error.c_str());
      return;
    }

    if (doc.containsKey("m")) {
      Serial.print("[INIT] Merchant status: ");
      Serial.println(doc["m"].as<String>());
    }

    if (doc.containsKey("vf")) {
      int vf = doc["vf"].as<int>();
      Serial.print("[INIT] Verification mode: ");
      Serial.println(vf);
      // vf: 0=none, 1=PIN only, 2=precondition only, 3=both
    }

    if (doc.containsKey("e")) {
      JsonArray entities = doc["e"];
      Serial.print("[INIT] Entities: ");
      Serial.println(entities.size());
      for (JsonObject entity : entities) {
        Serial.print("  GPIO ");
        Serial.print(entity["p"].as<int>());
        Serial.print(" = ");
        Serial.println(entity["v"].as<String>());
      }
    }
  });

  // Display PIN — show PIN on screen for guest verification
  fngin.onState(DISPLAY_PIN, [](DeviceState state) {
    String payload = state.getPayload();

    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      Serial.print("[PIN] JSON error: ");
      Serial.println(error.c_str());
      return;
    }

    pinDisplay.pin = doc["pi"] | "";
    pinDisplay.sessionId = doc["si"] | "";
    pinDisplay.ttlSeconds = doc["ttl"] | 0;
    pinDisplay.active = pinDisplay.pin.length() > 0;

    // Parse expiration time if available
    const char* ea = doc["ea"] | "";
    if (strlen(ea) > 0) {
      // Simple ISO-8601 parse: assume format "2026-05-05T10:01:00Z"
      // For production, use a proper time library
      Serial.print("[PIN] Expires at: ");
      Serial.println(ea);
    }

    if (pinDisplay.active) {
      pinDisplay.expiresAt = millis() + (pinDisplay.ttlSeconds * 1000);

      Serial.println("========================================");
      Serial.println("           PIN VERIFICATION             ");
      Serial.println("========================================");
      Serial.print("PIN:        ");
      Serial.println(pinDisplay.pin);
      Serial.print("Session ID: ");
      Serial.println(pinDisplay.sessionId);
      Serial.print("TTL:        ");
      Serial.print(pinDisplay.ttlSeconds);
      Serial.println(" seconds");
      Serial.println("========================================");

      // TODO: Display on your LCD/OLED here
      // Example (pseudo-code):
      // display.clear();
      // display.println("PIN:");
      // display.setLargeFont();
      // display.println(pinDisplay.pin);
      // display.setSmallFont();
      // display.print("SID: ");
      // display.println(pinDisplay.sessionId);
    }
  });

  // Verification Result — backend tells us if PIN/precondition passed
  fngin.onState(VERIFICATION_RESULT, [](DeviceState state) {
    String payload = state.getPayload();

    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      Serial.print("[VR] JSON error: ");
      Serial.println(error.c_str());
      return;
    }

    verification.pinMet = doc["pn"] | false;
    verification.preconditionMet = doc["pr"] | false;

    Serial.println("----------------------------------------");
    Serial.println("       VERIFICATION RESULT              ");
    Serial.println("----------------------------------------");
    Serial.print("PIN met:           ");
    Serial.println(verification.pinMet ? "YES" : "NO");
    Serial.print("Precondition met:  ");
    Serial.println(verification.preconditionMet ? "YES" : "NO");

    if (verification.pinMet && verification.preconditionMet) {
      Serial.println("STATUS: READY TO OPERATE");
      // TODO: Unlock device, enable controls, start service
    } else if (verification.pinMet && !verification.preconditionMet) {
      Serial.println("STATUS: PIN OK, waiting for precondition...");
      // TODO: Show "processing" or "waiting" on display
    } else {
      Serial.println("STATUS: VERIFICATION FAILED");
      // TODO: Show error, lock controls
    }
    Serial.println("----------------------------------------");
  });

  // Device state changes
  fngin.onState(DEVICE_STATUS, [](DeviceState state) {
    String payload = state.getPayload();
    DynamicJsonDocument doc(256);
    deserializeJson(doc, payload);
    Serial.print("[DS] State changed to: ");
    Serial.println(doc["s"] | "unknown");
  });

  // Payment success
  fngin.onState(PAYMENT, [](DeviceState state) {
    String payload = state.getPayload();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    Serial.println("[PM] Payment received!");
    Serial.print("  Item:  "); Serial.println(doc["it"].as<String>());
    Serial.print("  Price: "); Serial.println(doc["pc"].as<String>());
    Serial.print("  Order: "); Serial.println(doc["oid"].as<String>());
  });

  // Usage / quota warnings
  fngin.onState(USAGE_RESPONSE, [](DeviceState state) {
    DynamicJsonDocument doc(512);
    deserializeJson(doc, state.getPayload());
    Serial.print("[UR] Usage: ");
    Serial.print(doc["u"].as<int>());
    Serial.print(" / ");
    Serial.print(doc["l"].as<int>());
    Serial.print(" (");
    Serial.print(doc["pct"].as<int>());
    Serial.println("%)");
  });

  fngin.onState(NEAR_LIMIT, [](DeviceState state) {
    Serial.println("[NL] WARNING: Approaching MQTT quota limit!");
  });

  fngin.onState(LIMIT_EXCEEDED, [](DeviceState state) {
    Serial.println("[LE] ERROR: MQTT quota exceeded!");
  });
}

void setup() {
  Serial.begin(115200);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  setupHandlers();

  fngin.setDebug(true);
  fngin.setTimezone(7);
  fngin.begin();
}

void loop() {
  fngin.loop();

  // Handle PIN countdown
  if (pinDisplay.active && millis() > pinDisplay.expiresAt) {
    clearPinDisplay();
  }

  // Optional: update countdown display every second
  static unsigned long lastCountdownUpdate = 0;
  if (pinDisplay.active && millis() - lastCountdownUpdate > 1000) {
    lastCountdownUpdate = millis();
    unsigned long remaining = (pinDisplay.expiresAt - millis()) / 1000;
    if (remaining <= 10) {
      Serial.print("[PIN] Expires in: ");
      Serial.print(remaining);
      Serial.println("s");
    }
    // TODO: Update countdown on physical display
  }
}
