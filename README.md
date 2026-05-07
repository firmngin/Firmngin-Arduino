# FirmnginKit Library

IoT Library for the Firmngin Platform. Enables ESP8266/ESP32 devices to accept payments, receive commands, and communicate securely over MQTT with mTLS support.

Check out [firmngin.dev](https://firmngin.dev) for more information.

## Features

- **ESP8266 & ESP32** support
- **Auto reconnect** to MQTT broker with exponential backoff
- **Event-driven API** using `onState()` and `onCommand()` callbacks
- **mTLS authentication** via `keys.h` (optional but recommended)
- **Shortened JSON keys** to minimize payload size

## Installation

### PlatformIO

Add to your `platformio.ini`:

```ini
lib_deps =
    bblanchon/ArduinoJson @ ^6.21.3
    knolleary/PubSubClient @ ^2.8.0
```

> We use these excellent 3rd party libraries to power FirmnginKit. Make sure they are installed in your project.

Copy `src/firmnginKit.h` and `src/firmnginKit.cpp` to your project.

### Arduino IDE

1. Install dependencies via Library Manager:
   - **ArduinoJson** by Benoit Blanchon
   - **PubSubClient** by Nick O'Leary
2. Copy `firmnginKit.h`, `firmnginKit.cpp`, and `keys.h` (see below) to your sketch folder.

## Quick Start

```cpp
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

void setup() {
  Serial.begin(115200);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Register state handlers BEFORE begin()
  fngin.onState(PAYMENT, [](DeviceState state) {
    String payload = state.getPayload();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);

    Serial.print("Item: ");      Serial.println(doc["it"].as<String>());
    Serial.print("Price: ");     Serial.println(doc["pc"].as<String>());
    Serial.print("Order ID: ");  Serial.println(doc["oid"].as<String>());
  });

  fngin.onState(DEVICE_STATUS, [](DeviceState state) {
    DynamicJsonDocument doc(256);
    deserializeJson(doc, state.getPayload());
    Serial.print("New state: ");
    Serial.println(doc["s"].as<String>());
  });

  fngin.setDebug(true);
  fngin.setTimezone(7);          // GMT+7 (Indonesia)
  fngin.begin();
}

void loop() {
  fngin.loop();
}
```

## mTLS Setup (`keys.h`)

Create a `keys.h` file next to your sketch (do **not** commit this file).

You can download your device's `keys.h` directly from the **Firmngin Dashboard** → Devices → your device → Download `keys.h`.

If you prefer to create it manually, use the following template:

```cpp
#ifndef KEYS_H
#define KEYS_H

static const char* mqtt_server = "asia-jkt1.firmngin.dev";
static const int mqtt_port = 8883;

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

Register handlers using the enum constants:

| Enum                  | Description                             |
| --------------------- | --------------------------------------- |
| `PAYMENT`             | Payment Section State                   |
| `DEVICE_STATUS`       | Device entities state changed           |
| `PENDING_PAYMENT`     | Invoice created, waiting for payment    |
| `METADATA_ON_PENDING` | Custom metadata for pending payments    |
| `METADATA_ON_EXPIRED` | Custom metadata for expired payments    |
| `METADATA_ON_SUCCESS` | Custom metadata for successful payments |
| `INIT`                | Initial configuration after connection  |
| `DISPLAY_PIN`         | Show a PIN on the device screen         |
| `VERIFICATION_RESULT` | PIN or precondition check result        |
| `MENU_ITEMS`          | List of available services/products     |
| `USAGE_RESPONSE`      | Current MQTT usage and quota            |
| `LIMIT_EXCEEDED`      | Quota limit reached                     |
| `NEAR_LIMIT`          | Approaching quota limit                 |

You can also use raw topic suffix strings if needed:

```cpp
fngin.onState("pm", [](DeviceState state) { /* ... */ });
```

## JSON Payload Keys (Backend → Device)

All JSON keys sent by the backend are **max 3 characters** to minimize payload size:

### Payment (`pm`) / Pending Payment (`pp`)¡

```json
{ "it": "Cappuccino", "pc": "45000", "oid": "ODR-260506-12345678" }
```

- `it` — item title
- `pc` — price
- `oid` — order ID

### Device State (`ds`)

```json
{ "s": "idle" }
```

- `s` — state string

### Display PIN (`dpin`)

```json
{
  "t": "pin_display",
  "pi": "123456",
  "si": "abc...",
  "ttl": 60,
  "ea": "2026-05-05T10:01:00Z"
}
```

- `t` — type
- `pi` — PIN code
- `si` — session ID
- `ttl` — validity in seconds
- `ea` — expires at (ISO-8601)

### Verification Result (`vr`)

```json
{ "pn": true, "pr": false }
```

- `pn` — PIN met
- `pr` — precondition met

### Init (`init`)

```json
{ "e": [{ "p": 1, "v": "0" }], "m": "idle", "vf": 0 }
```

- `e` — entities array (`p` = pin, `v` = value)
- `m` — merchant status
- `vf` — verification flag (0-3)

### Menu Items (`mi`)

```json
[
  {
    "id": "uuid",
    "tl": "Title",
    "d": "Desc",
    "pr": "5000",
    "sku": "SKU",
    "pt": "single"
  }
]
```

### Usage / Limit (`ur`, `le`, `nl`)

```json
{
  "u": 1250,
  "l": 30000,
  "r": 28750,
  "pct": 4,
  "ra": "2026-05-06T00:00:00Z",
  "g": "day"
}
```

- `u` — used
- `l` — limit
- `r` — remaining
- `pct` — percentage
- `ra` — reset at
- `g` — granularity

## Last Will and Testament (LWT)

Automatically configured on `begin()`:

- **Topic**: `/d/{device_id}/lwt`
- **Offline payload**: `"0"`
- **Online payload**: `"1"` (published on connect)
- **QoS**: 1, **Retain**: true

## Development Workflow

```bash
# Edit library in src/
# Sync to example
python3 sync_lib.py

# Build example
pio run -e esp32dev
```

## API Reference

```cpp
FirmnginKit(const char* deviceId, const char* deviceKey);

void begin();
void loop();
void setDebug(bool debug);
void setTimezone(int timezone);          // -12 to 12
void setDaylightOffsetSec(int offset);
void setNtpServer(const char* server);
void setMQTTServer(const char* server, int port);
bool isPlatformSupported();

void onState(const char* state, StateCallbackFunction callback);
void onState(DeviceStateType state, StateCallbackFunction callback);
void onCommand(const char* command, StateCallbackFunction callback);
void onCommand(DeviceStateType command, StateCallbackFunction callback);
```

## License

MIT License
