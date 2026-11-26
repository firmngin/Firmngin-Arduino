#include "firmngin.h"
#include <algorithm>
#include <stdio.h>
#include <time.h>
#if defined(ESP_PLATFORM)
#include <cJSON.h>
#include <esp_random.h>
#endif

void Firmngin::enableGateway(bool enabled)
{
    _gatewayEnabled = enabled;
    _gatewaySynced = false;
    _gatewaySession = "";
    _gatewayRoutes.clear();
    _gatewayOnline.clear();
    _gatewayPending.clear();
#if defined(ESP_PLATFORM)
    if (enabled && _mqttIsConnected()) {
        String prefix = String("/d/") + _deviceId + "/gw/";
        esp_mqtt_client_subscribe(_mqtt, (prefix + "routes").c_str(), 0);
        esp_mqtt_client_subscribe(_mqtt, (prefix + "command").c_str(), 0);
        syncGatewayRoutes();
    }
#endif
}

void Firmngin::onGatewayRoutes(GatewayRoutesCallback callback) { _gatewayRoutesCallback = callback; }
void Firmngin::onChildCommand(GatewayCommandCallback callback) { _gatewayCommandCallback = callback; }

bool Firmngin::syncGatewayRoutes()
{
    if (!_gatewayEnabled || !_mqttIsConnected()) return false;
    // Keep the current snapshot until the authoritative replacement arrives.
    // Initial connection/reconnect readiness is reset by _gatewayConnected().
    _gatewayLastRequestMs = fng_now_ms();
    return _gatewayPublish("routes/request", "{\"v\":1}");
}

void Firmngin::_gatewayConnected()
{
    if (!_gatewayEnabled) return;
    _gatewaySynced = false;
    _gatewaySession = "";
    _gatewayRoutes.clear();
    _gatewayOnline.clear();
    _gatewayPending.clear();
    syncGatewayRoutes();
}

#if defined(ESP_PLATFORM)
namespace {
struct JsonOwner {
    cJSON *p;
    explicit JsonOwner(cJSON *value) : p(value) {}
    ~JsonOwner() { cJSON_Delete(p); }
    JsonOwner(const JsonOwner &) = delete;
    JsonOwner &operator=(const JsonOwner &) = delete;
};
String text(cJSON *object, const char *key)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsString(item) && item->valuestring ? String(item->valuestring) : String("");
}
int64_t version(cJSON *object)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(object, "route_version");
    if (!cJSON_IsNumber(item) || item->valuedouble < 1 || item->valuedouble > 9007199254740991.0) return 0;
    int64_t result = static_cast<int64_t>(item->valuedouble);
    return static_cast<double>(result) == item->valuedouble ? result : 0;
}
String encode(cJSON *object)
{
    char *raw = cJSON_PrintUnformatted(object);
    if (!raw) return "";
    String result(raw);
    cJSON_free(raw);
    return result;
}
String utcNow()
{
    time_t now = time(nullptr);
    if (now < 1700000000) return "";
    struct tm value = {};
    gmtime_r(&now, &value);
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &value);
    return String(buffer);
}
int64_t parseUTC(const String &input)
{
    // Commands use RFC3339 UTC. Fractional seconds are accepted, timezone offsets
    // are deliberately rejected rather than interpreted in the firmware timezone.
    int y, m, d, hour, minute, second, end = 0;
    if (sscanf(input.c_str(), "%d-%d-%dT%d:%d:%d%n", &y, &m, &d, &hour, &minute, &second, &end) != 6) return 0;
    const char *tail = input.c_str() + end;
    if (*tail == '.') { ++tail; if (*tail < '0' || *tail > '9') return 0; while (*tail >= '0' && *tail <= '9') ++tail; }
    if (tail[0] != 'Z' || tail[1] != 0 || y < 2020 || m < 1 || m > 12 || d < 1 || d > 31 || hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) return 0;
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned year = static_cast<unsigned>(y - era * 400);
    const unsigned day = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int64_t days = static_cast<int64_t>(era) * 146097 + year * 365 + year / 4 - year / 100 + day - 719468;
    return days * 86400 + hour * 3600 + minute * 60 + second;
}
}

String Firmngin::_gatewayEnvelope(const char *localDeviceId)
{
    auto route = _gatewayRoutes.find(String(localDeviceId ? localDeviceId : ""));
    if (!_gatewayEnabled || !_gatewaySynced || _gatewaySession.length() == 0 || !_mqttIsConnected() || route == _gatewayRoutes.end()) return "";
    String observed = utcNow();
    if (observed.length() == 0) return "";
    JsonOwner object(cJSON_CreateObject());
    if (!object.p) return "";
    char id[80];
    snprintf(id, sizeof(id), "%08lx-%08lx-%08lx", static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(++_gatewaySequence));
    cJSON_AddNumberToObject(object.p, "v", 1);
    cJSON_AddStringToObject(object.p, "message_id", id);
    cJSON_AddStringToObject(object.p, "local_device_id", route->second.localDeviceId.c_str());
    cJSON_AddNumberToObject(object.p, "route_version", static_cast<double>(route->second.routeVersion));
    cJSON_AddStringToObject(object.p, "gateway_session", _gatewaySession.c_str());
    cJSON_AddStringToObject(object.p, "observed_at", observed.c_str());
    return encode(object.p);
}

bool Firmngin::publishChild(const char *localDeviceId, const char *entitiesJson, const char *sensorAt)
{
    if (!entitiesJson) return false;
    String envelope = _gatewayEnvelope(localDeviceId);
    if (envelope.length() == 0 || !_gatewayOnline[String(localDeviceId)]) return false;
    JsonOwner object(cJSON_Parse(envelope.c_str()));
    JsonOwner entities(cJSON_Parse(entitiesJson));
    if (!object.p || !cJSON_IsObject(entities.p) || cJSON_GetArraySize(entities.p) == 0) return false;
    cJSON_AddItemToObject(object.p, "entities", entities.p);
    entities.p = nullptr;
    if (sensorAt && sensorAt[0]) cJSON_AddStringToObject(object.p, "sensor_at", sensorAt);
    return _gatewayPublish("telemetry", encode(object.p));
}

bool Firmngin::reportChildStatus(const char *localDeviceId, bool online)
{
    String envelope = _gatewayEnvelope(localDeviceId);
    if (envelope.length() == 0) return false;
    JsonOwner object(cJSON_Parse(envelope.c_str()));
    if (!object.p) return false;
    cJSON_AddStringToObject(object.p, "status", online ? "online" : "offline");
    bool sent = _gatewayPublish("status", encode(object.p));
    _gatewayOnline[String(localDeviceId)] = sent && online;
    return sent;
}

bool Firmngin::_gatewayAck(const GatewayChildCommand &command, const char *result)
{
    String envelope = _gatewayEnvelope(command.localDeviceId.c_str());
    if (envelope.length() == 0) return false;
    JsonOwner object(cJSON_Parse(envelope.c_str()));
    if (!object.p) return false;
    cJSON_AddStringToObject(object.p, "command_id", command.commandId.c_str());
    cJSON_AddStringToObject(object.p, "result", result);
    return _gatewayPublish("ack", encode(object.p));
}

bool Firmngin::reportChildCommandResult(const char *commandId, const char *result)
{
    if (!commandId || !result) return false;
    if (strcmp(result, "sensor_confirmed") && strcmp(result, "failed") && strcmp(result, "expired") && strcmp(result, "unconfirmed")) return false;
    auto found = _gatewayPending.find(String(commandId));
    if (found == _gatewayPending.end()) return false;
    const GatewayChildCommand command = found->second;
    // A late confirmation cannot revive an expired command.
    if (time(nullptr) >= command.expiresAt) result = "unconfirmed";
    bool sent = _gatewayAck(command, result);
    _gatewayPending.erase(found);
    return sent;
}

bool Firmngin::_gatewayHandle(const String &topic, const String &payload)
{
    String prefix = String("/d/") + _deviceId + "/gw/";
    if (!fng_startsWith(topic, prefix.c_str())) return false;
    if (!_gatewayEnabled || !_e2eeEnabled || _e2eeKeyLen == 0 || _mqttHelloPending || !_mqttIsConnected()) return true;
    JsonOwner root(cJSON_Parse(payload.c_str()));
    cJSON *v = root.p ? cJSON_GetObjectItemCaseSensitive(root.p, "v") : nullptr;
    if (!cJSON_IsNumber(v) || v->valuedouble != 1) return true;
    if (topic == prefix + "routes") {
        String session = text(root.p, "gateway_session");
        if (session.length() == 0 || session.length() > 100) return true;
        const bool sameSession = session == _gatewaySession;
        cJSON *routes = cJSON_GetObjectItemCaseSensitive(root.p, "routes");
        if (!cJSON_IsArray(routes) || cJSON_GetArraySize(routes) > 512) return true;
        std::map<String, GatewayRoute> next;
        cJSON *item = nullptr;
        cJSON_ArrayForEach(item, routes) {
            GatewayRoute route;
            route.localDeviceId = text(item, "local_device_id");
            route.deviceId = text(item, "device_id");
            route.routeVersion = version(item);
            if (route.localDeviceId.length() == 0 || route.deviceId.length() == 0 || route.routeVersion == 0 || next.count(route.localDeviceId)) return true;
            next[route.localDeviceId] = route;
        }
        // Preserve in-flight commands only while their exact device route is
        // still assigned. A periodic refresh must not discard valid sensor ACKs.
        for (auto pending = _gatewayPending.begin(); pending != _gatewayPending.end();) {
            auto route = next.find(pending->second.localDeviceId);
            auto previous = _gatewayRoutes.find(pending->second.localDeviceId);
            if (!sameSession || route == next.end() || previous == _gatewayRoutes.end() || route->second.routeVersion != pending->second.routeVersion || route->second.deviceId != previous->second.deviceId)
                pending = _gatewayPending.erase(pending);
            else ++pending;
        }
        std::map<String, bool> online;
        for (const auto &entry : next) {
            auto previous = _gatewayRoutes.find(entry.first);
            if (sameSession && previous != _gatewayRoutes.end() && previous->second.deviceId == entry.second.deviceId && previous->second.routeVersion == entry.second.routeVersion)
                online[entry.first] = _gatewayOnline[entry.first];
        }
        _gatewayRoutes = std::move(next);
        _gatewaySession = session;
        _gatewayOnline = std::move(online);
        _gatewaySynced = true;
        _gatewayLastRequestMs = fng_now_ms();
        if (_gatewayRoutesCallback) {
            std::vector<GatewayRoute> copy;
            for (const auto &route : _gatewayRoutes) copy.push_back(route.second);
            _gatewayRoutesCallback(copy); // App probes its local transport and reports each child status.
        }
        return true;
    }
    if (topic != prefix + "command" || !_gatewaySynced || text(root.p, "gateway_session") != _gatewaySession) return true;
    GatewayChildCommand command;
    command.commandId = text(root.p, "command_id");
    command.localDeviceId = text(root.p, "local_device_id");
    command.routeVersion = version(root.p);
    command.expiresAt = parseUTC(text(root.p, "expires_at"));
    cJSON *body = cJSON_GetObjectItemCaseSensitive(root.p, "payload");
    command.key = text(body, "k");
    cJSON *pin = cJSON_GetObjectItemCaseSensitive(body, "k");
    if (cJSON_IsNumber(pin) && pin->valuedouble >= 0 && pin->valuedouble <= 2147483647 && static_cast<double>(pin->valueint) == pin->valuedouble) {
        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%d", pin->valueint);
        command.key = buffer;
    }
    cJSON *value = cJSON_GetObjectItemCaseSensitive(body, "v");
    auto route = _gatewayRoutes.find(command.localDeviceId);
    if (command.commandId.length() == 0 || command.commandId.length() > 128 || command.key.length() == 0 || !value || route == _gatewayRoutes.end() || route->second.routeVersion != command.routeVersion || !_gatewayOnline[command.localDeviceId]) return true;
    if (_gatewaySeen.count(command.commandId)) return true;
    time_t now = time(nullptr);
    if (now < 1700000000 || command.expiresAt == 0) return true;
    if (now >= command.expiresAt) { _gatewayAck(command, "expired"); return true; }
    if (_gatewayPending.size() >= 64 || _gatewaySeen.size() >= 256) { _gatewayAck(command, "failed"); return true; }
    command.valueJson = encode(value);
    if (command.valueJson.length() == 0) return true;
    _gatewaySeen[command.commandId] = command.expiresAt;
    if (!_gatewayCommandCallback) { _gatewayAck(command, "failed"); return true; }
    if (!_gatewayAck(command, "gateway_received")) return true;
    _gatewayPending[command.commandId] = command;
    _gatewayCommandCallback(command); // Separate from gateway's own entity callbacks.
    return true;
}

void Firmngin::_gatewayTick()
{
    if (!_gatewayEnabled || !_mqttIsConnected() || _mqttHelloPending) return;
    time_t now = time(nullptr);
    for (auto it = _gatewaySeen.begin(); it != _gatewaySeen.end();) {
        if (now >= it->second) it = _gatewaySeen.erase(it);
        else ++it;
    }
    // A request lost during connect or before NTP sync is retried. A periodic
    // snapshot discovers new bindings and revokes local readiness for old routes.
    const unsigned long interval = _gatewaySynced ? 60000UL : 5000UL;
    if (fng_now_ms() - _gatewayLastRequestMs >= interval) {
        syncGatewayRoutes();
        return;
    }
    if (!_gatewaySynced) return;
    for (auto it = _gatewayPending.begin(); it != _gatewayPending.end();) {
        if (now >= it->second.expiresAt) {
            _gatewayAck(it->second, "unconfirmed");
            it = _gatewayPending.erase(it);
        } else ++it;
    }
}
#else
String Firmngin::_gatewayEnvelope(const char *) { return ""; }
bool Firmngin::publishChild(const char *, const char *, const char *) { return false; }
bool Firmngin::reportChildStatus(const char *, bool) { return false; }
bool Firmngin::reportChildCommandResult(const char *, const char *) { return false; }
bool Firmngin::_gatewayAck(const GatewayChildCommand &, const char *) { return false; }
bool Firmngin::_gatewayHandle(const String &, const String &) { return false; }
void Firmngin::_gatewayTick() {}
#endif
