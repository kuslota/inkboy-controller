#include "fakes/Arduino.h"

uint32_t fakeNow = 0;
int fakePins[16] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH,
                    HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};
FakeSerial Serial;

#include "../src/main.cpp"

#include <assert.h>

using namespace GamepadSerialProtocol;

static std::vector<Frame> takeFrames() {
    Parser parser;
    Frame frame;
    std::vector<Frame> frames;
    for (char value : Serial.output)
        if (parser.feed(value, frame)) frames.push_back(frame);
    Serial.output.clear();
    return frames;
}

int main() {
    setup();
    Serial.connected = true;
    fakeNow = 0;
    loop();
    std::vector<Frame> frames = takeFrames();
    assert(frames.size() == 1 && frames[0].sequence == 0 && frames[0].mask == 0);
    assert(!bleGamepad.isConnected()); // USB reports while BLE is disconnected.

    // A debounced press is retained while CDC has no room, then sent on retry.
    Serial.writeCapacity = 0;
    fakePins[0] = LOW;
    fakeNow = 2; loop();
    fakeNow = 22; loop();
    assert(takeFrames().empty());
    Serial.writeCapacity = 256;
    fakeNow = 24; loop();
    frames = takeFrames();
    assert(frames.size() == 1 && frames[0].sequence == 1 && frames[0].mask == 1);

    fakeNow = 274; loop();
    frames = takeFrames();
    assert(frames.size() == 1 && frames[0].sequence == 2 && frames[0].mask == 1);

    Serial.queue("tap 2\n");
    fakeNow = 276; loop();
    frames = takeFrames();
    assert(frames.size() == 1 && frames[0].sequence == 3 && frames[0].mask == 3);
    fakeNow = 476; loop();
    frames = takeFrames();
    assert(frames.size() == 1 && frames[0].sequence == 4 && frames[0].mask == 1);

    // USB backpressure does not gate the existing BLE report path.
    Serial.writeCapacity = 0;
    bleGamepad.connected = true;
    fakeNow = 500; loop();
    assert(bleGamepad.reportsSent == 1 && bleGamepad.pressedMask == 1);
    fakePins[0] = HIGH;
    fakeNow = 502; loop();
    fakeNow = 522; loop();
    assert(bleGamepad.reportsSent == 2 && bleGamepad.pressedMask == 0);

    // The USB release was not lost while full; it is reported when capacity returns.
    Serial.writeCapacity = 256;
    fakeNow = 524; loop();
    frames = takeFrames();
    assert(frames.size() == 1 && frames[0].sequence == 5 && frames[0].mask == 0);

    // A new CDC link starts with an all-released frame even if a key is held.
    fakePins[1] = LOW;
    fakeNow = 526; loop();
    fakeNow = 546; loop();
    takeFrames();
    Serial.connected = false;
    fakeNow = 548; loop();
    takeFrames();
    Serial.connected = true;
    fakeNow = 550; loop();
    frames = takeFrames();
    assert(frames.size() == 1 && frames[0].sequence == 7 && frames[0].mask == 0);
    fakeNow = 552; loop();
    frames = takeFrames();
    assert(frames.size() == 1 && frames[0].sequence == 8 && frames[0].mask == 2);

    // The live sender advances through 0xFFFF and wraps on the next queued frame.
    usbSequence = 0xFFFF;
    fakeNow = 802; loop();
    frames = takeFrames();
    assert(frames.size() == 1 && frames[0].sequence == 0xFFFF);
    assert(usbSequence == 0);
    fakeNow = 1052; loop();
    frames = takeFrames();
    assert(frames.size() == 1 && frames[0].sequence == 0);
    return 0;
}
