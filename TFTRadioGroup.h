// TFTRadioGroup.h
// =============================================================================
// Horizontal radio-button group for the JT-4000 TFT UI.
//
// Usage:
//
//   // Waveform selector — 5 options, horizontal, spanning full width
//   static const char* waveNames[] = { "SINE","TRI","SQR","SAW","SSAW" };
//   TFTRadioGroup waveSelect(5, 0, 40, 320, 36);
//   waveSelect.setOptions(waveNames, 5);
//   waveSelect.setDisplay(&tft);
//   waveSelect.setSelected(2);       // pre-select SAW
//   waveSelect.setCallback([](int idx){ synth.setWave(idx); });
//
//   // In main loop:
//   waveSelect.draw();
//   if (touch.newTap()) waveSelect.onTouch(tp.x, tp.y);
//
//   // From MIDI / engine sync — only repaints the two changed items:
//   waveSelect.setSelected(newIdx);
//
// Layout (horizontal):
//
//   ┌────────────────────────────────────────────────────┐
//   │  ●SINE   ○TRI   ○SQR   ○SAW   ○SSAW               │
//   └────────────────────────────────────────────────────┘
//
// Each option cell is (width / numOptions) pixels wide.
// Circle radius = 5 px. Label sits to the right of the circle.
// Selected option: filled circle + accent-coloured label.
// Unselected: outline circle + dim label.
// =============================================================================

#pragma once
#include "TFTWidget.h"

// Maximum number of options per group — stack-allocated, no heap.
static constexpr int RADIO_MAX_OPTIONS = 12;

class TFTRadioGroup : public TFTWidget {
public:
    // -------------------------------------------------------------------------
    // Callback type: fired when the user selects an option.
    // index = 0-based option index.
    // -------------------------------------------------------------------------
    using Callback = void (*)(int index);

    // -------------------------------------------------------------------------
    // Constructor
    //   x, y, w, h  — bounding rect on screen
    // -------------------------------------------------------------------------
    TFTRadioGroup(int16_t x, int16_t y, int16_t w, int16_t h)
        : TFTWidget(x, y, w, h)
        , _numOptions(0)
        , _selected(-1)         // -1 = nothing selected yet
        , _callback(nullptr)
    {
        // Zero the dirty-per-cell array; all drawn on first full paint
        for (int i = 0; i < RADIO_MAX_OPTIONS; ++i) {
            _optionDirty[i] = true;
            _labels[i]      = nullptr;
        }
    }

    // -------------------------------------------------------------------------
    // setOptions() — provide the label strings.
    //   labels[]  — array of const char* pointers (must outlive this widget)
    //   count     — number of options (max RADIO_MAX_OPTIONS)
    // Call before first draw().
    // -------------------------------------------------------------------------
    void setOptions(const char* const* labels, int count) {
        _numOptions = (count < RADIO_MAX_OPTIONS) ? count : RADIO_MAX_OPTIONS;
        for (int i = 0; i < _numOptions; ++i) _labels[i] = labels[i];
        // Force full repaint when options change
        markDirty();
        for (int i = 0; i < _numOptions; ++i) _optionDirty[i] = true;
    }

    // -------------------------------------------------------------------------
    // setSelected() — change the selected index.
    //   Fires the callback if fireCallback = true (default).
    //   Only redraws the two changed cells, not the whole widget.
    // -------------------------------------------------------------------------
    void setSelected(int index, bool fireCallback = false) {
        if (index < 0 || index >= _numOptions) return;
        if (index == _selected) return;

        const int prev = _selected;
        _selected = index;

        // Mark only the two cells that changed — minimal repaint
        if (prev >= 0 && prev < _numOptions) _optionDirty[prev] = true;
        _optionDirty[index] = true;
        _dirty = true;      // signal TFTWidget::draw() to call doDraw()

        if (fireCallback && _callback) _callback(_selected);
    }

    int  getSelected()                 const { return _selected; }
    void setCallback(Callback cb)            { _callback = cb;   }

    // -------------------------------------------------------------------------
    // onTouch() — hit-test which cell was tapped, select it.
    // Returns true if the touch was inside this widget.
    // -------------------------------------------------------------------------
    bool onTouch(int16_t x, int16_t y) override {
        if (!hitTest(x, y) || _numOptions == 0) return false;

        // Which cell?
        const int16_t cellW = _w / _numOptions;
        const int     idx   = (x - _x) / cellW;

        if (idx >= 0 && idx < _numOptions) {
            setSelected(idx, /*fireCallback=*/true);
        }
        return true;
    }

protected:
    // -------------------------------------------------------------------------
    // doDraw() — paints only the dirty option cells.
    // On a full-dirty (first paint or widget re-shown), all cells are painted.
    // Subsequent calls repaint only changed cells → minimal SPI traffic.
    // -------------------------------------------------------------------------
    void doDraw() override {
        if (_numOptions == 0 || !_display) return;

        const int16_t cellW  = _w / _numOptions;   // width of each option cell
        const int16_t midY   = _y + _h / 2;        // vertical centre of row
        const int16_t circR  = 5;                  // radio circle radius (px)
        const int16_t circX  = _x + 4 + circR;     // first circle X centre (4px left pad)
        const uint8_t fSize  = (_h >= 24) ? 1 : 1; // font size (always 1 at this height)

        for (int i = 0; i < _numOptions; ++i) {
            if (!_optionDirty[i]) continue;         // skip cells that haven't changed

            const bool   sel    = (i == _selected);
            const int16_t cellX = _x + i * cellW;

            // Clear cell background
            _display->fillRect(cellX, _y, cellW, _h, gTheme.bg);

            // Circle position for this cell
            const int16_t cx = cellX + 4 + circR;

            // Draw radio circle — filled if selected, outline if not
            if (sel) {
                _display->fillCircle(cx, midY, circR, gTheme.radioFill);
                // Thin outline to define the edge cleanly
                _display->drawCircle(cx, midY, circR, gTheme.radioBorder);
            } else {
                // Hollow circle on background
                _display->fillCircle(cx, midY, circR, gTheme.bg);
                _display->drawCircle(cx, midY, circR, gTheme.radioBorder);
            }

            // Label text to the right of the circle
            if (_labels[i]) {
                const int16_t labelX = cx + circR + 3;   // 3px gap after circle
                const int16_t labelY = midY - 4 * fSize; // vertically centred
                const uint16_t col   = sel ? gTheme.radioFill : gTheme.textDim;
                _display->setTextSize(fSize);
                _display->setTextColor(col);
                _display->setCursor(labelX, labelY);
                _display->print(_labels[i]);
            }

            _optionDirty[i] = false;
        }
    }

private:
    int              _numOptions;
    int              _selected;
    const char*      _labels[RADIO_MAX_OPTIONS];
    bool             _optionDirty[RADIO_MAX_OPTIONS]; // per-cell dirty flags
    Callback         _callback;
};
