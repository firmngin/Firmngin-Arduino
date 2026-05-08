/*
 * Firmngin Basic Example
 *
 * This example shows basic usage of DeviceState API with manual JSON parsing.
 * For mTLS authentication, create your own keys.h file with certificates.
 *
 * See README.md for complete setup instructions.
 */

#include <Arduino.h>
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

// Entity objects as key references
Entity relay1("gpio_1");
Entity relay2("relay_2");
Entity led("pwm_1");

const uint8_t RELAY1_PIN = 2;
const uint8_t RELAY2_PIN = 4;
const uint8_t PWM_PIN = 5;

// Register callbacks for entity objects
ON_ENTITY(relay1, [](EntityCommand &cmd) {
  digitalWrite(RELAY1_PIN, cmd.value() == "1" ? HIGH : LOW);
});

ON_ENTITY(relay2, [](EntityCommand &cmd) {
  digitalWrite(RELAY2_PIN, cmd.value() == "1" ? HIGH : LOW);
});

ON_ENTITY(led, [](EntityCommand &cmd) {
  analogWrite(PWM_PIN, cmd.value().toInt());
});

// Custom callback for complex logic (using string key)
ON_ENTITY_S("status", [](EntityCommand &cmd) {
  Serial.print("Status received: ");
  Serial.println(cmd.value());
});

void setupStates()
{
  // Payment flow — single callback handles both pending and success
  fngin.on(PAYMENTS, [](Payments &p) {
    if (p.isPending()) {
      Serial.println("---------- PAYMENT PENDING ----------");
    }
    if (p.isSuccess()) {
      Serial.println("========== PAYMENT SUCCESS ==========");
    }
    Serial.print("Item:  "); Serial.println(p.itemTitle());
    Serial.print("Price: "); Serial.println(p.price());
    Serial.print("Order: "); Serial.println(p.orderId());
    // Raw metadata available if needed
    // Serial.println(p.metadata());
  });

  // Device status handler — typed callback with boolean helpers
  fngin.on(DEVICE_STATUS, [](DeviceStates &ds) {
    if (ds.isIdle()) {
      Serial.println("Device is idle");
    }
    if (ds.isPendingPayment()) {
      Serial.println("Device is pending payment");
    }
    if (ds.isExpiredPayment()) {
      Serial.println("Device payment expired");
    }
    if (ds.isSuccessPayment()) {
      Serial.println("Device payment success");
    }
    if (ds.isMaintenance()) {
      Serial.println("Device is under maintenance");
    }
    if (ds.isOnActiveService()) {
      Serial.println("Device is on active service");
    }
    Serial.print("State: ");
    Serial.println(ds.state());
  });

  // Pending payment handler
  fngin.on(PENDING_PAYMENT, [](DeviceState state) {
    Serial.println("Pending payment notification received");
    String payload = state.getPayload();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    Serial.print("  Item:  "); Serial.println(doc["it"].as<String>());
    Serial.print("  Price: "); Serial.println(doc["pc"].as<String>());
    Serial.print("  Order: "); Serial.println(doc["oid"].as<String>());
  });

  // Metadata on pending payments (raw JSON from menu_items.on_pending_payments)
  fngin.on(METADATA_ON_PENDING, [](DeviceState state) {
    Serial.println("Metadata on pending payments received");
    Serial.println(state.getPayload());
  });

  // Metadata on success payments (raw JSON from menu_items.on_success_payments)
  fngin.on(METADATA_ON_SUCCESS, [](DeviceState state) {
    Serial.println("Metadata on success payments received");
    Serial.println(state.getPayload());
  });

  // Metadata on expired payments (raw JSON from menu_items.on_expired_payments)
  fngin.on(METADATA_ON_EXPIRED, [](DeviceState state) {
    Serial.println("Metadata on expired payments received");
    Serial.println(state.getPayload());
  });

  // Init response handler — typed callback with boolean helpers
  fngin.on(INIT, [](Inits &i) {
    Serial.println("Init response received");
    Serial.print("Entities: "); Serial.println(i.entities());  // raw JSON array
    
    if (i.isIdle()) {
      Serial.println("Merchant status: idle");
    }
    if (i.isPendingPayment()) {
      Serial.println("Merchant status: pending payment");
    }
    if (i.isOnActiveService()) {
      Serial.println("Merchant status: on active service");
    }
    
    Serial.print("Verification flag: "); Serial.println(i.verificationFlag());
    
    if (i.isPinEnabled()) {
      Serial.println("PIN verification enabled");
    }
    if (i.isPreconditionEnabled()) {
      Serial.println("Precondition verification enabled");
    }
    if (i.isVerificationRequired()) {
      Serial.println("Some form of verification is required");
    }
  });

  // Display PIN handler
  fngin.on(DISPLAY_PIN, [](DeviceState state) {
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
  fngin.on(VERIFICATION_RESULT, [](DeviceState state) {
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

  // Usage response handler
  fngin.on(USAGE_RESPONSE, [](DeviceState state) {
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
  fngin.on(LIMIT_EXCEEDED, [](DeviceState state) {
    Serial.println("LIMIT EXCEEDED!");
    Serial.println(state.getPayload());
  });

  // Near limit handler
  fngin.on(NEAR_LIMIT, [](DeviceState state) {
    Serial.println("Near limit warning!");
    Serial.println(state.getPayload());
  });

  // Entity command handler — backend sends commands to specific entities (e.g. gpio_1)
  fngin.on(ENTITIES, [](EntityCommand &cmd) {
    Serial.print("Entity command received — Key: ");
    Serial.print(cmd.key());
    Serial.print(" Value: ");
    Serial.println(cmd.value());

    // Control GPIO based on entity key
    if (cmd.key() == "gpio_1") {
      digitalWrite(RELAY1_PIN, cmd.value() == "1" ? HIGH : LOW);
    }
  });
}

void setup()
{
  Serial.begin(115200);

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(PWM_PIN, OUTPUT);

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

  // Example: periodically push sensor readings (every 10 seconds)
  static unsigned long lastPush = 0;
  if (millis() - lastPush > 10000) {
    lastPush = millis();
    fngin.pushEntity("temperature", "25.5");
    fngin.pushEntity("gpio_1", "1");
  }
}
