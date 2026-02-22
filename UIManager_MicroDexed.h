/**
 * UIManager_MicroDexed.h
 * 
 * UI Manager for MicroDexed hardware with ILI9341 320x240 TFT display.
 * 
 * Display features:
 * - 320x240 resolution (vs 128x64 OLED)
 * - 16-bit color (65,536 colors)
 * - SPI interface (faster than I2C)
 * - Optional capacitive touch input
 * 
 * Input model:
 * - 2 encoders (left/right) replace encoder + 4 pots
 * - Left encoder: Navigation (parameter selection, back, menu)
 * - Right encoder: Value adjustment (increment/decrement parameter)
 * - Button presses: Short (select/confirm), Long (mode switch)
 * - Touch: Direct parameter access, swipe gestures
 * 
 * Performance considerations:
 * - ILI9341_t3 is Teensy-optimized (DMA, fast SPI)
 * - Dirty region tracking to minimize redraws
 * - Throttled update rate (30-60 FPS sufficient for UI)
 * - Color-coded parameter categories for quick visual parsing
 */

#pragma once
#include <Arduino.h>
#include <ILI9341_t3.h>  // Teensy-optimized ILI9341 driver (faster than Adafruit)
#include <Wire.h>
#include "SynthEngine.h"
#include "HardwareInterface_MicroDexed.h"
#include "TouchInput.h"  // Optional touch support

class UIManager_MicroDexed {
public:
    // --- Display properties ---
    static constexpr int SCREEN_WIDTH  = 320;
    static constexpr int SCREEN_HEIGHT = 240;
    
    // --- Color palette (16-bit RGB565) ---
    // Using a synth-themed color scheme
    static constexpr uint16_t COLOR_BACKGROUND   = 0x0000;  // Black
    static constexpr uint16_t COLOR_TEXT         = 0xFFFF;  // White
    static constexpr uint16_t COLOR_TEXT_DIM     = 0x7BEF;  // Gray
    static constexpr uint16_t COLOR_HIGHLIGHT    = 0x07FF;  // Cyan (selected item)
    static constexpr uint16_t COLOR_ACCENT       = 0xF800;  // Red (important values)
    static constexpr uint16_t COLOR_OSC          = 0x07E0;  // Green (oscillator params)
    static constexpr uint16_t COLOR_FILTER       = 0xFFE0;  // Yellow (filter params)
    static constexpr uint16_t COLOR_ENV          = 0xF81F;  // Magenta (envelope params)
    static constexpr uint16_t COLOR_FX           = 0x07FF;  // Cyan (FX params)
    static constexpr uint16_t COLOR_HEADER       = 0x4208;  // Dark gray (headers)
    static constexpr uint16_t COLOR_BORDER       = 0x2104;  // Darker gray (borders)

    // --- Layout constants ---
    static constexpr int HEADER_HEIGHT    = 30;   // Top status bar
    static constexpr int FOOTER_HEIGHT    = 20;   // Bottom hint bar
    static constexpr int PARAM_ROW_HEIGHT = 35;   // Height per parameter row
    static constexpr int MARGIN           = 5;    // Screen edge margin
    static constexpr int FONT_SMALL       = 1;    // Built-in font size 1 (6x8)
    static constexpr int FONT_MEDIUM      = 2;    // Built-in font size 2 (12x16)
    static constexpr int FONT_LARGE       = 3;    // Built-in font size 3 (18x24)

    // --- Constructor & lifecycle ---
    UIManager_MicroDexed();
    void begin();

    // --- Main update functions ---
    /**
     * Update display contents based on engine state.
     * Uses dirty region tracking to minimize SPI traffic.
     * Call at 30-60 Hz for smooth UI.
     */
    void updateDisplay(SynthEngine& synth);

    /**
     * Poll hardware inputs (encoders, buttons) and update synth.
     * Handles navigation, value changes, mode switching.
     * Call in main loop at 100+ Hz for responsive input.
     */
    void pollInputs(HardwareInterface_MicroDexed& hw, SynthEngine& synth);

    // --- Page management ---
    void setPage(int page);
    int  getCurrentPage() const { return _currentPage; }
    
    // --- Parameter selection ---
    void selectParameter(int index);  // Select param by index (0-7 visible)
    int  getSelectedParameter() const { return _selectedParam; }

    // --- Engine synchronization ---
    /**
     * Refresh all displayed values from synth engine.
     * Call after preset load or external MIDI CC changes.
     */
    void syncFromEngine(SynthEngine& synth);

    // --- Parameter labels ---
    // No-op stub for API compatibility with UIManager (OLED) setup() calls.
    // MicroDexed derives all names live from UIPage::ccNames / CC::name()
    // at draw time, so stored labels are unnecessary.
    inline void setParameterLabel(int /*index*/, const char* /*label*/) {}

    // --- Display mode ---
    enum DisplayMode {
        MODE_PARAMETERS,  // Standard parameter grid view
        MODE_SCOPE,       // Oscilloscope view
        MODE_MENU,        // Settings/preset menu
        MODE_COUNT
    };
    void setMode(DisplayMode mode);
    DisplayMode getMode() const { return _currentMode; }

private:
    // --- Display driver ---
    ILI9341_t3 _display;
    
    // --- Touch input (optional) ---
    TouchInput _touch;
    bool _touchEnabled;
    
    // --- Pin definitions for ILI9341 (SPI) ---
    static constexpr uint8_t TFT_CS   = 41;  // Chip select
    static constexpr uint8_t TFT_DC   = 37;   // Data/command
    static constexpr uint8_t TFT_RST  = 24;  // Reset (tied to Teensy reset or 3.3V)

// #define TFT_SCK 27

    // --- UI state ---
    int _currentPage;           // Current parameter page (0-N)
    int _selectedParam;         // Currently selected parameter (0-7)
    DisplayMode _currentMode;   // Current display mode
    
    // --- Dirty region tracking ---
    bool _fullRedraw;           // Force complete screen redraw
    bool _headerDirty;          // Header needs update
    bool _paramsDirty[8];       // Each parameter row dirty flag
    bool _footerDirty;          // Footer needs update
    
    // --- Frame throttling ---
    uint32_t _lastFrameTime;
    static constexpr uint32_t FRAME_INTERVAL_MS = 33;  // ~30 FPS (sufficient for UI)

    // --- Input state ---
    int _lastEncoderLeft;       // Last left encoder position
    int _lastEncoderRight;      // Last right encoder position

    // --- Drawing functions ---
    void drawHeader(SynthEngine& synth);
    void drawParameterGrid(SynthEngine& synth);
    void drawParameterRow(int row, byte cc, int value, bool selected);
    void drawFooter();
    void drawScopeView(SynthEngine& synth);
    void drawMenuView();
    
    // --- Scope view helpers ---
    void drawWaveform(int16_t* samples, uint16_t numSamples, int triggerIdx, 
                      int topY, int botY, int width);
    int findZeroCrossing(int16_t* samples, uint16_t numSamples);
    void drawPeakMeters(int x, int y, int width, int height);
    void drawVoiceActivity(SynthEngine& synth, int x, int y, int width);
    
    // --- Touch input helpers ---
    void handleTouchInput(SynthEngine& synth);
    bool hitTestParameter(int paramIndex, int16_t x, int16_t y);
    
    // --- Helper functions ---
    uint16_t getParamColor(byte cc);  // Get color based on parameter category
    const char* getParamName(byte cc); // Get parameter name from CC
    void drawCenteredText(int x, int y, const char* text, uint16_t color, int fontSize = FONT_MEDIUM);
    void drawRightAlignedText(int x, int y, const char* text, uint16_t color, int fontSize = FONT_MEDIUM);
    
    // --- Dirty region management ---
    inline void markFullDirty() { _fullRedraw = true; }
    inline void markHeaderDirty() { _headerDirty = true; }
    inline void markParamDirty(int index) {
        if (index >= 0 && index < 8) _paramsDirty[index] = true;
    }
    inline void markFooterDirty() { _footerDirty = true; }
    inline void clearDirtyFlags() {
        _fullRedraw = false;
        _headerDirty = false;
        for (int i = 0; i < 8; ++i) _paramsDirty[i] = false;
        _footerDirty = false;
    }
};