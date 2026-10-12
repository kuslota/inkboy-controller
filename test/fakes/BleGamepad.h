#pragma once

#include <stdint.h>

class BleGamepadConfiguration {
public:
    void setAutoReport(bool) {}
};

class BleGamepad {
public:
    BleGamepad(const char*, const char*, uint8_t) {}
    void begin(BleGamepadConfiguration*) {}
    bool isConnected() const { return connected; }
    void press(uint8_t button) { pressedMask |= uint16_t(1u << (button - 1)); }
    void release(uint8_t button) { pressedMask &= uint16_t(~(1u << (button - 1))); }
    void sendReport() { ++reportsSent; }

    bool connected = false;
    uint16_t pressedMask = 0;
    unsigned reportsSent = 0;
};
