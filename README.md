# Firmngin Arduino Library

<p align="center">
  <img src="https://github.com/firmngin/Firmngin-Arduino/blob/main/logo.png?raw=true" alt="Firmngin" width="96">
</p>

IoT library for ESP32 with secure communication (mTLS), OTA firmware updates, image upload, GPS location, and payment/verification flows for the Firmngin platform.

Check out [firmngin.dev](https://firmngin.dev) for more information.

## Features

- **ESP32** support
- **Event-driven API** using `on()` callbacks: raw or typed
- **Typed objects** for common flows: `Verifications`, `Payments`, `Dispenses`, `Usages`, `DeviceStates`, `Inits`, `EntityCommand`
- **Vending mode** for vendingmachines and handle SKU dispense callbacks
- **Image Upload** image upload for camera-equipped devices
- **OTA Updates** easy firmware update and secure
- **Secure connection** via mTLS

## Installation

### PlatformIO

Add to your `platformio.ini`:

```ini
lib_deps =
    firmngin/firmngin  ; or symlink to https://github.com/firmngin/Firmngin-Arduino
```

### Arduino IDE

1. Download the [latest release](https://github.com/firmngin/Firmngin-Arduino/releases) `.zip` and install via **Sketch → Include Library → Add .ZIP Library**, or search **firmngin** in Arduino Library Manager.
2. Get your `keys.h`: Log in to [firmngin.dev](https://firmngin.dev) → **Device Registry** → select your device → **Secrets** → **Download `keys.h`**. Place it next to your sketch.

### Factory provisioning

Fleet master firmware built with `FIRMNGIN_FACTORY_SERIAL=1` uses factory protocol v3 and must be exported as a complete `.merged.bin`. Provisioning stores the device identity, TLS material, service CA, and HTTPS API/OTA base URLs under `/firmnginfs`. Both service URLs must be absolute `https://` URLs; missing or malformed endpoints fail provisioning before the metadata is committed. Older factory identities must be provisioned again.

## Quick Start

```cpp
#include <Arduino.h>
#include <firmngin.h>
#include "keys.h"
#include <WiFi.h>

#define relayPin 2

const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

Firmngin fngin(DEVICE_ID, DEVICE_KEY);

Entity relay1("2");
Entity temperature("temperature");

ON_ENTITY(relay1, [](EntityCommand &cmd) {
  digitalWrite(relayPin, cmd.value() == "1" ? HIGH : LOW);
});

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  pinMode(relayPin, OUTPUT);
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

## Available States

Register handlers using the enum constants. Use `on(ENUM, callback)` for raw callbacks or typed objects for automatic parsing.

| Enum                         | Description                                                 |
| ---------------------------- | ----------------------------------------------------------- |
| `PAYMENT`                    | Payment successfully settled                                |
| `DEVICE_STATUS`              | Device state changed                                        |
| `PENDING_PAYMENT`            | Invoice created, waiting for payment                        |
| `POSTPAID_READY`             | Postpaid order is ready and active on the device            |
| `METADATA_ON_PENDING`        | Custom metadata for pending payments (raw JSON)             |
| `METADATA_POSTPAID_READY`    | Optional custom metadata paired with postpaid ready         |
| `METADATA_ON_ACTIVE_SERVICE` | Custom metadata sent when the service becomes active        |
| `METADATA_ON_EXPIRED`        | Custom metadata for expired payments (raw JSON)             |
| `METADATA_ON_SUCCESS`        | Custom metadata for successful payments (raw JSON)          |
| `INIT`                       | Initial configuration after connection                      |
| `DISPLAY_PIN`                | Display PIN on screen for guest verification                |
| `VERIFICATION_RESULT`        | PIN or precondition check result                            |
| `VERIFICATIONS`              | Typed callback: handles PIN display and verification result |
| `PAYMENTS`                   | Typed callback: handles pending and success payments        |
| `ENTITIES`                   | Received commands to specific entities (e.g. gpio_1)        |

## Error Codes & Monitoring

The library logs errors with specific codes in format `[FNG-Exxxx]` for easy troubleshooting. Monitor production health using:

```cpp
uint32_t dropped = fngin.getDroppedMessageCount();
uint32_t reconnects = fngin.getReconnectCount();

if (dropped > 0) {
    Serial.printf("⚠ Warning: %u messages dropped\n", dropped);
}
```

### Error Code Reference

| Code   | Constant                    | Description                                          | Action Required                          |
| ------ | --------------------------- | ---------------------------------------------------- | ---------------------------------------- |
| `1001` | `FMSG_ERR_QUEUE_FULL`       | Message queue full, data dropped                     | Increase queue size or reduce send rate  |
| `1002` | `FMSG_ERR_CONN_REFUSED`     | Broker refused connection                            | Check credentials in keys.h              |
| `1003` | `FMSG_ERR_CONN_TIMEOUT`     | Connection timeout                                   | Check network/firewall                   |
| `1004` | `FMSG_ERR_TLS_HANDSHAKE`    | TLS handshake failed                                 | Verify CA certificate                    |
| `1005` | `FMSG_ERR_TLS_CERT_VERIFY`  | TLS certificate verification failed                  | Check certificate expiry & hostname      |
| `1006` | `FMSG_ERR_TCP_CONNECT`      | TCP connection failed                                | Network issue or broker down             |
| `1007` | `FMSG_ERR_DNS_LOOKUP`       | DNS lookup failed                                    | Check DNS server or broker hostname      |
| `1008` | `FMSG_ERR_MESSAGE_TOO_LARGE`| Message exceeds 4KB limit                            | Split large messages or reduce size      |
| `1009` | `FMSG_ERR_BUFFER_OVERFLOW`  | Memory allocation failed                             | Free memory or reduce buffer usage       |
| `1010` | `FMSG_ERR_MUTEX_TIMEOUT`    | Mutex lock timeout (deadlock risk)                   | Report to maintainer                     |
| `1999` | `FMSG_ERR_UNKNOWN`          | Unknown error                                        | Enable debug mode for details            |

### Production Monitoring

Monitor connection health in production:

```cpp
void loop() {
    fngin.loop();

    static unsigned long lastCheck = 0;
    if (millis() - lastCheck >= 60000) { // Check every minute
        lastCheck = millis();
        
        uint32_t dropped = fngin.getDroppedMessageCount();
        uint32_t reconnects = fngin.getReconnectCount();
        
        // Define your thresholds
        if (dropped > 100 || reconnects > 10) {
            // Alert: System unstable
            // Take action: reboot, notify admin, etc.
        }
    }
}
```

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

### Audio Stream (voice output)

Audio reaches a device over **one persistent WebSocket per device**, not a URL per clip. The backend streams 16 kHz mono signed-16 little-endian PCM down it in 640-byte frames (20 ms each), and a newer clip cancels the one still playing. Explicit `audio.flush` clears both the software FIFO and built-in I2S TX DMA immediately; natural `audio.end` still drains with a fade. The device plays what arrives, with no fetch and no per-clip URL.

`openAudioStream()` needs only the chunk callback: the endpoint is this device's own stream on the audio gateway, and the token rides in from provisioning. The transport is Espressif's official `esp_websocket_client` (Apache-2.0), vendored into `src/` because Arduino-ESP32 core 3.x stopped bundling it while core 2.0.x still ships it — the version guard picks the right one and there is no third-party WebSocket or audio-decoder dependency. The wire stays mono (640 bytes), while Firmngin expands every sample to interleaved L/R before the callback (1280 bytes) to avoid ESP32's 16-bit mono TX sample reordering. Drain happens from `loop()` so the callback may block on I2S.

```cpp
#include "keys.h"

Entity speaker1("speaker1"); // the audio_output entity from the dashboard

void setup() {
    // ... WiFi ...
    fngin.setAudioInterface(speaker1, MAX98357A).pins(I2S_BCLK, I2S_LRC, I2S_DOUT);
    fngin.begin(); // starts I2S, then connects this device's PCM stream and stands by
}
```

A registered `setAudioInterface(entity, chip)` is all it takes — every output must be
bound to an entity, there is no unnamed default. `begin()` arms the stream and
`loop()` opens it as soon as the clock is synced, so there is no `openAudioStream()`
to call. The endpoint itself is not guessed — it arrives in init, in the `t = 6`
entity's value. The stream waits for that
frame before connecting; a provisioned URL, when present, is used immediately. Pass a callback with `fngin.enableAudioStream(onAudioEvent)` when you want
stream events instead of the Serial error report, or call `closeAudioStream()` to
stop. Immediate DMA clearing applies to speakers registered with `setAudioInterface`;
a custom chunk callback owns and must clear its own hardware queue on `AUDIO_FLUSH`.

The `audio_output` entity from init (`t = 6`) binds like any other entity, but with its
own macro — a clip's control frame carries the `entity_key` it targets, so the handler
is routed to the right speaker:

```cpp
Entity speaker("gpio_6"); // the audio_output entity key from init
ON_AUDIO(speaker, [](FirmnginAudioEvent event, const char *entityKey) {
    if (event == AUDIO_START)
        Serial.printf("playing %s\n", entityKey);
    else if (event == AUDIO_END)
        Serial.println("done");
    else if (event == AUDIO_ERROR)
        Serial.println("audio error");
});
```

Prefer the manual path when you want to tune I2S before `begin()` or supply your own
chunk callback. One WebSocket carries every clip, so a second speaker needs no second
connection — register each output under its entity and clips route by the `entity_key`
the gateway tags them with. A missing or unknown key is rejected and its PCM is
discarded; the library never falls back to another speaker:

```cpp
Entity speaker1("speaker1"); // audio_output entity #1
Entity garden("garden");     // audio_output entity #2

fngin.setAudioInterface(speaker1, MAX98357A).pins(I2S_BCLK1, I2S_LRC1, I2S_DOUT1);
fngin.setAudioInterface(garden, PCM5102).pins(I2S_BCLK2, I2S_LRC2, I2S_DOUT2);
```

Every audio setting is optional in one object. An untouched object
preserves the defaults; set only the hardware-specific values that need overriding:
```cpp
FirmnginAudioConfig audio;
audio.url = "ws://192.168.100.31:8282/v1/streams/test?pace=0"; // optional; nullptr derives provisioning URL
audio.token = "";                 // optional; nullptr uses provisioned token, empty disables bearer
audio.bufferFrames = 48;          // optional; ring capacity (~960 ms)
audio.framesPerLoop = 8;          // optional; frames drained per loop() pass
audio.websocketBufferBytes = 1024; // optional; default 1024, must be >= 640
audio.websocketTaskStack = 4096;   // optional; default 4096, must be >= 2048

fngin.openAudioStream(audio, onAudioChunk, onAudioEvent);
```

Call `loop()` often: PCM is drained there into I2S. The websocket task never blocks on a
full ring (it drops a frame instead) so `audio.flush` on stop always stays readable. A
clean `audio.end` plays every queued sample at full level (no content fade).

### Audio Input Stream (INMP441)

Audio input uses the same authenticated device WebSocket. `startAudioInput()` sends a
capture start control, then a dedicated ESP32 task reads the INMP441 and sends 16 kHz
mono signed-16 PCM in 640-byte/20 ms frames. `stopAudioInput()` closes the capture;
the gateway forwards it to firmnginbe, which stores a WAV in R2. A capture is limited
to 30 seconds and input is half-duplex with speaker playback.

INMP441 emits 24-bit samples in a 32-clock I2S slot. Tie its L/R pin LOW for
`FIRMNGIN_I2S_LEFT` or HIGH for `FIRMNGIN_I2S_RIGHT`:

```cpp
Entity microphone("microphone"); // audio_input entity

fngin.setAudioInput(microphone, INMP441)
    .pins(MIC_BCLK, MIC_WS, MIC_SD)
    .channel(FIRMNGIN_I2S_LEFT);

// Push-to-talk handlers:
fngin.startAudioInput(microphone);
fngin.stopAudioInput();
```

`AUDIO_INPUT_ACCEPTED`, `AUDIO_INPUT_SAVED`, and `AUDIO_INPUT_ERROR` are delivered
through the same `ON_AUDIO(microphone, ...)` event mechanism. See
`examples/AudioInputStreamExample` for a button-controlled sketch.

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

#### Remote OTA Trigger

OTA can be triggered remotely from the dashboard.

```cpp
// Disable OTA if needed
fngin.setEnableOTA(false);
```

---

## DeviceBilling Flow

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

Register **one callback** for the entire order/payment flow. The same `Payments` object handles prepaid pending, payment success, and postpaid ready:

```cpp
fngin.on(PAYMENTS, [](Payments &p) {
    if (p.isPending()) {
        Serial.println("Payment pending...");
    }

    if (p.isSuccess()) {
        Serial.println("Payment received!");
    }

    if (p.isPostPaid()) {
        Serial.println("Postpaid order ready!");
    }

    if (p.isPrePaid()) {
        Serial.println("Prepaid order update!");
    }

    Serial.println(p.itemTitle());   // "Cappuccino"
    Serial.println(p.price());       // "45000"
    Serial.println(p.orderId());     // "ODR-260506-12345678"
    Serial.println(p.metadata());    // raw JSON string
});
```

Active-service metadata is a separate raw JSON event. It does not change the semantics of pending, success, or postpaid payment events:

```cpp
fngin.on(METADATA_ON_ACTIVE_SERVICE, [](DeviceState state) {
    Serial.println(state.getPayload());
});
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

    if (i.isVendingMode()) {
        Serial.println("Business mode: vending");
    }
    if (i.isServiceMode()) {
        Serial.println("Business mode: service");
    }
    if (i.isTerminalMode()) {
        Serial.println("Business mode: terminal");
    }

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

> The `Inits` object is automatically populated when the device receives its initial configuration. Use `entities()` to get the raw JSON array of GPIO entities. Use the boolean helpers to check merchant status, verification configuration, and business mode (service vs vending).

### Vending Dispense Flow

When the device business mode is **vending**, a successful prepaid payment delivers one or more catalog SKUs to dispense. Register **one callback** to receive typed `Dispenses`:

```cpp
fngin.on(DISPENSES, [](Dispenses &d) {
    if (!d.isValid()) {
        Serial.println("Invalid dispense payload");
        return;
    }
    for (int i = 0; i < d.itemCount(); i++) {
        Serial.print("Dispense SKU: ");
        Serial.println(d.skuAt(i));
        // drive motor / servo for d.skuAt(i)
    }
});
```

Or with the macro:

```cpp
ON_DISPENSES(d) {
    for (int i = 0; i < d.itemCount(); i++) {
        Serial.println(d.skuAt(i));
    }
}
```

> Each entry is a catalog SKU string. See `examples/VendingMachineExample`.

## API Reference

### Core Methods

| Function                                                       | Description                                                                                                   | Return Type      | Possible Values                             |
| -------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------- | ---------------- | ------------------------------------------- |
| `begin()`                                                      | Initialize library, sync time, setup mTLS, and connect to server                                              | `void`           | N/A                                         |
| `loop()`                                                       | Maintain connection and process incoming messages                                                             | `void`           | N/A                                         |
| `setDebug(bool)`                                               | Enable/disable debug output to Serial                                                                         | `void`           | `true`, `false`                             |
| `setTimezone(int)`                                             | Set timezone offset from GMT                                                                                  | `void`           | `-12` to `12`                               |
| `setDaylightOffsetSec(int)`                                    | Set daylight saving offset in seconds                                                                         | `void`           | Any integer                                 |
| `setNtpServer(const char*)`                                    | Set NTP server address                                                                                        | `void`           | e.g. `"pool.ntp.org"`                       |
| `mqttClientBegin(...)`                                         | Open a second broker on the built-in ESP-MQTT client                                                         | `bool`           | host, credentials, message handler          |
| `getDroppedMessageCount()`                                     | Get total dropped messages (queue full, allocation errors)                                                   | `uint32_t`       | Counter since boot                          |
| `getReconnectCount()`                                          | Get total reconnection attempts                                                                              | `uint32_t`       | Counter since boot                          |
| `resetMessageStats()`                                          | Reset dropped message and reconnect counters                                                                  | `void`           | N/A                                         |
| `isPlatformSupported()`                                        | Check if current board is supported                                                                           | `bool`           | `true` on ESP32, `false` otherwise          |
| `on(STATE, callback)`                                          | Register a raw callback for any state                                                                         | `void`           | See enum constants                          |
| `on(VERIFICATIONS, callback)`                                  | Register typed callback for verification steps                                                                | `void`           | `Verifications &`                           |
| `on(PAYMENTS, callback)`                                       | Register typed callback for payment events                                                                    | `void`           | `Payments &`                                |
| `on(DISPENSES, callback)`                                      | Register typed callback for vending dispense SKUs                                                             | `void`           | `Dispenses &`                               |
| `on(USAGES, callback)`                                         | Register typed callback for usage limits                                                                      | `void`           | `Usages &`                                  |
| `on(DEVICE_STATUS, callback)`                                  | Register typed callback for device status                                                                     | `void`           | `DeviceStates &`                            |
| `on(INIT, callback)`                                           | Register typed callback for initial configuration                                                             | `void`           | `Inits &`                                   |
| `on(ACTIVE_SESSION, callback)`                                 | Register active session callback                                                                              | `void`           | `ActiveSession &`                           |
| `on(ENTITIES, callback)`                                       | Register typed callback for entity commands                                                                   | `void`           | `EntityCommand &`                           |
| `onEntity(key, callback)`                                      | Register callback for a specific entity key (string or object)                                                | `void`           | `EntityCommand &`                           |
| `setAudioInterface(entity, chip).pins(...)` / `setAudioInterface(key, chip)` | Bind an audio_output entity to I2S (MAX98357A / PCM5102 / I2S_TX); every speaker must be bound to an entity | `FirmnginI2s &` | multi-speaker; key = entity_key on clips      |
| `setAudioInput(entity, INMP441).pins(...).channel(...)` | Bind an audio_input entity to an INMP441 source | `FirmnginI2s &` | left/right slot follows the module L/R pin |
| `startAudioInput(entity)` / `stopAudioInput()` / `isAudioInputRecording()` | Start, finalize, and inspect a push-to-talk capture over the persistent WSS | `bool` / `void` / `bool` | one capture at a time, max 30 seconds |
| `openAudioStream(onChunk, onEvent)`                             | Open this device's PCM stream with default audio config; deliver I2S-ready stereo frames                      | `bool`           | `false` = not opened; an error event says why |
| `openAudioStream(config, onChunk, onEvent)`                     | Open with optional URL/token/buffer/backpressure/WebSocket overrides in `FirmnginAudioConfig`                 | `bool`           | `false` = not opened; an error event says why |
| `openAudioStream(url, token, onChunk, onEvent)`                 | Compatibility overload for an explicit endpoint/token                                                        | `bool`           | `false` = not opened; an error event says why |
| `enableAudioStream(onEvent)`                                   | Re-arm the automatic stream after `closeAudioStream()`; optional event callback                                 | `void`           | N/A                                          |
| `closeAudioStream()` / `isAudioStreamConnected()`                | Stop and release the audio stream / report socket state                                                     | `void` / `bool`  | N/A                                          |
| `pushEntity(key, value)`                                       | Send a single entity state update (key: string/Entity. value: const char\*, String, int, float, double, bool) | `bool`           | `true` = sent, `false` = fail               |
| `pushEntity(entity, value)`                                    | Send entity state using Entity object as key                                                                  | `bool`           | `true` = sent, `false` = fail               |
| `pushEntity(key, value, decimals)`                             | Send numeric value with decimal precision                                                                     | `bool`           | `true` = sent, `false` = fail               |
| `updateEntities(json)`                                         | Send multiple entity states as JSON array                                                                     | `bool`           | `true` = sent, `false` = fail               |
| `pushBatchEntities()`                                          | Start batch state builder (chain `.add()`)                                                                    | `BatchState`     | Builder object                              |
| `entity(key)`                                                  | Read latest local entity value cached by the library                                                          | `EntityValue`    | `toString/toFloat/toInt/isOn`               |
| `requestInit()`                                                | Request initial configuration                                                                                 | `bool`           | `true` = sent, `false` = fail               |
| `pushLocation()`                                               | Start location update builder (chain `.lat().lon()`)                                                          | `LocationUpdate` | Builder object                              |
| `uploadImage(key, data, len, contentType, onSuccess, onError)` | Upload image via multipart POST                                                                               | `bool`           | `true` = sent, `false` = fail               |
| `setFirmwareInfo(version, board, model)`                       | Set firmware metadata for OTA                                                                                 | `void`           | N/A                                         |
| `setFirmwareInfo(version)`                                     | Set firmware version only                                                                                     | `void`           | N/A                                         |
| `getFirmwareVersion()`                                         | Get current firmware version                                                                                  | `const char*`    | Version string                              |
| `getFirmwareTargetBoard()`                                     | Get target board identifier                                                                                   | `const char*`    | Board string                                |
| `getFirmwareTargetModel()`                                     | Get target model string                                                                                       | `const char*`    | Model string                                |
| `syncFirmwareInfo()`                                           | Push firmware info to server                                                                                  | `bool`           | `true` = sent, `false` = fail               |
| `setEnableOTA(bool)`                                           | Enable or disable OTA (enabled by default)                                                                    | `void`           | `true`, `false`                             |
| `enableOTA(bool)`                                              | Alias for setEnableOTA                                                                                        | `void`           | `true`, `false`                             |
| `checkOTA()`                                                   | Check server for available firmware update                                                                    | `bool`           | `true` = update available                   |
| `performOTA(manifestUrl)`                                      | Start OTA firmware download and install                                                                       | `bool`           | `true` = started successfully               |
| `onOTAStatus(callback)`                                        | Register OTA status callback                                                                                  | `void`           | `(const char *status, const char *message)` |

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

| Function       | Description                                   | Return Type | Possible Values              |
| -------------- | --------------------------------------------- | ----------- | ---------------------------- |
| `isValid()`    | Check if payload was parsed successfully      | `bool`      | `true`, `false`              |
| `isPending()`  | True if this is a pending payment message     | `bool`      | `true`, `false`              |
| `isSuccess()`  | True if this is a payment success message     | `bool`      | `true`, `false`              |
| `isPostPaid()` | True for a postpaid order message             | `bool`      | `true`, `false`              |
| `isPrePaid()`  | True for a prepaid pending or success message | `bool`      | `true`, `false`              |
| `itemTitle()`  | Menu item title                               | `String`    | e.g. `"Cappuccino"`          |
| `price()`      | Price as string                               | `String`    | e.g. `"45000"`               |
| `orderId()`    | Human-readable order ID                       | `String`    | e.g. `"ODR-260506-12345678"` |
| `quantity()`   | Item quantity                                 | `int`       | e.g. `1`                     |
| `metadata()`   | Raw JSON payload                              | `String`    | Full JSON string             |

### Terminal QR Object

Typed payload for terminal business mode (`md: 3` in init): a device-entered monetary amount without a catalog or a service session.

| Member / Function | Description                                                                 | Return Type | Possible Values                                                                     |
| ------------- | --------------------------------------------------------------------------- | -------- | ----------------------------------------------------------------------------- |
| `orderCode`   | Human-readable order code                                                   | `String` | e.g. `"ODR-260506-12345678"`                                                  |
| `status`      | Transaction status; a non-standard value is a failure code                  | `String` | `pending`, `paid`, `expired`, `canceled`, `idle`, or failure code (see below) |
| `amount`      | Amount as integer-string IDR                                                | `String` | e.g. `"25000"`                                                                |
| `qrString`    | Raw QRIS content — render it with the application's QR encoder (not base64) | `String` | QRIS string                                                                   |
| `expiresAt`   | QR expiry                                                                   | `String` | e.g. `"2026-05-06T12:00:00Z"`                                                 |
| `isValid()`   | Check if payload was parsed successfully                                    | `bool`   | `true`, `false`                                                               |
| `isPending()` | True if waiting for payment                                                 | `bool`   | `true`, `false`                                                               |
| `isPaid()`    | True if payment settled                                                     | `bool`   | `true`, `false`                                                               |
| `isIdle()`    | True if device terminal is idle                                             | `bool`   | `true`, `false`                                                               |
| `hasError()`  | True if `status` carries a failure code (no separate error field)           | `bool`   | `true`, `false`                                                               |
| `isFailed()`  | True for an ended failed transaction (expired/canceled/failure, has `oc`)   | `bool`   | `true`, `false`                                                               |

Failure codes in `status`: `INVALID_INPUT`, `TERMINAL_MISMATCH` ("not yours" — only this device's own transaction can be cancelled), `PAYMENT_ALREADY_PAID`, `PAYMENT_BUSY`, `PAYMENT_CANCEL_FAILED`, `TERMINAL_FEE_POLICY_REQUIRED`.

### Terminal Methods

| Function                            | Description                                                                                                                                          | Return Type    | Possible Values                                                                                                                                                                                          |
| ----------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------- | -------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `setAmountTerminalQR(amount)`       | Charge an amount (digits only, whole IDR 1000–99999999): sets the amount and publishes the request in one call; the library generates the request id | `const char *` | `nullptr` = sent; otherwise a reason to show the operator (`amount is empty`, `digits only`, `amount too long`, `Failed to sent message`). A range rejection arrives later as a failure code in `status` |
| `cancelTerminalQR()`                | Cancel the transaction this device has open — no request id needed, no local QR object to keep                                                       | `bool`         | `true` = sent, `false` = fail                                                                                                                                                                            |
| `onTerminalQR(callback)`            | Register typed callback for QR events and request rejections                                                                                         | `void`         | `TerminalQR &`                                                                                                                                                                                           |
| `onTerminalPaymentFailed(callback)` | Register typed callback for failed/expired/canceled transactions                                                                                     | `void`         | `TerminalQR &`                                                                                                                                                                                           |
| `Inits.isTerminalMode()`            | True when the device runs in terminal mode (`md: 3`)                                                                                                 | `bool`         | `true`, `false`                                                                                                                                                                                          |

See `examples/TerminalQR/TerminalQR.ino` for a complete sketch. Events can repeat (QoS 1 + retries), so callbacks must be idempotent. A rejection of a newer input leaves the current QR visible; a late outcome for an older transaction cannot clear a newer QR — track the latest request id separately from the displayed order code.

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

| Function                   | Description                                  | Return Type | Possible Values                                                                                                 |
| -------------------------- | -------------------------------------------- | ----------- | --------------------------------------------------------------------------------------------------------------- |
| `isValid()`                | Check if payload was parsed successfully     | `bool`      | `true`, `false`                                                                                                 |
| `entities()`               | Raw JSON array of GPIO entities              | `String`    | e.g. `[{"p":1,"v":"0","t":41}]`                                                                                 |
| `merchantStatus()`         | Current merchant status                      | `String`    | `"idle"`, `"pending_payment"`, `"expired_payment"`, `"success_payment"`, `"maintenance"`, `"on_active_service"` |
| `activeOrderId()`          | Active paid order code from init             | `String`    | e.g. `"ODR-260601-12345678"`                                                                                    |
| `audioStreamUrl()`         | Stream endpoint from a `t=5` or `t=6` entity | `String`    | Empty when the device has no audio entity                                                                       |
| `verificationFlag()`       | Verification mode flag                       | `int`       | `0`, `1`, `2`, `3`                                                                                              |
| `isIdle()`                 | Merchant status is idle                      | `bool`      | `true`, `false`                                                                                                 |
| `isPendingPayment()`       | Merchant status is pending payment           | `bool`      | `true`, `false`                                                                                                 |
| `isExpiredPayment()`       | Merchant status is expired payment           | `bool`      | `true`, `false`                                                                                                 |
| `isSuccessPayment()`       | Merchant status is success payment           | `bool`      | `true`, `false`                                                                                                 |
| `isMaintenance()`          | Merchant status is maintenance               | `bool`      | `true`, `false`                                                                                                 |
| `isOnActiveService()`      | Merchant status is on active service         | `bool`      | `true`, `false`                                                                                                 |
| `isPinEnabled()`           | PIN verification is enabled                  | `bool`      | `true`, `false`                                                                                                 |
| `isPreconditionEnabled()`  | Precondition verification is enabled         | `bool`      | `true`, `false`                                                                                                 |
| `isVerificationRequired()` | Some form of verification is required        | `bool`      | `true`, `false`                                                                                                 |
| `isServiceMode()`          | Business mode is service                     | `bool`      | `true`, `false`                                                                                                 |
| `isVendingMode()`          | Business mode is vending                     | `bool`      | `true`, `false`                                                                                                 |
| `isTerminalMode()`         | Business mode is terminal (`md: 3`)          | `bool`      | `true`, `false`                                                                                                 |
| `metadata()`               | Raw JSON payload                             | `String`    | Full JSON string                                                                                                |

### Dispenses Object

| Function      | Description                              | Return Type | Possible Values     |
| ------------- | ---------------------------------------- | ----------- | ------------------- |
| `isValid()`   | Check if payload was parsed successfully | `bool`      | `true`, `false`     |
| `itemCount()` | Number of SKUs in the dispense payload   | `int`       | e.g. `1`, `3`       |
| `skuAt(i)`    | SKU string at index `i`                  | `String`    | e.g. `"bohlam_Aj8"` |
| `metadata()`  | Raw dispense payload                     | `String`    | Full payload string |

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

| Macro                               | Kind                      | Payload                            | Style                | Example                                                              |
| ----------------------------------- | ------------------------- | ---------------------------------- | -------------------- | -------------------------------------------------------------------- |
| `ON_ENTITY(entityObject, callback)` | Entity callback           | `EntityCommand &`                  | Inline lambda        | `ON_ENTITY(relay1, [](EntityCommand &cmd){ ... });`                  |
| `ON_ENTITY_S(key, callback)`        | Entity callback           | `EntityCommand &`                  | Inline lambda        | `ON_ENTITY_S("led_1", [](EntityCommand &cmd){ ... });`               |
| `ON_AUDIO(entityObject, callback)`  | Audio input/output entity | `FirmnginAudioEvent, const char *` | Inline lambda        | `ON_AUDIO(speaker, [](FirmnginAudioEvent e, const char *k){ ... });` |
| `fngin.onEntity(key, callback)`     | Entity callback           | `EntityCommand &`                  | Runtime registration | `fngin.onEntity("status", [](EntityCommand &cmd){ ... });`           |
| `ON_ENTITIES(cmd)`                  | Global entity callback    | `EntityCommand &`                  | Macro body           | `ON_ENTITIES(cmd) { ... }`                                           |
| `ON_VERIFICATIONS(v)`               | Verification flow         | `Verifications &`                  | Macro body           | `ON_VERIFICATIONS(v) { ... }`                                        |
| `ON_PAYMENTS(p)`                    | Payment flow              | `Payments &`                       | Macro body           | `ON_PAYMENTS(p) { ... }`                                             |
| `ON_DISPENSES(d)`                   | Vending dispense          | `Dispenses &`                      | Macro body           | `ON_DISPENSES(d) { for (...) d.skuAt(i); }`                          |
| `ON_USAGES(u)`                      | Usage flow                | `Usages &`                         | Macro body           | `ON_USAGES(u) { ... }`                                               |
| `ON_DEVICE_STATUS(ds)`              | Device status flow        | `DeviceStates &`                   | Macro body           | `ON_DEVICE_STATUS(ds) { ... }`                                       |
| `ON_INIT(i)`                        | Init flow                 | `Inits &`                          | Macro body           | `ON_INIT(i) { i.isVendingMode(); }`                                  |
| `ON_OTA_STATUS(status, message)`    | OTA status                | `const char *, const char *`       | Macro body           | `ON_OTA_STATUS(status, message) { ... }`                             |
| `ON_ACTIVE_SESSION(s)`              | Active session            | `ActiveSession &`                  | Macro body           | `ON_ACTIVE_SESSION(s) { ... }`                                       |

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
