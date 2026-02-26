// TFTButton.h
// =============================================================================
// Simple touchable button for the JT-4000 TFT UI.
//
// Features:
//   - Draws a filled rounded rect with a centred text label.
//   - Flashes to buttonPress colour on onTouch(), restores on onTouchRelease().
//   - Fires a callback on release (not on press) to allow cancel-by-sliding.
//   - Label string is a const char* — never heap-allocated.
//   - Optional "style" enum for confirm / cancel / normal variants.
//
// Usage:
//   TFTButton okBtn(230, 200, 80, 30, "OK", TFTButton::STYLE_CONFIRM);
//   okBtn.setDisplay(&tft);
//   okBtn.setCallback([]{ applyValue(); });
//
//   // In main loop:
//   okBtn.draw();
//
//   // On touch event:
//   okBtn.onTouch(tx, ty);
//
//   // On touch-release event:
//   okBtn.onTouchRelease(tx, ty);
// =============================================================================

#pragma once
#include "TFTWidget.h"

class TFTButton : public TFTWidget {
public:
    // -------------------------------------------------------------------------
    // Style variants — control background colour only.
    // -------------------------------------------------------------------------
    enum Style : uint8_t {
        STYLE_NORMAL  = 0,  // gTheme.buttonNormal
        STYLE_CONFIRM = 1,  // gTheme.keyConfirm  (green)
        STYLE_CANCEL  = 2,  // gTheme.keyCancel   (red)
    };

    using Callback = void (*)();

    // -------------------------------------------------------------------------
    // Constructor
    //   label  — const char* kept by pointer, never copied — must outlive widget
    //   style  — visual variant
    // -------------------------------------------------------------------------
    TFTButton(int16_t x, int16_t y, int16_t w, int16_t h,
              const char* label, Style style = STYLE_NORMAL)
        : TFTWidget(x, y, w, h)
        , _label(label)
        , _style(style)
        , _pressed(false)
        , _callback(nullptr)
    {}

    void setLabel(const char* label) {
        if (label == _label) return;
        _label = label;
        markDirty();
    }

    void setCallback(Callback cb) { _callback = cb; }

    // -------------------------------------------------------------------------
    // onTouch() — visual press state, does NOT fire callback yet.
    // -------------------------------------------------------------------------
    bool onTouch(int16_t x, int16_t y) override {
        if (!hitTest(x, y)) return false;
        if (!_pressed) {
            _pressed = true;
            markDirty();    // trigger repaint to show pressed colour
        }
        return true;
    }

    // -------------------------------------------------------------------------
    // onTouchRelease() — restore normal state; fire callback if released inside.
    // -------------------------------------------------------------------------
    void onTouchRelease(int16_t x, int16_t y) override {
        if (!_pressed) return;
        _pressed = false;
        markDirty();
        if (hitTest(x, y) && _callback) _callback();
    }

    // Allow programmatic press (e.g. encoder button mapped to a screen button)
    void triggerCallback() { if (_callback) _callback(); }

protected:
    void doDraw() override {
        if (!_display) return;

        // Choose background colour based on style and press state
        uint16_t bgCol;
        if (_pressed) {
            bgCol = gTheme.buttonPress;
        } else {
            switch (_style) {
                case STYLE_CONFIRM: bgCol = gTheme.keyConfirm;  break;
                case STYLE_CANCEL:  bgCol = gTheme.accent;      break;
                default:            bgCol = gTheme.buttonNormal; break;
            }
        }

        // Filled rounded rectangle — corner radius 4px
        _display->fillRoundRect(_x, _y, _w, _h, 4, bgCol);
        // Border
        _display->drawRoundRect(_x, _y, _w, _h, 4, gTheme.border);

        // Centred label
        if (_label) {
            const uint16_t textCol = _pressed ? gTheme.bg : gTheme.buttonText;
            drawTextCentred(_label, textCol, /*fontSize=*/1);
        }
    }

private:
    const char* _label;
    Style       _style;
    bool        _pressed;
    Callback    _callback;
};
