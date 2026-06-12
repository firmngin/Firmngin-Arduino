# Firmngin Arduino Library

<p align="center">
  <img src="https://github.com/firmngin/Firmngin-Arduino/blob/main/logo.png?raw=true" alt="Firmngin" width="96">
</p>

IoT library for the Firmngin platform. Enables ESP8266/ESP32 devices to accept payments, receive commands, and communicate securely with mTLS support. **Built on top of [PubSubClient](https://github.com/knolleary/pubsubclient)** for MQTT; a vendored copy is included under `src/PubSubClient/`.

Check out [firmngin.dev](https://firmngin.dev) for more information.

## Features

- **ESP8266 & ESP32** support
- **Event-driven API** using `on()` callbacks: raw or typed
- **Typed objects** for common flows: `Verifications`, `Payments`, `Usages`, `DeviceStates`, `Inits`, `EntityCommand`
- **GPS Location** — builder-pattern API for sending coordinate updates
- **Image Upload** — multipart image upload with HMAC-authenticated HTTP POST
- **OTA Updates** — remote firmware download, SHA256 verification, and installation
- **Secure connection** via mTLS

## Installation

### PlatformIO

Add to your `platformio.ini`:

```ini
lib_deps =
    firmngin/firmngin  ; or symlink to this repo
```

> MQTT is provided by **PubSubClient** (bundled in `src/PubSubClient/`). You may also depend on upstream `knolleary/PubSubClient` if not using the vendored copy.

For a minimal MQTT-only sketch without Firmngin credentials, see `examples/MqttOnlyExample` and `examples/README.md`.

### Arduino IDE

1. Install this library (PubSubClient is declared in `library.properties` `depends=` and vendored under `src/PubSubClient/`).
2. Add your `keys.h` next to your sketch for mTLS.

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

ON_ACTIVE_SESSION(s) {
  if (s.entity(temperature).toFloat() >= 40.0) {
    s.endSession();
  }
}

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

### Device Local End Session

The `ON_ACTIVE_SESSION` callback gives you control to end an active session directly from the device. This is useful for enforcing local rules based on sensor readings, energy consumption, or device state — for example, stopping a machine when a usage limit is reached or a temperature threshold is exceeded. The callback only fires when the device has an active paid order and its status is `on_active_service`.

```cpp
Entity energy("energy_kwh");

ON_ACTIVE_SESSION(s) {
  if (s.entity(energy).toFloat() >= 10.0) {
    stopMachine();
    s.endSession();
  }
}

void setup() {
  fngin.begin();
}
```

## mTLS Setup (`keys.h`)

You can download your device's `keys.h` directly from the **Firmngin Dashboard** → Devices → your device → Download `keys.h`.

## Available States

Register handlers using the enum constants. Use `on(ENUM, callback)` for raw callbacks or typed objects for automatic parsing.

| Enum                  | Description                                                 |
| --------------------- | ----------------------------------------------------------- |
| `PAYMENT`             | Payment successfully settled                                |
| `DEVICE_STATUS`       | Device state changed                                        |
| `PENDING_PAYMENT`     | Invoice created, waiting for payment                        |
| `METADATA_ON_PENDING` | Custom metadata for pending payments (raw JSON)             |
| `METADATA_ON_EXPIRED` | Custom metadata for expired payments (raw JSON)             |
| `METADATA_ON_SUCCESS` | Custom metadata for successful payments (raw JSON)          |
| `INIT`                | Initial configuration after connection                      |
| `DISPLAY_PIN`         | Display PIN on screen for guest verification                |
| `VERIFICATION_RESULT` | PIN or precondition check result                            |
| `VERIFICATIONS`       | Typed callback: handles PIN display and verification result |
| `PAYMENTS`            | Typed callback: handles pending and success payments        |
| `ENTITIES`            | Received commands to specific entities (e.g. gpio_1)        |

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
fngin.pushBatchEntities()
    .add("temperature", 25.5)
    .add("humidity", 80)
    .add("pressure", 1013.2)
    .add("light", 450)
    .add("status", "active");

// Or use Entity objects as keys
Entity temp("temperature");
Entity hum("humidity");
fngin.pushBatchEntities()
    .add(temp, 25.5)
    .add(hum, 80)
    .add("status", "active");

// Or send raw JSON array directly
fngin.updateEntities("[{\"key\":\"gpio_1\",\"value\":\"1\"},{\"key\":\"gpio_2\",\"value\":\"0\"}]");
```

### Request Init

Manually request initial configuration after connection:

```cpp
fngin.requestInit();
```

### Location

Send GPS location updates from your device using `pushLocation()`. The method returns a `LocationUpdate` builder object you chain setters on. Data is sent automatically when the builder goes out of scope:

```cpp
fngin.pushLocation()
    .lat(-6.208763)
    .lon(106.845599)
    .accuracy(5.0f)
    .alt(10.0f)
    .speed(1.2f);
```

### Image Upload

Upload images using `uploadImage()`:

```cpp
fngin.uploadImage("cam_1", imageData, imageLen, "image/jpeg",
    [](const char *response) {
        Serial.println(response);
    },
    [](int code, const char *message) {
        Serial.printf("Upload failed: %d %s\n", code, message);
    }
);
```

| Parameter     | Type                          | Description                                        |
| ------------- | ----------------------------- | -------------------------------------------------- |
| `entityKey`   | `const char *`                | Entity key to associate with the image             |
| `data`        | `uint8_t *`                   | Raw binary image data                              |
| `len`         | `size_t`                      | Length of the image data in bytes                  |
| `contentType` | `const char *`                | MIME type, e.g. `"image/jpeg"` or `"image/png"`    |
| `onSuccess`   | `UploadCallbackFunction`      | Called with server response on HTTP 200            |
| `onError`     | `UploadErrorCallbackFunction` | Called with HTTP code and error message on failure |

### OTA (Over-the-Air Updates)

The library supports remote firmware updates via OTA. The firmware is downloaded from the Firmngin server, verified with SHA256, and installed using the Arduino `Update` class.

#### Prerequisites

Define compile-time flags in your `platformio.ini` build flags or `keys.h`:

```cpp
// In platformio.ini build_flags or keys.h:
-D FIRMNGIN_FIRMWARE_VERSION="1.2.3"
-D FIRMNGIN_FIRMWARE_TARGET_BOARD="ESP32"     // auto-detected, optional
-D FIRMNGIN_FIRMWARE_TARGET_MODEL="v2"         // optional
```

| Flag                             | Default       | Description              |
| -------------------------------- | ------------- | ------------------------ |
| `FIRMNGIN_FIRMWARE_VERSION`      | `"0.0.0"`     | Current firmware version |
| `FIRMNGIN_FIRMWARE_TARGET_BOARD` | Auto-detected | Target board identifier  |
| `FIRMNGIN_FIRMWARE_TARGET_MODEL` | `""`          | Device model string      |

#### Usage

Set firmware metadata and start listening for OTA triggers:

```cpp
void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    // ...

    fngin.setFirmwareInfo("1.2.3", "ESP32", "v2");
    // or just version:
    fngin.setFirmwareInfo("1.2.3");

    fngin.onOTAStatus([](const char *status, const char *message) {
        Serial.print("OTA: ");
        Serial.print(status);
        Serial.print(" — ");
        Serial.println(message);
    });

    fngin.begin();
}
```

Use the `ON_OTA_STATUS` macro for a declarative handler:

```cpp
ON_OTA_STATUS(status, msg) {
    Serial.print("OTA: ");
    Serial.print(status);
    Serial.print(" — ");
    Serial.println(msg);
}
```

#### Checking and Performing OTA

```cpp
// Check if an update is available
if (fngin.checkOTA()) {
    Serial.println("Update available, starting download...");
    fngin.performOTA();
}

// Or performOTA triggers checkOTA automatically if no firmware ID is known
fngin.performOTA();
```

#### OTA State Machine

#### Remote OTA Trigger

OTA can be triggered remotely from the dashboard.

```cpp
// Disable OTA if needed
fngin.setEnableOTA(false);
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

## API Reference

### Core Methods

| Function                                                       | Description                                                      | Return Type      | Possible Values                             |
| -------------------------------------------------------------- | ---------------------------------------------------------------- | ---------------- | ------------------------------------------- |
| `begin()`                                                      | Initialize library, sync time, setup mTLS, and connect to server | `void`           | N/A                                         |
| `loop()`                                                       | Maintain connection and process incoming messages                | `void`           | N/A                                         |
| `setDebug(bool)`                                               | Enable/disable debug output to Serial                            | `void`           | `true`, `false`                             |
| `setTimezone(int)`                                             | Set timezone offset from GMT                                     | `void`           | `-12` to `12`                               |
| `setDaylightOffsetSec(int)`                                    | Set daylight saving offset in seconds                            | `void`           | Any integer                                 |
| `setNtpServer(const char*)`                                    | Set NTP server address                                           | `void`           | e.g. `"pool.ntp.org"`                       |
| `setClient(Client&)`                                           | Use an external network client                                   | `void`           | EthernetClient, other Client impl           |
| `isPlatformSupported()`                                        | Check if current board is supported                              | `bool`           | `true` (ESP8266/ESP32), `false`             |
| `on(STATE, callback)`                                          | Register a raw callback for any state                            | `void`           | See enum constants                          |
| `on(VERIFICATIONS, callback)`                                  | Register typed callback for dpin + vr                            | `void`           | `Verifications &`                           |
| `on(PAYMENTS, callback)`                                       | Register typed callback for pp + pm                              | `void`           | `Payments &`                                |
| `on(USAGES, callback)`                                         | Register typed callback for ur + le + nl                         | `void`           | `Usages &`                                  |
| `on(DEVICE_STATUS, callback)`                                  | Register typed callback for ds                                   | `void`           | `DeviceStates &`                            |
| `on(INIT, callback)`                                           | Register typed callback for init                                 | `void`           | `Inits &`                                   |
| `on(ACTIVE_SESSION, callback)`                                 | Register active session callback                                 | `void`           | `ActiveSession &`                           |
| `on(ENTITIES, callback)`                                       | Register typed callback for entity commands                      | `void`           | `EntityCommand &`                           |
| `onEntity(key, callback)`                                      | Register callback for a specific entity key (string or object)   | `void`           | `EntityCommand &`                           |
| `pushEntity(key, value)`                                       | Send a single entity state update (key: string/Entity. value: const char*, String, int, float, double, bool) | `bool`           | `true` = sent, `false` = fail               |
| `pushEntity(entity, value)`                                    | Send entity state using Entity object as key                     | `bool`           | `true` = sent, `false` = fail               |
| `pushEntity(key, value, decimals)`                             | Send numeric value with decimal precision                        | `bool`           | `true` = sent, `false` = fail               |
| `updateEntities(json)`                                         | Send multiple entity states as JSON array                        | `bool`           | `true` = sent, `false` = fail               |
| `pushBatchEntities()`                                          | Start batch state builder (chain `.add()`)                       | `BatchState`     | Builder object                              |
| `entity(key)`                                                  | Read latest local entity value cached by the library             | `EntityValue`    | `toString/toFloat/toInt/isOn`               |
| `requestInit()`                                                | Request initial configuration                                    | `bool`           | `true` = sent, `false` = fail               |
| `pushLocation()`                                               | Start location update builder (chain `.lat().lon()`)             | `LocationUpdate` | Builder object                              |
| `uploadImage(key, data, len, contentType, onSuccess, onError)` | Upload image via multipart POST                                  | `bool`           | `true` = sent, `false` = fail               |
| `setFirmwareInfo(version, board, model)`                       | Set firmware metadata for OTA                                    | `void`           | N/A                                         |
| `setFirmwareInfo(version)`                                     | Set firmware version only                                        | `void`           | N/A                                         |
| `getFirmwareVersion()`                                         | Get current firmware version                                     | `const char*`    | Version string                              |
| `getFirmwareTargetBoard()`                                     | Get target board identifier                                      | `const char*`    | Board string                                |
| `getFirmwareTargetModel()`                                     | Get target model string                                          | `const char*`    | Model string                                |
| `syncFirmwareInfo()`                                           | Push firmware info to server via MQTT                            | `bool`           | `true` = sent, `false` = fail               |
| `setOTABaseURL(url)`                                           | Set custom OTA server base URL                                   | `void`           | N/A                                         |
| `setEnableOTA(bool)`                                           | Enable or disable OTA (enabled by default)                       | `void`           | `true`, `false`                             |
| `enableOTA(bool)`                                              | Alias for setEnableOTA                                           | `void`           | `true`, `false`                             |
| `checkOTA()`                                                   | Check server for available firmware update                       | `bool`           | `true` = update available                   |
| `performOTA(manifestUrl)`                                      | Start OTA firmware download and install                          | `bool`           | `true` = started successfully               |
| `onOTAStatus(callback)`                                        | Register OTA status callback                                     | `void`           | `(const char *status, const char *message)` |

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
| `quantity()`  | Item quantity                                  | `int`       | e.g. `1`                     |
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
| `activeOrderId()`          | Active paid order code from init         | `String`    | e.g. `"ODR-260601-12345678"`                                                                                    |
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
| `Entity(key)` | Constructor with key    | N/A         | `Entity("temperature")` |
| `key()`       | Get entity key          | `String`    | e.g. `"gpio_1"`         |
| `gpio()`      | Get GPIO pin (optional) | `int`       | e.g. `2`                |
| `hasGpio()`   | Check if GPIO is set    | `bool`      | `true`, `false`         |

### LocationUpdate Object

| Function          | Description                             | Return Type        | Possible Values   |
| ----------------- | --------------------------------------- | ------------------ | ----------------- |
| `lat(double)`     | Set latitude (required, -90 to 90)      | `LocationUpdate &` | `this` (chaining) |
| `lon(double)`     | Set longitude (required, -180 to 180)   | `LocationUpdate &` | `this` (chaining) |
| `accuracy(float)` | Set accuracy in meters (optional, >= 0) | `LocationUpdate &` | `this` (chaining) |
| `alt(float)`      | Set altitude in meters (optional)       | `LocationUpdate &` | `this` (chaining) |
| `speed(float)`    | Set speed (optional, >= 0)              | `LocationUpdate &` | `this` (chaining) |

### Macros

| Macro                               | Kind                   | Payload                      | Style                | Example                                                    |
| ----------------------------------- | ---------------------- | ---------------------------- | -------------------- | ---------------------------------------------------------- |
| `ON_ENTITY(entityObject, callback)` | Entity callback        | `EntityCommand &`            | Inline lambda        | `ON_ENTITY(relay1, [](EntityCommand &cmd){ ... });`        |
| `ON_ENTITY_S(key, callback)`        | Entity callback        | `EntityCommand &`            | Inline lambda        | `ON_ENTITY_S("led_1", [](EntityCommand &cmd){ ... });`     |
| `fngin.onEntity(key, callback)`     | Entity callback        | `EntityCommand &`            | Runtime registration | `fngin.onEntity("status", [](EntityCommand &cmd){ ... });` |
| `ON_ENTITIES(cmd)`                  | Global entity callback | `EntityCommand &`            | Macro body           | `ON_ENTITIES(cmd) { ... }`                                 |
| `ON_VERIFICATIONS(v)`               | Verification flow      | `Verifications &`            | Macro body           | `ON_VERIFICATIONS(v) { ... }`                              |
| `ON_PAYMENTS(p)`                    | Payment flow           | `Payments &`                 | Macro body           | `ON_PAYMENTS(p) { ... }`                                   |
| `ON_USAGES(u)`                      | Usage flow             | `Usages &`                   | Macro body           | `ON_USAGES(u) { ... }`                                     |
| `ON_DEVICE_STATUS(ds)`              | Device status flow     | `DeviceStates &`             | Macro body           | `ON_DEVICE_STATUS(ds) { ... }`                             |
| `ON_INIT(i)`                        | Init flow              | `Inits &`                    | Macro body           | `ON_INIT(i) { ... }`                                       |
| `ON_OTA_STATUS(status, message)`    | OTA status             | `const char *, const char *` | Macro body           | `ON_OTA_STATUS(status, message) { ... }`                   |
| `ON_ACTIVE_SESSION(s)`              | Active session         | `ActiveSession &`            | Macro body           | `ON_ACTIVE_SESSION(s) { ... }`                             |

For runtime registration, active session uses:

```cpp
fngin.on(ON_ACTIVE_SESSION, [](ActiveSession &s) {
    if (s.canRun()) {
        s.endSession();
    }
});
```

## Contributing

If you find a bug or have a feature request or idea, feel free to create a PR or open an issue.

## License

MIT License
