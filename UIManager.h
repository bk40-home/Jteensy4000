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
    // Return non-null for discrete params that should display text (e.g., waveform)
    const char* ccToDisplayText(byte cc, SynthEngine& synth);

    void syncFromEngine(SynthEngine& synth);

    void pollInputs(HardwareInterface& hw, SynthEngine& synth);
    
    inline void markStatsDirty() { _statsDirty = true; }

    
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
    // static constexpr uint32_t SCREEN_SAVER_TIMEOUT_MS = 60000; // keep screensaver
    // uint32_t _lastActivityMs = 0;
    // bool _scopeForced = false;  // legacy screensaver path
    bool _scopeOn = true;  // 🆕 explicit toggle via button
    // Scope ring buffer for longer timebase (choose size you like)
    static const int SCOPE_RING = 4096;        // ≈ 93 ms @ 44.1 kHz
    int16_t _scopeRing[SCOPE_RING];
    volatile uint32_t _scopeWrite = 0;         // write index (wraps)


    AudioRecordQueue _scopeQueue;

    // ---------- Presets ----------
    int _currentPreset = 0;     // 🆕 cycles 0..8 while scope is on

    // ---------- Rendering ----------
    void renderPage();
    void renderScope();

    void drawRightAligned(const String& text, int y);

    uint32_t _statsNextMs = 0;   // next allowed refresh time
    bool     _statsDirty  = true; // event-driven refresh flag
    float    _cpuNowDisp  = 0.0f;
    uint16_t _blkNowDisp  = 0;
    // In class UIManager (private:)
    const char* _valueText[4];  // nullptr = show numeric


};
