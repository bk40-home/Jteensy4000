#pragma once
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include "SynthEngine.h"
#include "CCMap.h"
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
    int getCurrentPage() const;
    void highlightParameter(int index);

    // --- Parameter Interface
    void setParameterLabel(int index, const char* label);
    void setParameterValue(int index, int value);
    int getParameterValue(int index) const;
    int ccToDisplayValue(byte cc, SynthEngine& synth);
    // --- Sync
    void syncFromEngine(SynthEngine& synth);

    void pollInputs(HardwareInterface& hw, SynthEngine& synth);


private:
    Adafruit_SSD1306 _display;
    int _currentPage;
    int _highlightIndex;
    const char* _labels[4];
    int _values[4];


};
