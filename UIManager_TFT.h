// UIManager_TFT.h
// =============================================================================
// Top-level UI manager for JT-4000 MicroDexed TFT variant.
//
// Navigation flow:
//   HOME (scope + tiles) --> tap synth tile --> SECTION (page tabs + param rows)
//                                                   --> tap row  --> ENTRY OVERLAY
//                        --> tap PRESETS tile  --> BROWSER (full-screen preset list)
//                        --> hold-L            --> SCOPE FULL SCREEN
//
// Setup sequence (called from Jteensy4000.ino):
//   1. ui.beginDisplay()     — SPI init + splash. Call BEFORE AudioMemory().
//   2. ui.begin(synth)       — wire screens. Call AFTER synth is ready.
//   3. ui.syncFromEngine()   — after preset load to force repaint.
//   4. ui.pollInputs()       — call at >= 100 Hz in loop().
//   5. ui.updateDisplay()    — call at ~30 Hz in loop() (rate-limited internally).
//
// Why two init functions?
//   SPI1 init (beginDisplay) must happen before AudioMemory() to avoid a race
//   where the audio DMA scheduler and SPI DMA both try to configure the same
//   bus at startup. begin(synth) needs a live SynthEngine reference so it comes
//   after synth initialisation.
//
// SPI clock:
//   Running ILI9341_t3n at 30 MHz (not the 50 MHz default). The 50 MHz clock
//   causes intermittent hard-faults on boards with longer SPI traces due to
//   signal integrity issues. 30 MHz is safe and still gives >30 fps refresh.
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
#include "JT4000_Sections.h"
#include "PresetBrowser.h"
#include "Presets.h"          // Presets::presets_loadByGlobalIndex — used in browser callback
#include <math.h>

extern AudioScopeTap scopeTap;

class UIManager_TFT {
public:
    // SPI1 pins (Teensy 4.1)
    static constexpr uint8_t  TFT_CS   = 41;
    static constexpr uint8_t  TFT_DC   = 37;
    static constexpr uint8_t  TFT_RST  = 24;
    static constexpr uint8_t  TFT_MOSI = 26;
    static constexpr uint8_t  TFT_SCK  = 27;
    static constexpr uint8_t  TFT_MISO = 39;

    // 30 MHz is safe for typical wiring lengths.
    // The ILI9341_t3n default is 50 MHz which can cause hard-faults on
    // boards with longer traces or imperfect termination.
    static constexpr uint32_t SPI_CLOCK_HZ = 30000000;

    // Frame rate cap: 30 fps = 33 ms per frame.
    // Reduce to 66 (15 fps) if SPI load causes audio glitches.
    static constexpr uint32_t FRAME_MS = 33;

    enum class Mode : uint8_t { HOME = 0, SECTION, SCOPE_FULL, BROWSER };

    UIManager_TFT()
        : _display(TFT_CS, TFT_DC, TFT_RST, TFT_MOSI, TFT_SCK, TFT_MISO)
        , _touchOk(false)
        , _mode(Mode::HOME)
        , _activeSect(-1)
        , _lastFrame(0)
        , _synthRef(nullptr)
        , _currentPresetIdx(0)
        , _scopeFullFirstFrame(true)
    {}

    // =========================================================================
    // beginDisplay() — hardware-only init. Call BEFORE AudioMemory().
    //   - Initialises SPI1 at 30 MHz
    //   - Sets rotation and clears screen
    //   - Shows boot splash
    //   - Initialises touch controller
    // =========================================================================
    void beginDisplay() {
        // 30 MHz: safe, reliable. 50 MHz default causes marginal-hardware faults.
        _display.begin(SPI_CLOCK_HZ);
        _display.setRotation(3);          // landscape 320×240
        _display.fillScreen(0x0000);

        // Initialise touch now (I2C) — safe before AudioMemory
        _touchOk = _touch.begin();
        Serial.printf("[UI] Touch: %s\n", _touchOk ? "OK" : "not found");

        // Boot splash — confirms display is working before audio starts
        _display.setTextSize(3);
        _display.setTextColor(0xFD20);    // orange
        _display.setCursor(60, 90);
        _display.print("JT.4000");

        _display.setTextSize(1);
        _display.setTextColor(0x7BEF);    // mid-grey
        _display.setCursor(74, 124);
        _display.print("MicroDexed Edition");

        delay(800);   // splash visible long enough to read
        _display.fillScreen(0x0000);
    }

    // =========================================================================
    // begin() — wire screens to engine. Call AFTER AudioMemory() and synth init.
    // =========================================================================
    void begin(SynthEngine& synth) {
        _synthRef = &synth;
        _instance = this;

        // Wire HomeScreen: display, scope tap, and tile-tap callback
        _home.begin(&_display, &scopeTap,
            [](int idx) { if (_instance) _instance->_openSection(idx); });

        // Wire SectionScreen: display and back-button callback
        _section.begin(&_display);
        _section.setBackCallback(
            []() { if (_instance) _instance->_goHome(); });

        _home.markFullRedraw();
    }

    // =========================================================================
    // updateDisplay() — call at ~30 Hz from loop().
    //   Rate-limited internally to FRAME_MS so calling it more often is harmless.
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
                // syncFromEngine reads CC values from engine into rows (cheap).
                // draw() only repaints rows whose value changed (dirty-flag gated).
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

    // =========================================================================
    // pollInputs() — call at >= 100 Hz from loop().
    // =========================================================================
    void pollInputs(HardwareInterface_MicroDexed& hw, SynthEngine& synth) {
        _synthRef = &synth;

        // Touch input (polls the FT6206 via I2C)
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
                if (_section.isEntryOpen()) break;  // entry overlay eats all input
                if (dL)                    _section.onEncoderLeft(dL);
                if (dR)                    _section.onEncoderRight(dR);
                if (bL == HW::PRESS_SHORT) _section.onBackPress();
                if (bR == HW::PRESS_LONG)  _section.onEditPress();
                break;

            case Mode::BROWSER:
                if (dL)                    _browser.onEncoder(dL);
                if (bL == HW::PRESS_SHORT) _browser.onEncoderPress();
                // Left long or right short: cancel browser, back to HOME
                if (bL == HW::PRESS_LONG || bR == HW::PRESS_SHORT) {
                    _browser.close();
                    _goHome();
                }
                break;

            case Mode::SCOPE_FULL:
                // Any button returns to HOME
                if (bL != HW::PRESS_NONE || bR != HW::PRESS_NONE) _goHome();
                break;
        }
    }

    // =========================================================================
    // syncFromEngine() — force repaint after preset load.
    // =========================================================================
    void syncFromEngine(SynthEngine& synth) {
        if (_mode == Mode::SECTION) _section.syncFromEngine();
        _home.markFullRedraw();
    }

    void setCurrentPresetIdx(int idx) { _currentPresetIdx = idx; }
    int  getCurrentPresetIdx()  const { return _currentPresetIdx; }

    // Compatibility stubs (match UIManager_MicroDexed public API)
    void setPage(int)                           {}
    int  getCurrentPage()             const     { return 0; }
    void selectParameter(int)                   {}
    int  getSelectedParameter()                 { return 0; }
    void setParameterLabel(int, const char*)    {}

private:
    static UIManager_TFT* _instance;  // singleton for static callbacks

    // -------------------------------------------------------------------------
    // _setMode() — screen clear and mode switch.
    // -------------------------------------------------------------------------
    void _setMode(Mode m) {
        if (m == _mode) return;
        _mode = m;
        _display.fillScreen(0x0000);
        if (m == Mode::HOME)       _home.markFullRedraw();
        if (m == Mode::SCOPE_FULL) _scopeFullFirstFrame = true;
    }

    void _goHome() {
        _activeSect = -1;
        _setMode(Mode::HOME);
    }

    // -------------------------------------------------------------------------
    // _openSection() — tile tap handler.
    //   PRESETS tile (sectionIsBrowser() == true): opens PresetBrowser.
    //   All others: opens SectionScreen.
    // -------------------------------------------------------------------------
    void _openSection(int idx) {
        if (idx < 0 || idx >= SECTION_COUNT || !_synthRef) return;
        _activeSect = idx;
        const SectionDef& sect = kSections[idx];

        _display.fillScreen(0x0000);

        if (sectionIsBrowser(sect)) {
            // PRESETS tile — open the preset browser modal
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

    // -------------------------------------------------------------------------
    // _handleTouch() — route gesture to active mode handler.
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
                if (g == TouchInput::GESTURE_HOLD)   _setMode(Mode::SCOPE_FULL);
                if (!_touch.isTouched())              _home.onTouchRelease(p.x, p.y);
                break;

            case Mode::SECTION:
                if (g == TouchInput::GESTURE_TAP)        _section.onTouch(p.x, p.y);
                if (g == TouchInput::GESTURE_SWIPE_LEFT) _section.onBackPress();
                break;

            case Mode::BROWSER:
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
    // _drawFullScope() — full-screen oscilloscope.
    //
    // PERF: Static chrome (header/footer text) drawn once on mode entry only.
    //       Only the waveform band (y=20..219) is cleared each frame.
    //       This saves ~100,000 SPI bytes per frame vs fillScreen().
    // -------------------------------------------------------------------------
    void _drawFullScope(SynthEngine& /*synth*/) {

        // Draw static chrome only on first frame after entering SCOPE_FULL
        if (_scopeFullFirstFrame) {
            _scopeFullFirstFrame = false;

            // Header
            _display.fillRect(0, 0, 320, 20, 0x1082);
            _display.setTextSize(1);
            _display.setTextColor(0x07FF);
            _display.setCursor(4, 6);
            _display.print("OSCILLOSCOPE");

            // Footer
            _display.fillRect(0, 220, 320, 20, 0x1082);
            _display.setTextSize(1);
            _display.setTextColor(0x4208);
            _display.setCursor(4, 226);
            _display.print("TAP OR PRESS ANY BUTTON TO RETURN");
        }

        // CPU% in header — update every frame (small region only)
        {
            char cpuBuf[12];
            snprintf(cpuBuf, sizeof(cpuBuf), "CPU:%d%%", (int)AudioProcessorUsageMax());
            const int16_t cpuX = 320 - (int16_t)(strlen(cpuBuf) * 6) - 4;
            _display.fillRect(cpuX - 2, 2, 320 - cpuX + 2, 16, 0x1082);
            _display.setTextColor(0x7BEF);
            _display.setCursor(cpuX, 6);
            _display.print(cpuBuf);
        }

        // Clear ONLY the waveform area — not the whole screen
        _display.fillRect(0, 20, 320, 200, 0x0000);

        // Waveform area
        static int16_t buf[512];
        const uint16_t n  = scopeTap.snapshot(buf, 512);
        const int16_t  wy = 22, wh = 198, ww = 288;

        _display.drawRect(0, wy, ww, wh, 0x1082);

        if (n >= 64) {
            // Find rising zero-crossing for stable trigger
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

                // Box-filter: average spp samples per display column
                int32_t acc = 0; int cnt = 0;
                for (int s = 0; s < spp && (base + s) < (int)n; ++s) {
                    acc += buf[base + s]; cnt++;
                }
                const int16_t samp = cnt ? (int16_t)(acc / cnt) : 0;

                // Map ±32767 to display y, 80% of height so clipping is visible
                int cy = midY - (int)((int32_t)samp * (wh - 4) * 4 / (32767 * 5));
                cy = constrain(cy, wy + 1, wy + wh - 1);

                if (col > 0) _display.drawLine(px, py, col + 1, cy, 0x07E0);
                px = col + 1; py = cy;
            }
            _display.drawFastHLine(1, midY, ww - 2, 0x2104);   // zero reference line
        }
    }

    // ---- Members ------------------------------------------------------------
    ILI9341_t3n   _display;
    TouchInput    _touch;
    bool          _touchOk;
    Mode          _mode;
    int           _activeSect;
    uint32_t      _lastFrame;
    SynthEngine*  _synthRef;
    HomeScreen    _home;
    SectionScreen _section;
    PresetBrowser _browser;
    int           _currentPresetIdx;
    bool          _scopeFullFirstFrame;
};

UIManager_TFT* UIManager_TFT::_instance = nullptr;
