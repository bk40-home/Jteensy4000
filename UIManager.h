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

    // 🆕 Activity hooks (call whenever user interacts)
    inline void noteActivity() { _lastActivityMs = millis(); _scopeForced = false; }

    // 🆕 Expose scope input so SynthEngine can patch audio to it
    // Add this public accessor (by reference)
    AudioRecordQueue& scopeIn();



private:
    Adafruit_SSD1306 _display;
    int _currentPage;
    int _highlightIndex;
    const char* _labels[4];
    int _values[4];

    uint8_t _lastCCSent[128] = {0};
    bool    _hasCCSent[128]  = {false};

    // ---------- Screensaver / Scope ----------
    static constexpr uint32_t SCREEN_SAVER_TIMEOUT_MS = 60000; // keep screensaver
    uint32_t _lastActivityMs = 0;
    bool _scopeForced = false;  // legacy screensaver path
    bool _scopeOn     = false;  // 🆕 explicit toggle via button

    AudioRecordQueue _scopeQueue;

    // ---------- Presets ----------
    int _currentPreset = 0;     // 🆕 cycles 0..8 while scope is on

    // ---------- Rendering ----------
    void renderPage();
    void renderScope();

    void drawRightAligned(const String& text, int y);

};
