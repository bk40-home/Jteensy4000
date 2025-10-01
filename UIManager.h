#pragma once
#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include "SynthEngine.h"
#include "HardwareInterface.h"

class UIManager {
public:
    // --- Lifecycle
    UIManager();
    void begin();

    // --- Display Control
    void updateDisplay(SynthEngine& synth);

    // --- Page & Selection
    void setPage(int page);
    int  getCurrentPage() const;
    void highlightParameter(int index);

    // --- Parameter Interface
    void setParameterLabel(int index, const char* label);
    void setParameterValue(int index, int value);
    int  getParameterValue(int index) const;

    // Use Arduino's 'byte' in the API so signatures match exactly
    int         ccToDisplayValue(byte cc, SynthEngine& synth);
    const char* ccToDisplayText (byte cc, SynthEngine& synth);

    void syncFromEngine(SynthEngine& synth);
    void pollInputs(HardwareInterface& hw, SynthEngine& synth);

    inline void markStatsDirty() { _statsDirty = true; }

    // Scope tap access
    AudioRecordQueue& scopeIn();

private:
    Adafruit_SSD1306 _display;
    int _currentPage;
    int _highlightIndex;
    const char* _labels[4];
    int _values[4];

    uint8_t _lastCCSent[128] = {0};
    bool    _hasCCSent[128]  = {false};

    bool _scopeOn = true;

    static const int SCOPE_RING = 4096;
    int16_t _scopeRing[SCOPE_RING];
    volatile uint32_t _scopeWrite = 0;

    AudioRecordQueue _scopeQueue;

    int _currentPreset = 0;

    void renderPage();
    void renderScope();
    void drawRightAligned(const String& text, int y);

    uint32_t _statsNextMs = 0;
    bool     _statsDirty  = true;
    float    _cpuNowDisp  = 0.0f;
    uint16_t _blkNowDisp  = 0;

    const char* _valueText[4];  // nullptr = show numeric
};
