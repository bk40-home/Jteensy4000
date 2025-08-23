#include "HardwareInterface.h"

HardwareInterface::HardwareInterface() {
    // Initialise last values; actual pin reads will prime these in begin()
    for (int i = 0; i < 4; ++i) {
        _lastPotValues[i] = 0;
    }
}

void HardwareInterface::begin() {
    // ---------------- Encoder & button (unchanged) ----------------
    _navEncoder.begin(28, 29);
    pinMode(30, INPUT_PULLUP);  // encoder button

    // ---------------- Analog setup (new bits) ---------------------
    analogReadResolution(10);   // 0..1023; matches our scaling
    // analogReadAveraging(4);  // optional: hardware averaging (we already smooth)

    // Teensy 4.x tip: ensure analog-friendly state
    for (int i = 0; i < 4; ++i) {
        // Keep commented if you found INPUT hurt earlier; on T4.x it's recommended.
        pinMode(_potPins[i], INPUT);
    }

    // Configure and prime ResponsiveAnalogRead
    for (int i = 0; i < 4; ++i) {
        _potSmooth[i].setAnalogResolution(1023);
        _potSmooth[i].setSnapMultiplier(_snapMultiplier);
        _potSmooth[i].setActivityThreshold(_activityThresh);

        // Prime with an initial read so UI doesn’t see a jump at start
        int prime = analogRead(_potPins[i]);    // raw 0..1023
        _lastPotValues[i] = prime;              // store as last smoothed reference
        // Run a couple of updates so the smoother settles to the current position
        _potSmooth[i].update();
        _potSmooth[i].update();
    }
}

void HardwareInterface::update() {
    _navEncoder.tick();
    bool current = (digitalRead(30) == LOW);
    _fallingEdge = current && !_lastButton;
    _lastButton = current;

    for (int i = 0; i < 4; ++i) {
        _potSmooth[i].update();

        if (_debugPots) {
            int raw      = analogRead(_potPins[i]);             // 0..1023
            int smoothed = _potSmooth[i].getValue();            // 0..1023
            int ccRaw    = raw >> 3;                            // 0..127
            int ccSmooth = smoothed >> 3;                       // 0..127
            static uint32_t lastPrint = 0;
            if (millis() - lastPrint > 100) {
                lastPrint = millis();
                Serial.printf("[POT%d] raw=%4d cc=%3d | smooth=%4d cc=%3d\n", i, raw, ccRaw, smoothed, ccSmooth);
            }
        }
    }
}


int HardwareInterface::getEncoderDelta() {
    int32_t current = _navEncoder.getValue();
    int delta = current - _lastEncoderValue;
    _lastEncoderValue = current;
    return delta;
}

bool HardwareInterface::isButtonPressed() {
    if (_fallingEdge) {
        _fallingEdge = false;
        unsigned long now = millis();
        if (now - _lastButtonMs >= _debounceMs) {
            _lastButtonMs = now;
            return true;
        }
    }
    return false;
}

// -----------------------------------------------------------------------------
// readPot(index)
// ✅ API unchanged; now returns *smoothed* raw value (0..1023).
// UIManager continues to map() to 0..127 exactly as before.
// -----------------------------------------------------------------------------
int HardwareInterface::readPot(int index) {
    if (index < 0 || index >= 4) return 0;
    return _potSmooth[index].getValue();
}

// -----------------------------------------------------------------------------
// potChanged(index, threshold)
// ✅ API unchanged; now uses *smoothed* values and hasChanged() hint.
// Threshold remains in RAW counts (0..1023), same expectation as before.
// -----------------------------------------------------------------------------
bool HardwareInterface::potChanged(int index, int threshold) {
    if (index < 0 || index >= 4) return false;

    // Fast exit if the smoother says “no meaningful change”
    if (!_potSmooth[index].hasChanged()) return false;

    int current = _potSmooth[index].getValue();    // smoothed 0..1023
    if (abs(current - _lastPotValues[index]) > threshold) {
        _lastPotValues[index] = current;
        return true;
    }
    return false;
}
