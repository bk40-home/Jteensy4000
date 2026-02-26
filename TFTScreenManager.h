// TFTScreenManager.h
// =============================================================================
// Lightweight screen navigation stack for the JT-4000 TFT UI.
//
// Concept:
//   The manager maintains a stack of TFTScreen pointers.
//   Only the top screen is drawn and receives touch events.
//   push() opens a sub-screen (e.g. a parameter editor or preset browser).
//   pop()  returns to the previous screen, restoring it with a full repaint.
//
// Stack depth is fixed at SCREEN_STACK_DEPTH — no heap allocation.
//
// TFTNumericEntry is handled separately (not a TFTScreen) because it occupies
// the full display directly via ILI9341_t3n — route it via numericEntry() and
// check isEntryOpen() each frame.
//
// Usage:
//
//   TFTScreenManager mgr;
//   mgr.setDisplay(&tft);
//   mgr.push(&mainScreen);          // start on main screen
//
//   // When user taps "Edit" button:
//   mgr.push(&editScreen);          // edit screen slides on top
//
//   // When user confirms or cancels in editScreen:
//   mgr.pop();                      // back to mainScreen, full repaint
//
//   // Main loop:
//   mgr.update(touch.newTap(), touch.x, touch.y);
//
// =============================================================================

#pragma once
#include "TFTScreen.h"
#include "TFTNumericEntry.h"

static constexpr int SCREEN_STACK_DEPTH = 4;   // max nesting depth

class TFTScreenManager {
public:
    TFTScreenManager()
        : _display(nullptr), _stackDepth(0)
    {}

    // -------------------------------------------------------------------------
    // setDisplay() — must be called once before any push().
    // -------------------------------------------------------------------------
    void setDisplay(ILI9341_t3n* display) {
        _display = display;
        _numericEntry.setDisplay(display);
    }

    // -------------------------------------------------------------------------
    // push() — make a screen the active top-of-stack.
    // Calls clearAndRedraw() so the new screen paints cleanly over whatever
    // was showing before.
    // -------------------------------------------------------------------------
    bool push(TFTScreen* screen) {
        if (_stackDepth >= SCREEN_STACK_DEPTH || !screen) return false;
        screen->setDisplay(_display);
        screen->clearAndRedraw();           // full clear + mark all widgets dirty
        _stack[_stackDepth++] = screen;
        return true;
    }

    // -------------------------------------------------------------------------
    // pop() — return to the previous screen.
    // The revealed screen is marked fully dirty so it repaints completely
    // (it was hidden while the top screen was showing — its pixels may be gone).
    // -------------------------------------------------------------------------
    bool pop() {
        if (_stackDepth <= 1) return false;  // never pop the root screen
        --_stackDepth;
        // Restore the newly revealed screen with a full repaint
        if (_stack[_stackDepth - 1]) {
            _stack[_stackDepth - 1]->clearAndRedraw();
        }
        return true;
    }

    // -------------------------------------------------------------------------
    // topScreen() — access the currently active screen.
    // Returns nullptr if stack is empty.
    // -------------------------------------------------------------------------
    TFTScreen* topScreen() {
        return (_stackDepth > 0) ? _stack[_stackDepth - 1] : nullptr;
    }

    int stackDepth() const { return _stackDepth; }

    // -------------------------------------------------------------------------
    // TFTNumericEntry — integrated value editor.
    //
    // openNumeric() / openEnum() open the full-screen entry overlay.
    // While the entry is open, update() routes ALL touches to it and
    // suppresses normal screen drawing.
    //
    // The entry closes itself on Confirm or Cancel; isEntryOpen() becomes false
    // the same frame so normal operation resumes.
    //
    // The callback you supply should apply the value AND call
    //   topScreen()->markAllDirty()
    // so the parameter row refreshes immediately after the editor closes.
    // -------------------------------------------------------------------------
    TFTNumericEntry& numericEntry() { return _numericEntry; }

    bool isEntryOpen() const { return _numericEntry.isOpen(); }

    // -------------------------------------------------------------------------
    // update() — call each frame from the main loop.
    //   newTouch   — true if a new touch event arrived this frame
    //   tx, ty     — touch coordinates (screen pixels)
    //   newRelease — true if finger just lifted
    //   rx, ry     — release coordinates
    // -------------------------------------------------------------------------
    void update(bool newTouch, int16_t tx, int16_t ty,
                bool newRelease = false, int16_t rx = 0, int16_t ry = 0) {

        // --- Entry overlay takes priority over everything ---
        if (_numericEntry.isOpen()) {
            _numericEntry.draw();
            if (newTouch) _numericEntry.onTouch(tx, ty);
            return;
        }

        // --- Normal screen draw and touch routing ---
        TFTScreen* top = topScreen();
        if (!top) return;

        top->draw();

        if (newTouch)   top->onTouch(tx, ty);
        if (newRelease) top->onTouchRelease(rx, ry);
    }

private:
    ILI9341_t3n*    _display;
    TFTScreen*      _stack[SCREEN_STACK_DEPTH];
    int             _stackDepth;
    TFTNumericEntry _numericEntry;   // single shared entry overlay (one active at a time)
};
