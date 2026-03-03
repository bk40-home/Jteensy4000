// UIManager_TFT.cpp
// =============================================================================
// Implementation of UIManager_TFT — see UIManager_TFT.h for design notes.
// =============================================================================

#include "UIManager_TFT.h"
#include <math.h>

// Static singleton pointer — set in begin()
UIManager_TFT* UIManager_TFT::_instance = nullptr;

// =============================================================================
// Constructor
// =============================================================================
UIManager_TFT::UIManager_TFT()
    : _display(TFT_CS, TFT_DC, TFT_RST, TFT_MOSI, TFT_SCK, TFT_MISO)
    , _touchOk(false)
    , _mode(Mode::HOME)
    , _activeSect(-1)
    , _lastFrame(0)
    , _synthRef(nullptr)
    , _currentPresetIdx(0)
    , _scopeFullFirstFrame(true)
{}

// =============================================================================
// beginDisplay() — hardware-only: SPI init, splash, touch init.
// Call BEFORE AudioMemory() to avoid DMA bus conflicts at startup.
// =============================================================================
void UIManager_TFT::beginDisplay() {
    _display.begin(SPI_CLOCK_HZ);
    _display.setRotation(3);       // landscape 320×240
    _display.fillScreen(0x0000);

    // Touch controller init over I2C — safe before AudioMemory
    _touchOk = _touch.begin();
    Serial.printf("[UI] Touch: %s\n", _touchOk ? "OK" : "not found");

    // Boot splash — confirms the display is working before audio starts
    _display.setTextSize(3);
    _display.setTextColor(COLOUR_SYSTEXT);   // amber (BGR-corrected)
    _display.setCursor(60, 90);
    _display.print("JT.4000");

    _display.setTextSize(1);
    _display.setTextColor(COLOUR_TEXT_DIM);  // steel grey (BGR-corrected)
    _display.setCursor(74, 124);
    _display.print("MicroDexed Edition");

    delay(800);   // long enough to read; short enough not to delay boot
    _display.fillScreen(0x0000);
}

// =============================================================================
// begin() — wire screens. Call AFTER AudioMemory() and synth initialisation.
// =============================================================================
void UIManager_TFT::begin(SynthEngine& synth) {
    _synthRef = &synth;
    _instance = this;

    // HomeScreen: display pointer, scope tap, and tile-tap callback
    _home.begin(&_display, &scopeTap,
        [](int idx) { if (_instance) _instance->_openSection(idx); });

    // SectionScreen: display pointer and back-button callback
    _section.begin(&_display);
    _section.setBackCallback(
        []() { if (_instance) _instance->_goHome(); });

    _home.markFullRedraw();
}

// =============================================================================
// updateDisplay() — rate-limited to FRAME_MS; safe to call every loop iteration.
// =============================================================================
void UIManager_TFT::updateDisplay(SynthEngine& synth) {
    _synthRef = &synth;
    const uint32_t now = millis();
    if ((now - _lastFrame) < FRAME_MS) return;
    _lastFrame = now;

    switch (_mode) {

        case Mode::HOME:
            _home.draw(synth);
            break;

        case Mode::SECTION:
            // syncFromEngine(): read CC values → update rows (cheap, no draw)
            // draw(): repaint only rows whose dirty flag is set
            _section.syncFromEngine();
            _section.draw();
            break;

        case Mode::BROWSER:
            _browser.draw(_display);
            break;

        case Mode::SCOPE_FULL:
            _drawFullScope(synth);
            break;
    }
}

// =============================================================================
// pollInputs() — high-frequency (>= 100 Hz) input poll.
// =============================================================================
void UIManager_TFT::pollInputs(HardwareInterface_MicroDexed& hw, SynthEngine& synth) {
    _synthRef = &synth;

    // Touch input — I2C poll
    if (_touchOk) {
        _touch.update();
        _handleTouch(synth);
    }

    // Encoder deltas and button presses
    using HW = HardwareInterface_MicroDexed;
    const int  dL = hw.getEncoderDelta(HW::ENC_LEFT);
    const int  dR = hw.getEncoderDelta(HW::ENC_RIGHT);
    const auto bL = hw.getButtonPress(HW::ENC_LEFT);
    const auto bR = hw.getButtonPress(HW::ENC_RIGHT);

    switch (_mode) {

        case Mode::HOME:
            if (dL)                    _home.onEncoderDelta(dL);
            if (bL == HW::PRESS_SHORT) _home.onEncoderPress();
            if (bL == HW::PRESS_LONG)  _setMode(Mode::SCOPE_FULL);
            break;

        case Mode::SECTION:
            // When entry overlay is open, left encoder scrolls the enum list.
            // SectionScreen.onEncoderLeft() handles the isEntryOpen check internally,
            // routing delta to TFTNumericEntry.onEncoderDelta() when appropriate.
            if (dL) _section.onEncoderLeft(dL);
            // Right encoder only adjusts CC values when no entry is open
            if (dR && !_section.isEntryOpen()) _section.onEncoderRight(dR);
            if (bL == HW::PRESS_SHORT) _section.onBackPress();
            if (bR == HW::PRESS_LONG)  _section.onEditPress();
            break;

        case Mode::BROWSER:
            if (dL)                    _browser.onEncoder(dL);
            if (bL == HW::PRESS_SHORT) _browser.onEncoderPress();
            // Left long or right short → cancel browser, return HOME
            if (bL == HW::PRESS_LONG || bR == HW::PRESS_SHORT) {
                _browser.close();
                _goHome();
            }
            break;

        case Mode::SCOPE_FULL:
            // Any button press returns to HOME
            if (bL != HW::PRESS_NONE || bR != HW::PRESS_NONE) _goHome();
            break;
    }
}

// =============================================================================
// syncFromEngine() — force repaint after preset load
// =============================================================================
void UIManager_TFT::syncFromEngine(SynthEngine& synth) {
    if (_mode == Mode::SECTION) _section.syncFromEngine();
    _home.markFullRedraw();
}

void UIManager_TFT::setCurrentPresetIdx(int idx)  { _currentPresetIdx = idx; }
int  UIManager_TFT::getCurrentPresetIdx()   const  { return _currentPresetIdx; }

// =============================================================================
// Private: mode switch
// =============================================================================
void UIManager_TFT::_setMode(Mode m) {
    if (m == _mode) return;
    _mode = m;
    _display.fillScreen(0x0000);
    if (m == Mode::HOME)       _home.markFullRedraw();
    if (m == Mode::SCOPE_FULL) _scopeFullFirstFrame = true;
}

void UIManager_TFT::_goHome() {
    _activeSect = -1;
    _setMode(Mode::HOME);
}

// =============================================================================
// Private: _openSection()
// PRESETS tile opens the browser; all other tiles open the section screen.
// =============================================================================
void UIManager_TFT::_openSection(int idx) {
    if (idx < 0 || idx >= SECTION_COUNT || !_synthRef) return;
    _activeSect = idx;
    const SectionDef& sect = kSections[idx];

    _display.fillScreen(0x0000);

    if (sectionIsBrowser(sect)) {
        // PRESETS tile — open the full-screen preset browser
        _browser.open(_synthRef, _currentPresetIdx,
            [](int globalIdx) {
                if (!_instance) return;
                Presets::presets_loadByGlobalIndex(*_instance->_synthRef, globalIdx);
                _instance->_currentPresetIdx = globalIdx;
                _instance->syncFromEngine(*_instance->_synthRef);
                _instance->_goHome();
            });
        _setMode(Mode::BROWSER);
    } else {
        // Normal synth section — open parameter screen
        _section.open(sect, *_synthRef);
        _setMode(Mode::SECTION);
    }
}

// =============================================================================
// Private: touch routing
// =============================================================================
void UIManager_TFT::_handleTouch(SynthEngine& synth) {
    const TouchInput::Gesture g  = _touch.getGesture();
    const TouchInput::Point   p  = _touch.getTouchPoint();      // lift position
    const TouchInput::Point   gs = _touch.getGestureStart();    // touch-down position

    switch (_mode) {

        case Mode::HOME:
            if (g == TouchInput::GESTURE_TAP) {
                _home.onTouch(p.x, p.y);
                if (_home.isScopeTapped()) _setMode(Mode::SCOPE_FULL);
            }
            if (g == TouchInput::GESTURE_HOLD)  _setMode(Mode::SCOPE_FULL);
            if (!_touch.isTouched())             _home.onTouchRelease(p.x, p.y);
            break;

        case Mode::SECTION:
            // TAP: open entry / select row  (use lift position — finger didn't move)
            if (g == TouchInput::GESTURE_TAP)        _section.onTouch(p.x, p.y);
            // SWIPE LEFT: back to home
            if (g == TouchInput::GESTURE_SWIPE_LEFT) _section.onBackPress();
            // SWIPE UP/DOWN: adjust the CC at the ROW WHERE THE FINGER STARTED.
            // We use gestureStart (not liftPoint) because the swipe gesture
            // begins on the parameter row and ends above/below it.  Using the
            // lift position would miss the row completely for fast swipes.
            if (g == TouchInput::GESTURE_SWIPE_UP)
                _section.onSwipeAdjust(gs.x, gs.y, +1);
            if (g == TouchInput::GESTURE_SWIPE_DOWN)
                _section.onSwipeAdjust(gs.x, gs.y, -1);
            break;

        case Mode::BROWSER:
            if (g == TouchInput::GESTURE_TAP) {
                _browser.onTouch(p.x, p.y);
                if (!_browser.isOpen()) _goHome();
            }
            // Swipe up = earlier items (lower index), swipe down = later items
            if (g == TouchInput::GESTURE_SWIPE_UP)   _browser.onEncoder(-PBLayout::VISIBLE_ROWS);
            if (g == TouchInput::GESTURE_SWIPE_DOWN) _browser.onEncoder(+PBLayout::VISIBLE_ROWS);
            break;

        case Mode::SCOPE_FULL:
            if (g == TouchInput::GESTURE_TAP) _goHome();
            break;
    }
}

// =============================================================================
// Private: full-screen oscilloscope
//
// Performance: static chrome (header/footer text) is drawn only once on mode
// entry via _scopeFullFirstFrame. Only the waveform band (y=20..219) is cleared
// each frame. This saves ~100 000 SPI bytes/frame vs fillScreen().
// =============================================================================
// _drawFullScope — repurposed as a colour calibration screen.
// Shows every named palette colour as a filled swatch with its hex value and name.
// This lets you verify the BGR565 pre-swap values are displaying the intended colours.
// Drawn once on entry (_scopeFullFirstFrame); press any button to return to HOME.
void UIManager_TFT::_drawFullScope(SynthEngine& /*synth*/) {

    // Only draw on first frame — this is a static reference screen, not animated.
    if (!_scopeFullFirstFrame) return;
    _scopeFullFirstFrame = false;

    // Clear to dark background
    _display.fillScreen(COLOUR_BACKGROUND);

    // ---- Header ----
    _display.fillRect(0, 0, 320, 18, COLOUR_HEADER_BG);
    _display.setTextSize(1);
    _display.setTextColor(COLOUR_SYSTEXT, COLOUR_HEADER_BG);
    _display.setCursor(4, 5);
    _display.print("COLOUR CALIBRATION");
    _display.setTextColor(COLOUR_TEXT_DIM, COLOUR_HEADER_BG);
    _display.setCursor(200, 5);
    _display.print("PRESS BTN TO EXIT");

    // ---- Colour table ----
    // Each entry: { value, name, intended hex comment }
    struct ColourEntry { uint16_t val; const char* name; const char* hex; };
    static const ColourEntry kColours[] = {
        { COLOUR_BACKGROUND, "BACKGROUND", "#101428" },
        { COLOUR_TEXT,       "TEXT",       "#D2D7E1" },
        { COLOUR_SYSTEXT,    "SYSTEXT",    "#FFA000" },
        { COLOUR_TEXT_DIM,   "TEXT_DIM",   "#787D8C" },
        { COLOUR_SELECTED,   "SELECTED",   "#FFA000" },
        { COLOUR_ACCENT,     "ACCENT",     "#FF1C18" },
        { COLOUR_OSC,        "OSC",        "#00CBFF" },
        { COLOUR_FILTER,     "FILTER",     "#FFB428" },
        { COLOUR_ENV,        "ENV",        "#DC32A0" },
        { COLOUR_LFO,        "LFO",        "#FF6C00" },
        { COLOUR_FX,         "FX",         "#00DCFF" },
        { COLOUR_GLOBAL,     "GLOBAL",     "#828291" },
        { COLOUR_HEADER_BG,  "HEADER_BG",  "#19233C" },
        { COLOUR_BORDER,     "BORDER",     "#2D3750" },
        { COLOUR_SCOPE_WAVE, "SCOPE_WAVE", "#00FF38" },
        { COLOUR_SCOPE_ZERO, "SCOPE_ZERO", "#006414" },
        { COLOUR_SCOPE_BG,   "SCOPE_BG",   "#000C00" },
        { COLOUR_METER_GREEN,"METER_GRN",  "#14DC14" },
        { COLOUR_METER_YELLOW,"METER_YEL", "#FFDC00" },
        { COLOUR_METER_RED,  "METER_RED",  "#FF1C18" },
    };
    static constexpr int kCount = sizeof(kColours) / sizeof(kColours[0]);

    // Layout: 2 columns, rows from y=20 downward
    // Each row: 18px tall, swatch 28px wide, name, hex value
    static constexpr int16_t ROW_H   = 11;
    static constexpr int16_t COL_W   = 160;  // half screen width
    static constexpr int16_t SWATCH  = 26;
    static constexpr int16_t START_Y = 20;

    for (int i = 0; i < kCount; ++i) {
        const int col = i / 10;                // 0=left, 1=right
        const int row = i % 10;
        const int16_t x = col * COL_W;
        const int16_t y = START_Y + row * ROW_H;

        // Filled colour swatch
        _display.fillRect(x + 2, y + 1, SWATCH, ROW_H - 2, kColours[i].val);
        // Thin border around swatch so near-black swatches are visible
        _display.drawRect(x + 2, y + 1, SWATCH, ROW_H - 2, COLOUR_BORDER);

        // Name
        _display.setTextSize(1);
        _display.setTextColor(COLOUR_TEXT, COLOUR_BACKGROUND);
        _display.setCursor(x + SWATCH + 5, y + 2);
        _display.print(kColours[i].name);

        // Intended hex colour (target colour in standard RGB)
        _display.setTextColor(COLOUR_TEXT_DIM, COLOUR_BACKGROUND);
        _display.setCursor(x + SWATCH + 5 + 7 * 6, y + 2);
        _display.print(kColours[i].hex);
    }

    // ---- Footer: raw value key ----
    _display.fillRect(0, 230, 320, 10, COLOUR_HEADER_BG);
    _display.setTextColor(COLOUR_TEXT_DIM, COLOUR_HEADER_BG);
    _display.setCursor(4, 232);
    _display.print("Swatch shows SENT value. Name=intended colour. Report any mismatches.");
}
