#pragma once

#include <stddef.h>
#include <stdint.h>

// Wire format: GP1:SSSS:MM:CC\n (15 bytes; no terminator beyond the newline).
namespace GamepadSerialProtocol {

static const size_t kFrameSize = 15;
static const size_t kMaxInputSize = kFrameSize;

struct Frame {
    uint16_t sequence;
    uint8_t mask;
};

inline uint8_t crc8(const uint8_t* data, size_t length) {
    uint8_t crc = 0;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc & 0x80) ? uint8_t((crc << 1) ^ 0x07) : uint8_t(crc << 1);
    }
    return crc;
}

inline uint8_t frameCrc(uint16_t sequence, uint8_t mask) {
    const uint8_t bytes[] = {1, uint8_t(sequence >> 8), uint8_t(sequence), mask};
    return crc8(bytes, sizeof(bytes));
}

inline char hexDigit(uint8_t value) {
    return "0123456789ABCDEF"[value & 0x0F];
}

// Returns zero if output is null or too small, otherwise the exact wire size.
inline size_t encode(uint16_t sequence, uint8_t mask, char* output, size_t capacity) {
    if (!output || capacity < kFrameSize) return 0;
    output[0] = 'G'; output[1] = 'P'; output[2] = '1'; output[3] = ':';
    output[4] = hexDigit(sequence >> 12);
    output[5] = hexDigit(sequence >> 8);
    output[6] = hexDigit(sequence >> 4);
    output[7] = hexDigit(sequence);
    output[8] = ':';
    output[9] = hexDigit(mask >> 4);
    output[10] = hexDigit(mask);
    output[11] = ':';
    const uint8_t checksum = frameCrc(sequence, mask);
    output[12] = hexDigit(checksum >> 4);
    output[13] = hexDigit(checksum);
    output[14] = '\n';
    return kFrameSize;
}

inline int parseHex(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

class Parser {
public:
    Parser() : length_(0), overflowing_(false) {}

    // Feed one byte. Returns true only for a complete, valid frame.
    // Malformed lines and overflowed lines are discarded through their newline.
    bool feed(char value, Frame& frame) {
        if (value == '\n') {
            const bool valid = !overflowing_ && decode(frame);
            length_ = 0;
            overflowing_ = false;
            return valid;
        }
        if (overflowing_) return false;
        if (length_ >= kMaxInputSize) {
            overflowing_ = true;
            length_ = 0;
            return false;
        }
        buffer_[length_++] = value;
        return false;
    }

    void reset() { length_ = 0; overflowing_ = false; }

private:
    bool decode(Frame& frame) const {
        if (length_ != kFrameSize - 1 || buffer_[0] != 'G' || buffer_[1] != 'P' ||
            buffer_[2] != '1' || buffer_[3] != ':' || buffer_[8] != ':' ||
            buffer_[11] != ':') return false;
        uint16_t sequence = 0;
        for (size_t i = 4; i <= 7; ++i) {
            const int digit = parseHex(buffer_[i]);
            if (digit < 0) return false;
            sequence = uint16_t((sequence << 4) | digit);
        }
        uint8_t mask = 0;
        for (size_t i = 9; i <= 10; ++i) {
            const int digit = parseHex(buffer_[i]);
            if (digit < 0) return false;
            mask = uint8_t((mask << 4) | digit);
        }
        uint8_t checksum = 0;
        for (size_t i = 12; i <= 13; ++i) {
            const int digit = parseHex(buffer_[i]);
            if (digit < 0) return false;
            checksum = uint8_t((checksum << 4) | digit);
        }
        if (checksum != frameCrc(sequence, mask)) return false;
        frame.sequence = sequence;
        frame.mask = mask;
        return true;
    }

    char buffer_[kFrameSize];
    size_t length_;
    bool overflowing_;
};

// The 250 ms heartbeat keeps the host's 2 s stale-input timeout from firing while a button is held.
inline bool shouldSend(bool connected, bool justConnected, uint8_t report,
                       uint8_t lastReport, uint32_t now, uint32_t lastSentAt) {
    return connected && (justConnected || report != lastReport ||
                         uint32_t(now - lastSentAt) >= 250);
}

inline bool hasWriteCapacity(size_t available, size_t frameSize = kFrameSize) {
    return available >= frameSize;
}

}
