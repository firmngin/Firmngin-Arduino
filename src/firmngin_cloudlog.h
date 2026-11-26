#ifndef FIRMNGIN_CLOUDLOG_H
#define FIRMNGIN_CLOUDLOG_H

#include "fng_compat.h"
#include <stdarg.h>
#include <stdio.h>

// CloudLog: `Serial.println` yang sekaligus terkirim ke cloud.
//
// Sketch calls fngin.print/println/printf; every line is mirrored to Serial
// immediately (unchanged local debugging) and buffered in a fixed ring that a
// sink callback drains. The sink is wired to the platform MQTT connection in
// Firmngin::_cloudLogDrain(): lines leave on /d/{deviceId}/lg once hello is
// done, and are dropped (counted) while offline. Buffering is non-blocking on
// purpose — a cloud log must never stall loop() or an ISR-adjacent flow.

#define FNG_CLOUDLOG_LINE_MAX 200
#define FNG_CLOUDLOG_RING_CAPACITY (FNG_CLOUDLOG_LINE_MAX * 8)
// One wrapped publish when a drain is interrupted by a full buffer is fine
// (the JSON segment merges them); above this the publish gets too big.
#define FNG_CLOUDLOG_MAX_SEGMENT 24

#define FNG_CLOUDLOG_RING_NONE 0
#define FNG_CLOUDLOG_RING_OPEN 1

class CloudLog
{
public:
    // Sink returns true when the whole line was accepted for delivery.
    typedef bool (*SinkFn)(const char *line, unsigned long tsMs, void *user);

    // Until begin() the whole class is a no-op: print/println/printf do
    // nothing, so sketches that log during early setup must still use
    // Serial directly and never buffer dead lines or inflate dropped.
    void begin(unsigned long (*clockFn)()) { _clockFn = clockFn; _armed = true; }
    void onSink(SinkFn sink, void *user = nullptr)
    {
        _sink = sink;
        _sinkUser = user;
    }
    bool armed() const { return _armed; }

    // Drop a partially-written line (end of program flow or on clear()/end()).
    void reset()
    {
        _open = false;
        _len = 0;
    }
    void clear()
    {
        _head = 0;
        _tail = 0;
        _count = 0;
        reset();
    }
    void end() { reset(); }

    size_t dropped() const { return _dropped; }
    void resetDropped() { _dropped = 0; }
    bool hasPending() const { return _count > 0 || _open; }
    size_t pendingLines() const { return _count; }

    void print(const char *v) { emit(v != nullptr ? v : "", false); }
    void print(const FString &v) { emit(v.c_str(), false); }
    void print(int v);
    void print(unsigned int v);
    void print(long v);
    void print(unsigned long v);
    void print(float v);
    void print(double v);
    void print(char v);
    void println(const char *v) { emit(v != nullptr ? v : "", true); }
    void println(const FString &v) { emit(v.c_str(), true); }
    void println(int v);
    void println(unsigned int v);
    void println(long v);
    void println(unsigned long v);
    void println(float v);
    void println(double v);
    void println(char v);
    void println() { emit("", true); }
    void printf(const char *fmt, ...);

    // Pull everything buffered out through the sink. Returns the number of
    // lines accepted; stops at the first sink rejection (e.g. MQTT down) so
    // ordering is preserved for the next drain. Oldest line first.
    size_t drain();

private:
    void emit(const char *text, bool newline);
    void appendToLine(const char *text, bool newline);
    void pushLine(const char *line, size_t len);
    void mirrorSerial(const char *text, bool newline, const char *printfOut = nullptr);
    size_t usedBytes() const;

    SinkFn _sink = nullptr;
    void *_sinkUser = nullptr;
    unsigned long (*_clockFn)() = nullptr;
    bool _armed = false; // print/println/printf become no-ops until begin()

    // Write side: _ringBuf[_head..] holds the oldest line, each prefixed by a
    // 2-byte length so multi-line segments can walk the ring backwards safely.
    uint8_t _ringBuf[FNG_CLOUDLOG_RING_CAPACITY] = {};
    size_t _head = 0; // ring index of the oldest pending line
    size_t _tail = 0; // ring index one past the newest byte
    size_t _count = 0; // pending (already-newline-terminated) lines
    size_t _dropped = 0;

    // Current half-open line before a newline closes it onto the ring. This is
    // what "print then println" splits across two calls.
    char _lineBuf[FNG_CLOUDLOG_LINE_MAX];
    size_t _len = 0;
    unsigned long _tsMs = 0;
    bool _open = false; // true while the line above has bytes but no newline yet
};

#endif // FIRMNGIN_CLOUDLOG_H
