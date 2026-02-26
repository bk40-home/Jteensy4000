// TFTWidget.h
// =============================================================================
// Base class for all JT-4000 TFT UI widgets.
//
// Design philosophy:
//   - Every widget owns a bounding rect (x, y, w, h) on the 320×240 display.
//   - A "dirty" flag means "my pixels need repainting this frame".
//     Set _dirty = true on state change; clear it after drawing.
//   - draw() paints only if dirty AND the display pointer is set.
//   - onTouch(x, y) returns true if the point lands inside this widget.
//   - Subclasses override draw() and optionally onTouch().
//
// Thread safety: not thread-safe. All calls must come from the main loop.
//
// Performance rules (follow these in every subclass):
//   - Never call fillScreen() from a widget — clear only the widget's own rect.
//   - Never do heap allocation (no new/delete, no Arduino String).
//   - Use snprintf() into stack buffers for text; never sprintf().
//   - Only draw when _dirty is true.
// =============================================================================

#pragma once
#include <Arduino.h>
#include "ILI9341_t3n.h"


// =============================================================================
// TFTTheme — centralised colour and font constants.
//
// All widgets read from this struct so you can swap theme colours in one place.
// Colours are RGB565 — same format used by ILI9341_t3n.
// =============================================================================
struct TFTTheme {
    // --- Backgrounds
    uint16_t bg             = 0x0000;   // screen / widget background  (black)
    uint16_t headerBg       = 0x4208;   // header / footer bar         (dark grey)
    uint16_t selectedBg     = 0x07FF;   // selected-row highlight       (cyan)

    // --- Text
    uint16_t textNormal     = 0xFFFF;   // standard text               (white)
    uint16_t textDim        = 0x7BEF;   // secondary / hint text       (mid-grey)
    uint16_t textOnSelect   = 0x0000;   // text drawn on selected bg   (black)

    // --- Borders
    uint16_t border         = 0x2104;   // separator lines             (dark grey)

    // --- Widget chrome
    uint16_t radioBorder    = 0xC618;   // radio button outline        (light grey)
    uint16_t radioFill      = 0x07FF;   // radio button selected fill  (cyan)
    uint16_t buttonNormal   = 0x2965;   // un-pressed button           (dark blue-grey)
    uint16_t buttonPress    = 0x07FF;   // pressed button flash        (cyan)
    uint16_t buttonText     = 0xFFFF;   // button label text           (white)
    uint16_t accent         = 0xF800;   // alerts, cancel button       (red)

    // --- Value bar (filled strip)
    uint16_t barTrack       = 0x2104;   // empty track behind bar      (dark grey)
    // active bar colour comes from the parameter category (see UIManager_MicroDexed)

    // --- Keypad
    uint16_t keyBg          = 0x2965;   // numpad key background
    uint16_t keyText        = 0xFFFF;   // numpad key text
    uint16_t keyBorder      = 0x4208;   // numpad key border
    uint16_t keyConfirm     = 0x0320;   // confirm key (green)
    uint16_t keyCancel      = 0xF800;   // cancel key  (red)
    uint16_t keyBackspace   = 0x8410;   // backspace key (dark grey)
    uint16_t entryBg        = 0x0841;   // value entry display box
    uint16_t entryText      = 0xFFFF;   // value entry text
};

// Shared global theme instance — defined in TFTWidget.cpp.
// Widgets reference this rather than carrying their own copies.
extern TFTTheme gTheme;


// =============================================================================
// TFTWidget — abstract base class
// =============================================================================
class TFTWidget {
public:
    // -------------------------------------------------------------------------
    // Constructor: position and size must be set at construction time.
    // Call setDisplay() before using any widget.
    // -------------------------------------------------------------------------
    TFTWidget(int16_t x, int16_t y, int16_t w, int16_t h)
        : _x(x), _y(y), _w(w), _h(h)
        , _dirty(true)          // start dirty so first draw paints the widget
        , _visible(true)
        , _display(nullptr)
    {}

    virtual ~TFTWidget() = default;

    // -------------------------------------------------------------------------
    // Hardware binding — must be called before draw() or touch handling.
    // Typically called once by TFTScreen::setDisplay().
    // -------------------------------------------------------------------------
    void setDisplay(ILI9341_t3n* display) { _display = display; }

    // -------------------------------------------------------------------------
    // draw() — paint this widget onto the display.
    // Only paints when dirty; clears _dirty afterwards.
    // Subclasses implement doDraw() — draw() guards the dirty check.
    // -------------------------------------------------------------------------
    void draw() {
        if (!_dirty || !_visible || !_display) return;
        doDraw();
        _dirty = false;
    }

    // Force a repaint on the next draw() call.
    void markDirty()        { _dirty = true; }

    // Query state
    bool isDirty()   const  { return _dirty;   }
    bool isVisible() const  { return _visible; }
    void setVisible(bool v) { _visible = v; markDirty(); }

    // -------------------------------------------------------------------------
    // onTouch() — called by TFTScreen when a touch lands.
    // Returns true if the widget handled it (consumed the event).
    // Default: returns true on hit-test, false otherwise — subclasses override.
    // -------------------------------------------------------------------------
    virtual bool onTouch(int16_t x, int16_t y) {
        return hitTest(x, y);
    }

    // -------------------------------------------------------------------------
    // onTouchRelease() — called when finger lifts.
    // Used for button flash-back and confirming hold gestures.
    // -------------------------------------------------------------------------
    virtual void onTouchRelease(int16_t /*x*/, int16_t /*y*/) {}

    // -------------------------------------------------------------------------
    // Geometry access
    // -------------------------------------------------------------------------
    int16_t getX() const { return _x; }
    int16_t getY() const { return _y; }
    int16_t getW() const { return _w; }
    int16_t getH() const { return _h; }

    // Returns true if (x, y) is inside this widget's bounding rect.
    bool hitTest(int16_t x, int16_t y) const {
        return (x >= _x && x < _x + _w &&
                y >= _y && y < _y + _h);
    }

protected:
    // Subclass paint implementation — only called when dirty.
    virtual void doDraw() = 0;

    // -------------------------------------------------------------------------
    // Convenience helpers used by all subclasses
    // -------------------------------------------------------------------------

    // Clear the widget's own bounding rect (never the whole screen).
    void clearRect(uint16_t colour) {
        if (_display) _display->fillRect(_x, _y, _w, _h, colour);
    }

    // Draw text centred inside the widget's own rect, at vertical offset dy.
    // Uses the built-in 6×8 font scaled by fontSize.
    void drawTextCentred(const char* text, uint16_t colour,
                         uint8_t fontSize = 1, int16_t dy = 0) {
        if (!text || !_display) return;
        _display->setTextSize(fontSize);
        _display->setTextColor(colour);
        const int16_t textW = (int16_t)(strlen(text) * 6 * fontSize);
        const int16_t cx    = _x + (_w - textW) / 2;
        const int16_t cy    = _y + (_h / 2) - (4 * fontSize) + dy;
        _display->setCursor(cx, cy);
        _display->print(text);
    }

    // Draw text left-aligned at (lx, ly) — absolute screen coordinates.
    void drawTextAt(int16_t lx, int16_t ly, const char* text,
                    uint16_t colour, uint8_t fontSize = 1) {
        if (!text || !_display) return;
        _display->setTextSize(fontSize);
        _display->setTextColor(colour);
        _display->setCursor(lx, ly);
        _display->print(text);
    }

    // Draw text right-aligned so the rightmost pixel is at rx.
    void drawTextRight(int16_t rx, int16_t ly, const char* text,
                       uint16_t colour, uint8_t fontSize = 1) {
        if (!text || !_display) return;
        const int16_t textW = (int16_t)(strlen(text) * 6 * fontSize);
        drawTextAt(rx - textW, ly, text, colour, fontSize);
    }

    // -------------------------------------------------------------------------
    // Member data
    // -------------------------------------------------------------------------
    int16_t      _x, _y, _w, _h;   // bounding rect (screen pixels)
    bool         _dirty;            // true → needs repaint
    bool         _visible;
    ILI9341_t3n* _display;          // borrowed pointer — never freed here
};
