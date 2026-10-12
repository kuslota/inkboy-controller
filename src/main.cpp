#include <Arduino.h>
#include <BleGamepad.h>
#include "GamepadSerialProtocol.h"

// GPIO order is HID button order; receivers' saved button mappings depend on it.
static const uint8_t pins[] = {0,1,2,3,4,5,6,7};
static const uint8_t LED_PIN = 8;
BleGamepad bleGamepad("ESP32 Gamepad", "InkBoy", 100);
static uint8_t stable, candidate, lastSent;
static uint32_t changedAt[8], lastReport;
static bool wasConnected;
static bool usbWasConnected;
static bool usbNeedsInitialRelease;
static uint8_t usbLastSent;
static uint16_t usbSequence;
static uint32_t usbLastReport;
static uint8_t diagnosticMask;
static uint32_t diagnosticUntil;
static char command[32];
static unsigned commandLength;

void setup() {
    Serial.begin(115200);
    for (uint8_t pin : pins) pinMode(pin, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    BleGamepadConfiguration config;
    // Auto-reports would send one report per button; send one coherent report per change instead.
    config.setAutoReport(false);
    bleGamepad.begin(&config);
    Serial.println("GAMEPAD_READY: GPIO 0..7 = HID buttons 1..8; 20 ms debounce");
    Serial.println("USB diagnostics: status, tap 1 .. tap 8 (200 ms)");
    Serial.println("USB reports: GP1:SSSS:MM:CC\\n (uppercase hex, CRC-8/0x07)");
}

static void usbDiagnostic(uint32_t now) {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            command[commandLength] = 0;
            unsigned button;
            if (sscanf(command, "tap %u", &button) == 1 && button >= 1 && button <= 8) {
                diagnosticMask = 1u << (button - 1);
                diagnosticUntil = now + 200;
                Serial.printf("USB_TEST_BUTTON,%u\n", button);
            } else if (strcmp(command, "status") == 0) {
                Serial.printf("GAMEPAD_STATUS,connected,%u,physical,%02x\n", bleGamepad.isConnected(), stable);
            }
            commandLength = 0;
        } else if (commandLength < sizeof(command)-1) command[commandLength++] = c;
        else commandLength = 0;
    }
    if (diagnosticMask && (int32_t)(now-diagnosticUntil) >= 0) diagnosticMask = 0;
}

void loop() {
    const uint32_t now = millis();
    usbDiagnostic(now);
    for (unsigned i=0;i<8;++i) {
        const uint8_t bit = 1u << i;
        bool pressed = digitalRead(pins[i]) == LOW;
        if (pressed != bool(candidate & bit)) {
            candidate ^= bit;
            changedAt[i] = now;
        }
        if (now-changedAt[i] >= 20 && bool(stable & bit) != pressed) {
            stable ^= bit;
            Serial.printf("PHYSICAL_BUTTON,%u,pressed,%u\n", i+1, pressed);
        }
    }
    bool connected = bleGamepad.isConnected();
    bool usbConnected = bool(Serial);
    digitalWrite(LED_PIN, (connected || usbConnected) ? HIGH : LOW);
    if (connected != wasConnected) Serial.printf("BLE_CONNECTED,%u\n", connected);
    uint8_t report = stable | diagnosticMask;
    // Heartbeats let the host release stale keys if notifications stall.
    if (connected && (!wasConnected || report != lastSent || now-lastReport >= 250)) {
        for (unsigned i=0;i<8;++i) {
            if (report & (1u << i)) bleGamepad.press(i+1);
            else bleGamepad.release(i+1);
        }
        bleGamepad.sendReport();
        lastSent = report;
        lastReport = now;
    }
    wasConnected = connected;

    // USB CDC is an independent output path; its reports continue while BLE is idle.
    if (usbConnected && !usbWasConnected) usbNeedsInitialRelease = true;
    const uint8_t usbReport = usbNeedsInitialRelease ? 0 : report;
    const int usbCapacity = Serial.availableForWrite();
    if (GamepadSerialProtocol::shouldSend(
            usbConnected, usbNeedsInitialRelease, usbReport, usbLastSent,
            now, usbLastReport) &&
        usbCapacity >= 0 && GamepadSerialProtocol::hasWriteCapacity(
            static_cast<size_t>(usbCapacity))) {
        char frame[GamepadSerialProtocol::kFrameSize];
        const size_t frameSize = GamepadSerialProtocol::encode(
            usbSequence, usbReport, frame, sizeof(frame));
        if (frameSize && Serial.write(reinterpret_cast<const uint8_t*>(frame), frameSize) == frameSize) {
            ++usbSequence;  // Sequence counts successfully queued frames, including heartbeats.
            usbLastSent = usbReport;
            usbLastReport = now;
            usbNeedsInitialRelease = false;
        }
    }
    usbWasConnected = usbConnected;
    delay(2);
}
