#include "GamepadSerialProtocol.h"

#include <assert.h>
#include <string.h>

using namespace GamepadSerialProtocol;

static bool feedLine(Parser& parser, const char* text, Frame& frame) {
    bool decoded = false;
    while (*text) decoded = parser.feed(*text++, frame) || decoded;
    return decoded;
}

int main() {
    char encoded[kFrameSize];
    assert(encode(0x1234, 0xA5, encoded, sizeof(encoded)) == kFrameSize);
    const char golden[] = "GP1:1234:A5:BD\n";
    assert(sizeof(golden) - 1 == kFrameSize);
    assert(memcmp(encoded, golden, kFrameSize) == 0);
    assert(encode(0x1234, 0xA5, encoded, kFrameSize - 1) == 0);

    Frame frame = {0xFFFF, 0xFF};
    Parser parser;
    const char* fragmented = "noise\nGP1:12";
    for (const char* p = fragmented; *p; ++p) assert(!parser.feed(*p, frame));
    assert(!parser.feed('3', frame));
    assert(!parser.feed('4', frame));
    assert(!parser.feed(':', frame));
    assert(!parser.feed('A', frame));
    assert(!parser.feed('5', frame));
    assert(!parser.feed(':', frame));
    assert(!parser.feed('B', frame));
    assert(!parser.feed('D', frame));
    assert(parser.feed('\n', frame));
    assert(frame.sequence == 0x1234 && frame.mask == 0xA5);

    assert(!feedLine(parser, "GP2:1234:A5:BD\n", frame));
    assert(!feedLine(parser, "GP1:1234:A5:00\n", frame)); // bad CRC
    assert(!feedLine(parser, "GP1:1234:a5:BD\n", frame)); // lowercase is malformed
    assert(!feedLine(parser, "GP1:1234:A5:BD\r\n", frame)); // diagnostics may use CRLF

    char tooLong[kFrameSize + 40];
    memset(tooLong, 'x', sizeof(tooLong) - 1);
    tooLong[sizeof(tooLong) - 1] = '\n';
    for (size_t i = 0; i < sizeof(tooLong); ++i)
        assert(!parser.feed(tooLong[i], frame)); // overflow is discarded through LF
    const char releaseGolden[] = "GP1:0000:00:16\n";
    assert(feedLine(parser, releaseGolden, frame));
    assert(frame.sequence == 0 && frame.mask == 0); // explicit all-buttons-released frame

    uint16_t sequence = 0xFFFF;
    assert(encode(sequence, 0, encoded, sizeof(encoded)) == kFrameSize);
    ++sequence;
    assert(sequence == 0); // sequence wraps naturally after a successfully sent frame
    assert(encode(sequence, 0, encoded, sizeof(encoded)) == kFrameSize);
    assert(memcmp(encoded, "GP1:0000:00:16\n", kFrameSize) == 0);

    assert(!shouldSend(false, true, 0, 0, 0, 0));
    assert(shouldSend(true, true, 0, 0, 10, 0)); // initial release on link-up
    assert(shouldSend(true, false, 1, 0, 20, 10)); // button change
    assert(!shouldSend(true, false, 0, 0, 249, 0));
    assert(shouldSend(true, false, 0, 0, 250, 0)); // heartbeat at 250 ms
    assert(hasWriteCapacity(kFrameSize));
    assert(!hasWriteCapacity(kFrameSize - 1));
    return 0;
}
