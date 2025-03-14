# Firmngin-Kit Library

This library make easy flow payment systems to your IoT devices with simple and secure communication, opening new monetization opportunities for your system 💰.

Check this out [kit.firmngin.cloud](https://kit.firmngin.cloud) for more information and try for free

## Features

- ESP8266 and ESP32 support
- Auto reconnect to server
- Event-driven callback system

## Installation

1. Make sure you're using an ESP8266 or ESP32 board
2. Search library "firmnginKit" on Library Manager and install. [Click here to Learn how to install Arduino Library](https://docs.arduino.cc/software/ide-v1/tutorials/installing-libraries/)
3. Install required library dependencies:
   - ArduinoJson
   - PubSubClient

## Example Basic Usage

```cpp
#include "firmnginKit.h"

#define DEVICE_ID "YOUR_DEVICE_ID"
#define DEVICE_KEY "YOUR_DEVICE_KEY"

FirmnginKit fngin(DEVICE_ID, DEVICE_KEY);

void callback(Callback& cb) {
  cb.on(WAIT, [](Payload payload) {
    // Handle WAIT event
  });

  cb.on(IDLE, [](Payload payload) {
    // Handle IDLE event 
  });

//   other events
}

void setup() {
  // Connect WiFi first
  fngin.setCallback(callback);
  fngin.setDebug(true);
  fngin.begin();
}

void loop() {
  fngin.loop();
}
```

## Event States

The library supports several event states:

- `WAIT` - Device is in waiting status, user start transactions and wait for paid
- `IDLE` - Device is idle ready for accept transactions
- `OK` - Operation successful
- `HOLD` - Device is on hold
- `START_SESSION` - Session started, User successfully paid
- `END_SESSION` - Session ended
- `ERROR` - Error occurred

## Payload Data

Each event callback receives a Payload object containing:

- `referenceId` - Transaction reference ID
- `EventName` - Current event name
- `ActiveSessionID` - Active session ID
- `Amount` - Transaction amount
- `MessageId` - Message ID (Will generated every message sent from server)
- `Optionname` - Menu Option name
- `Metadata` - Additional data from users in JSON format

## Configuration

```cpp
// Set timezone (default is +7 Indonesia)
fngin.setTimezone(7);

// Enable/disable debug mode
fngin.setDebug(true);

// Set callback event
fngin.setCallback(callback);
```

## Troubleshooting

1. Ensure device ID and key are valid
2. Check if WiFi connection is stable
3. Enable debug mode for detailed logs
4. Restart device if connection issues persist

## License

MIT License

## Contributing

Feel free to open issues, ideas or submit pull requests. 
