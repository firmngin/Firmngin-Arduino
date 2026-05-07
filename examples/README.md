# FirmnginKit Examples

This folder contains examples for using the FirmnginKit library.

## Setup

1. **Copy library files**: Copy `firmnginKit.h` and `firmnginKit.cpp` from `src/` folder to your example folder
2. **Create keys.h**: For mTLS authentication, create a `keys.h` file with your certificates (see template in `src/keys.h.template`)
3. **Configure WiFi**: Update WiFi credentials in the example
4. **Upload and test**: Use PlatformIO or Arduino IDE to compile and upload

## Examples

### BasicExample
- Shows basic DeviceState usage
- Simple MQTT message handling
- Good starting point for beginners

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
