// TFTParamRow.h
// =============================================================================
// A single tappable parameter row used in SectionScreen.
//
// Visual layout (row is 320 × ~43 px):
//
//   ┌───────────────────────────────────────────────────────────────────┐
//   │  Cutoff                              1200 Hz  ›                   │
//   │  ████████████████░░░░░░░░░░░░░░░  ← 3 px value bar              │
//   └───────────────────────────────────────────────────────────────────┘
//
//   Selected state: category colour background, inverted text.
//   Empty slot (CC==255): draws "---" in dim text, not tappable.
//
// setValue() only marks dirty when value or text actually changes.
// Callback fires with the CC number so the parent can open the entry overlay.
// =============================================================================

#pragma once
#include "TFTWidget.h"
#include "JT4000Colours.h"

// Buffer sizes — fixed stack allocation, no heap
static constexpr int PROW_NAME_LEN = 12;   // ≤ UIPage::ccNames max width
static constexpr int PROW_VAL_LEN  = 16;   // value + unit string

class TFTParamRow : public TFTWidget {
public:
    using TapCallback = void (*)(uint8_t cc);

    // -------------------------------------------------------------------------
    // Constructor
    //   cc      — MIDI CC this row represents (255 = empty slot)
    //   name    — short label (copied into fixed buffer)
    //   colour  — category accent colour (bar + value text)
    // -------------------------------------------------------------------------
    TFTParamRow(int16_t x, int16_t y, int16_t w, int16_t h,
                uint8_t cc, const char* name, uint16_t colour)
        : TFTWidget(x, y, w, h)
        , _cc(cc)
        , _colour(colour)
        , _selected(false)
        , _rawValue(0)
        , _onTap(nullptr)
    {
        // Copy name — never store a pointer to transient string data
        strncpy(_name, name ? name : "---", PROW_NAME_LEN - 1);
        _name[PROW_NAME_LEN - 1] = '\0';
        _valText[0] = '\0';
    }

    void setCallback(TapCallback cb) { _onTap = cb; }

    // -------------------------------------------------------------------------
    // setValue() — update displayed value; only repaints if something changed.
    //   rawValue — 0-127 CC value (drives bar width)
    //   text     — human-readable string; nullptr → format rawValue as int
    // -------------------------------------------------------------------------
    void setValue(uint8_t rawValue, const char* text) {
        bool changed = false;

        if (rawValue != _rawValue) {
            _rawValue = rawValue;
            changed   = true;
        }

        // Build candidate text into a local buffer
        char newText[PROW_VAL_LEN];
        if (text && text[0]) {
            strncpy(newText, text, PROW_VAL_LEN - 1);
            newText[PROW_VAL_LEN - 1] = '\0';
        } else {
            snprintf(newText, PROW_VAL_LEN, "%d", (int)rawValue);
        }

        if (strncmp(newText, _valText, PROW_VAL_LEN) != 0) {
            strncpy(_valText, newText, PROW_VAL_LEN - 1);
            _valText[PROW_VAL_LEN - 1] = '\0';
            changed = true;
        }

        if (changed) markDirty();
    }

    // -------------------------------------------------------------------------
    // setSelected() — highlight/de-highlight this row
    // -------------------------------------------------------------------------
    void setSelected(bool sel) {
        if (sel == _selected) return;
        _selected = sel;
        markDirty();
    }

    bool    isSelected()  const { return _selected; }
    uint8_t getCC()       const { return _cc; }

    // -------------------------------------------------------------------------
    // onTouch() — consume tap, select this row, fire callback
    // -------------------------------------------------------------------------
    bool onTouch(int16_t x, int16_t y) override {
        if (_cc == 255 || !hitTest(x, y)) return false;
        if (_onTap) _onTap(_cc);
        return true;
    }

    void configure(uint8_t cc, const char* name, uint16_t colour)
{
    bool changed = false;

    if (_cc != cc) {
        _cc = cc;
        changed = true;
    }

    if (_colour != colour) {
        _colour = colour;
        changed = true;
    }

    char newName[PROW_NAME_LEN];
    strncpy(newName, name ? name : "---", PROW_NAME_LEN - 1);
    newName[PROW_NAME_LEN - 1] = '\0';

    if (strncmp(newName, _name, PROW_NAME_LEN) != 0) {
        strncpy(_name, newName, PROW_NAME_LEN);
        changed = true;
    }

    if (changed) {
        _rawValue = 0;
        _valText[0] = '\0';
        markDirty();
    }
}

protected:
    void doDraw() override {
        if (!_display) return;

        static constexpr int16_t PAD    = 5;   // left/right inner padding
        static constexpr int16_t BAR_H  = 3;   // value bar height (px)
        // Leave 1 px gap between rows via the height allocation in SectionScreen
        const int16_t contentH = _h - 1;

        // ---- Background + separator ----
        const uint16_t bgCol = _selected ? _colour : COLOUR_HEADER_BG;
        _display->fillRect(_x, _y, _w, contentH, bgCol);
        _display->drawFastHLine(_x, _y + contentH, _w, COLOUR_BORDER);

        // ---- Empty slot ----
        if (_cc == 255) {
            _display->setTextSize(1);
            _display->setTextColor(COLOUR_TEXT_DIM);
            _display->setCursor(_x + PAD, _y + (contentH - 8) / 2);
            _display->print("---");
            return;
        }

        const uint16_t textCol  = _selected ? COLOUR_BACKGROUND    : COLOUR_TEXT;
        const uint16_t dimCol   = _selected ? COLOUR_BACKGROUND    : COLOUR_TEXT_DIM;
        const uint16_t barCol   = _selected ? COLOUR_BACKGROUND    : _colour;

        // ---- Parameter name (left) ----
        _display->setTextSize(1);
        _display->setTextColor(textCol);
        _display->setCursor(_x + PAD, _y + 6);
        _display->print(_name);

        // ---- Value text (right-aligned) ----
        if (_valText[0]) {
            const int16_t valW = (int16_t)(strlen(_valText) * 6);
            _display->setTextColor(_selected ? COLOUR_BACKGROUND : _colour);
            _display->setCursor(_x + _w - PAD - valW - 8, _y + 6);
            _display->print(_valText);
        }

        // ---- Tap-to-edit arrow (far right, dim) ----
        _display->setTextColor(dimCol);
        _display->setCursor(_x + _w - PAD - 6, _y + 6);
        _display->print(">");

        // ---- Value bar (bottom of row) ----
        const int16_t barY    = _y + contentH - BAR_H - 2;
        const int16_t barMaxW = _w - 2 * PAD;
        // Integer multiply first to avoid overflow, then divide
        const int16_t barFill = (int16_t)((int32_t)barMaxW * _rawValue / 127);

        _display->drawFastHLine(_x + PAD, barY, barMaxW, COLOUR_BORDER);
        if (barFill > 0) {
            _display->fillRect(_x + PAD, barY, barFill, BAR_H, barCol);
        }
    }

private:
    uint8_t     _cc;
    uint16_t    _colour;
    bool        _selected;
    uint8_t     _rawValue;
    char        _name[PROW_NAME_LEN];
    char        _valText[PROW_VAL_LEN];
    TapCallback _onTap;
};
