// TFTScreen.h
// =============================================================================
// Container for TFTWidget objects — manages layout, dirty redraws, and touch
// routing for one "screen" (page) of the UI.
//
// Key rules:
//   - Maximum MAX_WIDGETS per screen — stack-allocated, no heap.
//   - addWidget() stores a pointer to a widget; widgets must outlive the screen.
//   - draw() iterates all widgets and calls widget->draw() — each widget only
//     repaints if its own dirty flag is set, so CPU cost is proportional to
//     the number of changed widgets, not total widget count.
//   - onTouch() routes a touch point to the first widget that claims it.
//   - setDisplay() propagates the display pointer to all added widgets.
//
// Usage:
//   TFTScreen myScreen;
//   myScreen.setDisplay(&tft);
//   myScreen.addWidget(&myRadioGroup);
//   myScreen.addWidget(&myButton);
//
//   // Main loop:
//   myScreen.draw();
//   if (touch.newTap()) myScreen.onTouch(tp.x, tp.y);
// =============================================================================

#pragma once
#include "TFTWidget.h"

static constexpr int MAX_WIDGETS = 16;   // max widgets per screen

class TFTScreen {
public:
    TFTScreen()
        : _display(nullptr), _numWidgets(0), _bgColour(0x0000)
    {}

    // -------------------------------------------------------------------------
    // setDisplay() — must be called before any draw() or addWidget().
    // Propagates pointer to all currently registered widgets.
    // -------------------------------------------------------------------------
    void setDisplay(ILI9341_t3n* display) {
        _display = display;
        for (int i = 0; i < _numWidgets; ++i) {
            if (_widgets[i]) _widgets[i]->setDisplay(display);
        }
    }

    // -------------------------------------------------------------------------
    // setBackground() — colour to fill when a full repaint is triggered.
    // Default is black (0x0000).
    // -------------------------------------------------------------------------
    void setBackground(uint16_t colour) { _bgColour = colour; }

    // -------------------------------------------------------------------------
    // addWidget() — register a widget on this screen.
    // Returns true on success, false if the screen is full.
    // -------------------------------------------------------------------------
    bool addWidget(TFTWidget* widget) {
        if (_numWidgets >= MAX_WIDGETS || !widget) return false;
        widget->setDisplay(_display);  // connect to display immediately
        _widgets[_numWidgets++] = widget;
        return true;
    }

    // -------------------------------------------------------------------------
    // markAllDirty() — force all widgets to repaint on next draw().
    // Call after a full screen clear or when re-entering this screen from
    // another (e.g. returning from a TFTNumericEntry overlay).
    // -------------------------------------------------------------------------
    void markAllDirty() {
        for (int i = 0; i < _numWidgets; ++i) {
            if (_widgets[i]) _widgets[i]->markDirty();
        }
    }

    // -------------------------------------------------------------------------
    // clearAndRedraw() — fills background then marks all widgets dirty.
    // Use when switching to this screen from another screen — avoids ghost
    // pixels left by the previous screen's contents.
    // -------------------------------------------------------------------------
    void clearAndRedraw() {
        if (_display) _display->fillScreen(_bgColour);
        markAllDirty();
    }

    // -------------------------------------------------------------------------
    // draw() — repaint all dirty widgets.
    // Cost is O(numDirtyWidgets) — unchanged widgets are skipped.
    // -------------------------------------------------------------------------
    void draw() {
        for (int i = 0; i < _numWidgets; ++i) {
            if (_widgets[i]) _widgets[i]->draw();
        }
    }

    // -------------------------------------------------------------------------
    // onTouch() — route a touch point to widgets.
    // Iterates widgets in order; stops at the first one that returns true.
    // Returns true if any widget consumed the event.
    // -------------------------------------------------------------------------
    bool onTouch(int16_t x, int16_t y) {
        for (int i = 0; i < _numWidgets; ++i) {
            if (_widgets[i] && _widgets[i]->onTouch(x, y)) return true;
        }
        return false;
    }

    // -------------------------------------------------------------------------
    // onTouchRelease() — propagate release to all widgets so buttons can
    // restore normal appearance regardless of where the finger lifts.
    // -------------------------------------------------------------------------
    void onTouchRelease(int16_t x, int16_t y) {
        for (int i = 0; i < _numWidgets; ++i) {
            if (_widgets[i]) _widgets[i]->onTouchRelease(x, y);
        }
    }

    int numWidgets() const { return _numWidgets; }

private:
    ILI9341_t3n* _display;
    TFTWidget*   _widgets[MAX_WIDGETS];
    int          _numWidgets;
    uint16_t     _bgColour;
};
