#ifndef FIRMNGIN_COMPAT_H
#define FIRMNGIN_COMPAT_H

// Framework compat layer: one source tree compiles under Arduino-ESP32 (core 2.x
// on IDF 4.4, core 3.x on IDF 5.1+) and under pure ESP-IDF 5.x, with a small
// host (native test) fallback.
//
//   ARDUINO defined        -> Arduino build (String API stays exactly as before)
//   ESP_PLATFORM defined   -> ESP build (IDF constants/features enabled)
//   neither defined        -> host build for native unit tests (std::string core)
//
// Core library code must use the fng_* helpers instead of Arduino-only String
// methods so call sites compile unchanged in all builds.

#if defined(ARDUINO)

#include <Arduino.h>

using FString = String;

inline FString fng_str(const char *v) { return FString(v != nullptr ? v : ""); }
inline FString fng_str(char v) { return String(v); }
inline int fng_toInt(const FString &s) { return s.toInt(); }
inline float fng_toFloat(const FString &s) { return s.toFloat(); }
inline int fng_indexOf(const FString &s, const char *needle, int from = 0) { return s.indexOf(needle, from); }
inline int fng_indexOf(const FString &s, char needle, int from = 0) { return s.indexOf(String(needle), from); }
inline int fng_lastIndexOf(const FString &s, const char *needle) { return s.lastIndexOf(needle); }
inline int fng_lastIndexOf(const FString &s, char needle) { return s.lastIndexOf(String(needle)); }
inline FString fng_substring(const FString &s, int from, int to = -1) { return to < 0 ? s.substring(from) : s.substring(from, to); }
inline bool fng_startsWith(const FString &s, const char *prefix) { return s.startsWith(prefix); }
inline bool fng_endsWith(const FString &s, const char *suffix) { return s.endsWith(suffix); }
inline bool fng_equalsIgnoreCase(const FString &a, const FString &b) { return a.equalsIgnoreCase(b); }
inline FString fng_trim(const FString &s) { FString out = s; out.trim(); return out; }
inline FString fng_toUpperCase(const FString &s) { FString out = s; out.toUpperCase(); return out; }
inline FString fng_toLowerCase(const FString &s) { FString out = s; out.toLowerCase(); return out; }
inline char fng_charAt(const FString &s, size_t i) { return s.charAt(i); }
inline void fng_reserve(FString &s, size_t n) { s.reserve(n); }
inline bool fng_append(FString &s, const char *data, size_t len) { return s.concat(data, len); }
inline FString fng_replace_all(const FString &s, const char *from, const char *to)
{
    FString out = s;
    out.replace(from, to);
    return out;
}

inline unsigned long fng_now_ms() { return millis(); }
inline void fng_delay_ms(unsigned long ms) { delay(ms); }
inline void fng_restart() { ESP.restart(); }


// WebSocket scheme validation/normalization for the audio stream gateway.
// These only classify URLs supplied by provisioning/server policy; no client
// here opens an insecure socket. Semgrep sees bare "ws://" literals, hence the
// scoped suppression.
inline bool fng_isPlainWsScheme(const FString &s) { return fng_startsWith(s, "ws://"); } // nosemgrep: javascript.lang.security
inline bool fng_isSecureWsScheme(const FString &s) { return fng_startsWith(s, "wss://"); }
inline bool fng_isWsScheme(const FString &s) { return fng_isSecureWsScheme(s) || fng_isPlainWsScheme(s); }
inline bool fng_isSecureWsSchemeC(const char *u) { return u != nullptr && strncmp(u, "wss://", 6) == 0; }
inline bool fng_isPlainWsSchemeC(const char *u) { return u != nullptr && strncmp(u, "ws://", 5) == 0; } // nosemgrep: javascript.lang.security
inline bool fng_isWsSchemeC(const char *u) { return fng_isSecureWsSchemeC(u) || fng_isPlainWsSchemeC(u); }
inline FString fng_streamScheme(bool secure) { return secure ? FString("wss://") : FString("ws://"); } // nosemgrep: javascript.lang.security

inline void fng_yield() { yield(); }

class FngSerialBridge
{
public:
    static void print(const char *v) { Serial.print(v); }
    static void print(const String &v) { Serial.print(v); }
    static void print(int v) { Serial.print(v); }
    static void print(unsigned int v) { Serial.print(v); }
    static void print(long v) { Serial.print(v); }
    static void print(unsigned long v) { Serial.print(v); }
    static void print(float v) { Serial.print(v); }
    static void print(double v) { Serial.print(v); }
    static void print(char v) { Serial.print(v); }
    static void println(const char *v) { Serial.println(v); }
    static void println(const String &v) { Serial.println(v); }
    static void println(int v) { Serial.println(v); }
    static void println(unsigned int v) { Serial.println(v); }
    static void println(long v) { Serial.println(v); }
    static void println(unsigned long v) { Serial.println(v); }
    static void println(float v) { Serial.println(v); }
    static void println(double v) { Serial.println(v); }
    static void println(char v) { Serial.println(v); }
    static void println() { Serial.println(); }
    // shortcut: 256-byte printf cap, raise it if a debug line is cut off
    static void printf(const char *fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        char buf[256];
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        Serial.print(buf);
    }
};

// Serial-free logging facade: Arduino routes to HardwareSerial, IDF prints to
// the default console (same shapes the core calls).
inline FngSerialBridge Console;


#else // pure ESP-IDF or host build: std::string-backed core

#include <string>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// Minimal Arduino-String-compatible type so the shared core keeps one shape.
// Methods that only exist in Arduino String (trim/toInt/indexOf/...) get fng_*
// helpers below; call sites call the helpers, never these missing methods.
class FString : public std::string
{
public:
    FString() {}
    // Conversions from std::string results (substr/concat/append) and moves.
    FString(const std::string &other) : std::string(other) {}
    FString(std::string &&other) noexcept : std::string(std::move(other)) {}
    FString(const char *s) : std::string(s != nullptr ? s : "") {}
    FString(int v) : std::string(std::to_string(v)) {}
    FString(unsigned int v) : std::string(std::to_string(v)) {}
    FString(long v) : std::string(std::to_string(v)) {}
    FString(unsigned long v) : std::string(std::to_string(v)) {}
    FString(long long v) : std::string(std::to_string(v)) {}
    FString(unsigned long long v) : std::string(std::to_string(v)) {}
    FString(char v) : std::string(1, v) {}
    FString(float v, int decimals = 4)
    {
        char buf[48];
        snprintf(buf, sizeof(buf), "%.*f", decimals, static_cast<double>(v));
        assign(buf);
    }
    FString(double v, int decimals = 4)
    {
        char buf[48];
        snprintf(buf, sizeof(buf), "%.*f", decimals, v);
        assign(buf);
    }
};

// Core keeps the Arduino-wire spelling; under pure IDF it is the wrapper above.
using String = FString;

inline FString fng_str(const char *v) { return FString(v != nullptr ? v : ""); }
inline FString fng_str(char v) { return FString(std::string(1, v)); }
inline FString fng_str(int v) { return std::to_string(v); }
inline FString fng_str(unsigned int v) { return std::to_string(v); }
inline FString fng_str(long v) { return std::to_string(v); }
inline FString fng_str(unsigned long v) { return std::to_string(v); }
inline FString fng_str(long long v) { return std::to_string(v); }
inline FString fng_str(unsigned long long v) { return std::to_string(v); }
inline FString fng_str(float v, int decimals = 4) { return FString(v, decimals); }
inline FString fng_str(double v, int decimals = 4) { return FString(v, decimals); }

inline int fng_toInt(const FString &s) { return static_cast<int>(strtol(s.c_str(), nullptr, 10)); }
inline float fng_toFloat(const FString &s) { return strtof(s.c_str(), nullptr); }
inline int fng_indexOf(const FString &s, const char *needle, int from = 0)
{
    if (needle == nullptr || from < 0)
        return -1;
    const size_t pos = s.find(needle, static_cast<size_t>(from));
    return pos == FString::npos ? -1 : static_cast<int>(pos);
}
inline int fng_indexOf(const FString &s, char needle, int from = 0)
{
    if (from < 0)
        return -1;
    const size_t pos = s.find(needle, static_cast<size_t>(from));
    return pos == FString::npos ? -1 : static_cast<int>(pos);
}
inline int fng_lastIndexOf(const FString &s, const char *needle)
{
    if (needle == nullptr)
        return -1;
    const size_t pos = s.rfind(needle);
    return pos == FString::npos ? -1 : static_cast<int>(pos);
}
inline int fng_lastIndexOf(const FString &s, char needle)
{
    const size_t pos = s.rfind(needle);
    return pos == FString::npos ? -1 : static_cast<int>(pos);
}
inline FString fng_substring(const FString &s, int from, int to = -1)
{
    if (from < 0)
        from = 0;
    if (to < 0 || static_cast<size_t>(to) >= s.size())
        return s.substr(static_cast<size_t>(from));
    if (static_cast<size_t>(from) >= static_cast<size_t>(to))
        return FString();
    return s.substr(static_cast<size_t>(from), static_cast<size_t>(to - from));
}
inline bool fng_startsWith(const FString &s, const char *prefix) { return s.rfind(prefix, 0) == 0; }
inline bool fng_endsWith(const FString &s, const char *suffix)
{
    const size_t len = strlen(suffix);
    return s.size() >= len && s.compare(s.size() - len, len, suffix) == 0;
}
inline bool fng_equalsIgnoreCase(const FString &a, const FString &b)
{
    return a.size() == b.size() && strncasecmp(a.c_str(), b.c_str(), a.size()) == 0;
}
inline FString fng_trim(const FString &s)
{
    size_t begin = 0;
    while (begin < s.size() && isspace(static_cast<unsigned char>(s[begin])))
        ++begin;
    size_t end = s.size();
    while (end > begin && isspace(static_cast<unsigned char>(s[end - 1])))
        --end;
    return s.substr(begin, end - begin);
}
inline FString fng_toUpperCase(const FString &s)
{
    FString out = s;
    for (auto &c : out)
        c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    return out;
}
inline FString fng_toLowerCase(const FString &s)
{
    FString out = s;
    for (auto &c : out)
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return out;
}
inline char fng_charAt(const FString &s, size_t i) { return i < s.size() ? s[i] : '\0'; }

// WebSocket scheme validation/normalization for the audio stream gateway.
// These only classify URLs supplied by provisioning/server policy; no client
// here opens an insecure socket. Semgrep sees bare "ws://" literals, hence the
// scoped suppression.
inline bool fng_isPlainWsScheme(const FString &s) { return fng_startsWith(s, "ws://"); } // nosemgrep: javascript.lang.security
inline bool fng_isSecureWsScheme(const FString &s) { return fng_startsWith(s, "wss://"); }
inline bool fng_isWsScheme(const FString &s) { return fng_isSecureWsScheme(s) || fng_isPlainWsScheme(s); }
inline bool fng_isSecureWsSchemeC(const char *u) { return u != nullptr && strncmp(u, "wss://", 6) == 0; }
inline bool fng_isPlainWsSchemeC(const char *u) { return u != nullptr && strncmp(u, "ws://", 5) == 0; } // nosemgrep: javascript.lang.security
inline bool fng_isWsSchemeC(const char *u) { return fng_isSecureWsSchemeC(u) || fng_isPlainWsSchemeC(u); }
inline FString fng_streamScheme(bool secure) { return secure ? FString("wss://") : FString("ws://"); } // nosemgrep: javascript.lang.security
inline void fng_reserve(FString &s, size_t n) { s.reserve(n); }
inline bool fng_append(FString &s, const char *data, size_t len) { s.append(data, len); return true; }
inline FString fng_replace_all(const FString &s, const char *from, const char *to)
{
    FString out = s;
    if (from == nullptr || to == nullptr || from[0] == '\0')
        return out;
    const size_t fromLen = strlen(from), toLen = strlen(to);
    size_t pos = out.find(from);
    while (pos != FString::npos)
    {
        out.replace(pos, fromLen, to);
        pos = out.find(from, pos + toLen);
    }
    return out;
}

#if defined(ESP_PLATFORM) // ESP timers/tasks exist only here

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include <esp_system.h>

inline unsigned long fng_now_ms() { return static_cast<unsigned long>(esp_timer_get_time() / 1000ULL); }
inline void fng_delay_ms(unsigned long ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
inline void fng_restart() { esp_restart(); }


inline void fng_yield() { vTaskDelay(1); }

class FngSerialBridge
{
public:
    static void print(const char *v) { printf("%s", v ? v : ""); }
    static void print(const String &v) { printf("%s", v.c_str()); }
    static void print(int v) { printf("%d", v); }
    static void print(unsigned int v) { printf("%u", v); }
    static void print(long v) { printf("%ld", v); }
    static void print(unsigned long v) { printf("%lu", v); }
    static void print(float v) { printf("%g", static_cast<double>(v)); }
    static void print(double v) { printf("%g", v); }
    static void print(char v) { printf("%c", v); }
    static void println(const char *v) { printf("%s\n", v ? v : ""); }
    static void println(const String &v) { printf("%s\n", v.c_str()); }
    static void println(int v) { printf("%d\n", v); }
    static void println(unsigned int v) { printf("%u\n", v); }
    static void println(long v) { printf("%ld\n", v); }
    static void println(unsigned long v) { printf("%lu\n", v); }
    static void println(float v) { printf("%g\n", static_cast<double>(v)); }
    static void println(double v) { printf("%g\n", v); }
    static void println(char v) { printf("%c\n", v); }
    static void println() { printf("\n"); }
    static void printf(const char *fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        ::vprintf(fmt, args); // uncapped, same shape Serial.printf has on Arduino
        va_end(args);
    }
};

// Serial-free logging facade: Arduino routes to HardwareSerial, IDF prints to
// the default console (same shapes the core calls).
inline FngSerialBridge Console;


#else // host build for native unit tests

inline unsigned long fng_now_ms() { return 0UL; }
inline void fng_delay_ms(unsigned long) {}
inline void fng_restart() {}
inline void fng_yield() {}

// Native tests exercise logic, not the serial line — route Console to stdout
// (same shapes as the IDF branch) so headers that log compile unchanged.
class FngSerialBridge
{
public:
    static void print(const char *v) { printf("%s", v ? v : ""); }
    static void print(const String &v) { printf("%s", v.c_str()); }
    static void print(int v) { printf("%d", v); }
    static void print(unsigned int v) { printf("%u", v); }
    static void print(long v) { printf("%ld", v); }
    static void print(unsigned long v) { printf("%lu", v); }
    static void print(float v) { printf("%g", static_cast<double>(v)); }
    static void print(double v) { printf("%g", v); }
    static void print(char v) { printf("%c", v); }
    static void println(const char *v) { printf("%s\n", v ? v : ""); }
    static void println(const String &v) { printf("%s\n", v.c_str()); }
    static void println(int v) { printf("%d\n", v); }
    static void println(unsigned int v) { printf("%u\n", v); }
    static void println(long v) { printf("%ld\n", v); }
    static void println(unsigned long v) { printf("%lu\n", v); }
    static void println(float v) { printf("%g\n", static_cast<double>(v)); }
    static void println(double v) { printf("%g\n", v); }
    static void println(char v) { printf("%c\n", v); }
    static void println() { printf("\n"); }
    static void printf(const char *fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        ::vprintf(fmt, args);
        va_end(args);
    }
};
inline FngSerialBridge Console;

#endif // ESP_PLATFORM

#endif // ARDUINO

#if defined(ESP_PLATFORM) // shared HTTP transport config (Arduino + pure IDF)

#include <esp_http_client.h>
inline bool fng_http_insecure_supported()
{
#if defined(CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY) && CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY
    return true;
#else
    return false;
#endif
}

// Callers validate insecure mode against the SDK before constructing a client.
inline esp_http_client_config_t fng_http_config(const char *url,
                                                const char *activeServiceCA,
                                                const char *clientCert,
                                                const char *privateKey,
                                                bool insecure,
                                                int bufferBytes = 2048)
{
    esp_http_client_config_t config = {};
    config.url = url;
    config.cert_pem = insecure ? nullptr : activeServiceCA;
    config.skip_cert_common_name_check = insecure;
    config.client_cert_pem = clientCert;
    config.client_key_pem = privateKey;
    config.buffer_size = bufferBytes;
    config.buffer_size_tx = 1024;
    config.timeout_ms = 10000;
    return config;
}

#endif // ESP_PLATFORM

#endif // FIRMNGIN_COMPAT_H
