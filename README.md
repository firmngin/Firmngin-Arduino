# Firmngin Arduino Library

<p align="center">
  <img src="https://github.com/firmngin/Firmngin-Arduino/blob/main/logo.png?raw=true" alt="Firmngin" width="96">
</p>

IoT Library for the Firmngin Platform. Enables ESP8266/ESP32 devices to accept payments, receive commands, and communicate securely with mTLS support.

Check out [firmngin.dev](https://firmngin.dev) for more information.

## Features

- **ESP8266 & ESP32** support
- **Event-driven API** using `on()` callbacks: raw or typed
- **Typed objects** for common flows: `Verifications`, `Payments`, `Usages`, `DeviceStates`, `Inits`, `EntityCommand`
- **mTLS authentication** via `keys.h` (optional but recommended)

## Installation

### PlatformIO

Add to your `platformio.ini`:

```ini
lib_deps =
    bblanchon/ArduinoJson @ ^7.4.3
    knolleary/PubSubClient @ ^2.8.0
```

> We use these excellent 3rd party libraries to power Firmngin Arduino Library. Make sure they are installed in your project.

Copy `src/firmngin.h` and `src/firmngin.cpp` to your project.

### Arduino IDE

1. Install dependencies via Library Manager:
   - **ArduinoJson** by Benoit Blanchon
   - **PubSubClient** by Nick O'Leary
2. Install the library, then add your optional `keys.h` file next to your sketch if you use mTLS.

## Quick Start

```cpp
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

Entity relay1("gpio_1");
Entity temperature("temperature");

ON_ENTITY(relay1, [](EntityCommand &cmd) {
  digitalWrite(2, cmd.value() == "1" ? HIGH : LOW);
});

ON_ENTITY_S("status", [](EntityCommand &cmd) {
  Serial.println(cmd.value());
});

void setup() {
  Serial.begin(115200);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  pinMode(2, OUTPUT);

  fngin.setDebug(true);
  fngin.begin();
}

void loop() {
  fngin.loop();

  // Push entity state every 10 seconds
  static unsigned long lastPush = 0;
  if (millis() - lastPush > 10000) {
    lastPush = millis();
    fngin.pushEntity(temperature, "25.5");
  }
}
```

## mTLS Setup (`keys.h`)

Create a `keys.h` file next to your sketch (do **not** commit this file).

You can download your device's `keys.h` directly from the **Firmngin Dashboard** → Devices → your device → Download `keys.h`.

### Server Validation Modes

Choose how the device validates the MQTT broker's identity:

| Mode | Define | Best For | Pros | Cons |
|------|--------|----------|------|------|
| **CA Certificate** | `USE_CA_CERT` | ESP32, Production | Full chain validation, auto-renew safe | Uses more memory |
| **Fingerprint** | `USE_FINGERPRINT` | ESP8266, Prototypes | Lightweight, memory efficient | Must update if server cert changes |

If you want the server to renew freely without touching the client, use `USE_CA_CERT` and keep the Root CA stable. Only fingerprint mode requires client regeneration when the server certificate changes.

### Using `keys.h` Template

```cpp
#ifndef KEYS_H
#define KEYS_H

// Choose ONE validation mode:
// #define USE_CA_CERT       // Recommended for ESP32
// #define USE_FINGERPRINT   // Recommended for ESP8266

// Optional. If omitted, the library uses asia-jkt1.firmngin.dev:8883.
// #define FIRMNGIN_SERVER_ADDR "asia-jkt1.firmngin.dev"
// #define FIRMNGIN_SERVER_PORT 8883

static const char CA_CERT[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
[Your CA Certificate]
-----END CERTIFICATE-----
)EOF";

static const char CLIENT_CERT[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
[Your Client Certificate]
-----END CERTIFICATE-----
)EOF";

static const char PRIVATE_KEY[] PROGMEM = R"EOF(
-----BEGIN PRIVATE KEY-----
[Your Private Key]
-----END PRIVATE KEY-----
)EOF";

static const uint8_t SERVER_FINGERPRINT_BYTES[20] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

#endif
```

A template is available in `src/keys.h.template`.

## Available States

Register handlers using the enum constants. Use `on(ENUM, callback)` for raw callbacks or typed objects for automatic parsing.

| Enum                  | Description                                                    |
| --------------------- | -------------------------------------------------------------- |
| `PAYMENT`             | Payment successfully settled                                   |
| `DEVICE_STATUS`       | Device state changed                                           |
| `PENDING_PAYMENT`     | Invoice created, waiting for payment                           |
| `METADATA_ON_PENDING` | Custom metadata for pending payments (raw JSON)                |
| `METADATA_ON_EXPIRED` | Custom metadata for expired payments (raw JSON)                |
| `METADATA_ON_SUCCESS` | Custom metadata for successful payments (raw JSON)             |
| `INIT`                | Initial configuration after connection                         |
| `DISPLAY_PIN`         | Display PIN on screen for guest verification                   |
| `VERIFICATION_RESULT` | PIN or precondition check result                               |
| `VERIFICATIONS`       | Typed callback: handles PIN display and verification result   |
| `PAYMENTS`            | Typed callback: handles pending and success payments          |
| `USAGES`              | Typed callback: handles usage, limit exceeded, and near limit |
| `USAGE_RESPONSE`      | Current MQTT usage and quota                                   |
| `LIMIT_EXCEEDED`      | Quota limit reached                                            |
| `NEAR_LIMIT`          | Approaching quota limit                                        |
| `ENTITIES`            | Received commands to specific entities (e.g. gpio_1)           |

### Entity Commands

Register **one callback** to receive commands sent to specific entities (e.g. `gpio_1`, `relay_2`). Use the provided key to route commands to the correct GPIO or component in your code:

```cpp
fngin.on(ENTITIES, [](EntityCommand &cmd) {
    Serial.print("Key:   "); Serial.println(cmd.key());
    Serial.print("Value: "); Serial.println(cmd.value());

    // Control GPIO based on entity key
    if (cmd.key() == "gpio_1") {
        digitalWrite(LED_BUILTIN, cmd.value() == "1" ? HIGH : LOW);
    }
});
```

#### Per-Entity Callbacks

Register callbacks for individual entities using `onEntity()`. The library will route commands to the correct handler automatically:

```cpp
fngin.onEntity("gpio_1", [](EntityCommand &cmd) {
    digitalWrite(LED_BUILTIN, cmd.value() == "1" ? HIGH : LOW);
});

fngin.onEntity("relay_2", [](EntityCommand &cmd) {
    digitalWrite(RELAY_PIN, cmd.value() == "1" ? HIGH : LOW);
});
```

#### Entity Class

Use the `Entity` class as a typed key reference for push and receive operations:

```cpp
// Define entities (key only, no GPIO logic)
Entity relay1("gpio_1");                    // entity "gpio_1"
Entity relay2("relay_2");                   // entity "relay_2"

// Register callbacks for entity objects
ON_ENTITY(relay1, [](EntityCommand &cmd) {
    digitalWrite(2, cmd.value() == "1" ? HIGH : LOW);
});

ON_ENTITY(relay2, [](EntityCommand &cmd) {
    digitalWrite(4, cmd.value() == "1" ? HIGH : LOW);
});

// Or register by string key directly
ON_ENTITY_S("status", [](EntityCommand &cmd) {
    Serial.print("Status received: ");
    Serial.println(cmd.value());
});
```

### Pushing The State

Send entity state updates from your device using `pushEntity()` or `updateEntities()`. Use string keys or `Entity` objects as keys:

```cpp
// Send a single state update (string key)
fngin.pushEntity("gpio_1", "1");
fngin.pushEntity("temperature", "25.5");

// Or use Entity object as key
Entity relay1("gpio_1");
fngin.pushEntity(relay1, "1");

Entity sensor("temperature");
fngin.pushEntity(sensor, "25.5");

// Send batch state updates with builder pattern
bool sent = fngin.pushBatchEntities()
    .add(10, String(temperature))      // int key, string value
    .add(20, String(humidity))         // int key, string value
    .add("pressure", String(pressure)) // string key, string value
    .add("light", String(light))       // string key, string value
    .add("status", status)             // string key, string value
    .send();

if (sent) {
    Serial.println("Batch state sent successfully!");
}

// Or send raw JSON array directly
fngin.updateEntities("[{\"key\":\"gpio_1\",\"value\":\"1\"},{\"key\":\"gpio_2\",\"value\":\"0\"}]");
```

### Request Init

Manually request initial configuration after connection:

```cpp
fngin.requestInit();
```

---

## Monetization Flow

The following flows are used when your device is integrated with Firmngin's payment and verification system.

### Verification Flow

Register **one callback** for the entire verification flow. The same `Verifications` object handles both PIN display and verification result:

```cpp
fngin.on(VERIFICATIONS, [](Verifications &v) {
    if (v.hasPinNumber()) {
        Serial.println(v.pin());        // "123456"
        Serial.println(v.sessionId());  // "abc..."
        Serial.println(v.ttl());        // 60
    }

    if (v.hasResult()) {
        Serial.println(v.pinMet());           // true / false
        Serial.println(v.preconditionMet());  // true / false
    }
});
```

### Payment Flow

Register **one callback** for the entire payment flow. The same `Payments` object handles both pending and success:

```cpp
fngin.on(PAYMENTS, [](Payments &p) {
    if (p.isPending()) {
        Serial.println("Payment pending...");
    }

    if (p.isSuccess()) {
        Serial.println("Payment received!");
    }

    Serial.println(p.itemTitle());   // "Cappuccino"
    Serial.println(p.price());       // "45000"
    Serial.println(p.orderId());     // "ODR-260506-12345678"
    Serial.println(p.metadata());    // raw JSON string
});
```

### Usage Flow

Register **one callback** for the entire usage flow. The same `Usages` object handles usage response, limit exceeded, and near limit:

```cpp
fngin.on(USAGES, [](Usages &u) {
    if (u.isNormal()) {
        Serial.println("Usage normal");
    }
    if (u.isNearLimit()) {
        Serial.println("WARNING: Near limit!");
    }
    if (u.isLimitExceeded()) {
        Serial.println("ERROR: Limit exceeded!");
    }

    Serial.print("Used:       "); Serial.println(u.used());
    Serial.print("Limit:      "); Serial.println(u.limit());
    Serial.print("Remaining:  "); Serial.println(u.remaining());
    Serial.print("Percentage: "); Serial.println(u.percentage());
    Serial.print("Reset at:   "); Serial.println(u.resetAt());
    Serial.print("Granularity:"); Serial.println(u.granularity());
});
```

### Device State

Register **one callback** for device state changes. The `DeviceStates` object provides boolean helpers for each state:

```cpp
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

    Serial.print("State: "); Serial.println(ds.state());
});
```

> The `DeviceStates` object is automatically populated when a `ds` message arrives. Use the boolean helpers to check the current state.

### Init Flow

Register **one callback** for device initialization. The `Inits` object provides access to entities, merchant status, and verification flags:

```cpp
fngin.on(INIT, [](Inits &i) {
    Serial.print("Entities: "); Serial.println(i.entities());  // raw JSON array

    if (i.isIdle()) {
        Serial.println("Merchant status: idle");
    }
    if (i.isPendingPayment()) {
        Serial.println("Merchant status: pending payment");
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
```

> The `Inits` object is automatically populated when an `init` message arrives. Use `entities()` to get the raw JSON array of GPIO entities. Use the boolean helpers to check merchant status and verification configuration.

## Development Workflow

```bash
# Edit library in src/
# Sync to example
python3 sync_lib.py

# Build example
pio run -e esp32dev
```

## API Reference

### Core Methods

| Function                      | Description                                                      | Return Type  | Possible Values                   |
| ----------------------------- | ---------------------------------------------------------------- | ------------ | --------------------------------- |
| `begin()`                     | Initialize library, sync time, setup mTLS, and connect to server | `void`       | N/A                               |
| `loop()`                      | Maintain connection and process incoming messages                | `void`       | N/A                               |
| `setDebug(bool)`              | Enable/disable debug output to Serial                            | `void`       | `true`, `false`                   |
| `setTimezone(int)`            | Set timezone offset from GMT                                     | `void`       | `-12` to `12`                     |
| `setDaylightOffsetSec(int)`   | Set daylight saving offset in seconds                            | `void`       | Any integer                       |
| `setNtpServer(const char*)`   | Set NTP server address                                           | `void`       | e.g. `"pool.ntp.org"`             |
| `setClient(Client&)`          | Use an external network client                                   | `void`       | EthernetClient, other Client impl |
| `isPlatformSupported()`       | Check if current board is supported                              | `bool`       | `true` (ESP8266/ESP32), `false`   |
| `on(STATE, callback)`         | Register a raw callback for any state                            | `void`       | See enum constants                |
| `on(VERIFICATIONS, callback)` | Register typed callback for dpin + vr                            | `void`       | `Verifications &`                 |
| `on(PAYMENTS, callback)`      | Register typed callback for pp + pm                              | `void`       | `Payments &`                      |
| `on(USAGES, callback)`        | Register typed callback for ur + le + nl                         | `void`       | `Usages &`                        |
| `on(DEVICE_STATUS, callback)` | Register typed callback for ds                                   | `void`       | `DeviceStates &`                  |
| `on(INIT, callback)`          | Register typed callback for init                                 | `void`       | `Inits &`                         |
| `on(ENTITIES, callback)`      | Register typed callback for entity commands                      | `void`       | `EntityCommand &`                 |
| `onEntity(key, callback)`     | Register callback for a specific entity key (string or object)   | `void`       | `EntityCommand &`                 |
| `pushEntity(key, value)`      | Send a single entity state update                                | `bool`       | `true` = sent, `false` = fail     |
| `pushEntity(entity, value)`   | Send entity state using Entity object as key                     | `bool`       | `true` = sent, `false` = fail     |
| `updateEntities(json)`        | Send multiple entity states as JSON array                        | `bool`       | `true` = sent, `false` = fail     |
| `pushBatchEntities()`         | Start batch state builder (chain `.add().send()`)                | `BatchState` | Builder object                    |
| `requestInit()`               | Request initial configuration                                    | `bool`       | `true` = sent, `false` = fail     |

### Verifications Object

| Function            | Description                              | Return Type | Possible Values  |
| ------------------- | ---------------------------------------- | ----------- | ---------------- |
| `isValid()`         | Check if payload was parsed successfully | `bool`      | `true`, `false`  |
| `hasPinNumber()`    | True if this is a dpin message           | `bool`      | `true`, `false`  |
| `hasResult()`       | True if this is a vr message             | `bool`      | `true`, `false`  |
| `pin()`             | 6-digit PIN code                         | `String`    | e.g. `"123456"`  |
| `sessionId()`       | Masked session ID                        | `String`    | e.g. `"abc..."`  |
| `ttl()`             | PIN validity in seconds                  | `int`       | e.g. `60`        |
| `pinMet()`          | PIN verification passed                  | `bool`      | `true`, `false`  |
| `preconditionMet()` | Precondition check passed                | `bool`      | `true`, `false`  |
| `metadata()`        | Raw JSON payload                         | `String`    | Full JSON string |

### Payments Object

| Function      | Description                                    | Return Type | Possible Values              |
| ------------- | ---------------------------------------------- | ----------- | ---------------------------- |
| `isValid()`   | Check if payload was parsed successfully       | `bool`      | `true`, `false`              |
| `isPending()` | True if this is a pp (pending payment) message | `bool`      | `true`, `false`              |
| `isSuccess()` | True if this is a pm (payment success) message | `bool`      | `true`, `false`              |
| `itemTitle()` | Menu item title                                | `String`    | e.g. `"Cappuccino"`          |
| `price()`     | Price as string                                | `String`    | e.g. `"45000"`               |
| `orderId()`   | Human-readable order ID                        | `String`    | e.g. `"ODR-260506-12345678"` |
| `metadata()`  | Raw JSON payload                               | `String`    | Full JSON string             |

### Usages Object

| Function            | Description                              | Return Type | Possible Values                          |
| ------------------- | ---------------------------------------- | ----------- | ---------------------------------------- |
| `isValid()`         | Check if payload was parsed successfully | `bool`      | `true`, `false`                          |
| `isNormal()`        | Usage is within normal range             | `bool`      | `true`, `false`                          |
| `isNearLimit()`     | Quota is nearly exceeded (>80%)          | `bool`      | `true`, `false`                          |
| `isLimitExceeded()` | Quota limit reached                      | `bool`      | `true`, `false`                          |
| `used()`            | Messages used in current window          | `int`       | e.g. `1250`                              |
| `limit()`           | Plan limit for current window            | `int`       | e.g. `30000`                             |
| `remaining()`       | Messages remaining                       | `int`       | e.g. `28750`                             |
| `percentage()`      | Usage percentage (0-100)                 | `int`       | e.g. `4`                                 |
| `resetAt()`         | ISO-8601 window reset time               | `String`    | e.g. `"2026-05-06T00:00:00Z"`            |
| `granularity()`     | Window granularity                       | `String`    | `"minute"`, `"hour"`, `"day"`, `"month"` |
| `metadata()`        | Raw JSON payload                         | `String`    | Full JSON string                         |

### DeviceStates Object

| Function              | Description                              | Return Type | Possible Values                                                                                                 |
| --------------------- | ---------------------------------------- | ----------- | --------------------------------------------------------------------------------------------------------------- |
| `isValid()`           | Check if payload was parsed successfully | `bool`      | `true`, `false`                                                                                                 |
| `state()`             | Raw state string                         | `String`    | `"idle"`, `"pending_payment"`, `"expired_payment"`, `"success_payment"`, `"maintenance"`, `"on_active_service"` |
| `isIdle()`            | Device is idle                           | `bool`      | `true`, `false`                                                                                                 |
| `isPendingPayment()`  | Device is pending payment                | `bool`      | `true`, `false`                                                                                                 |
| `isExpiredPayment()`  | Device payment expired                   | `bool`      | `true`, `false`                                                                                                 |
| `isSuccessPayment()`  | Device payment success                   | `bool`      | `true`, `false`                                                                                                 |
| `isMaintenance()`     | Device is under maintenance              | `bool`      | `true`, `false`                                                                                                 |
| `isOnActiveService()` | Device is on active service              | `bool`      | `true`, `false`                                                                                                 |
| `metadata()`          | Raw JSON payload                         | `String`    | Full JSON string                                                                                                |

### Inits Object

| Function                   | Description                              | Return Type | Possible Values                                                                                                 |
| -------------------------- | ---------------------------------------- | ----------- | --------------------------------------------------------------------------------------------------------------- |
| `isValid()`                | Check if payload was parsed successfully | `bool`      | `true`, `false`                                                                                                 |
| `entities()`               | Raw JSON array of GPIO entities          | `String`    | e.g. `[{"p":1,"v":"0"}]`                                                                                        |
| `merchantStatus()`         | Current merchant status                  | `String`    | `"idle"`, `"pending_payment"`, `"expired_payment"`, `"success_payment"`, `"maintenance"`, `"on_active_service"` |
| `verificationFlag()`       | Verification mode flag                   | `int`       | `0`, `1`, `2`, `3`                                                                                              |
| `isIdle()`                 | Merchant status is idle                  | `bool`      | `true`, `false`                                                                                                 |
| `isPendingPayment()`       | Merchant status is pending payment       | `bool`      | `true`, `false`                                                                                                 |
| `isExpiredPayment()`       | Merchant status is expired payment       | `bool`      | `true`, `false`                                                                                                 |
| `isSuccessPayment()`       | Merchant status is success payment       | `bool`      | `true`, `false`                                                                                                 |
| `isMaintenance()`          | Merchant status is maintenance           | `bool`      | `true`, `false`                                                                                                 |
| `isOnActiveService()`      | Merchant status is on active service     | `bool`      | `true`, `false`                                                                                                 |
| `isPinEnabled()`           | PIN verification is enabled              | `bool`      | `true`, `false`                                                                                                 |
| `isPreconditionEnabled()`  | Precondition verification is enabled     | `bool`      | `true`, `false`                                                                                                 |
| `isVerificationRequired()` | Some form of verification is required    | `bool`      | `true`, `false`                                                                                                 |
| `metadata()`               | Raw JSON payload                         | `String`    | Full JSON string                                                                                                |

### EntityCommand Object

| Function     | Description                              | Return Type | Possible Values             |
| ------------ | ---------------------------------------- | ----------- | --------------------------- |
| `isValid()`  | Check if payload was parsed successfully | `bool`      | `true`, `false`             |
| `key()`      | Entity key (e.g. gpio_1, relay_2)        | `String`    | e.g. `"gpio_1"`             |
| `value()`    | Raw command value                        | `String`    | e.g. `"1"`, `"ON"`, `"255"` |
| `metadata()` | Raw payload string                       | `String`    | Full payload string         |

### Entity Class

| Function      | Description             | Return Type | Example                 |
| ------------- | ----------------------- | ----------- | ----------------------- |
| `Entity(key)` | Constructor with key    | N/A           | `Entity("temperature")` |
| `key()`       | Get entity key          | `String`    | e.g. `"gpio_1"`         |
| `gpio()`      | Get GPIO pin (optional) | `int`       | e.g. `2`                |
| `hasGpio()`   | Check if GPIO is set    | `bool`      | `true`, `false`         |

### Macros

| Macro                               | Description                                | Example                                                    |
| ----------------------------------- | ------------------------------------------ | ---------------------------------------------------------- |
| `ON_ENTITY(entityObject, callback)` | Register custom callback for entity object | `ON_ENTITY(relay1, [](EntityCommand &cmd){ ... });`        |
| `ON_ENTITY_S(key, callback)`        | Register custom callback by string key     | `ON_ENTITY_S("led_1", [](EntityCommand &cmd){ ... });`     |
| `fngin.onEntity(key, callback)`     | Register callback by string key (manual)   | `fngin.onEntity("status", [](EntityCommand &cmd){ ... });` |

## License

MIT License
