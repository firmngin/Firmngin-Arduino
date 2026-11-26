#ifndef FIRMNGIN_GATEWAY_H
#define FIRMNGIN_GATEWAY_H
#include "fng_compat.h"
#include <stdint.h>
#include <functional>
#include <vector>

struct GatewayRoute {
    String localDeviceId;
    String deviceId;
    int64_t routeVersion = 0;
};

// Values remain JSON, preserving booleans, numbers and structured entity values.
struct GatewayChildCommand {
    String commandId;
    String localDeviceId;
    int64_t routeVersion = 0;
    String key;
    String valueJson;
    int64_t expiresAt = 0;
};

using GatewayRoutesCallback = std::function<void(const std::vector<GatewayRoute> &)>;
using GatewayCommandCallback = std::function<void(const GatewayChildCommand &)>;
#endif
