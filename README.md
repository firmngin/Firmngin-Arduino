# Firmngin-Kit Library

A powerful and easy-to-use library that seamlessly integrates your ESP8266/ESP32 IoT devices into the Firmngin Platform, enabling real-time device management, remote control, and secure payment processing capabilities. Built with secure communication, this library provides a simple event-driven API that handles all the complexity of device connectivity, allowing you to focus on building amazing IoT applications 💰.

Visit [firmngin.dev](https://firmngin.dev) for more information and try it for free.

## Features

- ESP8266 and ESP32 support
- Event-driven callback system
- Simple state-based API

## Installation

### Arduino IDE

1. Install required libraries via Library Manager:
   - ArduinoJson
   - PubSubClient

2. Download this library and place it in Arduino IDE `libraries` folder

3. Download `keys.h`  and put it in your sketch folder with main sketch files:

```cpp
// keys.h
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
```

## Basic Usage

```cpp
#include "keys.h"          // Your certificate file
#include "firmnginKit.h"
#include <ArduinoJson.h>

#define DEVICE_ID "YOUR_DEVICE_ID"
#define DEVICE_KEY "YOUR_DEVICE_KEY"

// ESP8266
FirmnginKit fngin(DEVICE_ID, DEVICE_KEY, CLIENT_CERT, PRIVATE_KEY, SERVER_FINGERPRINT_BYTES);

// ESP32
// FirmnginKit fngin(DEVICE_ID, DEVICE_KEY, CA_CERT, CLIENT_CERT, PRIVATE_KEY);

void setup() {
  Serial.begin(115200);
  
  // Connect WiFi first
  WiFi.begin("SSID", "PASSWORD");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  // Setup handler for any event
  fngin.onState(PAYMENT_SUCCESS, [](DeviceState state) {
    String payload = state.getPayload();
    Serial.println("Payment received!");
    Serial.println(payload);
    
    // Parse JSON with ArduinoJson
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    
    if (doc.containsKey("amount")) {
      int amount = doc["amount"];
      Serial.print("Amount: ");
      Serial.println(amount);
    }
  });
  

  fngin.onState(DEVICE_STATUS, [](DeviceState state) {
    Serial.println("Device status requested");
    Serial.println(state.getPayload());
  });
  
  fngin.onState(PAYMENT_PENDING, [](DeviceState state) {
    Serial.println("Payment pending");
    Serial.println(state.getPayload());
  });
  
  fngin.onState(CUSTOM_ON_PENDING_PAYMENTS, [](DeviceState state) {
    Serial.println("On pending payments");
    Serial.println(state.getPayload());
  });
  
  fngin.onState(CUSTOM_ON_EXPIRED_PAYMENTS, [](DeviceState state) {
    Serial.println("On expired payments");
    Serial.println(state.getPayload());
  });
  
  fngin.onState(CUSTOM_ON_SUCCESS_PAYMENTS, [](DeviceState state) {
    Serial.println("On success payments");
    Serial.println(state.getPayload());
  });
  
  fngin.setDebug(false);        // Enable debug output
  fngin.setTimezone(7);        // Set timezone (GMT+7 for Indonesia)
  
  fngin.begin();
}

void loop() {
  fngin.loop();  // Must be called continuously
}
```

## Main Functions

### Constructor

```cpp
// ESP8266
FirmnginKit fngin(deviceId, deviceKey, clientCert, privateKey, fingerprint);

// ESP32
FirmnginKit fngin(deviceId, deviceKey, caCert, clientCert, privateKey);
```

### Setup & Loop

- `fngin.begin()` - Start connection to server (call after WiFi connected)
- `fngin.loop()` - Must be called continuously in `loop()`

### Event Handlers

- `fngin.onState(PAYMENT_SUCCESS, callback)` - Handler for payment success
- `fngin.onState(DEVICE_STATUS, callback)` - Handler for device status request
- `fngin.onState(PAYMENT_PENDING, callback)` - Handler for payment pending
- `fngin.onState(CUSTOM_ON_PENDING_PAYMENTS, callback)` - Handler for pending payments
- `fngin.onState(CUSTOM_ON_EXPIRED_PAYMENTS, callback)` - Handler for expired payments
- `fngin.onState(CUSTOM_ON_SUCCESS_PAYMENTS, callback)` - Handler for success payments

### Configuration

- `fngin.setDebug(true/false)` - Enable/disable debug output
- `fngin.setTimezone(7)` - Set timezone (default: +7 for Indonesia)
- `fngin.setNtpServer("pool.ntp.org")` - Set NTP server
- `fngin.setMQTTServer("server.com", 8883)` - Set custom MQTT server

## Event Types

The library provides easy-to-read enums:

| Enum | Description |
|------|-------------|
| `PAYMENT_SUCCESS` | Payment successfully received |
| `DEVICE_STATUS` | Device status request |
| `PAYMENT_PENDING` | Payment is pending |
| `CUSTOM_ON_PENDING_PAYMENTS` | Your data when pending payments event |
| `CUSTOM_ON_EXPIRED_PAYMENTS` | Your data when expired payments event |
| `CUSTOM_ON_SUCCESS_PAYMENTS` | Your data when success payments event |

## Accessing Data

All data is returned as JSON string. Use ArduinoJson for parsing:

```cpp
fngin.onState(PAYMENT_SUCCESS, [](DeviceState state) {
  String payload = state.getPayload();
  
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, payload);
  
  // Access fields
  String refId = doc["reference_id"];
  int amount = doc["amount"];
  int sessionId = doc["active_session_id"];
});
```

## Troubleshooting

1. **Ensure WiFi is connected** before calling `begin()`
2. **Ensure `keys.h` file exists** in sketch folder with valid certificates
3. **Enable debug mode** to see detailed logs: `fngin.setDebug(true)`
4. **Restart device** if connection issues occur
5. **Check Serial Monitor** to see error messages

## Complete Example

See `examples/BasicExample/` folder for complete library usage example.

## License

MIT License

## Support

Visit [firmngin.dev](https://firmngin.dev) for complete documentation and support.
