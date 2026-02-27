#pragma once
// =============================================================================
// PresetBrowser.h
// JT-4000 Preset Browser for ILI9341 TFT (320x240)
//
// OVERVIEW:
//   Provides a full-screen preset browser modal that sits on top of the normal
//   UI. The caller pushes the browser open, the user scrolls/selects, and the
//   browser fires a callback then closes itself.
//
// LAYOUT  (320 x 240 px):
//
//   ┌──────────────────────────────────────┐
//   │  PRESET BROWSER            [CANCEL]  │  <- header row  (h=28)
//   ├──────────────────────────────────────┤
//   │► 00 PORTAPAD                         │  <- selected row (highlighted)
//   │  01 CHROME PD                        │
//   │  02 LYRE                             │
//   │  03 WISHFISH                         │
//   │  04 PULSAR                           │
//   │  05 TAPESTORM                        │  <- 7 visible rows (h=30 each)
//   │  06 TIBEPIUM                         │
//   ├──────────────────────────────────────┤
//   │  [◄ PREV]              [NEXT ►]      │  <- footer row   (h=30)
//   └──────────────────────────────────────┘
//
// INTERACTION:
//   Encoder A (left):  scroll up/down through list
//   Encoder B (right): not used in browser
//   Left encoder push: load selected patch and close
//   Touch CANCEL:      close without loading
//   Touch PREV/NEXT:   page up/down (7 patches at a time)
//   Touch a row:       load that patch and close
//
// USAGE:
//   In your UIManager, add a PresetBrowser member:
//       PresetBrowser _browser;
//   Open it from any button press:
//       _browser.open(synth, currentGlobalIdx);
//   In your draw() call:
//       if (_browser.isOpen()) { _browser.draw(tft); return; }
//   In your encoder/touch handlers:
//       if (_browser.isOpen()) { _browser.onEncoder(delta); return; }
//       if (_browser.isOpen()) { _browser.onTouch(tx, ty); return; }
//
// DEPENDENCIES:
//   ILI9341_t3n (or compatible)
//   Presets.h / Presets.cpp
//   SynthEngine.h
//   JT4000_Colours.h
// =============================================================================

#include "ILI9341_t3n.h"
#include "Presets.h"
#include "SynthEngine.h"

// ---- Layout constants -------------------------------------------------------
namespace PBLayout {
    static constexpr uint16_t W            = 320;
    static constexpr uint16_t H            = 240;

    static constexpr uint16_t HDR_H        = 28;   // header bar height
    static constexpr uint16_t FTR_H        = 30;   // footer bar height
    static constexpr uint16_t ROW_H        = 26;   // each list row height
    static constexpr int      VISIBLE_ROWS = 7;    // rows shown at once

    // List area
    static constexpr uint16_t LIST_Y       = HDR_H;
    static constexpr uint16_t LIST_H       = VISIBLE_ROWS * ROW_H;  // 182 px

    // Footer bar (PREV / NEXT buttons)
    static constexpr uint16_t FTR_Y        = LIST_Y + LIST_H;       // 210
    static constexpr uint16_t BTN_W        = 80;
    static constexpr uint16_t BTN_H        = FTR_H - 4;

    // CANCEL button in header
    static constexpr uint16_t CANCEL_W     = 70;
    static constexpr uint16_t CANCEL_X     = W - CANCEL_W - 4;
    static constexpr uint16_t CANCEL_Y     = 2;
    static constexpr uint16_t CANCEL_H     = HDR_H - 4;
}

// ---- Colour palette ---------------------------------------------------------
namespace PBColour {
    static constexpr uint16_t BG          = COLOUR_BACKGROUND;
    static constexpr uint16_t HDR_BG      = 0x1082;   // very dark blue-grey
    static constexpr uint16_t HDR_TEXT    = COLOUR_TEXT;
    static constexpr uint16_t ROW_BG      = COLOUR_HEADER_BG;
    static constexpr uint16_t ROW_ALT     = 0x0841;   // slightly lighter black
    static constexpr uint16_t SEL_BG      = 0x0336;   // teal highlight
    static constexpr uint16_t SEL_TEXT    = COLOUR_TEXT;
    static constexpr uint16_t ROW_TEXT    = 0xBDD7;   // light grey
    static constexpr uint16_t IDX_TEXT    = 0x7BEF;   // mid grey (index number)
    static constexpr uint16_t FTR_BG      = 0x1082;
    static constexpr uint16_t BTN_BG      = 0x2965;   // dark teal button
    static constexpr uint16_t BTN_TEXT    = COLOUR_TEXT;
    static constexpr uint16_t CANCEL_BG   = 0x6000;   // dark red
    static constexpr uint16_t BORDER      = 0x39C7;   // mid grey
}

// =============================================================================
class PresetBrowser {
public:
    // Callback type: called when the user selects a preset
    // Signature: void myCallback(int globalIndex)
    using LoadCallback = void(*)(int globalIndex);

    PresetBrowser() = default;

    // -------------------------------------------------------------------------
    // open()
    //   Show the browser. startIdx = the currently loaded patch's global index
    //   (used to pre-select the cursor on open).
    //   loadCb: called immediately when the user confirms a selection.
    //   If loadCb is nullptr, presets_loadByGlobalIndex() is called directly.
    // -------------------------------------------------------------------------
    void open(SynthEngine* synth, int startIdx = 0, LoadCallback loadCb = nullptr) {
        _synth      = synth;
        _loadCb     = loadCb;
        _totalCount = Presets::presets_totalCount();
        _cursorIdx  = constrain(startIdx, 0, _totalCount - 1);
        _scrollTop  = _clampScrollTop(_cursorIdx - PBLayout::VISIBLE_ROWS / 2);
        _open       = true;
        _dirty      = true;   // force full redraw
    }

    void close() {
        _open  = false;
        _dirty = false;
    }

    bool isOpen()  const { return _open;  }
    int  selected() const { return _cursorIdx; }

    // -------------------------------------------------------------------------
    // draw()
    //   Call this every frame while the browser is open.
    //   Only redraws what has changed (header/footer are static after open).
    // -------------------------------------------------------------------------
    void draw(ILI9341_t3n& tft) {
        if (!_open) return;

        if (_dirty) {
            // Full redraw
            _drawHeader(tft);
            _drawFooter(tft);
            for (int r = 0; r < PBLayout::VISIBLE_ROWS; r++) {
                _drawRow(tft, r);
            }
            _dirty = false;
        } else {
            // Partial: only redraw the two rows that changed
            // (_prevCursor and _cursorIdx)
            if (_prevCursor != _cursorIdx || _prevScroll != _scrollTop) {
                if (_prevScroll != _scrollTop) {
                    // Scroll happened — redraw all rows
                    for (int r = 0; r < PBLayout::VISIBLE_ROWS; r++) {
                        _drawRow(tft, r);
                    }
                } else {
                    // Just cursor moved — redraw old and new row only
                    _drawRowForIdx(tft, _prevCursor);
                    _drawRowForIdx(tft, _cursorIdx);
                }
                _prevCursor = _cursorIdx;
                _prevScroll = _scrollTop;
            }
        }
    }

    // -------------------------------------------------------------------------
    // onEncoder()
    //   delta = +1 (down) or -1 (up). Wraps around.
    // -------------------------------------------------------------------------
    void onEncoder(int delta) {
        if (!_open) return;
        _prevCursor = _cursorIdx;
        _prevScroll = _scrollTop;

        _cursorIdx = (_cursorIdx + delta + _totalCount) % _totalCount;

        // Scroll to keep cursor visible
        if (_cursorIdx < _scrollTop) {
            _scrollTop = _cursorIdx;
        } else if (_cursorIdx >= _scrollTop + PBLayout::VISIBLE_ROWS) {
            _scrollTop = _cursorIdx - PBLayout::VISIBLE_ROWS + 1;
        }
    }

    // -------------------------------------------------------------------------
    // onEncoderPress()
    //   Confirm selection and close.
    // -------------------------------------------------------------------------
    void onEncoderPress() {
        if (!_open) return;
        _loadPreset(_cursorIdx);
        close();
    }

    // -------------------------------------------------------------------------
    // onTouch()
    //   Handle a touch event. tx/ty in display pixels.
    //   Returns true if the touch was consumed by the browser.
    // -------------------------------------------------------------------------
    bool onTouch(int tx, int ty) {
        if (!_open) return false;

        // CANCEL button
        if (tx >= PBLayout::CANCEL_X && tx < PBLayout::CANCEL_X + PBLayout::CANCEL_W
         && ty >= PBLayout::CANCEL_Y && ty < PBLayout::CANCEL_Y + PBLayout::CANCEL_H) {
            close();
            return true;
        }

        // PREV button (bottom-left)
        if (tx >= 4 && tx < 4 + PBLayout::BTN_W
         && ty >= PBLayout::FTR_Y && ty < PBLayout::FTR_Y + PBLayout::FTR_H) {
            _prevCursor = _cursorIdx;
            _prevScroll = _scrollTop;
            _scrollTop  = _clampScrollTop(_scrollTop - PBLayout::VISIBLE_ROWS);
            // Move cursor to top of new page if it's now off screen
            if (_cursorIdx < _scrollTop || _cursorIdx >= _scrollTop + PBLayout::VISIBLE_ROWS) {
                _cursorIdx = _scrollTop;
            }
            _dirty = true;
            return true;
        }

        // NEXT button (bottom-right)
        int nextBtnX = PBLayout::W - 4 - PBLayout::BTN_W;
        if (tx >= nextBtnX && tx < nextBtnX + PBLayout::BTN_W
         && ty >= PBLayout::FTR_Y && ty < PBLayout::FTR_Y + PBLayout::FTR_H) {
            _prevCursor = _cursorIdx;
            _prevScroll = _scrollTop;
            _scrollTop  = _clampScrollTop(_scrollTop + PBLayout::VISIBLE_ROWS);
            if (_cursorIdx < _scrollTop || _cursorIdx >= _scrollTop + PBLayout::VISIBLE_ROWS) {
                _cursorIdx = _scrollTop;
            }
            _dirty = true;
            return true;
        }

        // List row tap
        if (ty >= PBLayout::LIST_Y && ty < PBLayout::LIST_Y + PBLayout::LIST_H) {
            int row = (ty - PBLayout::LIST_Y) / PBLayout::ROW_H;
            int idx = _scrollTop + row;
            if (idx >= 0 && idx < _totalCount) {
                if (idx == _cursorIdx) {
                    // Double-tap: confirm load
                    _loadPreset(idx);
                    close();
                } else {
                    // Single tap: move cursor
                    _prevCursor = _cursorIdx;
                    _prevScroll = _scrollTop;
                    _cursorIdx  = idx;
                }
            }
            return true;
        }

        return true;  // consume all touches while open
    }

private:
    SynthEngine*  _synth       = nullptr;
    LoadCallback  _loadCb      = nullptr;

    bool  _open       = false;
    bool  _dirty      = false;
    int   _totalCount = 0;
    int   _cursorIdx  = 0;
    int   _scrollTop  = 0;
    int   _prevCursor = -1;
    int   _prevScroll = -1;

    // ---- helpers ------------------------------------------------------------

    int _clampScrollTop(int st) const {
        int maxScroll = _totalCount - PBLayout::VISIBLE_ROWS;
        return constrain(st, 0, max(0, maxScroll));
    }

    // Load and optionally close
    void _loadPreset(int idx) {
        if (_loadCb) {
            _loadCb(idx);
        } else if (_synth) {
            Presets::presets_loadByGlobalIndex(*_synth, idx);
        }
    }

    // Draw the full header bar
    void _drawHeader(ILI9341_t3n& tft) {
        tft.fillRect(0, 0, PBLayout::W, PBLayout::HDR_H, PBColour::HDR_BG);
        tft.drawFastHLine(0, PBLayout::HDR_H - 1, PBLayout::W, PBColour::BORDER);

        tft.setTextColor(PBColour::HDR_TEXT, PBColour::HDR_BG);
        tft.setTextSize(1);
        tft.setCursor(6, 9);
        tft.print("PRESET BROWSER");

        // CANCEL button
        tft.fillRect(PBLayout::CANCEL_X, PBLayout::CANCEL_Y,
                     PBLayout::CANCEL_W, PBLayout::CANCEL_H, PBColour::CANCEL_BG);
        tft.setTextColor(PBColour::BTN_TEXT, PBColour::CANCEL_BG);
        tft.setCursor(PBLayout::CANCEL_X + 8, PBLayout::CANCEL_Y + 5);
        tft.print("CANCEL");
    }

    // Draw the footer bar with PREV / NEXT buttons
    void _drawFooter(ILI9341_t3n& tft) {
        tft.fillRect(0, PBLayout::FTR_Y, PBLayout::W, PBLayout::FTR_H, PBColour::FTR_BG);
        tft.drawFastHLine(0, PBLayout::FTR_Y, PBLayout::W, PBColour::BORDER);

        // PREV
        tft.fillRect(4, PBLayout::FTR_Y + 2, PBLayout::BTN_W, PBLayout::BTN_H, PBColour::BTN_BG);
        tft.setTextColor(PBColour::BTN_TEXT, PBColour::BTN_BG);
        tft.setCursor(14, PBLayout::FTR_Y + 8);
        tft.print("< PREV");

        // NEXT
        int nextX = PBLayout::W - 4 - PBLayout::BTN_W;
        tft.fillRect(nextX, PBLayout::FTR_Y + 2, PBLayout::BTN_W, PBLayout::BTN_H, PBColour::BTN_BG);
        tft.setTextColor(PBColour::BTN_TEXT, PBColour::BTN_BG);
        tft.setCursor(nextX + 10, PBLayout::FTR_Y + 8);
        tft.print("NEXT >");

        // Page info
        int page     = _scrollTop / PBLayout::VISIBLE_ROWS;
        int maxPage  = (_totalCount - 1) / PBLayout::VISIBLE_ROWS;
        char buf[16];
        snprintf(buf, sizeof(buf), "%d / %d", page + 1, maxPage + 1);
        tft.setTextColor(PBColour::IDX_TEXT, PBColour::FTR_BG);
        tft.setCursor(PBLayout::W / 2 - 20, PBLayout::FTR_Y + 8);
        tft.print(buf);
    }

    // Draw a single visible row by row slot [0..VISIBLE_ROWS-1]
    void _drawRow(ILI9341_t3n& tft, int row) {
        int idx = _scrollTop + row;
        uint16_t y = PBLayout::LIST_Y + row * PBLayout::ROW_H;

        // Background: alternating, selected highlighted
        uint16_t bg;
        if (idx == _cursorIdx) {
            bg = PBColour::SEL_BG;
        } else {
            bg = (row & 1) ? PBColour::ROW_ALT : PBColour::ROW_BG;
        }
        tft.fillRect(0, y, PBLayout::W, PBLayout::ROW_H, bg);

        if (idx < 0 || idx >= _totalCount) return;  // empty slot

        bool isSel = (idx == _cursorIdx);

        // Cursor arrow
        tft.setTextColor(isSel ? YELLOW : bg, bg);
        tft.setCursor(2, y + 8);
        tft.print(isSel ? ">" : " ");

        // Index  (e.g. "00" or "T2" for templates)
        tft.setTextColor(PBColour::IDX_TEXT, bg);
        tft.setCursor(12, y + 8);

        char idxBuf[5];
        int templateCount = Presets::presets_templateCount();
        if (idx < templateCount) {
            snprintf(idxBuf, sizeof(idxBuf), "T%d ", idx);   // templates: T0..T8
        } else {
            snprintf(idxBuf, sizeof(idxBuf), "%02d ", idx - templateCount);  // bank: 00..31
        }
        tft.print(idxBuf);

        // Name
        const char* name = Presets::presets_nameByGlobalIndex(idx);
        tft.setTextColor(isSel ? PBColour::SEL_TEXT : PBColour::ROW_TEXT, bg);
        tft.setCursor(46, y + 8);
        tft.print(name ? name : "---");

        // Bottom divider (subtle)
        tft.drawFastHLine(0, y + PBLayout::ROW_H - 1, PBLayout::W, 0x1082);
    }

    // Draw the row that contains the given global index (if visible)
    void _drawRowForIdx(ILI9341_t3n& tft, int idx) {
        int row = idx - _scrollTop;
        if (row >= 0 && row < PBLayout::VISIBLE_ROWS) {
            _drawRow(tft, row);
        }
    }
};
