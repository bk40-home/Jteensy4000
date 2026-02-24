/**
 * UIManager_MicroDexed.cpp
 *
 * UI implementation for MicroDexed hardware: ILI9341 320×240 TFT display,
 * two encoders, optional capacitive touch.
 *
 * Key fixes vs. previous version:
 *   1. synth.getCC() / synth.setCC() — now exist via _ccState cache in SynthEngine.
 *   2. UIPageEnhanced::ccMap → UIPage::ccMap (Enhanced layout is a future stub).
 *   3. getParamColor() — all CC enum names corrected to match CCDefs.h.
 *   4. yStart calculated once and used throughout drawParameterGrid().
 *   5. drawHeader() drives page name from UIPage::ccNames (scalable to all pages).
 *   6. MODE_COUNT added to every switch as an explicit no-op.
 *
 * Performance notes:
 *   - Dirty region tracking avoids full redraws on every frame.
 *   - Frame rate capped at ~30 FPS (FRAME_INTERVAL_MS = 33) — sufficient for UI.
 *   - snprintf() used instead of String to avoid heap fragmentation.
 */

// UIManager_MicroDexed.h must be first — defines the class, pulls in Arduino.h,
// ILI9341_t3n.h, SPI, SynthEngine etc. Everything below depends on those types.
#include "UIManager_MicroDexed.h"
#include "UIPageLayout.h"   // UIPage::ccMap, UIPage::ccNames, UIPage::NUM_PAGES
#include "CCDefs.h"

// ---------------------------------------------------------------------------
// Stubs required by ILI9341_t3n.cpp when built outside the full MicroDexed project.
//
// remote_active — bool that suppresses display drawing during remote MIDI streaming.
//   Always false here; the display is always local.
//
// ColorHSV — declared extern in ILI9341_t3n.cpp; only called by fillRectRainbow()
//   which we never invoke. Minimal HSV→RGB565 implementation satisfies the linker.
//
// IMPORTANT: these must appear AFTER all #includes so that Arduino.h has already
// defined uint8_t / uint16_t and the class declaration is visible.
// ---------------------------------------------------------------------------

bool remote_active = false;

uint16_t ColorHSV(uint16_t hue, uint8_t sat, uint8_t val) {
    uint8_t r, g, b;
    if (sat == 0) {
        r = g = b = val;
    } else {
        const uint8_t  region = (uint8_t)(hue / 11000u);
        const uint16_t rem    = (uint16_t)((hue - region * 11000u) * 6u);
        const uint8_t  p = (uint8_t)((val * (255u - sat)) >> 8);
        const uint8_t  q = (uint8_t)((val * (255u - ((sat * (rem >> 8)) >> 8))) >> 8);
        const uint8_t  t = (uint8_t)((val * (255u - ((sat * (255u - (rem >> 8))) >> 8))) >> 8);
        switch (region % 6u) {
            case 0: r=val; g=t;   b=p;   break;
            case 1: r=q;   g=val; b=p;   break;
            case 2: r=p;   g=val; b=t;   break;
            case 3: r=p;   g=q;   b=val; break;
            case 4: r=t;   g=p;   b=val; break;
            default:r=val; g=p;   b=q;   break;
        }
    }
    return (uint16_t)(((r & 0xF8u) << 8) | ((g & 0xFCu) << 3) | (b >> 3));
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
UIManager_MicroDexed::UIManager_MicroDexed()
    // ILI9341_t3n requires all 6 SPI pins so it can configure SPI1 at runtime.
    // (The 3-pin form would silently use SPI0 — display stays blank.)
    : _display(TFT_CS, TFT_DC, TFT_RST, TFT_MOSI, TFT_SCK, TFT_MISO)
    , _touchEnabled(false)
    , _currentPage(0)
    , _selectedParam(0)
    , _currentMode(MODE_PARAMETERS)
    , _fullRedraw(true)
    , _headerDirty(true)
    , _footerDirty(true)
    , _lastFrameTime(0)
    , _lastEncoderLeft(0)
    , _lastEncoderRight(0)
{
    // Zero all per-row dirty flags
    memset(_paramsDirty, 0, sizeof(_paramsDirty));
}

// ---------------------------------------------------------------------------
// begin() — hardware init
// ---------------------------------------------------------------------------
void UIManager_MicroDexed::begin() {
    // SPI.begin() must be called before display.begin() — ILI9341_t3n calls
    // _pspi->begin() internally but the outer SPI object needs initialising first.
    // MicroDexed does this at line 1298 of MicroDexed-touch.ino.
    SPI.begin();

    // ILI9341_t3n::begin() selects SPI1, sets MOSI/SCK/MISO pins,
    // pulses RST (pin 24), then sends the full init command sequence.
    _display.begin();

    // Rotation 3 = MicroDexed default landscape (matches DISPLAY_ROTATION_DEFAULT in config.h)
    // Rotation 1 = landscape but flipped 180° — was wrong for this hardware
    _display.setRotation(3);

    Serial.println("[UI] Display begin() called — expecting red flash");
    _display.fillScreen(COLOR_BACKGROUND);
    delay(400);
    _display.fillScreen(COLOR_BACKGROUND);
    Serial.println("[UI] Display init OK");

    _display.setTextColor(COLOR_TEXT);
    _display.setTextSize(FONT_MEDIUM);

    // Capacitive touch is optional — begin() returns false if controller not found
    _touchEnabled = _touch.begin();
    Serial.println(_touchEnabled ? "[UI] Touch: enabled" : "[UI] Touch: not found (I2C check wiring)");

    // Boot splash
    drawCenteredText(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2,
                     "JTeensy 4000", COLOR_ACCENT, FONT_LARGE);
    drawCenteredText(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 30,
                     "MicroDexed Edition", COLOR_TEXT, FONT_SMALL);
    delay(1000);

    // Force full repaint on first updateDisplay() call
    _fullRedraw  = true;
    _headerDirty = true;
    _footerDirty = true;
    for (int i = 0; i < 8; ++i) _paramsDirty[i] = true;
    Serial.println("[UI] begin() complete — ready");
}

// ---------------------------------------------------------------------------
// updateDisplay() — called at ~30 FPS from main loop
// ---------------------------------------------------------------------------
void UIManager_MicroDexed::updateDisplay(SynthEngine& synth) {
    // Frame rate limiter — updateDisplay() owns the gate; remove the external
    // millis() gate in loop() to avoid double-gating at mismatched intervals.
    const uint32_t now = millis();
    if ((now - _lastFrameTime) < FRAME_INTERVAL_MS && !_fullRedraw) {
        return;
    }
    _lastFrameTime = now;

    switch (_currentMode) {
        case MODE_PARAMETERS:
            if (_fullRedraw) {
                // Full repaint
                _display.fillScreen(COLOR_BACKGROUND);
                drawHeader(synth);
                drawParameterGrid(synth);
                drawFooter();
            } else {
                // Selective repaint — only dirty regions
                if (_headerDirty) drawHeader(synth);
                if (_footerDirty) drawFooter();

                // Cap at PARAMS_PER_PAGE — the 8-slot dirty array is for the future
                // Enhanced layout. For now only 4 rows exist per page.
                for (int i = 0; i < UIPage::PARAMS_PER_PAGE; ++i) {
                    if (_paramsDirty[i]) {
                        const byte cc       = UIPage::ccMap[_currentPage][i];
                        const int  value    = (cc != 255) ? synth.getCC(cc) : 0;
                        const bool selected = (i == _selectedParam);
                        drawParameterRow(i, cc, value, selected);
                    }
                }
            }
            break;

        case MODE_SCOPE:
            drawScopeView(synth);
            break;

        case MODE_MENU:
            drawMenuView();
            break;

        case MODE_COUNT:
            break;
    }

    clearDirtyFlags();
}

// ---------------------------------------------------------------------------
// pollInputs() — called at 100+ Hz from main loop for responsive input
// ---------------------------------------------------------------------------
void UIManager_MicroDexed::pollInputs(HardwareInterface_MicroDexed& hw,
                                      SynthEngine& synth) {
    // Touch — update state before encoder reads
    if (_touchEnabled) {
        _touch.update();
        handleTouchInput(synth);
    }

    const int deltaLeft  = hw.getEncoderDelta(HardwareInterface_MicroDexed::ENC_LEFT);
    const int deltaRight = hw.getEncoderDelta(HardwareInterface_MicroDexed::ENC_RIGHT);

    const auto pressLeft  = hw.getButtonPress(HardwareInterface_MicroDexed::ENC_LEFT);
    const auto pressRight = hw.getButtonPress(HardwareInterface_MicroDexed::ENC_RIGHT);

    // DEBUG: Print any encoder/button activity so we can confirm the HW layer delivers data.
    // Remove these prints once confirmed working (they add ~10 µs Serial overhead per call).
    if (deltaLeft  != 0) Serial.printf("[UI] Enc LEFT  delta=%d  page=%d param=%d\n",  deltaLeft,  _currentPage, _selectedParam);
    if (deltaRight != 0) Serial.printf("[UI] Enc RIGHT delta=%d  page=%d param=%d  CC=%d\n", deltaRight, _currentPage, _selectedParam,
                                       (int)UIPage::ccMap[_currentPage][_selectedParam % UIPage::PARAMS_PER_PAGE]);
    if (pressLeft  != HardwareInterface_MicroDexed::PRESS_NONE) Serial.printf("[UI] Btn LEFT  press=%d\n",  (int)pressLeft);
    if (pressRight != HardwareInterface_MicroDexed::PRESS_NONE) Serial.printf("[UI] Btn RIGHT press=%d\n", (int)pressRight);

    switch (_currentMode) {
        case MODE_PARAMETERS:
            // Left encoder — navigate between parameters on this page
            if (deltaLeft != 0) {
                // Wrap within the actual number of params on this page
                _selectedParam = (_selectedParam + deltaLeft + UIPage::PARAMS_PER_PAGE)
                                  % UIPage::PARAMS_PER_PAGE;
                markFullDirty();
            }

            // Right encoder — adjust value of selected parameter
            if (deltaRight != 0) {
                const byte cc = UIPage::ccMap[_currentPage][_selectedParam % UIPage::PARAMS_PER_PAGE];
                if (cc != 255) {
                    const int newValue = constrain((int)synth.getCC(cc) + deltaRight, 0, 127);
                    Serial.printf("[UI] setCC %d → %d\n", (int)cc, newValue);
                    synth.setCC(cc, (uint8_t)newValue);
                    markParamDirty(_selectedParam);
                } else {
                    Serial.println("[UI] Enc RIGHT: slot is empty (CC=255) — no CC sent");
                }
            }

            // Left button SHORT — advance to next page
            if (pressLeft == HardwareInterface_MicroDexed::PRESS_SHORT) {
                _currentPage = (_currentPage + 1) % UIPage::NUM_PAGES;
                _selectedParam = 0;
                markFullDirty();
                Serial.printf("[UI] Page → %d\n", _currentPage);
            }

            // Left button LONG — toggle oscilloscope view
            if (pressLeft == HardwareInterface_MicroDexed::PRESS_LONG) {
                setMode(MODE_SCOPE);
            }

            // Right button LONG — open menu
            if (pressRight == HardwareInterface_MicroDexed::PRESS_LONG) {
                setMode(MODE_MENU);
            }
            break;

        case MODE_SCOPE:
            // Any button press returns to parameter view
            if (pressLeft  != HardwareInterface_MicroDexed::PRESS_NONE ||
                pressRight != HardwareInterface_MicroDexed::PRESS_NONE) {
                setMode(MODE_PARAMETERS);
            }
            break;

        case MODE_MENU:
            if (pressLeft != HardwareInterface_MicroDexed::PRESS_NONE) {
                setMode(MODE_PARAMETERS);
            }
            break;

        case MODE_COUNT:
            break;
    }
}

// ---------------------------------------------------------------------------
// Page / parameter management
// ---------------------------------------------------------------------------

void UIManager_MicroDexed::setPage(int page) {
    if (page == _currentPage) return;
    _currentPage   = constrain(page, 0, UIPage::NUM_PAGES - 1);
    _selectedParam = 0;
    markFullDirty();
}

void UIManager_MicroDexed::selectParameter(int index) {
    if (index == _selectedParam) return;
    _selectedParam = constrain(index, 0, 7);
    markFullDirty();
}

void UIManager_MicroDexed::setMode(DisplayMode mode) {
    if (mode == _currentMode) return;
    _currentMode = mode;
    markFullDirty();
}

void UIManager_MicroDexed::syncFromEngine(SynthEngine& /*synth*/) {
    // Invalidate the whole screen — next updateDisplay() repaints everything
    markFullDirty();
}

// ===========================================================================
// Drawing — Header
// ===========================================================================

void UIManager_MicroDexed::drawHeader(SynthEngine& /*synth*/) {
    _display.fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, COLOR_HEADER);

    // Page number + first param name as the page title
    // Falls back to UIPage::ccNames[page][0] which is always a short label
    char pageTitle[24];
    const char* firstName = UIPage::ccNames[_currentPage][0];
    snprintf(pageTitle, sizeof(pageTitle), "P%d: %s",
             _currentPage + 1, firstName ? firstName : "---");

    _display.setCursor(MARGIN, MARGIN);
    _display.setTextColor(COLOR_TEXT);
    _display.setTextSize(FONT_MEDIUM);
    _display.print(pageTitle);

    // CPU usage — top right corner
    char cpuStr[12];
    snprintf(cpuStr, sizeof(cpuStr), "CPU:%d%%", (int)AudioProcessorUsageMax());
    drawRightAlignedText(SCREEN_WIDTH - MARGIN, MARGIN, cpuStr, COLOR_TEXT_DIM, FONT_SMALL);

    // Separator line
    _display.drawFastHLine(0, HEADER_HEIGHT - 1, SCREEN_WIDTH, COLOR_BORDER);
}

// ===========================================================================
// Drawing — Parameter grid
// ===========================================================================

void UIManager_MicroDexed::drawParameterGrid(SynthEngine& synth) {
    // UIPage has PARAMS_PER_PAGE (4) slots per page.
    // Previously this looped to 8 and used (i % PARAMS_PER_PAGE) which silently
    // drew the same 4 parameters twice, and with PARAM_ROW_HEIGHT=35 the rows
    // at i=4..7 started at y = 30+5+(4*35) = 175 which runs off the 240px screen
    // into/over the footer. Cap the loop at PARAMS_PER_PAGE.
    // When the Enhanced 8-param layout is adopted, increase PARAMS_PER_PAGE to 8.
    for (int i = 0; i < UIPage::PARAMS_PER_PAGE; ++i) {
        const byte cc       = UIPage::ccMap[_currentPage][i];
        const int  value    = (cc != 255) ? synth.getCC(cc) : 0;
        const bool selected = (i == _selectedParam);
        drawParameterRow(i, cc, value, selected);
    }
}

void UIManager_MicroDexed::drawParameterRow(int row, byte cc,
                                             int value, bool selected) {
    // Layout constants — keep in sync with HEADER_HEIGHT, PARAM_ROW_HEIGHT, MARGIN
    const int yStart = HEADER_HEIGHT + MARGIN;
    const int y      = yStart + row * PARAM_ROW_HEIGHT;
    const int x      = MARGIN;
    const int width  = SCREEN_WIDTH - 2 * MARGIN;
    const int height = PARAM_ROW_HEIGHT - 2;  // Small gap between rows

    const uint16_t bgColor   = selected ? COLOR_HIGHLIGHT  : COLOR_BACKGROUND;
    const uint16_t textColor = selected ? COLOR_BACKGROUND : COLOR_TEXT;

    _display.fillRect(x, y, width, height, bgColor);

    // Parameter name (left-aligned)
    const char* name = (cc != 255) ? getParamName(cc) : "---";
    _display.setCursor(x + MARGIN, y + 5);
    _display.setTextColor(textColor);
    _display.setTextSize(FONT_MEDIUM);
    _display.print(name);

    if (cc == 255) return;  // Skip value/bar for unused slots

    // Value (right-aligned)
    char valueStr[8];
    snprintf(valueStr, sizeof(valueStr), "%d", value);
    drawRightAlignedText(x + width - MARGIN, y + 5, valueStr, textColor, FONT_MEDIUM);

    // Value bar — proportional to 0-127
    const int barWidth = (width - 4 * MARGIN) * value / 127;
    const int barY     = y + height - 8;
    _display.fillRect(x + MARGIN, barY, barWidth, 4, getParamColor(cc));
}

// ===========================================================================
// Drawing — Footer
// ===========================================================================

void UIManager_MicroDexed::drawFooter() {
    const int y = SCREEN_HEIGHT - FOOTER_HEIGHT;
    _display.fillRect(0, y, SCREEN_WIDTH, FOOTER_HEIGHT, COLOR_HEADER);
    _display.drawFastHLine(0, y, SCREEN_WIDTH, COLOR_BORDER);

    _display.setCursor(MARGIN, y + 5);
    _display.setTextColor(COLOR_TEXT_DIM);
    _display.setTextSize(FONT_SMALL);
    _display.print("L:Page  R:Value  Hold-L:Scope  Hold-R:Menu");
}

// ===========================================================================
// Drawing — Scope view
// ===========================================================================
// NOTE: drawScopeView() is implemented in ScopeView.cpp.
//       Only the declaration lives in UIManager_MicroDexed.h.
//       Do NOT define it here — duplicate definition causes a linker error.

void UIManager_MicroDexed::drawMenuView() {
    _display.fillScreen(COLOR_BACKGROUND);
    drawCenteredText(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2,
                     "MENU", COLOR_ACCENT, FONT_LARGE);
    drawCenteredText(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 30,
                     "(Not yet implemented)", COLOR_TEXT_DIM, FONT_SMALL);
}

// ===========================================================================
// Helper — parameter colour coding by CC category
// ===========================================================================

uint16_t UIManager_MicroDexed::getParamColor(byte cc) {
    // Oscillator parameters
    if (cc == CC::OSC1_WAVE        || cc == CC::OSC2_WAVE        ||
        cc == CC::OSC1_PITCH_OFFSET|| cc == CC::OSC2_PITCH_OFFSET||
        cc == CC::OSC1_DETUNE      || cc == CC::OSC2_DETUNE      ||
        cc == CC::OSC1_FINE_TUNE   || cc == CC::OSC2_FINE_TUNE   ||
        cc == CC::OSC_MIX_BALANCE  || cc == CC::OSC1_MIX         ||
        cc == CC::OSC2_MIX         || cc == CC::SUB_MIX          ||
        cc == CC::NOISE_MIX        || cc == CC::RING1_MIX        ||
        cc == CC::RING2_MIX        ||
        cc == CC::SUPERSAW1_DETUNE || cc == CC::SUPERSAW1_MIX    ||
        cc == CC::SUPERSAW2_DETUNE || cc == CC::SUPERSAW2_MIX    ||
        cc == CC::OSC1_FREQ_DC     || cc == CC::OSC1_SHAPE_DC    ||
        cc == CC::OSC2_FREQ_DC     || cc == CC::OSC2_SHAPE_DC    ||
        cc == CC::OSC1_FEEDBACK_AMOUNT || cc == CC::OSC1_FEEDBACK_MIX ||
        cc == CC::OSC2_FEEDBACK_AMOUNT || cc == CC::OSC2_FEEDBACK_MIX) {
        return COLOR_OSC;
    }

    // Filter parameters
    if (cc == CC::FILTER_CUTOFF          || cc == CC::FILTER_RESONANCE       ||
        cc == CC::FILTER_ENV_AMOUNT      || cc == CC::FILTER_KEY_TRACK       ||
        cc == CC::FILTER_OCTAVE_CONTROL  ||
        cc == CC::FILTER_OBXA_MULTIMODE  || cc == CC::FILTER_OBXA_TWO_POLE   ||
        cc == CC::FILTER_OBXA_XPANDER_4_POLE || cc == CC::FILTER_OBXA_XPANDER_MODE ||
        cc == CC::FILTER_OBXA_BP_BLEND_2_POLE || cc == CC::FILTER_OBXA_PUSH_2_POLE ||
        cc == CC::FILTER_OBXA_RES_MOD_DEPTH) {
        return COLOR_FILTER;
    }

    // Envelope parameters (amp + filter)
    if (cc == CC::AMP_ATTACK       || cc == CC::AMP_DECAY          ||
        cc == CC::AMP_SUSTAIN      || cc == CC::AMP_RELEASE        ||
        cc == CC::FILTER_ENV_ATTACK|| cc == CC::FILTER_ENV_DECAY   ||
        cc == CC::FILTER_ENV_SUSTAIN || cc == CC::FILTER_ENV_RELEASE) {
        return COLOR_ENV;
    }

    // FX parameters
    if (cc == CC::FX_REVERB_SIZE      || cc == CC::FX_REVERB_DAMP      ||
        cc == CC::FX_REVERB_LODAMP    || cc == CC::FX_REVERB_MIX       ||
        cc == CC::FX_REVERB_BYPASS    || cc == CC::FX_DRY_MIX          ||
        cc == CC::FX_JPFX_MIX        ||
        cc == CC::FX_BASS_GAIN        || cc == CC::FX_TREBLE_GAIN      ||
        cc == CC::FX_MOD_EFFECT       || cc == CC::FX_MOD_MIX          ||
        cc == CC::FX_MOD_RATE         || cc == CC::FX_MOD_FEEDBACK     ||
        cc == CC::FX_JPFX_DELAY_EFFECT|| cc == CC::FX_JPFX_DELAY_MIX  ||
        cc == CC::FX_JPFX_DELAY_FEEDBACK || cc == CC::FX_JPFX_DELAY_TIME) {
        return COLOR_FX;
    }

    return COLOR_TEXT;  // Default — white for unclassified CCs
}

// ---------------------------------------------------------------------------
// Helper — CC number → human-readable name
// ---------------------------------------------------------------------------

const char* UIManager_MicroDexed::getParamName(byte cc) {
    // CC::name() is an inline switch in CCDefs.h covering all known CCs
    const char* n = CC::name(cc);
    return n ? n : "?";
}

// ---------------------------------------------------------------------------
// Helpers — text positioning
// ---------------------------------------------------------------------------

void UIManager_MicroDexed::drawCenteredText(int x, int y, const char* text,
                                             uint16_t color, int fontSize) {
    _display.setTextColor(color);
    _display.setTextSize(fontSize);
    // ILI9341 built-in font: 6×8 pixels per char at size 1
    const int textWidth = (int)strlen(text) * 6 * fontSize;
    _display.setCursor(x - textWidth / 2, y);
    _display.print(text);
}

void UIManager_MicroDexed::drawRightAlignedText(int x, int y, const char* text,
                                                 uint16_t color, int fontSize) {
    _display.setTextColor(color);
    _display.setTextSize(fontSize);
    const int textWidth = (int)strlen(text) * 6 * fontSize;
    _display.setCursor(x - textWidth, y);
    _display.print(text);
}

// ===========================================================================
// Touch input handling
// ===========================================================================

void UIManager_MicroDexed::handleTouchInput(SynthEngine& synth) {
    const TouchInput::Gesture gesture = _touch.getGesture();

    switch (_currentMode) {
        case MODE_PARAMETERS:
            // Swipe up → previous page
            if (gesture == TouchInput::GESTURE_SWIPE_UP) {
                _currentPage = (_currentPage - 1 + UIPage::NUM_PAGES) % UIPage::NUM_PAGES;
                markFullDirty();
            }
            // Swipe down → next page
            else if (gesture == TouchInput::GESTURE_SWIPE_DOWN) {
                _currentPage = (_currentPage + 1) % UIPage::NUM_PAGES;
                markFullDirty();
            }
            // Tap → select parameter by touch position
            else if (gesture == TouchInput::GESTURE_TAP) {
                const TouchInput::Point p = _touch.getTouchPoint();
                for (int i = 0; i < 8; ++i) {
                    if (hitTestParameter(i, p.x, p.y)) {
                        _selectedParam = i;
                        markFullDirty();
                        break;
                    }
                }
            }
            // Swipe left/right → adjust selected parameter by ±10
            else if (gesture == TouchInput::GESTURE_SWIPE_LEFT ||
                     gesture == TouchInput::GESTURE_SWIPE_RIGHT) {
                // Use UIPage::ccMap — consistent with encoder handler
                const byte cc = UIPage::ccMap[_currentPage]
                                             [_selectedParam % UIPage::PARAMS_PER_PAGE];
                if (cc != 255) {
                    const int delta    = (gesture == TouchInput::GESTURE_SWIPE_RIGHT) ? 10 : -10;
                    const int newValue = constrain((int)synth.getCC(cc) + delta, 0, 127);
                    synth.setCC(cc, (uint8_t)newValue);
                    markParamDirty(_selectedParam);
                }
            }
            // Hold → scope view
            else if (gesture == TouchInput::GESTURE_HOLD) {
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

bool UIManager_MicroDexed::hitTestParameter(int paramIndex,
                                             int16_t x, int16_t y) {
    // Calculate row bounding box using the same constants as drawParameterRow
    const int rowY  = HEADER_HEIGHT + MARGIN + paramIndex * PARAM_ROW_HEIGHT;
    const int rowH  = PARAM_ROW_HEIGHT - 2;
    const int rowX  = MARGIN;
    const int rowW  = SCREEN_WIDTH - 2 * MARGIN;

    return (x >= rowX && x < rowX + rowW &&
            y >= rowY && y < rowY + rowH);
}