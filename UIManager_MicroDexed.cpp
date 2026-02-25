// UIManager_MicroDexed.cpp
// =============================================================================
// ILI9341 320x240 TFT UI implementation for the JT-4000 MicroDexed variant.
//
// Key design points:
//   1. ccDisplayText() — enum CCs show human-readable text instead of 0-127:
//        OSC1/2 wave, LFO wave     -> waveformShortName()
//        LFO destination           -> getLFO1/2DestinationName()
//        AKWF bank                 -> akwf_bankName()
//        Glide enable/reverb bypass-> "On" / "Off"
//        LFO / delay timing mode   -> TimingModeNames[]
//        BPM clock source          -> "Internal" / "External"
//        Filter boolean toggles    -> "On" / "Off"
//
//   2. ccDisplayValue() — returns the CC integer for the current engine state,
//      using the same inverse-mapping curves as the OLED UIManager so encoder
//      positions align with actual parameter values.
//
//   3. Dirty-region tracking avoids full-screen redraws every frame.
//
//   4. Page titles come from UIPage::pageTitle[] — not the first param name.
//
//   5. snprintf() only — no heap-allocating Arduino String objects.
// =============================================================================

#include "UIManager_MicroDexed.h"
#include "UIPageLayout.h"       // UIPage::ccMap / ccNames / pageTitle / NUM_PAGES
#include "Mapping.h"            // JT4000Map inverse-curve helpers
#include "Waveforms.h"          // waveformShortName(), ccFromWaveform()
#include "AKWF_All.h"           // akwf_bankName(), akwf_bankCount()
#include "BPMClockManager.h"    // TimingModeNames[]


// =============================================================================
// Constructor
// =============================================================================

UIManager_MicroDexed::UIManager_MicroDexed()
    // ILI9341_t3n requires ALL SIX SPI pins so it can configure SPI1 at runtime.
    // Omitting MOSI/SCK/MISO causes t3n to leave SPI1 unconfigured -> blank display.
    : _display(TFT_CS, TFT_DC, TFT_RST, TFT_MOSI, TFT_SCK, TFT_MISO)
    , _touchEnabled(false)
    , _currentPage(0)
    , _selectedParam(0)
    , _displayMode(MODE_PARAMETERS)
    , _fullRedraw(true)
    , _headerDirty(true)
    , _footerDirty(true)
    , _lastFrameMillis(0)
    , _lastEncoderLeft(0)
    , _lastEncoderRight(0)
{
    // Dirty row flags — initialise to false (no rows to repaint yet)
    for (int i = 0; i < 4; ++i) _rowDirty[i] = false;
}


// =============================================================================
// begin() — hardware initialisation; call once in setup()
// =============================================================================

void UIManager_MicroDexed::begin() {
    _display.begin();
    _display.setRotation(3);                    // landscape 320x240
    _display.fillScreen(COLOUR_BACKGROUND);
    _display.setTextColor(COLOUR_TEXT);
    _display.setTextSize(FONT_MEDIUM);

    // Capacitive touch is optional — begin() returns false if hardware absent
    _touchEnabled = _touch.begin();
    Serial.printf("[UI] Touch: %s\n", _touchEnabled ? "enabled" : "not found");

    // Boot splash — visible for 1 second to confirm display is working
    drawTextCentred(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2,
                    "JTeensy 4000", COLOUR_ACCENT, FONT_LARGE);
    drawTextCentred(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 30,
                    "MicroDexed Edition", COLOUR_TEXT, FONT_SMALL);
    delay(1000);

    markFullRedraw();   // force complete repaint on first updateDisplay() call
}


// =============================================================================
// updateDisplay() — call at ~30 Hz from the main loop
// =============================================================================

void UIManager_MicroDexed::updateDisplay(SynthEngine& synth) {
    // Frame-rate cap: skip if not enough time has elapsed AND no urgent redraw
    const uint32_t now = millis();
    if ((now - _lastFrameMillis) < FRAME_INTERVAL_MS && !_fullRedraw) return;
    _lastFrameMillis = now;

    switch (_displayMode) {

        // Standard 4-parameter grid ------------------------------------------
        case MODE_PARAMETERS:
            if (_fullRedraw) {
                // Full repaint: clear screen, then draw all three regions
                _display.fillScreen(COLOUR_BACKGROUND);
                drawHeader(synth);
                drawParamGrid(synth);
                drawFooter();
            } else {
                // Incremental repaint: only update dirty regions
                if (_headerDirty) drawHeader(synth);
                if (_footerDirty) drawFooter();
                for (int i = 0; i < 4; ++i) {
                    if (_rowDirty[i]) {
                        drawParamRow(i, UIPage::ccMap[_currentPage][i],
                                     synth, (i == _selectedParam));
                    }
                }
            }
            break;

        // Oscilloscope view --------------------------------------------------
        case MODE_SCOPE:
            // Scope always redraws because the waveform changes every frame
            drawScopeView(synth);
            break;

        // Settings / preset menu ---------------------------------------------
        case MODE_MENU:
            if (_fullRedraw) drawMenuView();
            break;

        // Sentinel — should never be reached ---------------------------------
        case MODE_COUNT:
            break;
    }

    clearDirtyFlags();
}


// =============================================================================
// pollInputs() — call at >= 100 Hz for snappy encoder response
// =============================================================================

void UIManager_MicroDexed::pollInputs(HardwareInterface_MicroDexed& hw,
                                      SynthEngine& synth) {
    // Process touch before encoders so gesture state is current this cycle
    if (_touchEnabled) {
        _touch.update();
        handleTouch(synth);
    }

    // Read encoder deltas and button states from hardware abstraction layer
    const int  deltaLeft   = hw.getEncoderDelta(HardwareInterface_MicroDexed::ENC_LEFT);
    const int  deltaRight  = hw.getEncoderDelta(HardwareInterface_MicroDexed::ENC_RIGHT);
    const auto buttonLeft  = hw.getButtonPress(HardwareInterface_MicroDexed::ENC_LEFT);
    const auto buttonRight = hw.getButtonPress(HardwareInterface_MicroDexed::ENC_RIGHT);

    using HW = HardwareInterface_MicroDexed;

    switch (_displayMode) {

        // Parameter edit mode ------------------------------------------------
        case MODE_PARAMETERS:

            // Left encoder: scroll parameter selection (0-3, wraps)
            if (deltaLeft != 0) {
                _selectedParam = (_selectedParam + deltaLeft + 4) % 4;
                markFullRedraw();   // selection highlight requires full grid repaint
            }

            // Right encoder: adjust selected parameter value
            if (deltaRight != 0) {
                const uint8_t cc = UIPage::ccMap[_currentPage][_selectedParam];
                if (cc != 255) {    // 255 = empty slot, ignore
                    const int currentValue = synth.getCC(cc);
                    const int newValue     = constrain(currentValue + deltaRight, 0, 127);
                    if (newValue != currentValue) {
                        synth.setCC(cc, (uint8_t)newValue);
                        markRowDirty(_selectedParam);
                    }
                }
            }

            // Left button short press: advance to next page
            if (buttonLeft == HW::PRESS_SHORT) {
                _currentPage   = (_currentPage + 1) % UIPage::NUM_PAGES;
                _selectedParam = 0;
                markFullRedraw();
            }

            // Left button long press: enter oscilloscope view
            if (buttonLeft == HW::PRESS_LONG) {
                setMode(MODE_SCOPE);
            }

            // Right button long press: enter menu
            if (buttonRight == HW::PRESS_LONG) {
                setMode(MODE_MENU);
            }
            break;

        // Scope mode ---------------------------------------------------------
        case MODE_SCOPE:
            // Any button press returns to parameter view
            if (buttonLeft != HW::PRESS_NONE || buttonRight != HW::PRESS_NONE) {
                setMode(MODE_PARAMETERS);
            }
            break;

        // Menu mode ----------------------------------------------------------
        case MODE_MENU:
            // Any left button press returns to parameter view
            if (buttonLeft != HW::PRESS_NONE) {
                setMode(MODE_PARAMETERS);
            }
            break;

        case MODE_COUNT:
            break;
    }
}


// =============================================================================
// Page / parameter / mode management
// =============================================================================

void UIManager_MicroDexed::setPage(int page) {
    const int clamped = constrain(page, 0, UIPage::NUM_PAGES - 1);
    if (clamped == _currentPage) return;
    _currentPage   = clamped;
    _selectedParam = 0;
    markFullRedraw();
}

void UIManager_MicroDexed::selectParameter(int index) {
    const int clamped = constrain(index, 0, 3);
    if (clamped == _selectedParam) return;
    _selectedParam = clamped;
    markFullRedraw();
}

void UIManager_MicroDexed::setMode(DisplayMode mode) {
    if (mode == _displayMode) return;
    _displayMode = mode;
    markFullRedraw();
}

void UIManager_MicroDexed::syncFromEngine(SynthEngine& /*synth*/) {
    // Invalidate everything; next updateDisplay() will repaint from engine state
    markFullRedraw();
}


// =============================================================================
// ENUM-AWARE DISPLAY HELPERS
// =============================================================================

// -----------------------------------------------------------------------------
// ccDisplayText() — returns a text label for enum-like CCs, nullptr otherwise.
// nullptr signals the caller to print the raw 0-127 number instead.
// -----------------------------------------------------------------------------

const char* UIManager_MicroDexed::ccDisplayText(uint8_t cc,
                                                  SynthEngine& synth) const {
    switch (cc) {

        // Oscillator waveforms — e.g. "SAW", "SQR", "SSAW", "SINE"
        case CC::OSC1_WAVE:      return synth.getOsc1WaveformName();
        case CC::OSC2_WAVE:      return synth.getOsc2WaveformName();

        // LFO waveforms — same short names as oscillator waveforms
        case CC::LFO1_WAVEFORM:  return synth.getLFO1WaveformName();
        case CC::LFO2_WAVEFORM:  return synth.getLFO2WaveformName();

        // LFO modulation destinations — e.g. "Pitch", "Filter", "Shape", "Amp"
        case CC::LFO1_DESTINATION: return synth.getLFO1DestinationName();
        case CC::LFO2_DESTINATION: return synth.getLFO2DestinationName();

        // AKWF arbitrary waveform bank name — e.g. "BwBlended", "BwSaw"
        case CC::OSC1_ARB_BANK:  return akwf_bankName(synth.getOsc1ArbBank());
        case CC::OSC2_ARB_BANK:  return akwf_bankName(synth.getOsc2ArbBank());

        // Boolean toggles
        case CC::GLIDE_ENABLE:
            return synth.getGlideEnabled() ? "On" : "Off";
        case CC::FX_REVERB_BYPASS:
            return synth.getFXReverbBypass() ? "Bypass" : "Active";
        case CC::FILTER_OBXA_TWO_POLE:
            return synth.getFilterTwoPole() ? "On" : "Off";
        case CC::FILTER_OBXA_XPANDER_4_POLE:
            return synth.getFilterXpander4Pole() ? "On" : "Off";
        case CC::FILTER_OBXA_BP_BLEND_2_POLE:
            return synth.getFilterBPBlend2Pole() ? "On" : "Off";
        case CC::FILTER_OBXA_PUSH_2_POLE:
            return synth.getFilterPush2Pole() ? "On" : "Off";

        // LFO BPM sync timing modes — e.g. "Free", "1/4", "1/8T"
        case CC::LFO1_TIMING_MODE: {
            const int mode = (int)synth.getLFO1TimingMode();
            return (mode >= 0 && mode < 12) ? TimingModeNames[mode] : "?";
        }
        case CC::LFO2_TIMING_MODE: {
            const int mode = (int)synth.getLFO2TimingMode();
            return (mode >= 0 && mode < 12) ? TimingModeNames[mode] : "?";
        }
        case CC::DELAY_TIMING_MODE: {
            const int mode = (int)synth.getDelayTimingMode();
            return (mode >= 0 && mode < 12) ? TimingModeNames[mode] : "?";
        }

        // BPM clock source — below 64 = internal MIDI clock, above = external
        case CC::BPM_CLOCK_SOURCE:
            return (synth.getCC(CC::BPM_CLOCK_SOURCE) >= 64) ? "External" : "Internal";

        // Purely numeric CCs — let the caller format the number
        default:
            return nullptr;
    }
}

// -----------------------------------------------------------------------------
// ccDisplayValue() — the integer CC value for the current engine state,
// using the correct inverse-mapping curve for each parameter type.
// -----------------------------------------------------------------------------

int UIManager_MicroDexed::ccDisplayValue(uint8_t cc, SynthEngine& synth) const {
    using namespace JT4000Map;

    switch (cc) {

        // Oscillator waveforms — round-trip through waveform->CC lookup
        case CC::OSC1_WAVE:
            return ccFromWaveform((WaveformType)synth.getOsc1Waveform());
        case CC::OSC2_WAVE:
            return ccFromWaveform((WaveformType)synth.getOsc2Waveform());

        // Filter — exponential forward curve, so use inverse here
        case CC::FILTER_CUTOFF:
            return obxa_cutoff_hz_to_cc(synth.getFilterCutoff());
        case CC::FILTER_RESONANCE:
            return obxa_res01_to_cc(synth.getFilterResonance());

        // Amplitude envelope — inverse log time curve
        case CC::AMP_ATTACK:   return time_ms_to_cc(synth.getAmpAttack());
        case CC::AMP_DECAY:    return time_ms_to_cc(synth.getAmpDecay());
        case CC::AMP_SUSTAIN:  return norm_to_cc(synth.getAmpSustain());
        case CC::AMP_RELEASE:  return time_ms_to_cc(synth.getAmpRelease());

        // Filter envelope — same log time curve
        case CC::FILTER_ENV_ATTACK:
            return time_ms_to_cc(synth.getFilterEnvAttack());
        case CC::FILTER_ENV_DECAY:
            return time_ms_to_cc(synth.getFilterEnvDecay());
        case CC::FILTER_ENV_SUSTAIN:
            return norm_to_cc(synth.getFilterEnvSustain());
        case CC::FILTER_ENV_RELEASE:
            return time_ms_to_cc(synth.getFilterEnvRelease());

        // Bipolar parameters — remap ±1.0 range to 0-127
        case CC::FILTER_ENV_AMOUNT:
            return norm_to_cc((synth.getFilterEnvAmount() + 1.0f) * 0.5f);
        case CC::FILTER_KEY_TRACK:
            return norm_to_cc((synth.getFilterKeyTrackAmount() + 1.0f) * 0.5f);
        case CC::OSC1_DETUNE:
            return norm_to_cc((synth.getOsc1Detune() + 1.0f) * 0.5f);
        case CC::OSC2_DETUNE:
            return norm_to_cc((synth.getOsc2Detune() + 1.0f) * 0.5f);
        case CC::OSC1_FINE_TUNE:
            return norm_to_cc((synth.getOsc1FineTune() + 100.0f) / 200.0f);
        case CC::OSC2_FINE_TUNE:
            return norm_to_cc((synth.getOsc2FineTune() + 100.0f) / 200.0f);

        // Coarse pitch — 5 steps: -24/-12/0/+12/+24 semitones -> 0..127
        case CC::OSC1_PITCH_OFFSET: {
            const float semitones = synth.getOsc1PitchOffset();
            if (semitones <= -24.0f) return 13;
            if (semitones <= -12.0f) return 38;
            if (semitones <=   0.0f) return 64;
            if (semitones <=  12.0f) return 89;
            return 114;
        }
        case CC::OSC2_PITCH_OFFSET: {
            const float semitones = synth.getOsc2PitchOffset();
            if (semitones <= -24.0f) return 13;
            if (semitones <= -12.0f) return 38;
            if (semitones <=   0.0f) return 64;
            if (semitones <=  12.0f) return 89;
            return 114;
        }

        // LFO frequency — log curve inverse
        case CC::LFO1_FREQ:  return lfo_hz_to_cc(synth.getLFO1Frequency());
        case CC::LFO2_FREQ:  return lfo_hz_to_cc(synth.getLFO2Frequency());
        case CC::LFO1_DEPTH: return norm_to_cc(synth.getLFO1Amount());
        case CC::LFO2_DEPTH: return norm_to_cc(synth.getLFO2Amount());

        // LFO modulation destinations — bin index to CC midpoint
        case CC::LFO1_DESTINATION:
            return ccFromLfoDest((int)synth.getLFO1Destination());
        case CC::LFO2_DESTINATION:
            return ccFromLfoDest((int)synth.getLFO2Destination());

        // LFO waveforms
        case CC::LFO1_WAVEFORM:
            return ccFromWaveform((WaveformType)synth.getLFO1Waveform());
        case CC::LFO2_WAVEFORM:
            return ccFromWaveform((WaveformType)synth.getLFO2Waveform());

        // AKWF arbitrary waveform index — scaled relative to bank size
        case CC::OSC1_ARB_INDEX: {
            const uint16_t count = akwf_bankCount(synth.getOsc1ArbBank());
            if (count == 0) return 0;
            return (int)((synth.getOsc1ArbIndex() * 127u) / (count - 1));
        }
        case CC::OSC2_ARB_INDEX: {
            const uint16_t count = akwf_bankCount(synth.getOsc2ArbBank());
            if (count == 0) return 0;
            return (int)((synth.getOsc2ArbIndex() * 127u) / (count - 1));
        }

        // All other CCs are linear 0-127 — read directly from the CC cache
        default:
            return (int)synth.getCC(cc);
    }
}


// =============================================================================
// DRAWING — HEADER BAR
// =============================================================================

void UIManager_MicroDexed::drawHeader(SynthEngine& synth) {
    _display.fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, COLOUR_HEADER_BG);

    // Page number and descriptive title from UIPage::pageTitle[]
    char titleBuffer[28];
    snprintf(titleBuffer, sizeof(titleBuffer), "P%d: %s",
             _currentPage + 1, UIPage::pageTitle[_currentPage]);
    _display.setCursor(SCREEN_MARGIN, SCREEN_MARGIN);
    _display.setTextColor(COLOUR_TEXT);
    _display.setTextSize(FONT_MEDIUM);
    _display.print(titleBuffer);

    // CPU usage % — top-right corner (live performance monitoring)
    char cpuBuffer[14];
    snprintf(cpuBuffer, sizeof(cpuBuffer), "CPU:%d%%", (int)AudioProcessorUsageMax());
    drawTextRight(SCREEN_WIDTH - SCREEN_MARGIN, SCREEN_MARGIN,
                  cpuBuffer, COLOUR_TEXT_DIM, FONT_SMALL);

    // Horizontal separator between header and parameter rows
    _display.drawFastHLine(0, HEADER_HEIGHT - 1, SCREEN_WIDTH, COLOUR_BORDER);
}


// =============================================================================
// DRAWING — PARAMETER GRID
// =============================================================================

void UIManager_MicroDexed::drawParamGrid(SynthEngine& synth) {
    for (int i = 0; i < 4; ++i) {
        const uint8_t cc = UIPage::ccMap[_currentPage][i];
        drawParamRow(i, cc, synth, (i == _selectedParam));
    }
}

// -----------------------------------------------------------------------------
// drawParamRow() — renders one parameter row.
//
// Row layout (PARAM_ROW_HEIGHT px tall):
//   [ Name (left-aligned)              Value text or number (right-aligned) ]
//   [ XXXXXXXX bar (filled) ........... track (empty) ..................... ]
//
// Selected row: COLOUR_SELECTED background, COLOUR_BACKGROUND text
// Empty slot (cc == 255): shows "---" name only — no value or bar drawn
// Enum CC: human-readable text label instead of raw number
// -----------------------------------------------------------------------------

void UIManager_MicroDexed::drawParamRow(int row, uint8_t cc,
                                        SynthEngine& synth, bool selected) {
    const int rowY = HEADER_HEIGHT + SCREEN_MARGIN + row * PARAM_ROW_HEIGHT;
    const int rowX = SCREEN_MARGIN;
    const int rowW = SCREEN_WIDTH  - 2 * SCREEN_MARGIN;
    const int rowH = PARAM_ROW_HEIGHT - 2;   // 2px gap between rows

    // Row background — highlighted if selected
    const uint16_t backgroundColour = selected ? COLOUR_SELECTED   : COLOUR_BACKGROUND;
    const uint16_t textColour       = selected ? COLOUR_BACKGROUND : COLOUR_TEXT;
    _display.fillRect(rowX, rowY, rowW, rowH, backgroundColour);

    // Parameter name — left-aligned, from UIPage::ccNames[]
    const char* name = UIPage::ccNames[_currentPage][row];
    if (!name || name[0] == '\0') name = "---";

    _display.setTextColor(textColour);
    _display.setTextSize(FONT_MEDIUM);
    _display.setCursor(rowX + SCREEN_MARGIN, rowY + 6);
    _display.print(name);

    // Empty slot — name only, nothing else to draw
    if (cc == 255) return;

    // Value — enum text has priority over raw number
    const char* enumText = ccDisplayText(cc, synth);
    const int   rawValue = ccDisplayValue(cc, synth);   // 0-127

    if (enumText) {
        drawTextRight(rowX + rowW - SCREEN_MARGIN, rowY + 6,
                      enumText, textColour, FONT_MEDIUM);
    } else {
        char valueBuffer[8];
        snprintf(valueBuffer, sizeof(valueBuffer), "%d", rawValue);
        drawTextRight(rowX + rowW - SCREEN_MARGIN, rowY + 6,
                      valueBuffer, textColour, FONT_MEDIUM);
    }

    // Value bar — 4px tall, filled width proportional to 0-127
    // Background track (full width, dim) drawn first, then active portion
    const uint16_t barColour   = selected ? COLOUR_BACKGROUND : paramColour(cc);
    const int      barMaxWidth = rowW - 2 * SCREEN_MARGIN;
    const int      barWidth    = (barMaxWidth * rawValue) / 127;
    const int      barY        = rowY + rowH - 5;

    _display.drawFastHLine(rowX + SCREEN_MARGIN, barY, barMaxWidth, COLOUR_BORDER);
    if (barWidth > 0) {
        _display.fillRect(rowX + SCREEN_MARGIN, barY, barWidth, 4, barColour);
    }
}


// =============================================================================
// DRAWING — FOOTER BAR
// =============================================================================

void UIManager_MicroDexed::drawFooter() {
    const int footerY = SCREEN_HEIGHT - FOOTER_HEIGHT;
    _display.fillRect(0, footerY, SCREEN_WIDTH, FOOTER_HEIGHT, COLOUR_HEADER_BG);
    _display.drawFastHLine(0, footerY, SCREEN_WIDTH, COLOUR_BORDER);
    _display.setCursor(SCREEN_MARGIN, footerY + 5);
    _display.setTextColor(COLOUR_TEXT_DIM);
    _display.setTextSize(FONT_SMALL);
    // Encoder hint line: L selects parameter, R adjusts, Hold-L scope, Hold-R menu
    _display.print("L:Param  R:Adjust  Hold-L:Scope  Hold-R:Menu");
}


// =============================================================================
// DRAWING — MENU VIEW (placeholder)
// =============================================================================

void UIManager_MicroDexed::drawMenuView() {
    _display.fillScreen(COLOUR_BACKGROUND);
    drawTextCentred(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2,
                    "MENU", COLOUR_ACCENT, FONT_LARGE);
    drawTextCentred(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 30,
                    "(not yet implemented)", COLOUR_TEXT_DIM, FONT_SMALL);
}


// =============================================================================
// TOUCH INPUT
// =============================================================================

void UIManager_MicroDexed::handleTouch(SynthEngine& synth) {
    const TouchInput::Gesture gesture = _touch.getGesture();

    switch (_displayMode) {

        case MODE_PARAMETERS:
            if (gesture == TouchInput::GESTURE_SWIPE_UP) {
                // Swipe up -> previous page
                _currentPage = (_currentPage - 1 + UIPage::NUM_PAGES) % UIPage::NUM_PAGES;
                markFullRedraw();

            } else if (gesture == TouchInput::GESTURE_SWIPE_DOWN) {
                // Swipe down -> next page
                _currentPage = (_currentPage + 1) % UIPage::NUM_PAGES;
                markFullRedraw();

            } else if (gesture == TouchInput::GESTURE_TAP) {
                // Tap -> select the tapped parameter row
                const TouchInput::Point p = _touch.getTouchPoint();
                for (int i = 0; i < 4; ++i) {
                    if (hitTestRow(i, p.x, p.y)) {
                        _selectedParam = i;
                        markFullRedraw();
                        break;
                    }
                }

            } else if (gesture == TouchInput::GESTURE_SWIPE_LEFT ||
                       gesture == TouchInput::GESTURE_SWIPE_RIGHT) {
                // Swipe left/right -> coarse ±10 adjustment of selected param
                const uint8_t cc = UIPage::ccMap[_currentPage][_selectedParam];
                if (cc != 255) {
                    const int delta    = (gesture == TouchInput::GESTURE_SWIPE_RIGHT) ? 10 : -10;
                    const int newValue = constrain((int)synth.getCC(cc) + delta, 0, 127);
                    synth.setCC(cc, (uint8_t)newValue);
                    markRowDirty(_selectedParam);
                }

            } else if (gesture == TouchInput::GESTURE_HOLD) {
                // Long hold -> enter oscilloscope view
                setMode(MODE_SCOPE);
            }
            break;

        case MODE_SCOPE:
            if (gesture == TouchInput::GESTURE_TAP) setMode(MODE_PARAMETERS);
            break;

        case MODE_MENU:
            if (gesture == TouchInput::GESTURE_TAP) setMode(MODE_PARAMETERS);
            break;

        case MODE_COUNT:
            break;
    }
}

bool UIManager_MicroDexed::hitTestRow(int row, int16_t x, int16_t y) const {
    const int rowY = HEADER_HEIGHT + SCREEN_MARGIN + row * PARAM_ROW_HEIGHT;
    const int rowH = PARAM_ROW_HEIGHT - 2;
    return (x >= SCREEN_MARGIN           && x < SCREEN_WIDTH - SCREEN_MARGIN &&
            y >= rowY                    && y < rowY + rowH);
}


// =============================================================================
// TEXT DRAWING HELPERS
// =============================================================================

// Draw text centred horizontally around cx, at vertical position y.
void UIManager_MicroDexed::drawTextCentred(int cx, int y, const char* text,
                                           uint16_t colour, int fontSize) {
    if (!text) return;
    _display.setTextColor(colour);
    _display.setTextSize(fontSize);
    // Built-in font: 6x8 px per character at size 1; scales linearly with fontSize
    const int textWidth = (int)strlen(text) * 6 * fontSize;
    _display.setCursor(cx - textWidth / 2, y);
    _display.print(text);
}

// Draw text right-aligned: rightmost pixel edge at rx.
void UIManager_MicroDexed::drawTextRight(int rx, int y, const char* text,
                                         uint16_t colour, int fontSize) {
    if (!text) return;
    _display.setTextColor(colour);
    _display.setTextSize(fontSize);
    const int textWidth = (int)strlen(text) * 6 * fontSize;
    _display.setCursor(rx - textWidth, y);
    _display.print(text);
}


// =============================================================================
// COLOUR CODING
// Returns the category colour for a parameter's value bar based on CC family.
// =============================================================================

uint16_t UIManager_MicroDexed::paramColour(uint8_t cc) const {

    // Oscillator family
    if (cc == CC::OSC1_WAVE            || cc == CC::OSC2_WAVE           ||
        cc == CC::OSC1_PITCH_OFFSET    || cc == CC::OSC2_PITCH_OFFSET   ||
        cc == CC::OSC1_DETUNE          || cc == CC::OSC2_DETUNE         ||
        cc == CC::OSC1_FINE_TUNE       || cc == CC::OSC2_FINE_TUNE      ||
        cc == CC::OSC_MIX_BALANCE      || cc == CC::OSC1_MIX            ||
        cc == CC::OSC2_MIX             || cc == CC::SUB_MIX             ||
        cc == CC::NOISE_MIX            || cc == CC::RING1_MIX           ||
        cc == CC::RING2_MIX            ||
        cc == CC::SUPERSAW1_DETUNE     || cc == CC::SUPERSAW1_MIX       ||
        cc == CC::SUPERSAW2_DETUNE     || cc == CC::SUPERSAW2_MIX       ||
        cc == CC::OSC1_FREQ_DC         || cc == CC::OSC1_SHAPE_DC       ||
        cc == CC::OSC2_FREQ_DC         || cc == CC::OSC2_SHAPE_DC       ||
        cc == CC::OSC1_FEEDBACK_AMOUNT || cc == CC::OSC1_FEEDBACK_MIX   ||
        cc == CC::OSC2_FEEDBACK_AMOUNT || cc == CC::OSC2_FEEDBACK_MIX   ||
        cc == CC::OSC1_ARB_BANK        || cc == CC::OSC1_ARB_INDEX      ||
        cc == CC::OSC2_ARB_BANK        || cc == CC::OSC2_ARB_INDEX) {
        return COLOUR_OSC;
    }

    // Filter family
    if (cc == CC::FILTER_CUTOFF                || cc == CC::FILTER_RESONANCE            ||
        cc == CC::FILTER_ENV_AMOUNT            || cc == CC::FILTER_KEY_TRACK            ||
        cc == CC::FILTER_OCTAVE_CONTROL        || cc == CC::FILTER_OBXA_MULTIMODE       ||
        cc == CC::FILTER_OBXA_TWO_POLE         || cc == CC::FILTER_OBXA_XPANDER_4_POLE  ||
        cc == CC::FILTER_OBXA_XPANDER_MODE     || cc == CC::FILTER_OBXA_BP_BLEND_2_POLE ||
        cc == CC::FILTER_OBXA_PUSH_2_POLE      || cc == CC::FILTER_OBXA_RES_MOD_DEPTH) {
        return COLOUR_FILTER;
    }

    // Envelope family
    if (cc == CC::AMP_ATTACK          || cc == CC::AMP_DECAY           ||
        cc == CC::AMP_SUSTAIN         || cc == CC::AMP_RELEASE         ||
        cc == CC::FILTER_ENV_ATTACK   || cc == CC::FILTER_ENV_DECAY    ||
        cc == CC::FILTER_ENV_SUSTAIN  || cc == CC::FILTER_ENV_RELEASE) {
        return COLOUR_ENV;
    }

    // LFO family
    if (cc == CC::LFO1_FREQ           || cc == CC::LFO1_DEPTH          ||
        cc == CC::LFO1_DESTINATION    || cc == CC::LFO1_WAVEFORM       ||
        cc == CC::LFO2_FREQ           || cc == CC::LFO2_DEPTH          ||
        cc == CC::LFO2_DESTINATION    || cc == CC::LFO2_WAVEFORM       ||
        cc == CC::LFO1_TIMING_MODE    || cc == CC::LFO2_TIMING_MODE) {
        return COLOUR_LFO;
    }

    // FX family
    if (cc == CC::FX_REVERB_SIZE          || cc == CC::FX_REVERB_DAMP         ||
        cc == CC::FX_REVERB_LODAMP        || cc == CC::FX_REVERB_MIX          ||
        cc == CC::FX_REVERB_BYPASS        || cc == CC::FX_DRY_MIX             ||
        cc == CC::FX_JPFX_MIX             || cc == CC::FX_BASS_GAIN           ||
        cc == CC::FX_TREBLE_GAIN          || cc == CC::FX_MOD_EFFECT          ||
        cc == CC::FX_MOD_MIX              || cc == CC::FX_MOD_RATE            ||
        cc == CC::FX_MOD_FEEDBACK         || cc == CC::FX_JPFX_DELAY_EFFECT   ||
        cc == CC::FX_JPFX_DELAY_MIX      || cc == CC::FX_JPFX_DELAY_FEEDBACK ||
        cc == CC::FX_JPFX_DELAY_TIME     || cc == CC::DELAY_TIMING_MODE) {
        return COLOUR_FX;
    }

    // Global / performance — fallback
    return COLOUR_GLOBAL;
}
