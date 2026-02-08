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
    // ---------------------------------------------------------------------
    // Display update suppression / dirty-region tracking
    // ---------------------------------------------------------------------
    // NOTE: Adafruit_SSD1306 maintains a full framebuffer. Any call to
    // _display.display() pushes the entire buffer over I2C, so the best CPU win
    // is to avoid calling display() unless something has changed.
    //
    // We track "dirty" regions so we can re-render only when needed, and we
    // throttle flush rate to cap UI bandwidth.
    enum UiDirty : uint16_t {
        DIRTY_NONE   = 0,
        DIRTY_FULL   = 1 << 0,  // full page redraw required
        DIRTY_ROW0   = 1 << 1,
        DIRTY_ROW1   = 1 << 2,
        DIRTY_ROW2   = 1 << 3,
        DIRTY_ROW3   = 1 << 4,
        DIRTY_SCOPE  = 1 << 5,  // scope redraw requested
    };

    // Mark a specific row dirty.
    inline void markRowDirty(int row) {
        if (row < 0 || row > 3) return;
        _dirtyFlags |= (DIRTY_ROW0 << row);
    }

    // Mark everything dirty (page change / mode change).
    inline void markFullDirty() { _dirtyFlags |= DIRTY_FULL; }

    // Mark scope dirty (used when preset changes or pots adjust in scope mode).
    inline void markScopeDirty() { _dirtyFlags |= DIRTY_SCOPE; }

    Adafruit_SSD1306 _display;
    int _currentPage;
    int _highlightIndex;
    const char* _labels[4];
    int _values[4];

    uint8_t _lastCCSent[128] = {0};
    bool    _hasCCSent[128]  = {false};

    bool _scopeOn = true;

    // Dirty flags + throttling timestamps.
    volatile uint16_t _dirtyFlags = DIRTY_FULL;
    uint32_t _nextFlushMs = 0;      // next time we are allowed to flush a text page
    uint32_t _nextScopeMs = 0;      // next time we are allowed to flush the scope
    uint16_t _scopeSilentFrames = 0; // used to slow scope refresh when silent

    static const int SCOPE_RING = 4096;
    int16_t _scopeRing[SCOPE_RING];
    volatile uint32_t _scopeWrite = 0;

    AudioRecordQueue _scopeQueue;

    int _currentPreset = 0;

    void renderPage();
    void renderScope();

    // Prefer const char* to avoid Arduino String heap churn.
    void drawRightAligned(const char* text, int y);
    void drawRightAligned(const String& text, int y); // legacy overload (kept for compatibility)

    // Draw only one row (label + value). Used for dirty-row redraw.
    void renderRow(int row);

    uint32_t _statsNextMs = 0;
    bool     _statsDirty  = true;
    float    _cpuNowDisp  = 0.0f;
    uint16_t _blkNowDisp  = 0;

    const char* _valueText[4];  // nullptr = show numeric
};
