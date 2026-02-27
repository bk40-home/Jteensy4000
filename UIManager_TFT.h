// UIManager_TFT.h
// =============================================================================
// Top-level UI manager for JT-4000 MicroDexed TFT variant.
// Replaces UIManager_MicroDexed with section-based navigation.
//
// Navigation flow:
//   HOME (scope + tiles) --> tap synth tile --> SECTION (page tabs + param rows)
//                                                   --> tap row  --> ENTRY OVERLAY
//                        --> tap PRESETS tile  --> BROWSER (full-screen preset list)
//                        --> hold-L            --> SCOPE FULL SCREEN
//
// Call sites in your main sketch are UNCHANGED:
//   ui.begin(synth)              in setup()
//   ui.pollInputs(hw, synth)     in loop()
//   ui.updateDisplay(synth)      in loop()
//   ui.syncFromEngine(synth)     after preset load
//
// Encoder wiring:
//   HOME:    L-delta=scroll tile  L-short=open tile  L-long=full scope
//   SECTION: L-delta=scroll row   L-short=back        R-delta=adjust value
//            R-long=open entry overlay
//   BROWSER: L-delta=scroll list  L-short=load+return  L-long=cancel+return
//   SCOPE:   any button=back to home
//
// Changes from previous version:
//   - Mode::BROWSER added
//   - _openSection() detects PRESETS tile (sectionIsBrowser()) and opens browser
//   - updateDisplay(), pollInputs(), _handleTouch() all handle Mode::BROWSER
//   - _browser.open() is called with a callback that loads the preset and
//     calls syncFromEngine(), then returns to HOME
//   - _currentPresetIdx tracks the globally loaded preset for browser pre-select
// =============================================================================

#pragma once
#include <Arduino.h>
#include "ILI9341_t3n.h"
#include "SynthEngine.h"
#include "HardwareInterface_MicroDexed.h"
#include "TouchInput.h"
#include "AudioScopeTap.h"
#include "HomeScreen.h"
#include "SectionScreen.h"
#include "JT4000_Sections.h"   // includes sectionIsBrowser() helper
#include "PresetBrowser.h"
#include <math.h>

extern AudioScopeTap scopeTap;

class UIManager_TFT {
public:
    // SPI1 pins — unchanged from UIManager_MicroDexed
    static constexpr uint8_t TFT_CS   = 41;
    static constexpr uint8_t TFT_DC   = 37;
    static constexpr uint8_t TFT_RST  = 24;
    static constexpr uint8_t TFT_MOSI = 26;
    static constexpr uint8_t TFT_SCK  = 27;
    static constexpr uint8_t TFT_MISO = 39;

    static constexpr uint32_t FRAME_MS = 33;  // ~30 fps cap

    // BROWSER: full-screen PresetBrowser modal (new)
    enum class Mode : uint8_t { HOME = 0, SECTION, SCOPE_FULL, BROWSER };

    UIManager_TFT()
        : _display(TFT_CS, TFT_DC, TFT_RST, TFT_MOSI, TFT_SCK, TFT_MISO)
        , _touchOk(false), _mode(Mode::HOME)
        , _activeSect(-1), _lastFrame(0), _synthRef(nullptr)
        , _currentPresetIdx(0)   // tracks which preset is loaded (for browser pre-select)
    {}

    // =========================================================================
    // begin() — call once in setup()
    // =========================================================================
    void begin(SynthEngine& synth) {
        _synthRef = &synth;
        _instance = this;

        _display.begin();
        _display.setRotation(3);
        _display.fillScreen(0x2104);

        _touchOk = _touch.begin();
        Serial.printf("[UI] Touch: %s\n", _touchOk ? "OK" : "not found");

        // Boot splash
        _display.setTextSize(3); _display.setTextColor(0xFD20);
        _display.setCursor(60, 90);  _display.print("JT.4000");
        _display.setTextSize(1);    _display.setTextColor(0xFD20);
        _display.setCursor(90, 124); _display.print("MicroDexed Edition");
        delay(900);
        _display.fillScreen(0x0000);

        // Wire up screens
        _home.begin(&_display, &scopeTap,
            [](int idx){ if (_instance) _instance->_openSection(idx); });

        _section.begin(&_display);
        _section.setBackCallback(
            [](){ if (_instance) _instance->_goHome(); });

        _home.markFullRedraw();
    }

    // =========================================================================
    // updateDisplay() — call at ~30 Hz from loop()
    // =========================================================================
    void updateDisplay(SynthEngine& synth) {
        _synthRef = &synth;
        const uint32_t now = millis();
        if ((now - _lastFrame) < FRAME_MS) return;
        _lastFrame = now;

        switch (_mode) {
            case Mode::HOME:
                _home.draw(synth);
                break;

            case Mode::SECTION:
                _section.syncFromEngine();
                _section.draw();
                break;

            case Mode::BROWSER:
                // PresetBrowser manages its own dirty/partial redraw internally
                _browser.draw(_display);
                break;

            case Mode::SCOPE_FULL:
                _drawFullScope(synth);
                break;
        }
    }

    // =========================================================================
    // pollInputs() — call at >= 100 Hz from loop()
    // =========================================================================
    void pollInputs(HardwareInterface_MicroDexed& hw, SynthEngine& synth) {
        _synthRef = &synth;

        if (_touchOk) {
            _touch.update();
            _handleTouch(synth);
        }

        using HW = HardwareInterface_MicroDexed;
        const int  dL = hw.getEncoderDelta(HW::ENC_LEFT);
        const int  dR = hw.getEncoderDelta(HW::ENC_RIGHT);
        const auto bL = hw.getButtonPress(HW::ENC_LEFT);
        const auto bR = hw.getButtonPress(HW::ENC_RIGHT);

        switch (_mode) {

            case Mode::HOME:
                if (dL)                   _home.onEncoderDelta(dL);
                if (bL == HW::PRESS_SHORT) _home.onEncoderPress();
                if (bL == HW::PRESS_LONG)  _setMode(Mode::SCOPE_FULL);
                break;

            case Mode::SECTION:
                if (_section.isEntryOpen()) break;
                if (dL)                   _section.onEncoderLeft(dL);
                if (dR)                   _section.onEncoderRight(dR);
                if (bL == HW::PRESS_SHORT) _section.onBackPress();
                if (bR == HW::PRESS_LONG)  _section.onEditPress();
                break;

            case Mode::BROWSER:
                // Left encoder: scroll the preset list
                if (dL) _browser.onEncoder(dL);

                // Left short press: confirm and load selected preset
                if (bL == HW::PRESS_SHORT) _browser.onEncoderPress();

                // Left long press or right short press: cancel, back to HOME
                if (bL == HW::PRESS_LONG || bR == HW::PRESS_SHORT) {
                    _browser.close();
                    _goHome();
                }
                break;

            case Mode::SCOPE_FULL:
                if (bL != HW::PRESS_NONE || bR != HW::PRESS_NONE) _goHome();
                break;
        }
    }

    // =========================================================================
    // syncFromEngine() — call after preset load to force repaint
    // =========================================================================
    void syncFromEngine(SynthEngine& synth) {
        if (_mode == Mode::SECTION) _section.syncFromEngine();
        _home.markFullRedraw();
    }

    // =========================================================================
    // setCurrentPresetIdx() — call after loading any preset so the browser
    // knows which entry to pre-select when opened next time.
    // =========================================================================
    void setCurrentPresetIdx(int idx) { _currentPresetIdx = idx; }
    int  getCurrentPresetIdx()  const { return _currentPresetIdx; }

    // Compatibility stubs — match UIManager_MicroDexed public API
    void setPage(int)              {}
    int  getCurrentPage()  const   { return 0; }
    void selectParameter(int)      {}
    int  getSelectedParameter()    { return 0; }
    void setParameterLabel(int, const char*) {}

private:
    static UIManager_TFT* _instance;

    // -------------------------------------------------------------------------
    // _setMode() — clear screen and switch mode
    // -------------------------------------------------------------------------
    void _setMode(Mode m) {
        if (m == _mode) return;
        _mode = m;
        _display.fillScreen(0x0000);
        if (m == Mode::HOME)       _home.markFullRedraw();
        if (m == Mode::SCOPE_FULL) _scopeFullFirstFrame = true;  // redraw chrome on first frame
    }

    void _goHome() {
        _activeSect = -1;
        _setMode(Mode::HOME);
    }

    // -------------------------------------------------------------------------
    // _openSection() — called when a home-screen tile is tapped.
    //   If the section is the PRESETS tile (sectionIsBrowser() == true),
    //   open the PresetBrowser instead of SectionScreen.
    // -------------------------------------------------------------------------
    void _openSection(int idx) {
        if (idx < 0 || idx >= SECTION_COUNT || !_synthRef) return;
        _activeSect = idx;

        const SectionDef& sect = kSections[idx];

        if (sectionIsBrowser(sect)) {
            // --- PRESETS tile: open PresetBrowser ---
            _display.fillScreen(0x0000);

            // Pass a static callback so the browser can load the patch and
            // return us to HOME.  The callback is a free function captured via
            // the static _instance pointer (same pattern used for all callbacks).
            _browser.open(_synthRef, _currentPresetIdx,
                [](int globalIdx) {
                    if (!_instance) return;
                    // Load the selected preset into the engine
                    Presets::presets_loadByGlobalIndex(*_instance->_synthRef, globalIdx);
                    // Remember which preset is now active
                    _instance->_currentPresetIdx = globalIdx;
                    // Sync any section screens
                    _instance->syncFromEngine(*_instance->_synthRef);
                    // Return to home (browser is already closed by PresetBrowser)
                    _instance->_goHome();
                });
            _setMode(Mode::BROWSER);

        } else {
            // --- Normal synth section: open SectionScreen ---
            _display.fillScreen(0x0000);
            _section.open(sect, *_synthRef);
            _setMode(Mode::SECTION);
        }
    }

    // -------------------------------------------------------------------------
    // _handleTouch() — route touch events to the active mode's handler
    // -------------------------------------------------------------------------
    void _handleTouch(SynthEngine& synth) {
        const TouchInput::Gesture g = _touch.getGesture();
        const TouchInput::Point   p = _touch.getTouchPoint();

        switch (_mode) {

            case Mode::HOME:
                if (g == TouchInput::GESTURE_TAP) {
                    _home.onTouch(p.x, p.y);
                    if (_home.isScopeTapped()) _setMode(Mode::SCOPE_FULL);
                }
                if (g == TouchInput::GESTURE_HOLD)    _setMode(Mode::SCOPE_FULL);
                if (!_touch.isTouched())               _home.onTouchRelease(p.x, p.y);
                break;

            case Mode::SECTION:
                if (g == TouchInput::GESTURE_TAP)       _section.onTouch(p.x, p.y);
                if (g == TouchInput::GESTURE_SWIPE_LEFT) _section.onBackPress();
                break;

            case Mode::BROWSER:
                // PresetBrowser.onTouch() returns true if it consumed the event.
                // If the browser closes itself (CANCEL or load), we go home.
                if (g == TouchInput::GESTURE_TAP) {
                    _browser.onTouch(p.x, p.y);
                    if (!_browser.isOpen()) _goHome();
                }
                break;

            case Mode::SCOPE_FULL:
                if (g == TouchInput::GESTURE_TAP) _goHome();
                break;
        }
    }

    // -------------------------------------------------------------------------
    // _drawFullScope() — full-screen oscilloscope view
    //
    // PERF FIX: Do NOT call fillScreen() here.  The header and footer are
    // static — drawn once on mode entry via _setMode().  Only the waveform
    // area (y=22..219) needs clearing each frame.  fillScreen at 30 fps
    // writes 153,600 bytes of SPI per frame for no visual benefit.
    //
    // _scopeFullFirstFrame tracks whether the static chrome has been drawn
    // so we skip it on subsequent frames.
    // -------------------------------------------------------------------------
    void _drawFullScope(SynthEngine& synth) {
        // Draw static chrome only on the first frame after entering SCOPE_FULL
        if (_scopeFullFirstFrame) {
            _scopeFullFirstFrame = false;
            // Header
            _display.fillRect(0, 0, 320, 20, 0x1082);
            _display.setTextSize(1); _display.setTextColor(0x07FF);
            _display.setCursor(4, 6); _display.print("OSCILLOSCOPE");
            // Footer (static hint text)
            _display.fillRect(0, 220, 320, 20, 0x1082);
            _display.setTextSize(1); _display.setTextColor(0x4208);
            _display.setCursor(4, 226); _display.print("TAP OR PRESS ANY BUTTON TO RETURN");
        }

        // CPU% in header updates every frame — clear just that region
        {
            char cpuBuf[12];
            snprintf(cpuBuf, sizeof(cpuBuf), "CPU:%d%%", (int)AudioProcessorUsageMax());
            const int16_t cpuX = 320 - (int16_t)(strlen(cpuBuf) * 6) - 4;
            _display.fillRect(cpuX - 2, 2, 320 - cpuX + 2, 16, 0x1082);  // clear CPU% region only
            _display.setTextColor(0x7BEF);
            _display.setCursor(cpuX, 6);
            _display.print(cpuBuf);
        }

        // Clear only the waveform area between header and footer (NOT the whole screen)
        _display.fillRect(0, 20, 320, 200, 0x0000);

        // Waveform area (adjusted for new header/footer offsets)
        static int16_t buf[512];
        const uint16_t n   = scopeTap.snapshot(buf, 512);
        const int16_t  wy  = 22, wh = 198, ww = 290;
        _display.drawRect(0, wy, ww, wh, 0x1082);

        if (n >= 64) {
            // Trigger: find first rising zero-crossing
            int trig = n / 4;
            for (int i = n / 4; i < (int)n - 64; ++i) {
                if (buf[i] <= 0 && buf[i + 1] > 0) { trig = i; break; }
            }

            const int16_t midY = wy + wh / 2;
            const int     spp  = ((int)n > ww) ? (n / ww) : 1;
            int16_t px = 1, py = midY;

            for (int col = 0; col < ww - 2; ++col) {
                const int base = trig + col * spp;
                if (base >= (int)n) break;

                // Average samples within this pixel column
                int32_t acc = 0; int cnt = 0;
                for (int s = 0; s < spp && (base + s) < (int)n; ++s) {
                    acc += buf[base + s]; cnt++;
                }
                const int16_t samp = cnt ? (int16_t)(acc / cnt) : 0;
                int cy = midY - (int)((int32_t)samp * (wh - 4) * 4 / (32767 * 5));
                cy = constrain(cy, wy + 1, wy + wh - 1);
                if (col > 0) _display.drawLine(px, py, col + 1, cy, 0x07E0);
                px = col + 1; py = cy;
            }
            _display.drawFastHLine(1, midY, ww - 2, 0x2104);  // centre line
        }
    }

    // ---- Members ------------------------------------------------------------
    ILI9341_t3n    _display;
    TouchInput     _touch;
    bool           _touchOk;
    Mode           _mode;
    int            _activeSect;
    uint32_t       _lastFrame;
    SynthEngine*   _synthRef;
    HomeScreen     _home;
    SectionScreen  _section;
    PresetBrowser  _browser;       // preset browser modal (was declared but unused)
    int            _currentPresetIdx;  // global index of the currently loaded preset
    bool           _scopeFullFirstFrame = true;  // true = draw static chrome on next scope frame
};

UIManager_TFT* UIManager_TFT::_instance = nullptr;