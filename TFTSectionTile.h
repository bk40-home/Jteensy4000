// TFTSectionTile.h
// =============================================================================
// Home-screen section tile widget.
//
// Visual layout (tile is ~77 × 60 px):
//
//   ┌───────────────────┐
//   ║▌▌▌▌▌▌▌▌▌▌▌▌▌▌▌▌▌║  ← 2 px top accent bar (section colour)
//   │                   │
//   │    FILTER         │  ← section label, centred, section colour
//   │    4 pages        │  ← dim hint below
//   │                   │
//   └───────────────────┘
//
// States:
//   Normal  — dark background, dim border, coloured top bar
//   Pressed — tinted background + accent border (flash, restores on release)
//
// Fires callback on touch-release inside bounds (same contract as TFTButton).
// No engine dependency — stateless display only.
// =============================================================================

#pragma once
#include "TFTWidget.h"
#include "JT4000_Sections.h"
#include "JT4000Colours.h"

class TFTSectionTile : public TFTWidget {
public:
    using Callback = void (*)();

    TFTSectionTile(int16_t x, int16_t y, int16_t w, int16_t h,
                   const SectionDef& section)
        : TFTWidget(x, y, w, h)
        , _section(section)
        , _pressed(false)
        , _callback(nullptr)
    {}

    void setCallback(Callback cb) { _callback = cb; }

    // Fire callback programmatically (encoder selection)
    void activate() { if (_callback) _callback(); }

    // ------------------------------------------------------------------
    bool onTouch(int16_t x, int16_t y) override {
        if (!hitTest(x, y)) return false;
        if (!_pressed) { _pressed = true; markDirty(); }
        return true;
    }

    void onTouchRelease(int16_t x, int16_t y) override {
        if (!_pressed) return;
        _pressed = false;
        markDirty();
        // Fire only if finger lifted inside the tile
        if (hitTest(x, y) && _callback) _callback();
    }

protected:
    void doDraw() override {
        if (!_display) return;

        // Background — dark normally, faint accent tint when pressed
        const uint16_t bgCol = _pressed
            ? (uint16_t)((_section.colour >> 2) & 0x39E7)  // ¼ brightness tint
            : (uint16_t)0x1082;   // very dark blue-grey

        _display->fillRect(_x, _y, _w, _h, bgCol);

        // Outer border — accent when pressed, dim otherwise
        _display->drawRect(_x, _y, _w, _h,
                           _pressed ? _section.colour : RED);

        // 2 px top accent bar (drawn inside the border)
        _display->drawFastHLine(_x + 1, _y + 1, _w - 2, _section.colour);
        _display->drawFastHLine(_x + 1, _y + 2, _w - 2, _section.colour);

        // Section label — centred, section colour, font 1
        _display->setTextSize(1);
        _display->setTextColor(_section.colour);
        const int16_t labelW = (int16_t)(strlen(_section.label) * 6);
        _display->setCursor(_x + (_w - labelW) / 2,
                            _y + (_h / 2) - 8);
        _display->print(_section.label);

        // Page-count hint — centred, dim, 2 px below label
        char hint[6];
        snprintf(hint, sizeof(hint), "%dp", _section.pageCount);
        const int16_t hintW = (int16_t)(strlen(hint) * 6);
        _display->setTextColor(COLOUR_TEXT_DIM);
        _display->setCursor(_x + (_w - hintW) / 2,
                            _y + (_h / 2) + 2);
        _display->print(hint);
    }

private:
    const SectionDef& _section;
    bool              _pressed;
    Callback          _callback;
};
