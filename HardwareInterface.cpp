#include "HardwareInterface.h"

HardwareInterface::HardwareInterface() {
    for (int i = 0; i < 4; ++i) _lastPotValues[i] = 0;
}

void HardwareInterface::begin() {
    // ---------------- Encoder & button ----------------
    _navEncoder.begin(
        ENC_A_PIN,
        ENC_B_PIN,
        ENC_SW_PIN,
        EncoderTool::CountMode::quarter,
        INPUT_PULLUP
    );
    // pinMode(ENC_SW_PIN, INPUT_PULLUP); // not needed; kept for reference

    // ---------------- Analog setup --------------------
    analogReadResolution(10); // 0..1023
    for (int i = 0; i < 4; ++i) {
        pinMode(_potPins[i], INPUT);
        _potSmooth[i].setAnalogResolution(POT_MAX);
        _potSmooth[i].setSnapMultiplier(_snapMultiplier);
        _potSmooth[i].setActivityThreshold(_activityThresh);

        // Prime smoother and store *transformed* initial value
        int raw = analogRead(_potPins[i]);      // 0..1023
        int sm  = raw;                          // initial seed
        _potSmooth[i].update();
        _potSmooth[i].update();

        // Apply inversion on the value we expose/compare
        int exposed = _potInvert[i] ? (POT_MAX - sm) : sm;
        _lastPotValues[i] = exposed;
    }
}

void HardwareInterface::update() {
    _navEncoder.tick();

    // Manual button handling
    bool current = (digitalRead(ENC_SW_PIN) == LOW);
    _fallingEdge = current && !_lastButton;
    _lastButton  = current;

    // Pots
    for (int i = 0; i < 4; ++i) {
        _potSmooth[i].update();

        if (_debugPots) {
            int raw      = analogRead(_potPins[i]);     // raw
            int smoothed = _potSmooth[i].getValue();    // smoothed
            int exposed  = _potInvert[i] ? (POT_MAX - smoothed) : smoothed;
            int ccRaw    = raw      >> 3;               // 0..127
            int ccExp    = exposed  >> 3;               // 0..127
            static uint32_t lastPrint = 0;
            if (millis() - lastPrint > 100) {
                lastPrint = millis();
                Serial.printf("[POT%d] raw=%4d cc=%3d | smooth=%4d -> exp=%4d cc=%3d (inv:%d)\n",
                              i, raw, ccRaw, smoothed, exposed, ccExp, _potInvert[i]);
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

int HardwareInterface::readPot(int index) {
    if (index < 0 || index >= 4) return 0;
    int smoothed = _potSmooth[index].getValue();                 // 0..1023
    int exposed  = _potInvert[index] ? (POT_MAX - smoothed) : smoothed;
    return exposed;
}

bool HardwareInterface::potChanged(int index, int threshold) {
    if (index < 0 || index >= 4) return false;

    int smoothed = _potSmooth[index].getValue();
    int exposed  = _potInvert[index] ? (POT_MAX - smoothed) : smoothed;

    // Compare and store the *exposed* value so UI and thresholds
    // work in the same "clockwise increases" domain
    if (abs(exposed - _lastPotValues[index]) > threshold) {
        _lastPotValues[index] = exposed;
        return true;
    }
    return false;
}

void HardwareInterface::setPotInverted(int index, bool inverted) {
    if (index < 0 || index >= 4) return;
    _potInvert[index] = inverted;
    // Optionally re-seed last value so potChanged() doesn't false-trigger:
    int smoothed = _potSmooth[index].getValue();
    _lastPotValues[index] = inverted ? (POT_MAX - smoothed) : smoothed;
}
