#include "firmngin_cloudlog.h"

void CloudLog::print(int v) { char b[16]; snprintf(b, sizeof(b), "%d", v); emit(b, false); }
void CloudLog::print(unsigned int v) { char b[16]; snprintf(b, sizeof(b), "%u", v); emit(b, false); }
void CloudLog::print(long v) { char b[24]; snprintf(b, sizeof(b), "%ld", v); emit(b, false); }
void CloudLog::print(unsigned long v) { char b[24]; snprintf(b, sizeof(b), "%lu", v); emit(b, false); }
void CloudLog::print(float v) { char b[40]; snprintf(b, sizeof(b), "%g", static_cast<double>(v)); emit(b, false); }
void CloudLog::print(double v) { char b[40]; snprintf(b, sizeof(b), "%g", v); emit(b, false); }
void CloudLog::print(char v) { char b[2] = {v, 0}; emit(b, false); }

void CloudLog::println(int v) { print(v); emit("", true); }
void CloudLog::println(unsigned int v) { print(v); emit("", true); }
void CloudLog::println(long v) { print(v); emit("", true); }
void CloudLog::println(unsigned long v) { print(v); emit("", true); }
void CloudLog::println(float v) { print(v); emit("", true); }
void CloudLog::println(double v) { print(v); emit("", true); }
void CloudLog::println(char v) { print(v); emit("", true); }

// Mirrors exactly what is about to be buffered, so the Serial view matches the
// cloud view. printf() hands over its whole rendered string; emit() fragments.
void CloudLog::mirrorSerial(const char *text, bool newline, const char *printfOut)
{
    if (printfOut != nullptr)
        Console.printf("%s%s", printfOut, newline ? "\n" : "");
    else
        Console.print(text);
    if (newline)
        Console.println();
}

void CloudLog::appendToLine(const char *text, bool newline)
{
    const size_t textLen = strlen(text);
    if (!_open)
    {
        _len = 0;
        _tsMs = _clockFn != nullptr ? _clockFn() : 0UL;
        _open = true;
    }
    // shortcut: cloud lines cap at FNG_CLOUDLOG_LINE_MAX-1 chars; Serial keeps
    // the full text (mirrorSerial above). Raise the cap if cloud lines get cut.
    const size_t room = FNG_CLOUDLOG_LINE_MAX - 1 - _len;
    const size_t take = textLen < room ? textLen : room;
    memcpy(_lineBuf + _len, text, take);
    _len += take;
    _lineBuf[_len] = '\0';
    if (newline)
        pushLine(_lineBuf, _len);
}

// Framing (same byte order on wire and in ring): [len 2B][ts_ms 4B][text].
// Text is stored newline-free; the sink re-adds it.
void CloudLog::pushLine(const char *line, size_t len)
{
    _open = false;
    const size_t frameLen = len + 6;
    if (_count >= FNG_CLOUDLOG_MAX_SEGMENT ||
        frameLen > sizeof(_ringBuf) - usedBytes())
    {
        _dropped++;
        return;
    }
    for (size_t i = 0; i < 2; i++)
        _ringBuf[(_tail + i) % sizeof(_ringBuf)] = static_cast<uint8_t>((len >> (8 * i)) & 0xFF);
    for (size_t i = 0; i < 4; i++)
        _ringBuf[(_tail + 2 + i) % sizeof(_ringBuf)] = static_cast<uint8_t>((_tsMs >> (8 * i)) & 0xFF);
    for (size_t i = 0; i < len; i++)
        _ringBuf[(_tail + 6 + i) % sizeof(_ringBuf)] = static_cast<uint8_t>(line[i]);
    _tail = (_tail + frameLen) % sizeof(_ringBuf);
    _count++;
}

size_t CloudLog::usedBytes() const
{
    if (_tail >= _head)
        return _tail - _head;
    return (sizeof(_ringBuf) - _head) + _tail;
}

void CloudLog::emit(const char *text, bool newline)
{
    // No-op until begin(): early sketch setup keeps Serial as the single
    // output, no dead buffering and no inflated dropped counters.
    if (!_armed)
        return;
    mirrorSerial(text, newline);
    appendToLine(text, newline);
}

void CloudLog::printf(const char *fmt, ...)
{
    if (!_armed)
        return;
    char buf[FNG_CLOUDLOG_LINE_MAX];
    va_list args;
    va_start(args, fmt);
    const int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (n <= 0)
        return;
    // A format string ending in "\n" must close the line, same as println.
    const bool endsNewline = buf[n < (int)sizeof(buf) ? (size_t)n - 1 : sizeof(buf) - 2] == '\n';
    mirrorSerial(nullptr, false, buf);
    appendToLine(buf, endsNewline);
}

size_t CloudLog::drain()
{
    size_t accepted = 0;
    while (_count > 0 && accepted < FNG_CLOUDLOG_MAX_SEGMENT)
    {
        const size_t head = _head;
        const size_t len = static_cast<size_t>(_ringBuf[head]) |
                           (static_cast<size_t>(_ringBuf[(head + 1) % sizeof(_ringBuf)]) << 8);
        unsigned long tsMs = 0;
        for (size_t i = 0; i < 4; i++)
            tsMs |= static_cast<unsigned long>(_ringBuf[(head + 2 + i) % sizeof(_ringBuf)]) << (8 * i);

        char line[FNG_CLOUDLOG_LINE_MAX];
        for (size_t i = 0; i < len; i++)
            line[i] = static_cast<char>(_ringBuf[(head + 6 + i) % sizeof(_ringBuf)]);
        line[len] = '\0';

        if (_sink == nullptr || !_sink(line, tsMs, _sinkUser))
            break; // sink refused — retry on next drain, order preserved
        _head = (head + len + 6) % sizeof(_ringBuf);
        _count--;
        accepted++;
    }
    return accepted;
}
