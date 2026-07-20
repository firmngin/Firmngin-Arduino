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
