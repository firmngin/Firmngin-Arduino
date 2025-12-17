/*
 * FirmnginKit Ethernet Example
 *
 * Example of using setClient() for Ethernet connection
 * Supports: W5500, ENC28J60, or other Ethernet Shields
 *
 * website: https://firmngin.dev
 * author: (Arif) Firmngin.dev
 */

#include "keys.h"
#include "firmnginKit.h"
#include <SPI.h>
#include <Ethernet.h>

#define DEVICE_ID "FNG_YOUR_DEVICE_ID"
#define DEVICE_KEY "FNG_YOUR_DEVICE_KEY"


#if defined(ESP8266)
FirmnginKit fngin(DEVICE_ID, DEVICE_KEY, CLIENT_CERT, PRIVATE_KEY, SERVER_FINGERPRINT_BYTES);
#elif defined(ESP32)
FirmnginKit fngin(DEVICE_ID, DEVICE_KEY, SERVER_FINGERPRINT_BYTES, CLIENT_CERT, PRIVATE_KEY);
#endif


// MAC Address (can be changed as needed)
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// Ethernet Client
EthernetClient ethClient;

// PinMap for relay control
PinMap relay1(10, 2);
PinMap relay2(20, 4, ACTIVE_LOW);

ON_VPIN(relay1);
ON_VPIN(relay2);

void setup()
{
  Serial.begin(115200);
  Serial.println("FirmnginKit Ethernet Example");

  // Initialize Ethernet with DHCP
  Serial.print("Initializing Ethernet with DHCP...");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed!");
    
    // If DHCP fails, try with static IP
    IPAddress ip(192, 168, 1, 100);
    IPAddress dns(8, 8, 8, 8);
    IPAddress gateway(192, 168, 1, 1);
    IPAddress subnet(255, 255, 255, 0);
    Ethernet.begin(mac, ip, dns, gateway, subnet);
    
    Serial.print("Using static IP: ");
    Serial.println(Ethernet.localIP());
  } else {
    Serial.println("OK!");
    Serial.print("IP Address: ");
    Serial.println(Ethernet.localIP());
  }

  // Set custom Ethernet client
  fngin.setClient(ethClient);

  fngin.begin();
}

void loop()
{
  // Maintain Ethernet connection
  Ethernet.maintain();
  
  fngin.loop();
  delay(100);
}
