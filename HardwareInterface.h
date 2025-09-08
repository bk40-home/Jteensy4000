#include "pins_arduino.h"
#pragma once
#include <Arduino.h>
#include <EncoderTool.h>
#include <ResponsiveAnalogRead.h>
using namespace EncoderTool;

class HardwareInterface {
public:
    HardwareInterface();
    void begin();
    void update();

    int  getEncoderDelta();
    bool isButtonPressed();

    // Returns SMOOTHED raw (0..1023), after optional inversion
    int  readPot(int index);
    bool potChanged(int index, int threshold = 4);

    void setPotDebug(bool on) { _debugPots = on; }

    // --- Optional: runtime control if you ever want to flip individual pots in code ---
    void setPotInverted(int index, bool inverted);

private:
    // ----------- Encoder pins -----------
    static constexpr uint8_t ENC_A_PIN  = 36;
    static constexpr uint8_t ENC_B_PIN  = 37;
    static constexpr uint8_t ENC_SW_PIN = 35;

    // ----------- Encoder & button -----------
    PolledEncoder _navEncoder;
    int32_t _lastEncoderValue = 0;
    bool _lastButton = false;
    bool _fallingEdge = false;
    unsigned long _lastButtonMs = 0;
    const unsigned long _debounceMs = 200;

    // ----------- Pots -----------
    // If you keep current wiring, set all to true so clockwise increases.
    // If you rewire (preferred), set all to false (no inversion).
    static constexpr int POT_MAX = 1023;
    uint8_t _potPins[4] = {A17, A16, A15, A14};
    bool    _potInvert[4] = { true, true, true, true }; // <-- flip sense in software

    ResponsiveAnalogRead _potSmooth[4] = {
        ResponsiveAnalogRead(A17, true),
        ResponsiveAnalogRead(A16, true),
        ResponsiveAnalogRead(A15, true),
        ResponsiveAnalogRead(A14, true)
    };

    // Store last *transformed* (post-inversion) smoothed values
    int _lastPotValues[4] = {0, 0, 0, 0};

    // Smoother tunables
    float _snapMultiplier = 0.05f;
    int   _activityThresh = 6;

    bool _debugPots = false;
};
