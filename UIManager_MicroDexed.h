// UIManager_MicroDexed.h
// =============================================================================
// UI Manager for MicroDexed hardware variant:
//   - ILI9341 320×240 TFT display (SPI1 bus, ILI9341_t3n driver)
//   - FT6206 / FT6336U capacitive touch (I2C)
//   - Dual rotary encoders with push buttons
//
// Design overview:
//   - 26 pages × 4 parameters  (UIPageLayout.h owns all CC→page mappings)
//   - Dirty-region tracking     (only repaint what changed each frame)
//   - 30 FPS frame-rate cap     (33 ms minimum between repaints)
//   - Enum CC display           (waveforms, destinations, banks, modes as text)
//   - snprintf() only           (no heap-allocating Arduino String objects)
//
// SPI1 pin wiring (Teensy 4.1):
//   CS   = 41   DC  = 37   RST  = 24
//   MOSI = 26   SCK = 27   MISO = 39
//
// IMPORTANT — why ILI9341_t3n and why all 6 pins:
//   ILI9341_t3  (3-pin constructor) silently defaults to SPI0.
//   ILI9341_t3n (Kurt's extended driver) supports SPI1 DMA but ONLY when
//   all 6 SPI pins are passed to the constructor so it can route correctly.
//   Passing only CS/DC/RST to _t3n leaves MOSI/SCK unset → blank display.
// =============================================================================

#pragma once

#include <Arduino.h>
#include "ILI9341_t3n.h"         // Kurt's SPI1-aware DMA driver
#include <Wire.h>
#include "SynthEngine.h"
#include "HardwareInterface_MicroDexed.h"
#include "TouchInput.h"
#include "CCDefs.h"
#include "UIPageLayout.h"        // UIPage::ccMap, ccNames, pageTitle, NUM_PAGES
#include "Waveforms.h"           // waveformShortName(), ccFromWaveform()
#include "AKWF_All.h"            // akwf_bankName(), akwf_bankCount()
#include "BPMClockManager.h"     // TimingModeNames[]


class UIManager_MicroDexed {
public:

    // =========================================================================
    // Display geometry (pixels)
    // =========================================================================
    static constexpr int SCREEN_WIDTH   = 320;
    static constexpr int SCREEN_HEIGHT  = 240;

    // =========================================================================
    // Layout constants (pixels)
    // =========================================================================
    static constexpr int HEADER_HEIGHT      = 30;   // top status bar
    static constexpr int FOOTER_HEIGHT      = 20;   // bottom hint bar
    static constexpr int PARAM_ROW_HEIGHT   = 40;   // height of each parameter row
    static constexpr int SCREEN_MARGIN      = 5;    // edge margin

    // =========================================================================
    // Font size constants
    // ILI9341_t3n built-in Adafruit font: 6x8 px per character at size 1.
    // =========================================================================
    static constexpr int FONT_SMALL  = 1;   //  6x8  px per character
    static constexpr int FONT_MEDIUM = 2;   // 12x16 px per character
    static constexpr int FONT_LARGE  = 3;   // 18x24 px per character

    // =========================================================================
    // Colour palette (RGB565)
    // Category colours are shared with the OLED UIManager for visual consistency
    // when switching hardware variants.
    // =========================================================================
    static constexpr uint16_t COLOUR_BACKGROUND     = 0x0000;  // black
    static constexpr uint16_t COLOUR_TEXT           = 0xFFFF;  // white
    static constexpr uint16_t COLOUR_TEXT_DIM       = 0x7BEF;  // mid-grey
    static constexpr uint16_t COLOUR_SELECTED       = 0x07FF;  // cyan  — selected row highlight
    static constexpr uint16_t COLOUR_ACCENT         = 0xF800;  // red   — alerts / clip indicator
    static constexpr uint16_t COLOUR_OSC            = 0x07E0;  // green   — oscillator params
    static constexpr uint16_t COLOUR_FILTER         = 0xFFE0;  // yellow  — filter params
    static constexpr uint16_t COLOUR_ENV            = 0xF81F;  // magenta — envelope params
    static constexpr uint16_t COLOUR_LFO            = 0xFD20;  // orange  — LFO params
    static constexpr uint16_t COLOUR_FX             = 0x07FF;  // cyan    — FX params
    static constexpr uint16_t COLOUR_GLOBAL         = 0xC618;  // light grey — global params
    static constexpr uint16_t COLOUR_HEADER_BG      = 0x4208;  // dark grey  — header/footer bg
    static constexpr uint16_t COLOUR_BORDER         = 0x2104;  // darker grey — separators


    // =========================================================================
    // Lifecycle
    // =========================================================================

    UIManager_MicroDexed();

    // Call once in setup() — initialises display hardware and shows boot splash.
    void begin();


    // =========================================================================
    // Main loop hooks
    // =========================================================================

    // Call at ~30 Hz from the main loop.
    // Internally throttled; only repaints dirty regions each frame.
    void updateDisplay(SynthEngine& synth);

    // Call at >= 100 Hz for responsive encoder and touch response.
    // Reads hardware deltas and routes parameter changes to the synth engine.
    void pollInputs(HardwareInterface_MicroDexed& hw, SynthEngine& synth);


    // =========================================================================
    // Page and parameter navigation
    // =========================================================================
    void setPage(int page);
    int  getCurrentPage()       const { return _currentPage;   }
    void selectParameter(int index);
    int  getSelectedParameter() const { return _selectedParam; }


    // =========================================================================
    // Engine sync
    // =========================================================================

    // Call after a preset load or external CC flood.
    // Forces a full repaint so the display reflects the new engine state.
    void syncFromEngine(SynthEngine& synth);

    // Compatibility stub — OLED UIManager caches label strings; MicroDexed
    // derives them from UIPage::ccNames at draw time, so this is a no-op.
    inline void setParameterLabel(int /*index*/, const char* /*label*/) {}


    // =========================================================================
    // Display modes
    // =========================================================================
    enum DisplayMode : uint8_t {
        MODE_PARAMETERS = 0,    // standard 4-parameter grid  (default)
        MODE_SCOPE,             // oscilloscope + peak meters
        MODE_MENU,              // settings / preset browser
        MODE_COUNT              // sentinel — never use as an actual mode
    };

    void        setMode(DisplayMode mode);
    DisplayMode getMode() const { return _displayMode; }


    // =========================================================================
    // Enum-aware display helpers
    // Public so external code (e.g. a MIDI monitor) can use them if needed.
    // =========================================================================

    // Returns a human-readable string for enum-like CCs (waveforms, LFO
    // destinations, AKWF bank names, timing modes, boolean toggles).
    // Returns nullptr for purely numeric CCs — caller should print the integer.
    const char* ccDisplayText(uint8_t cc, SynthEngine& synth) const;

    // Returns the 0-127 CC value representing the current engine state,
    // applying the correct inverse-mapping curve for each parameter type.
    int ccDisplayValue(uint8_t cc, SynthEngine& synth) const;


private:

    // =========================================================================
    // SPI1 pin assignments — ILI9341_t3n on Teensy 4.1
    //
    // All 6 pins MUST be passed to the constructor.
    // If only CS/DC/RST are given, t3n cannot configure SPI1 and the display
    // stays blank.  This was the original cause of the "no display" fault.
    // =========================================================================
    static constexpr uint8_t TFT_CS   = 41;  // chip select
    static constexpr uint8_t TFT_DC   = 37;  // data / command
    static constexpr uint8_t TFT_RST  = 24;  // hardware reset
    static constexpr uint8_t TFT_MOSI = 26;  // SPI1 MOSI — must be explicit for SPI1 routing
    static constexpr uint8_t TFT_SCK  = 27;  // SPI1 SCK  — must be explicit for SPI1 routing
    static constexpr uint8_t TFT_MISO = 39;  // SPI1 MISO — must be explicit for SPI1 routing

    // =========================================================================
    // Hardware objects
    // =========================================================================
    ILI9341_t3n _display;       // 6-pin constructor — routes correctly to SPI1
    TouchInput  _touch;
    bool        _touchEnabled;  // false if no touch hardware found during begin()

    // =========================================================================
    // UI state
    // =========================================================================
    int         _currentPage;   // active page index (0 .. UIPage::NUM_PAGES-1)
    int         _selectedParam; // active parameter row (0-3)
    DisplayMode _displayMode;   // current display mode

    // =========================================================================
    // Dirty-region tracking
    // Only repaint regions that have actually changed — avoids saturating the
    // SPI1 bus pushing the full 320x240 framebuffer every frame.
    // =========================================================================
    bool _fullRedraw;           // true -> repaint everything on next frame
    bool _headerDirty;          // true -> header bar needs repaint
    bool _footerDirty;          // true -> footer bar needs repaint
    bool _rowDirty[4];          // true -> that parameter row needs repaint (4 rows per page)

    // Convenience setters used by input handlers and mode changes
    inline void markFullRedraw()        { _fullRedraw  = true; }
    inline void markHeaderDirty()       { _headerDirty = true; }
    inline void markFooterDirty()       { _footerDirty = true; }
    inline void markRowDirty(int i)     { if (i >= 0 && i < 4) _rowDirty[i] = true; }

    // Called at the end of each frame to reset all dirty flags
    inline void clearDirtyFlags() {
        _fullRedraw  = false;
        _headerDirty = false;
        _footerDirty = false;
        _rowDirty[0] = _rowDirty[1] = _rowDirty[2] = _rowDirty[3] = false;
    }

    // =========================================================================
    // Frame-rate cap — 30 FPS (33 ms minimum between full repaints)
    // =========================================================================
    uint32_t _lastFrameMillis;
    static constexpr uint32_t FRAME_INTERVAL_MS = 33;

    // =========================================================================
    // Encoder position tracking (used to compute delta per poll cycle)
    // =========================================================================
    int _lastEncoderLeft;
    int _lastEncoderRight;


    // =========================================================================
    // Drawing — full view repaints
    // =========================================================================
    void drawHeader(SynthEngine& synth);
    void drawParamGrid(SynthEngine& synth);
    void drawFooter();
    void drawMenuView();

    // Scope view and all its helpers are implemented in ScopeView.cpp.
    // Declared here so they can access private display and colour members.
    void drawScopeView(SynthEngine& synth);
    void drawWaveform(int16_t* samples, uint16_t numSamples,
                      int triggerIndex, int topY, int bottomY, int width);
    int  findZeroCrossing(int16_t* samples, uint16_t numSamples);
    void drawPeakMeters(int x, int y, int width, int height);
    void drawVoiceActivity(SynthEngine& synth, int x, int y, int width);

    // =========================================================================
    // Drawing — single parameter row
    //
    // Row layout (PARAM_ROW_HEIGHT px tall):
    //
    //   [ Name (left-aligned)       Value text or number (right-aligned) ]
    //   [ XXXXXXXX bar (filled) ............ track (empty) ............. ]  <- 4px tall
    // =========================================================================
    void drawParamRow(int row, uint8_t cc, SynthEngine& synth, bool selected);

    // =========================================================================
    // Touch input
    // =========================================================================
    void handleTouch(SynthEngine& synth);
    bool hitTestRow(int row, int16_t touchX, int16_t touchY) const;

    // =========================================================================
    // Text drawing helpers
    // =========================================================================

    // Draw text centred horizontally around cx, at vertical position y.
    void drawTextCentred(int cx, int y, const char* text,
                         uint16_t colour, int fontSize = FONT_MEDIUM);

    // Draw text right-aligned: rightmost pixel edge at rx.
    void drawTextRight(int rx, int y, const char* text,
                       uint16_t colour, int fontSize = FONT_MEDIUM);

    // =========================================================================
    // Colour coding — category colour for a parameter's value bar
    // =========================================================================
    uint16_t paramColour(uint8_t cc) const;
};
