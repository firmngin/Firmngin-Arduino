# Firmngin Examples

This folder contains examples for using the Firmngin library.

## Setup

1. **Use the shared library**: Include `<firmngin.h>` from the root `src/` folder; do not copy library files into each example
2. **Create keys.h**: Copy `src/keys.h.template` to your sketch folder, set `DEVICE_ID` / `DEVICE_KEY`, and add mTLS certificates
3. **Configure WiFi**: Update WiFi credentials in the example
4. **Upload and test**: Use PlatformIO or Arduino IDE to compile and upload

## MQTT dependency (PubSubClient)

The **Firmngin Arduino library is built on top of [PubSubClient](https://github.com/knolleary/pubsubclient)** (Nick O'Leary) as its MQTT client dependency. This repository vendors PubSubClient under `src/PubSubClient/`; platform MQTT and optional `mqttClient()` both use that stack.

## MQTT only (without Firmngin)

If you only need MQTT to your own broker — without `DEVICE_ID`, `DEVICE_KEY`, `begin()`, entities, or payments — use the bundled **PubSubClient** directly:

```cpp
#include <PubSubClient/PubSubClient.h>
#include <WiFiClientSecure.h>

WiFiClientSecure net;
PubSubClient mqtt(net);
```

- Do **not** `#include <firmngin.h>` and do not construct `Firmngin`.
- The full PubSubClient API is available (`connect`, `publish`, `subscribe`, `loop`, LWT, QoS, `beginPublish`, etc.).
- See **MqttOnlyExample** for a complete sketch.

> **Arduino IDE note:** Installing the `firmngin` library still compiles all sources under `src/`. For the smallest firmware footprint, you may copy only `src/PubSubClient` into your project; in this monorepo, including the header above is enough for development.

## Examples

### BasicExample
- Shows basic DeviceState usage
- Simple MQTT message handling
- Good starting point for beginners

### MqttOnlyExample
- Standalone MQTT via bundled PubSubClient (no `Firmngin`, no device credentials)
- WiFi + TLS (`setInsecure` demo), connect + LWT, QoS 1 subscribe, publish, manual reconnect

### MqttClientExample
- Firmngin platform MQTT (`begin` / `loop`) plus a second broker via `mqttClient()` (full PubSubClient API)
- Covers `mqttClientPrepare`, `mqttClientSetBufferSize`, `mqttClientSetInsecure`, `mqttTransport`, connect with LWT, QoS subscribe, `mqttClientLoop`, manual reconnect, and `mqttClientDisconnect`

### OTAUpdateExample
- OTA firmware update with checkOTA() and performOTA()
- Async download with progress tracking via onOTAStatus() callback
- Manual trigger via hardware button or backend entity command
- Periodic auto-check for updates

### ActiveSessionEndExample
- Simple `ON_ACTIVE_SESSION` usage
- Also available as `fngin.on(ON_ACTIVE_SESSION, [](ActiveSession &s){ ... });`
- Ends current session with `s.endSession()`
- Uses device-side entity logic

### GPSTrackerExample
- GPS tracking with TinyGPS++ and `pushLocation()`
- Sends `lat` and `lon` every 10 seconds
- Optional fields: `accuracy()`, `alt()`, `speed()`
- Compatible with NEO-6M, NEO-8M, and similar GPS modules

### AdvancedExample (Coming Soon)
- Advanced JSON parsing
- Complex device logic
- Multiple state handlers

## mTLS Setup

For secure mTLS authentication, create a `keys.h` file in your project:

```cpp
// Client Certificate (PEM format)
const char CLIENT_CERT[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
[Your Client Certificate]
-----END CERTIFICATE-----
)EOF;

// Private Key (PEM format)
const char PRIVATE_KEY[] PROGMEM = R"EOF(
-----BEGIN PRIVATE KEY-----
[Your Private Key]
-----END PRIVATE KEY-----
)EOF;

// Server Fingerprint
const uint8_t SERVER_FINGERPRINT_BYTES[] = {
  // Your server certificate fingerprint
};
```

Without `keys.h`, the library will show an error and refuse to connect.
