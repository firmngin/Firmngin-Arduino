#ifndef FIRMNGIN_JSON_H
#define FIRMNGIN_JSON_H

#include <Arduino.h>

namespace firmngin_json {

// ==== Internal helpers ====================================================

inline bool _isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

inline const char* _skipSpace(const char* p, const char* end)
{
    while (p < end && _isSpace(*p)) p++;
    return p;
}

// Search for "KEY": pattern in JSON buffer. Returns pointer after ':' or nullptr.
// Validates context: key must be preceded by '{' or ',' (with optional whitespace).
inline const char* _findKey(const char* json, size_t len, const char* key)
{
    size_t keyLen = strlen(key);
    if (keyLen == 0) return nullptr;

    const char* end = json + len;
    for (const char* p = json; p < end; p++) {
        if (*p != '"') continue;

        // Check if this could be our key
        const char* keyStart = p + 1;
        if (keyStart + keyLen >= end) continue;
        if (keyStart[keyLen] != '"') continue;
        if (strncmp(keyStart, key, keyLen) != 0) continue;

        // Validate context: before the opening quote, scan back through whitespace
        const char* before = p;
        bool valid = (before == json);
        if (!valid) {
            before--;
            while (before > json && _isSpace(*before)) before--;
            valid = (before < json) || (*before == '{' || *before == ',');
        }
        if (!valid) continue;

        // Found key, now find ':'
        const char* afterKey = keyStart + keyLen + 1; // after closing '"'
        afterKey = _skipSpace(afterKey, end);
        if (afterKey < end && *afterKey == ':') {
            return afterKey + 1;
        }
    }
    return nullptr;
}

// Extract a quoted JSON string value. Handles basic escapes (\\", \\\\).
// Returns number of bytes written to out (excluding null terminator), or 0 on failure.
inline size_t _extractString(const char* start, const char* end, char* out, size_t maxLen)
{
    if (start >= end || *start != '"') return 0;
    start++; // skip opening '"'

    size_t w = 0;
    while (start < end && *start != '"') {
        if (*start == '\\' && start + 1 < end) {
            start++;
            if (*start == '"') {
                if (w < maxLen - 1) out[w++] = '"';
            } else if (*start == '\\') {
                if (w < maxLen - 1) out[w++] = '\\';
            } else if (*start == 'n') {
                if (w < maxLen - 1) out[w++] = '\n';
            } else if (*start == 't') {
                if (w < maxLen - 1) out[w++] = '\t';
            } else if (*start == 'r') {
                if (w < maxLen - 1) out[w++] = '\r';
            } else {
                if (w < maxLen - 1) out[w++] = *start;
            }
        } else {
            if (w < maxLen - 1) out[w++] = *start;
        }
        start++;
    }
    out[w] = '\0';
    return w;
}

// Parse a JSON integer value.
inline int _parseInt(const char* start, const char* end, int def)
{
    if (start >= end) return def;
    start = _skipSpace(start, end);
    if (start >= end) return def;

    bool negative = false;
    if (*start == '-') { negative = true; start++; }
    if (start >= end || !isdigit(*start)) return def;

    int val = 0;
    while (start < end && isdigit(*start)) {
        val = val * 10 + (*start - '0');
        start++;
    }
    return negative ? -val : val;
}

// Parse a JSON boolean value.
inline bool _parseBool(const char* start, const char* end, bool def)
{
    if (start >= end) return def;
    start = _skipSpace(start, end);
    if (start + 4 <= end && strncmp(start, "true", 4) == 0) return true;
    if (start + 5 <= end && strncmp(start, "false", 5) == 0) return false;
    return def;
}

// Extract raw JSON value (string, number, object, array, bool, null) as a substring.
// Handles nested {} and [] by tracking depth.
// Returns pointer to start of value and sets *outLen.
inline const char* _extractRaw(const char* start, const char* end, size_t& outLen)
{
    if (start >= end) return nullptr;
    start = _skipSpace(start, end);
    if (start >= end) return nullptr;

    const char* valStart = start;
    outLen = 0;

    if (*start == '"') {
        // String: scan until closing unescaped "
        start++;
        while (start < end) {
            if (*start == '\\' && start + 1 < end) { start += 2; continue; }
            if (*start == '"') { start++; break; }
            start++;
        }
        outLen = start - valStart;
        return valStart;
    }

    if (*start == '{' || *start == '[') {
        // Object or array: track nesting depth
        char open = *start;
        char close = (open == '{') ? '}' : ']';
        int depth = 1;
        start++;
        while (start < end && depth > 0) {
            if (*start == '"') {
                // Skip strings
                start++;
                while (start < end && *start != '"') {
                    if (*start == '\\' && start + 1 < end) start++;
                    start++;
                }
                if (start < end) start++; // skip closing "
                continue;
            }
            if (*start == open) depth++;
            if (*start == close) depth--;
            start++;
        }
        outLen = start - valStart;
        return valStart;
    }

    // Number, bool, or null: scan until delimiter
    while (start < end && *start != ',' && *start != '}' && *start != ']' && !_isSpace(*start)) {
        start++;
    }
    outLen = start - valStart;
    return valStart;
}

// Escapes a string for JSON. Returns bytes written (excluding null).
inline size_t _jsonEscape(const char* src, char* dst, size_t maxLen)
{
    size_t w = 0;
    while (*src && w < maxLen - 1) {
        switch (*src) {
            case '"':  if (w < maxLen - 2) { dst[w++] = '\\'; dst[w++] = '"';  } break;
            case '\\': if (w < maxLen - 2) { dst[w++] = '\\'; dst[w++] = '\\'; } break;
            case '\n': if (w < maxLen - 2) { dst[w++] = '\\'; dst[w++] = 'n';  } break;
            case '\r': if (w < maxLen - 2) { dst[w++] = '\\'; dst[w++] = 'r';  } break;
            case '\t': if (w < maxLen - 2) { dst[w++] = '\\'; dst[w++] = 't';  } break;
            default:   dst[w++] = *src; break;
        }
        src++;
    }
    dst[w] = '\0';
    return w;
}

// ==========================================================================
// Parser: works directly on a raw JSON buffer. Zero heap allocation.
// ==========================================================================
class Parser {
public:
    Parser(const char* json, size_t length)
        : _json(json), _len(length) {}

    // Check if a key exists in this JSON.
    bool has(const char* key) const {
        return _findKey(_json, _len, key) != nullptr;
    }

    // Extract a string value by key. Returns number of bytes written to out.
    // Returns 0 if key not found or value is not a string.
    size_t getString(const char* key, char* out, size_t maxLen) const {
        const char* val = _findKey(_json, _len, key);
        if (!val) return 0;
        const char* end = _json + _len;
        val = _skipSpace(val, end);
        return _extractString(val, end, out, maxLen);
    }

    // Extract an integer value by key.
    int getInt(const char* key, int def = 0) const {
        const char* val = _findKey(_json, _len, key);
        if (!val) return def;
        return _parseInt(val, _json + _len, def);
    }

    // Extract a boolean value by key.
    bool getBool(const char* key, bool def = false) const {
        const char* val = _findKey(_json, _len, key);
        if (!val) return def;
        return _parseBool(val, _json + _len, def);
    }

    // Extract raw JSON value (including nested objects/arrays) by key.
    // Returns pointer into original buffer and sets outLen. Returns nullptr if not found.
    const char* getRaw(const char* key, size_t& outLen) const {
        const char* val = _findKey(_json, _len, key);
        if (!val) return nullptr;
        return _extractRaw(val, _json + _len, outLen);
    }

    const char* data() const { return _json; }
    size_t length() const { return _len; }

private:
    const char* _json;
    size_t _len;
};

// ==========================================================================
// Builder: builds a JSON object {"key":"val",...} into a caller-owned buffer.
// Zero heap allocation. All operations work within the provided buffer.
// ==========================================================================
class Builder {
public:
    Builder(char* buffer, size_t capacity)
        : _buf(buffer), _cap(capacity), _pos(0), _first(true)
    {
        if (_cap > 0) _buf[0] = '\0';
    }

    // Add a key-value pair. Returns false if buffer is full.
    bool add(const char* key, const char* value) {
        size_t needed = 6; // "":"",
        needed += strlen(key) * 2;   // worst case: all chars need escaping
        needed += strlen(value) * 2;
        if (!_first) needed += 1;    // comma

        if (_pos + needed >= _cap) return false;

        if (!_first) _buf[_pos++] = ',';
        _buf[_pos++] = '"';
        _pos += _jsonEscape(key, _buf + _pos, _cap - _pos);
        _buf[_pos++] = '"';
        _buf[_pos++] = ':';
        _buf[_pos++] = '"';
        _pos += _jsonEscape(value, _buf + _pos, _cap - _pos);
        _buf[_pos++] = '"';

        _first = false;
        return true;
    }

    // Finalize: adds closing brace and returns pointer to buffer.
    const char* build() {
        if (_pos + 2 <= _cap) {
            _buf[_pos++] = '}';
            _buf[_pos] = '\0';
        }
        return _buf;
    }

    // Start building: writes opening brace.
    void reset() {
        if (_cap > 0) {
            _buf[0] = '{';
            _pos = 1;
        }
        _first = true;
    }

    size_t length() const { return _pos; }
    bool full() const { return _pos >= _cap - 2; }

private:
    char* _buf;
    size_t _cap;
    size_t _pos;
    bool _first;
};

// ==========================================================================
// ArrayBuilder: builds a JSON array [{"key":"val"},...] into a caller-owned buffer.
// Zero heap allocation.
// ==========================================================================
class ArrayBuilder {
public:
    ArrayBuilder(char* buffer, size_t capacity)
        : _buf(buffer), _cap(capacity), _pos(0), _count(0), _inObject(false)
    {
        if (_cap > 0) _buf[0] = '\0';
    }

    // Start building: writes opening bracket.
    void reset() {
        if (_cap > 1) {
            _buf[0] = '[';
            _buf[1] = '\0';
            _pos = 1;
        }
        _count = 0;
        _inObject = false;
    }

    // Add a key-value object. Returns false if buffer is full.
    bool add(const char* key, const char* value) {
        size_t needed = 11; // {"":"",}
        needed += strlen(key) * 2;
        needed += strlen(value) * 2;

        if (_pos + needed >= _cap) return false;

        if (_count > 0) _buf[_pos++] = ',';
        _buf[_pos++] = '{';
        _buf[_pos++] = '"';
        _pos += _jsonEscape(key, _buf + _pos, _cap - _pos);
        _buf[_pos++] = '"';
        _buf[_pos++] = ':';
        _buf[_pos++] = '"';
        _pos += _jsonEscape(value, _buf + _pos, _cap - _pos);
        _buf[_pos++] = '"';
        _buf[_pos++] = '}';

        _count++;
        return true;
    }

    // Finalize: adds closing bracket and returns pointer to buffer.
    const char* build() {
        if (_pos + 2 <= _cap) {
            _buf[_pos++] = ']';
            _buf[_pos] = '\0';
        }
        return _buf;
    }

    int count() const { return _count; }

    void clear() {
        _count = 0;
        _pos = 1; // keep the opening '['
        if (_cap > 1) _buf[1] = '\0';
    }

    size_t length() const { return _pos; }
    bool full() const { return _pos >= _cap - 2; }

private:
    char* _buf;
    size_t _cap;
    size_t _pos;
    int _count;
    bool _inObject;
};

} // namespace firmngin_json

#endif // FIRMNGIN_JSON_H
