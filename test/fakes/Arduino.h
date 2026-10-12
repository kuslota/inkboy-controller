#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <string>
#include <vector>

#define INPUT_PULLUP 2
#define OUTPUT 1
#define HIGH 1
#define LOW 0

extern uint32_t fakeNow;
extern int fakePins[16];

inline uint32_t millis() { return fakeNow; }
inline void pinMode(uint8_t, int) {}
inline int digitalRead(uint8_t pin) { return fakePins[pin]; }
inline void digitalWrite(uint8_t, int) {}
inline void delay(unsigned long) {}

class FakeSerial {
public:
    bool connected = false;
    int writeCapacity = 256;
    std::string output;
    std::vector<char> input;

    void begin(unsigned long) {}
    explicit operator bool() const { return connected; }
    int available() const { return static_cast<int>(input.size()); }
    char read() {
        const char value = input.front();
        input.erase(input.begin());
        return value;
    }
    int availableForWrite() const { return writeCapacity; }
    size_t write(const uint8_t* data, size_t size) {
        if (writeCapacity < static_cast<int>(size)) return 0;
        output.append(reinterpret_cast<const char*>(data), size);
        return size;
    }
    void println(const char* value) { output += value; output += '\n'; }
    int printf(const char* format, ...) {
        char buffer[128];
        va_list args;
        va_start(args, format);
        const int count = vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        if (count > 0) output.append(buffer, static_cast<size_t>(count));
        return count;
    }
    void queue(const char* value) { while (*value) input.push_back(*value++); }
};

extern FakeSerial Serial;
