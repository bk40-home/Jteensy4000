#pragma once
#include <Arduino.h>
#include <EncoderTool.h>
#include <ResponsiveAnalogRead.h>   // 🆕 smoothing library
using namespace EncoderTool;

class HardwareInterface {
public:
    HardwareInterface();
    void begin();
    void update();

    int getEncoderDelta();          // One encoder for nav
    bool isButtonPressed();         // Single button

    // ✅ API unchanged — now returns SMOOTHED raw (0..1023)
    int readPot(int index);         // A17–A14
    // ✅ API unchanged — now detects change on SMOOTHED values (threshold in raw counts)
    bool potChanged(int index, int threshold = 4);

    void setPotDebug(bool on) { _debugPots = on; }

private:
    // -------------------- Encoder & button -----------------------
    PolledEncoder _navEncoder;
    int32_t _lastEncoderValue = 0;

    bool _lastButton = false;
    bool _fallingEdge = false;
    unsigned long _lastButtonMs = 0;
    const unsigned long _debounceMs = 200;

    // -------------------- Pots (hardware pins) -------------------
    uint8_t _potPins[4] = {A17, A16, A15, A14};

    // -------------------- Smoothing state ------------------------
    // Construct smoothers per-pin; 'true' enables sleep behavior (quieter when idle)
    ResponsiveAnalogRead _potSmooth[4] = {
        ResponsiveAnalogRead(A17, true),
        ResponsiveAnalogRead(A16, true),
        ResponsiveAnalogRead(A15, true),
        ResponsiveAnalogRead(A14, true)
    };

    // Last *smoothed* raw values (0..1023) used by potChanged thresholding
    int _lastPotValues[4] = {0, 0, 0, 0};

    // Optional tunables (reasonable defaults)
    float _snapMultiplier = 0.05f;  // 0.01..0.20; higher = snappier, lower = smoother
    int   _activityThresh = 6;      // raw counts to ignore wiggle until “real” move
    
    // DEBUG: enable to print raw vs smoothed reads
    bool _debugPots = false;




};
