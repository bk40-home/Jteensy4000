/**
 * UIManager_MicroDexed.cpp
 * 
 * Implementation of ILI9341-based UI for MicroDexed hardware.
 * 
 * Performance optimizations:
 * - Dirty region tracking (only redraw changed areas)
 * - Frame rate throttling (30 FPS sufficient)
 * - DMA-accelerated SPI transfers (via ILI9341_t3)
 * - Minimal text redraws (only changed values)
 * 
 * TODO for complete implementation:
 * - Add scope visualization using scopeTap data
 * - Implement touch input for direct parameter access
 * - Add preset selection menu
 * - Add waveform visualization
 * - Add voice activity indicators
 */

#include "UIManager_MicroDexed.h"
#include "UIPageLayout.h"
#include "CCDefs.h"

UIManager_MicroDexed::UIManager_MicroDexed()
    : _display(TFT_CS, TFT_DC, TFT_RST)  // Initialize with pin assignments
    , _currentPage(0)
    , _selectedParam(0)
    , _currentMode(MODE_PARAMETERS)
    , _fullRedraw(true)
    , _lastFrameTime(0)
    , _lastEncoderLeft(0)
    , _lastEncoderRight(0)
{
    // Initialize dirty flags
    _headerDirty = false;
    _footerDirty = false;
    for (int i = 0; i < 8; ++i) {
        _paramsDirty[i] = false;
    }
}

void UIManager_MicroDexed::begin() {
    // --- Initialize ILI9341 display ---
    _display.begin();
    
    // Set rotation (landscape mode)
    // 0 = portrait, 1 = landscape, 2 = portrait flipped, 3 = landscape flipped
    _display.setRotation(1);  // 320x240 landscape
    
    // Clear screen to black
    _display.fillScreen(COLOR_BACKGROUND);
    
    // Set text defaults
    _display.setTextColor(COLOR_TEXT);
    _display.setTextSize(FONT_MEDIUM);
    
    // --- Initialize touch input (optional) ---
    _touchEnabled = _touch.begin();
    if (_touchEnabled) {
        Serial.println("Touch input enabled");
    } else {
        Serial.println("Touch input disabled (not found or disabled in code)");
    }
    
    // Show boot message
    drawCenteredText(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, "JTeensy 4000", COLOR_ACCENT, FONT_LARGE);
    drawCenteredText(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 30, "MicroDexed Edition", COLOR_TEXT, FONT_SMALL);
    delay(1000);  // Show for 1 second
    
    // Force full redraw on first update
    markFullDirty();
}

void UIManager_MicroDexed::updateDisplay(SynthEngine& synth) {
    // --- Frame rate throttling ---
    // Only update display at ~30 FPS to save CPU
    uint32_t now = millis();
    if (now - _lastFrameTime < FRAME_INTERVAL_MS && !_fullRedraw) {
        return;  // Skip this frame
    }
    _lastFrameTime = now;

    // --- Render based on current mode ---
    switch (_currentMode) {
        case MODE_PARAMETERS:
            if (_fullRedraw) {
                _display.fillScreen(COLOR_BACKGROUND);
                drawHeader(synth);
                drawParameterGrid(synth);
                drawFooter();
            } else {
                // Selective updates
                if (_headerDirty) drawHeader(synth);
                if (_footerDirty) drawFooter();
                
                // Redraw only dirty parameter rows
                for (int i = 0; i < 8; ++i) {
                    if (_paramsDirty[i]) {
                        byte cc = UIPage::ccMap[_currentPage][i % 4];
                        int value = synth.getCC(cc);
                        bool selected = (i == _selectedParam);
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
    }

    clearDirtyFlags();
}

void UIManager_MicroDexed::pollInputs(HardwareInterface_MicroDexed& hw, SynthEngine& synth) {
    // --- Update touch input if enabled ---
    if (_touchEnabled) {
        _touch.update();
        handleTouchInput(synth);
    }
    
    // --- Read encoder deltas ---
    int deltaLeft  = hw.getEncoderDelta(HardwareInterface_MicroDexed::ENC_LEFT);
    int deltaRight = hw.getEncoderDelta(HardwareInterface_MicroDexed::ENC_RIGHT);
    
    // --- Read button presses ---
    auto pressLeft  = hw.getButtonPress(HardwareInterface_MicroDexed::ENC_LEFT);
    auto pressRight = hw.getButtonPress(HardwareInterface_MicroDexed::ENC_RIGHT);

    // --- Handle input based on current mode ---
    switch (_currentMode) {
        case MODE_PARAMETERS:
            // Left encoder: Navigate between parameters
            if (deltaLeft != 0) {
                _selectedParam += deltaLeft;
                // Wrap around parameter selection (0-7 visible params)
                if (_selectedParam < 0) _selectedParam = 7;
                if (_selectedParam > 7) _selectedParam = 0;
                markFullDirty();  // Highlight changes
            }

            // Right encoder: Adjust selected parameter value
            if (deltaRight != 0) {
                byte cc = UIPage::ccMap[_currentPage][_selectedParam % 4];
                if (cc != 255) {  // Valid CC mapping
                    int currentValue = synth.getCC(cc);
                    int newValue = constrain(currentValue + deltaRight, 0, 127);
                    synth.setCC(cc, newValue);
                    markParamDirty(_selectedParam);
                }
            }

            // Left button SHORT: Go back / change page
            if (pressLeft == HardwareInterface_MicroDexed::PRESS_SHORT) {
                _currentPage = (_currentPage + 1) % 4;  // Cycle through pages
                markFullDirty();
            }

            // Left button LONG: Toggle scope mode
            if (pressLeft == HardwareInterface_MicroDexed::PRESS_LONG) {
                setMode(MODE_SCOPE);
            }

            // Right button SHORT: Reserved for future use
            if (pressRight == HardwareInterface_MicroDexed::PRESS_SHORT) {
                // TODO: Quick preset select?
            }

            // Right button LONG: Enter menu
            if (pressRight == HardwareInterface_MicroDexed::PRESS_LONG) {
                setMode(MODE_MENU);
            }
            break;

        case MODE_SCOPE:
            // Any button press returns to parameter view
            if (pressLeft != HardwareInterface_MicroDexed::PRESS_NONE ||
                pressRight != HardwareInterface_MicroDexed::PRESS_NONE) {
                setMode(MODE_PARAMETERS);
            }
            break;

        case MODE_MENU:
            // TODO: Implement menu navigation
            // For now, any button returns to parameters
            if (pressLeft != HardwareInterface_MicroDexed::PRESS_NONE) {
                setMode(MODE_PARAMETERS);
            }
            break;
    }
}

void UIManager_MicroDexed::setPage(int page) {
    if (page != _currentPage) {
        _currentPage = constrain(page, 0, 3);  // 4 pages total
        _selectedParam = 0;  // Reset selection
        markFullDirty();
    }
}

void UIManager_MicroDexed::selectParameter(int index) {
    if (index != _selectedParam) {
        _selectedParam = constrain(index, 0, 7);
        markFullDirty();
    }
}

void UIManager_MicroDexed::setMode(DisplayMode mode) {
    if (mode != _currentMode) {
        _currentMode = mode;
        markFullDirty();
    }
}

void UIManager_MicroDexed::syncFromEngine(SynthEngine& synth) {
    // Force full UI refresh from engine state
    markFullDirty();
}

// ============================================================================
// DRAWING FUNCTIONS
// ============================================================================

void UIManager_MicroDexed::drawHeader(SynthEngine& synth) {
    // Clear header area
    _display.fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, COLOR_HEADER);
    
    // Draw page name
    _display.setCursor(MARGIN, MARGIN);
    _display.setTextColor(COLOR_TEXT);
    _display.setTextSize(FONT_MEDIUM);
    
    const char* pageName = "UNKNOWN";
    switch (_currentPage) {
        case 0: pageName = "OSCILLATOR"; break;
        case 1: pageName = "FILTER"; break;
        case 2: pageName = "ENVELOPE"; break;
        case 3: pageName = "EFFECTS"; break;
    }
    _display.print(pageName);
    
    // Draw CPU usage (top right)
    float cpu = AudioProcessorUsageMax();
    char cpuStr[16];
    snprintf(cpuStr, sizeof(cpuStr), "CPU:%d%%", (int)cpu);
    drawRightAlignedText(SCREEN_WIDTH - MARGIN, MARGIN, cpuStr, COLOR_TEXT_DIM, FONT_SMALL);
    
    // Draw separator line
    _display.drawFastHLine(0, HEADER_HEIGHT - 1, SCREEN_WIDTH, COLOR_BORDER);
}

void UIManager_MicroDexed::drawParameterGrid(SynthEngine& synth) {
    // Draw 8 parameter rows (4 params x 2 columns)
    int yStart = HEADER_HEIGHT + MARGIN;
    
    for (int i = 0; i < 8; ++i) {
        byte cc = UIPage::ccMap[_currentPage][i % 4];
        int value = (cc != 255) ? synth.getCC(cc) : 0;
        bool selected = (i == _selectedParam);
        
        drawParameterRow(i, cc, value, selected);
    }
}

void UIManager_MicroDexed::drawParameterRow(int row, byte cc, int value, bool selected) {
    // Calculate position
    int yStart = HEADER_HEIGHT + MARGIN;
    int y = yStart + (row * PARAM_ROW_HEIGHT);
    int x = MARGIN;
    int width = SCREEN_WIDTH - (2 * MARGIN);
    int height = PARAM_ROW_HEIGHT - 2;  // Leave small gap between rows
    
    // Background color (highlight if selected)
    uint16_t bgColor = selected ? COLOR_HIGHLIGHT : COLOR_BACKGROUND;
    uint16_t textColor = selected ? COLOR_BACKGROUND : COLOR_TEXT;
    
    // Clear row area
    _display.fillRect(x, y, width, height, bgColor);
    
    // Draw parameter name
    const char* name = (cc != 255) ? getParamName(cc) : "---";
    _display.setCursor(x + MARGIN, y + 5);
    _display.setTextColor(textColor);
    _display.setTextSize(FONT_MEDIUM);
    _display.print(name);
    
    // Draw value (right-aligned)
    if (cc != 255) {
        char valueStr[8];
        snprintf(valueStr, sizeof(valueStr), "%d", value);
        drawRightAlignedText(x + width - MARGIN, y + 5, valueStr, textColor, FONT_MEDIUM);
        
        // Draw value bar
        int barWidth = (width - (4 * MARGIN)) * value / 127;
        int barY = y + height - 8;
        uint16_t barColor = getParamColor(cc);
        _display.fillRect(x + MARGIN, barY, barWidth, 4, barColor);
    }
}

void UIManager_MicroDexed::drawFooter() {
    int y = SCREEN_HEIGHT - FOOTER_HEIGHT;
    
    // Clear footer
    _display.fillRect(0, y, SCREEN_WIDTH, FOOTER_HEIGHT, COLOR_HEADER);
    
    // Draw separator line
    _display.drawFastHLine(0, y, SCREEN_WIDTH, COLOR_BORDER);
    
    // Draw hints
    _display.setCursor(MARGIN, y + 5);
    _display.setTextColor(COLOR_TEXT_DIM);
    _display.setTextSize(FONT_SMALL);
    _display.print("L:Page R:Value  Hold L:Scope R:Menu");
}

void UIManager_MicroDexed::drawScopeView(SynthEngine& synth) {
    // TODO: Implement oscilloscope view using AudioScopeTap data
    _display.fillScreen(COLOR_BACKGROUND);
    drawCenteredText(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, "SCOPE VIEW", COLOR_ACCENT, FONT_LARGE);
    drawCenteredText(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 30, "(Not yet implemented)", COLOR_TEXT_DIM, FONT_SMALL);
}

void UIManager_MicroDexed::drawMenuView() {
    // TODO: Implement menu view (presets, settings, etc.)
    _display.fillScreen(COLOR_BACKGROUND);
    drawCenteredText(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, "MENU", COLOR_ACCENT, FONT_LARGE);
    drawCenteredText(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 30, "(Not yet implemented)", COLOR_TEXT_DIM, FONT_SMALL);
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

uint16_t UIManager_MicroDexed::getParamColor(byte cc) {
    // Return color based on parameter category
    // This helps with visual organization
    
    // Oscillator CCs (typically 14-20)
    if (cc >= CC::OSC1_LEVEL && cc <= CC::OSC_DETUNE) return COLOR_OSC;
    
    // Filter CCs (typically 74-78)
    if (cc >= CC::FILTER_FREQ && cc <= CC::FILTER_RES) return COLOR_FILTER;
    
    // Envelope CCs (typically 73, 75, etc.)
    if (cc == CC::ENV_ATTACK || cc == CC::ENV_DECAY || 
        cc == CC::ENV_SUSTAIN || cc == CC::ENV_RELEASE) return COLOR_ENV;
    
    // FX CCs (typically 91-93)
    if (cc >= CC::FX_MIX && cc <= CC::FX_SIZE) return COLOR_FX;
    
    // Default
    return COLOR_TEXT;
}

const char* UIManager_MicroDexed::getParamName(byte cc) {
    // Use CC::name() from CCMap.h
    const char* name = CC::name(cc);
    return name ? name : "Unknown";
}

void UIManager_MicroDexed::drawCenteredText(int x, int y, const char* text, uint16_t color, int fontSize) {
    _display.setTextColor(color);
    _display.setTextSize(fontSize);
    
    // Calculate text width (approximate: 6 pixels per char * fontSize)
    int textWidth = strlen(text) * 6 * fontSize;
    int textX = x - (textWidth / 2);
    
    _display.setCursor(textX, y);
    _display.print(text);
}

void UIManager_MicroDexed::drawRightAlignedText(int x, int y, const char* text, uint16_t color, int fontSize) {
    _display.setTextColor(color);
    _display.setTextSize(fontSize);
    
    // Calculate text width (approximate: 6 pixels per char * fontSize)
    int textWidth = strlen(text) * 6 * fontSize;
    int textX = x - textWidth;
    
    _display.setCursor(textX, y);
    _display.print(text);
}

// ============================================================================
// TOUCH INPUT HANDLING
// ============================================================================

void UIManager_MicroDexed::handleTouchInput(SynthEngine& synth) {
    // Get detected gesture
    TouchInput::Gesture gesture = _touch.getGesture();
    
    // Handle gestures based on current mode
    switch (_currentMode) {
        case MODE_PARAMETERS:
            // --- Swipe up/down: Change pages ---
            if (gesture == TouchInput::GESTURE_SWIPE_UP) {
                _currentPage = (_currentPage - 1 + 8) % 8;  // Previous page (wrap)
                markFullDirty();
            }
            else if (gesture == TouchInput::GESTURE_SWIPE_DOWN) {
                _currentPage = (_currentPage + 1) % 8;  // Next page (wrap)
                markFullDirty();
            }
            
            // --- Tap: Select parameter ---
            else if (gesture == TouchInput::GESTURE_TAP) {
                TouchInput::Point p = _touch.getTouchPoint();
                
                // Check which parameter was tapped
                for (int i = 0; i < 8; ++i) {
                    if (hitTestParameter(i, p.x, p.y)) {
                        _selectedParam = i;
                        markFullDirty();
                        break;
                    }
                }
            }
            
            // --- Swipe left/right on parameter: Adjust value ---
            else if (gesture == TouchInput::GESTURE_SWIPE_LEFT || 
                     gesture == TouchInput::GESTURE_SWIPE_RIGHT) {
                byte cc = UIPageEnhanced::ccMap[_currentPage][_selectedParam];
                if (cc != 255) {
                    int currentValue = synth.getCC(cc);
                    int delta = (gesture == TouchInput::GESTURE_SWIPE_RIGHT) ? 10 : -10;
                    int newValue = constrain(currentValue + delta, 0, 127);
                    synth.setCC(cc, newValue);
                    markParamDirty(_selectedParam);
                }
            }
            
            // --- Hold: Enter scope mode ---
            else if (gesture == TouchInput::GESTURE_HOLD) {
                setMode(MODE_SCOPE);
            }
            break;

        case MODE_SCOPE:
            // Any tap returns to parameter view
            if (gesture == TouchInput::GESTURE_TAP) {
                setMode(MODE_PARAMETERS);
            }
            break;

        case MODE_MENU:
            // TODO: Menu touch navigation
            if (gesture == TouchInput::GESTURE_TAP) {
                setMode(MODE_PARAMETERS);
            }
            break;
    }
}

bool UIManager_MicroDexed::hitTestParameter(int paramIndex, int16_t x, int16_t y) {
    // Calculate parameter row bounds
    const int HEADER_H = 30;
    const int PARAM_H = 35;
    const int MARGIN = 5;
    
    int rowY = HEADER_H + MARGIN + (paramIndex * PARAM_H);
    int rowH = PARAM_H - 2;
    int rowX = MARGIN;
    int rowW = SCREEN_WIDTH - (2 * MARGIN);
    
    // Check if touch point is within this row
    return (x >= rowX && x < rowX + rowW &&
            y >= rowY && y < rowY + rowH);
}
