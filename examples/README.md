# Firmngin Examples

This folder contains examples for using the Firmngin library.

## Setup

1. **Use the shared library**: Include `<firmngin.h>` from the root `src/` folder; do not copy library files into each example
2. **Create keys.h**: For mTLS authentication, create a `keys.h` file with your certificates (see template in `src/keys.h.template`)
3. **Configure WiFi**: Update WiFi credentials in the example
4. **Upload and test**: Use PlatformIO or Arduino IDE to compile and upload

## Examples

### BasicExample
- Shows basic DeviceState usage
- Simple MQTT message handling
- Good starting point for beginners

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
- GPS tracking with TinyGPS++ and `pushBatchEntities()`
- Sends `lat` and `lon` every 10 seconds
- Compatible with NEO-6M, NEO-8M, and similar GPS modules
- No library changes needed — zero library overhead

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
