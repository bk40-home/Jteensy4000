// UIManager_TFT.h
// =============================================================================
// Top-level UI manager for JT-4000 MicroDexed TFT variant.
// Replaces UIManager_MicroDexed with section-based navigation.
//
// Navigation flow:
//   HOME (scope + tiles) --> tap tile --> SECTION (page tabs + param rows)
//                                               --> tap row  --> ENTRY OVERLAY
//                        --> hold-L   --> SCOPE FULL SCREEN
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
//   SCOPE:   any button=back to home
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

    enum class Mode : uint8_t { HOME=0, SECTION, SCOPE_FULL };

    UIManager_TFT()
        : _display(TFT_CS,TFT_DC,TFT_RST,TFT_MOSI,TFT_SCK,TFT_MISO)
        , _touchOk(false), _mode(Mode::HOME)
        , _activeSect(-1), _lastFrame(0), _synthRef(nullptr)
    {}

    // =========================================================================
    // begin() — call once in setup()
    // =========================================================================
    void begin(SynthEngine& synth) {
        _synthRef = &synth;
        _instance = this;

        _display.begin();
        _display.setRotation(3);
        _display.fillScreen(0x0000);

        _touchOk = _touch.begin();
        Serial.printf("[UI] Touch: %s\n", _touchOk ? "OK" : "not found");

        // Boot splash
        _display.setTextSize(3); _display.setTextColor(0xF800);
        _display.setCursor(60,90); _display.print("JT.4000");
        _display.setTextSize(1); _display.setTextColor(0x7BEF);
        _display.setCursor(90,124); _display.print("MicroDexed Edition");
        delay(900);
        _display.fillScreen(0x0000);

        // Wire up screens
        _home.begin(&_display, &scopeTap,
            [](int idx){ if(_instance) _instance->_openSection(idx); });

        _section.begin(&_display);
        _section.setBackCallback(
            [](){ if(_instance) _instance->_goHome(); });

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
                if (bL==HW::PRESS_SHORT)  _home.onEncoderPress();
                if (bL==HW::PRESS_LONG)   _setMode(Mode::SCOPE_FULL);
                break;

            case Mode::SECTION:
                if (_section.isEntryOpen()) break;
                if (dL)                   _section.onEncoderLeft(dL);
                if (dR)                   _section.onEncoderRight(dR);
                if (bL==HW::PRESS_SHORT)  _section.onBackPress();
                if (bR==HW::PRESS_LONG)   _section.onEditPress();
                break;

            case Mode::SCOPE_FULL:
                if (bL!=HW::PRESS_NONE || bR!=HW::PRESS_NONE) _goHome();
                break;
        }
    }

    // =========================================================================
    // syncFromEngine() — call after preset load to force repaint
    // =========================================================================
    void syncFromEngine(SynthEngine& synth) {
        if (_mode==Mode::SECTION) _section.syncFromEngine();
        _home.markFullRedraw();
    }

    // Compatibility stubs — match UIManager_MicroDexed public API
    void setPage(int)              {}
    int  getCurrentPage()  const   { return 0; }
    void selectParameter(int)      {}
    int  getSelectedParameter()    { return 0; }
    void setParameterLabel(int, const char*) {}

private:
    static UIManager_TFT* _instance;

    void _setMode(Mode m) {
        if (m==_mode) return;
        _mode = m;
        _display.fillScreen(0x0000);
        if (m==Mode::HOME) _home.markFullRedraw();
    }

    void _goHome() { _activeSect=-1; _setMode(Mode::HOME); }

    void _openSection(int idx) {
        if (idx<0 || idx>=SECTION_COUNT || !_synthRef) return;
        _activeSect = idx;
        _display.fillScreen(0x0000);
        _section.open(kSections[idx], *_synthRef);
        _setMode(Mode::SECTION);
    }

    void _handleTouch(SynthEngine& synth) {
        const TouchInput::Gesture g = _touch.getGesture();
        const TouchInput::Point   p = _touch.getTouchPoint();

        switch (_mode) {
            case Mode::HOME:
                if (g==TouchInput::GESTURE_TAP) {
                    _home.onTouch(p.x, p.y);
                    if (_home.isScopeTapped()) _setMode(Mode::SCOPE_FULL);
                }
                if (g==TouchInput::GESTURE_HOLD) _setMode(Mode::SCOPE_FULL);
                if (!_touch.isTouched()) _home.onTouchRelease(p.x, p.y);
                break;

            case Mode::SECTION:
                if (g==TouchInput::GESTURE_TAP)        _section.onTouch(p.x,p.y);
                if (g==TouchInput::GESTURE_SWIPE_LEFT)  _section.onBackPress();
                break;

            case Mode::SCOPE_FULL:
                if (g==TouchInput::GESTURE_TAP) _goHome();
                break;
        }
    }

    // Full-screen scope view (no tiles, waveform only)
    void _drawFullScope(SynthEngine& synth) {
        _display.fillScreen(0x0000);

        // Header
        _display.fillRect(0,0,320,20,0x1082);
        _display.setTextSize(1); _display.setTextColor(0x07FF);
        _display.setCursor(4,6); _display.print("OSCILLOSCOPE");
        char cpuBuf[12];
        snprintf(cpuBuf,sizeof(cpuBuf),"CPU:%d%%",(int)AudioProcessorUsageMax());
        _display.setTextColor(0x7BEF);
        _display.setCursor(320-(int16_t)(strlen(cpuBuf)*6)-4,6);
        _display.print(cpuBuf);

        // Waveform
        static int16_t buf[512];
        const uint16_t n = scopeTap.snapshot(buf,512);
        const int16_t wy=22, wh=198, ww=290;
        _display.drawRect(0,wy,ww,wh,0x1082);

        if (n>=64) {
            int trig=n/4;
            for (int i=n/4; i<(int)n-64; ++i)
                if (buf[i]<=0 && buf[i+1]>0) { trig=i; break; }
            const int16_t midY=wy+wh/2;
            const int spp=((int)n>ww)?(n/ww):1;
            int16_t px=1,py=midY;
            for (int col=0; col<ww-2; ++col) {
                const int base=trig+col*spp;
                if (base>=(int)n) break;
                int32_t acc=0; int cnt=0;
                for (int s=0;s<spp&&(base+s)<(int)n;++s){acc+=buf[base+s];cnt++;}
                const int16_t samp=cnt?(int16_t)(acc/cnt):0;
                int cy=midY-(int)((int32_t)samp*(wh-4)*4/(32767*5));
                cy=constrain(cy,wy+1,wy+wh-1);
                if(col>0) _display.drawLine(px,py,col+1,cy,0x07E0);
                px=col+1; py=cy;
            }
            _display.drawFastHLine(1,midY,ww-2,0x2104);
        }

        // Footer
        _display.fillRect(0,220,320,20,0x1082);
        _display.setTextSize(1); _display.setTextColor(0x4208);
        _display.setCursor(4,226); _display.print("TAP OR PRESS ANY BUTTON TO RETURN");
    }

    ILI9341_t3n    _display;
    TouchInput     _touch;
    bool           _touchOk;
    Mode           _mode;
    int            _activeSect;
    uint32_t       _lastFrame;
    SynthEngine*   _synthRef;
    HomeScreen     _home;
    SectionScreen  _section;
};

UIManager_TFT* UIManager_TFT::_instance = nullptr;
