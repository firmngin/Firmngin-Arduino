#ifndef FIRMNGIN_IDENTITY_H
#define FIRMNGIN_IDENTITY_H

#include <Arduino.h>

struct FirmnginIdentityRecord
{
    String deviceId;
    String deviceKey;
    String decryptor;
    String tlsMode;
    String fingerprintHex;
    String caCert;
    String serviceCaCert;
    String clientCert;
    String privateKey;
    String brokerAddr;
    int brokerPort = 0;
    String apiBaseUrl;
    String otaBaseUrl;
    // Audio gateway endpoint and the token for this device's stream. Both are
    // optional: a device without them still derives an endpoint from the gateway base.
    String audioStreamUrl;
    String audioStreamToken;
};

namespace FirmnginIdentity
{
    bool isProvisioned();
    bool load(FirmnginIdentityRecord &out);
    bool save(const FirmnginIdentityRecord &record);
    bool clear();
    void pollSerialProvision();
}

#endif
