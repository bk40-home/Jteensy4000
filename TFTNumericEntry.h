// TFTNumericEntry.h
// =============================================================================
// Full-screen value entry for the JT-4000 TFT UI.
//
// Two modes:
//
//   MODE_NUMBER — on-screen numeric keypad.
//     Displays the current human-readable value at the top.
//     Digits 0-9, a backspace key, Confirm (✓) and Cancel (✗).
//     User types a new value; on Confirm the callback is called with the
//     raw typed value clamped to [minVal, maxVal].
//     The value is displayed in human units (Hz, ms, %, semitones etc.)
//     — the caller formats the display string via a format callback.
//
//   MODE_ENUM — scrollable list picker.
//     Shows all option strings from an options[] array.
//     User taps or scrolls to an item, confirms or cancels.
//     On Confirm, the callback fires with the selected index.
//
// Usage (numeric):
//   TFTNumericEntry entry;
//   entry.setDisplay(&tft);
//   entry.openNumeric(
//       "Filter Cutoff",          // title shown at top
//       "Hz",                     // unit string shown next to value
//       20, 20000,                // min / max (human units, integers here)
//       1200,                     // current value
//       [](int v){ synth.setCutoffHz(v); }  // callback
//   );
//
// Usage (enum):
//   static const char* waveNames[] = {"SINE","TRI","SQR","SAW","SSAW"};
//   entry.openEnum(
//       "OSC1 Wave",
//       waveNames, 5,
//       2,                        // current selection index
//       [](int i){ synth.setWave(i); }
//   );
//
// Drawing:
//   entry.draw();              // call each frame; only redraws when dirty
//   entry.onTouch(tx, ty);     // route touch events here when entry is open
//
// bool TFTNumericEntry::isOpen() — returns true while the entry screen is visible.
// When the user confirms or cancels, isOpen() becomes false and the parent
// screen should resume normal display.
//
// Screen layout (MODE_NUMBER, 320×240):
//
//   ┌──────────────────────────────────────────────┐
//   │  Filter Cutoff                    [✗ Cancel] │  ← title bar (30px)
//   │  ┌────────────────────────────────────────┐  │
//   │  │   1 2 0 0 Hz      ← typed value        │  │  ← value display (40px)
//   │  └────────────────────────────────────────┘  │
//   │                                              │
//   │  [ 7 ] [ 8 ] [ 9 ]                          │
//   │  [ 4 ] [ 5 ] [ 6 ]                          │  ← keypad (3×3 + row)
//   │  [ 1 ] [ 2 ] [ 3 ]                          │
//   │  [  0  ]  [  ⌫  ]  [    ✓    ]             │
//   └──────────────────────────────────────────────┘
//
// Screen layout (MODE_ENUM, 320×240):
//   Title bar, then a scrollable list of options with the current one
//   highlighted. Confirm / Cancel buttons at the bottom.
// =============================================================================

#pragma once
#include "TFTWidget.h"

// Maximum digits the user can type (limits the buffer size)
static constexpr int ENTRY_MAX_DIGITS  = 7;
// Maximum enum options in the picker
static constexpr int ENTRY_MAX_ENUM    = 24;
// Maximum characters in the title bar
static constexpr int ENTRY_TITLE_LEN   = 24;
// Maximum characters in the unit string
static constexpr int ENTRY_UNIT_LEN    = 8;

class TFTNumericEntry {
public:
    // -------------------------------------------------------------------------
    // Callback: fired on Confirm, NOT on Cancel.
    // For MODE_NUMBER: value = the typed integer, clamped to [min, max].
    // For MODE_ENUM:   value = selected option index.
    // -------------------------------------------------------------------------
    using Callback = void (*)(int value);

    // -------------------------------------------------------------------------
    // Mode enum
    // -------------------------------------------------------------------------
    enum Mode : uint8_t { MODE_CLOSED = 0, MODE_NUMBER, MODE_ENUM };

    TFTNumericEntry()
        : _display(nullptr), _mode(MODE_CLOSED)
        , _minVal(0), _maxVal(127), _currentVal(0)
        , _digitCount(0)
        , _selectedEnum(0), _numEnumOptions(0)
        , _callback(nullptr)
        , _scrollOffset(0)
        , _fullRedraw(false)
        , _valueDirty(false)
    {
        _titleBuf[0] = '\0';
        _unitBuf[0]  = '\0';
        _digitBuf[0] = '\0';
        for (int i = 0; i < ENTRY_MAX_ENUM; ++i) _enumLabels[i] = nullptr;
    }

    void setDisplay(ILI9341_t3n* d) { _display = d; }

    // -------------------------------------------------------------------------
    // openNumeric() — open the keypad entry screen.
    //   title       — parameter name (max 23 chars, no heap copy)
    //   unit        — human unit string e.g. "Hz", "ms", "%"
    //   minVal      — minimum allowed value (human units)
    //   maxVal      — maximum allowed value (human units)
    //   currentVal  — pre-filled value (human units)
    //   cb          — called with the confirmed value
    // -------------------------------------------------------------------------
    void openNumeric(const char* title, const char* unit,
                     int minVal, int maxVal, int currentVal, Callback cb) {
        if (!_display) return;

        _mode      = MODE_NUMBER;
        _minVal    = minVal;
        _maxVal    = maxVal;
        _currentVal = currentVal;
        _callback  = cb;

        // Copy title and unit into fixed-size stack buffers (no heap)
        strncpy(_titleBuf, title ? title : "", ENTRY_TITLE_LEN - 1);
        _titleBuf[ENTRY_TITLE_LEN - 1] = '\0';
        strncpy(_unitBuf, unit ? unit : "", ENTRY_UNIT_LEN - 1);
        _unitBuf[ENTRY_UNIT_LEN - 1] = '\0';

        // Pre-fill display with current value so user sees it before editing
        snprintf(_digitBuf, sizeof(_digitBuf), "%d", currentVal);
        _digitCount = (int)strlen(_digitBuf);

        _fullRedraw  = true;
        _valueDirty  = false;
    }

    // -------------------------------------------------------------------------
    // openEnum() — open the list picker.
    //   labels[]    — array of const char* (must outlive this call)
    //   count       — number of options
    //   currentIdx  — pre-selected index
    //   cb          — called with confirmed index
    // -------------------------------------------------------------------------
    void openEnum(const char* title, const char* const* labels, int count,
                  int currentIdx, Callback cb) {
        if (!_display) return;

        _mode       = MODE_ENUM;
        _callback   = cb;
        _numEnumOptions = (count < ENTRY_MAX_ENUM) ? count : ENTRY_MAX_ENUM;
        _selectedEnum   = constrain(currentIdx, 0, _numEnumOptions - 1);
        _scrollOffset   = 0;

        strncpy(_titleBuf, title ? title : "", ENTRY_TITLE_LEN - 1);
        _titleBuf[ENTRY_TITLE_LEN - 1] = '\0';

        for (int i = 0; i < _numEnumOptions; ++i) _enumLabels[i] = labels[i];

        // Scroll so the selected item is visible
        _scrollToSelection();

        _fullRedraw = true;
        _valueDirty = false;
    }

    // -------------------------------------------------------------------------
    // draw() — repaint as needed. Call each frame while isOpen().
    // -------------------------------------------------------------------------
    void draw() {
        if (_mode == MODE_CLOSED || !_display) return;
        if (_fullRedraw) {
            _drawFull();
            _fullRedraw = false;
            _valueDirty = false;
        } else if (_valueDirty) {
            _drawValueBox();
            _valueDirty = false;
        }
    }

    // -------------------------------------------------------------------------
    // onTouch() — route all touch events here while isOpen().
    // Returns true always (consumes all touch while open to block the parent).
    // -------------------------------------------------------------------------
    bool onTouch(int16_t x, int16_t y) {
        if (_mode == MODE_CLOSED) return false;

        if (_mode == MODE_NUMBER) _handleNumericTouch(x, y);
        else                      _handleEnumTouch(x, y);
        return true;  // consume all touches while open
    }

    bool isOpen() const { return _mode != MODE_CLOSED; }
    Mode getMode() const { return _mode; }

    // Close without firing callback (used by cancel)
    void close() { _mode = MODE_CLOSED; }

private:
    // =========================================================================
    // LAYOUT CONSTANTS (pixels, 320×240 screen)
    // =========================================================================
    static constexpr int SW = 320;   // screen width
    static constexpr int SH = 240;   // screen height

    // Title bar
    static constexpr int TB_H  = 30;

    // Value display box
    static constexpr int VB_Y  = TB_H + 4;
    static constexpr int VB_H  = 36;

    // Keypad area (starts below value box, leaves 10px gap)
    static constexpr int KP_Y  = VB_Y + VB_H + 8;    // top of keypad
    static constexpr int KP_X  = 10;                  // left edge
    static constexpr int KP_W  = 300;                 // total keypad width
    static constexpr int KEY_WIDTH= 94;                   // key width (3 keys + 6px gaps = 290)
    static constexpr int KEY_HEIGHT = 36;                   // key height
    static constexpr int KEY_GAP = 4;                    // gap between keys

    // Bottom row (0, ⌫, ✓) uses wider keys
    static constexpr int BR_Y  = KP_Y + 3 * (KEY_HEIGHT + KEY_GAP);   // bottom row Y
    static constexpr int BR0_W = 90;   // zero key width
    static constexpr int BRBK_W= 90;   // backspace key width
    static constexpr int BRCO_W= 106;  // confirm key width

    // Cancel button — top-right of title bar
    static constexpr int CANCEL_X = 240;
     static constexpr int CANCEL_Y = 160;
    static constexpr int CANCEL_W = 75;
    static constexpr int CANCEL_H = 22;

    // Enum list
    static constexpr int EN_ROW_H = 32;                          // height of each list row
    static constexpr int EN_ROWS  = (SH - TB_H - 40) / EN_ROW_H; // visible rows (~5)
    static constexpr int EN_BTN_Y = SH - 36;                    // confirm/cancel buttons Y

    // =========================================================================
    // FULL REDRAW
    // =========================================================================
    void _drawFull() {
        _display->fillScreen(gTheme.bg);

        // Title bar
        _display->fillRect(0, 0, SW, TB_H, gTheme.headerBg);
        _display->setTextSize(2);
        _display->setTextColor(gTheme.textNormal);
        _display->setCursor(6, 7);
        _display->print(_titleBuf);

        // Cancel button — always present
        _drawCancelButton(false);

        if (_mode == MODE_NUMBER) {
            _drawValueBox();
            _drawKeypad();
        } else {
            _drawEnumList();
            _drawEnumButtons();
        }
    }

    // =========================================================================
    // CANCEL BUTTON (top-right)
    // =========================================================================
    void _drawCancelButton(bool pressed) {
        const uint16_t bg = pressed ? gTheme.buttonPress : gTheme.accent;
        _display->fillRect(CANCEL_X,CANCEL_Y, CANCEL_W, CANCEL_H,  bg);
        //void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
        _display->setTextSize(1);
        _display->setTextColor(gTheme.buttonText);
        // Centred "Cancel" in the button
        const int16_t lx = CANCEL_X + (CANCEL_W - 6 * 6) / 2;
        _display->setCursor(lx, 11);
        _display->print("Cancel");
    }

    // =========================================================================
    // VALUE DISPLAY BOX
    // =========================================================================
    void _drawValueBox() {
        // Clear the value box area
        _display->fillRect(KP_X, VB_Y, KP_W, VB_H, gTheme.entryBg);
        _display->drawRect(KP_X, VB_Y, KP_W, VB_H, gTheme.border);

        // Build display string: digits + unit
        char dispBuf[ENTRY_MAX_DIGITS + ENTRY_UNIT_LEN + 2];
        if (_digitCount == 0) {
            // Nothing typed yet — show current value dimmed as hint
            snprintf(dispBuf, sizeof(dispBuf), "%d %s", _currentVal, _unitBuf);
            _display->setTextColor(gTheme.textDim);
        } else {
            snprintf(dispBuf, sizeof(dispBuf), "%s %s", _digitBuf, _unitBuf);
            _display->setTextColor(gTheme.entryText);
        }

        _display->setTextSize(2);
        const int16_t tw = (int16_t)(strlen(dispBuf) * 12); // 6px×2 per char
        const int16_t tx = KP_X + (KP_W - tw) / 2;
        const int16_t ty = VB_Y + (VB_H - 14) / 2;
        _display->setCursor(tx, ty);
        _display->print(dispBuf);
    }

    // =========================================================================
    // NUMERIC KEYPAD
    //
    // Layout:
    //   [7][8][9]
    //   [4][5][6]
    //   [1][2][3]
    //   [0]  [⌫]  [✓ Confirm]
    // =========================================================================
    void _drawKeypad() {
        // Rows 7-9, 4-6, 1-3
        const int digits[3][3] = { {7,8,9}, {4,5,6}, {1,2,3} };
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                const int16_t kx = KP_X + col * (KEY_WIDTH+ KEY_GAP);
                const int16_t ky = KP_Y + row * (KEY_HEIGHT + KEY_GAP);
                _drawKey(kx, ky, KEY_W, KEY_HEIGHT,
                         _digitStr(digits[row][col]), gTheme.keyBg, false);
            }
        }

        // Bottom row: [0] [⌫] [✓]
        _drawKey(KP_X,                           BR_Y, BR0_W,  KEY_HEIGHT, "0",  gTheme.keyBg,     false);
        _drawKey(KP_X + BR0_W + KEY_GAP,           BR_Y, BRBK_W, KEY_HEIGHT, "<-", gTheme.keyBackspace, false);
        _drawKey(KP_X + BR0_W + BRBK_W + 2*KEY_GAP, BR_Y, BRCO_W, KEY_HEIGHT, "OK", gTheme.keyConfirm, false);
    }

    void _drawKey(int16_t kx, int16_t ky, int16_t kw, int16_t kh,
                  const char* label, uint16_t bgCol, bool pressed) {
        const uint16_t bg = pressed ? gTheme.buttonPress : bgCol;
        _display->fillRect(kx, ky, kw, kh, bg);
        _display->drawRect(kx, ky, kw, kh, gTheme.keyBorder);
        _display->setTextSize(1);
        _display->setTextColor(gTheme.keyText);
        const int16_t tw = (int16_t)(strlen(label) * 6);
        _display->setCursor(kx + (kw - tw) / 2, ky + (kh - 8) / 2);
        _display->print(label);
    }

    // Small helper: digit int → "0".."9" string (stack buffer, one character)
    static const char* _digitStr(int d) {
        static char buf[2] = "0";
        buf[0] = '0' + (char)d;
        return buf;
    }

    // =========================================================================
    // NUMERIC TOUCH HANDLER
    // =========================================================================
    void _handleNumericTouch(int16_t x, int16_t y) {
        // Cancel button
        if (x >= CANCEL_X && x < CANCEL_X + CANCEL_W &&
            y >= 4 && y < 4 + CANCEL_H) {
            close();
            return;
        }

        // Digit rows 1-3 (rows of 7-9, 4-6, 1-3)
        const int digits[3][3] = { {7,8,9}, {4,5,6}, {1,2,3} };
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                const int16_t kx = KP_X + col * (KEY_WIDTH+ KEY_GAP);
                const int16_t ky = KP_Y + row * (KEY_HEIGHT + KEY_GAP);
                if (x >= kx && x < kx + KEY_WIDTH&& y >= ky && y < ky + KEY_HEIGHT) {
                    _appendDigit(digits[row][col]);
                    return;
                }
            }
        }

        // Bottom row: 0
        if (x >= KP_X && x < KP_X + BR0_W &&
            y >= BR_Y && y < BR_Y + KEY_HEIGHT) {
            _appendDigit(0);
            return;
        }
        // Backspace
        if (x >= KP_X + BR0_W + KEY_GAP &&
            x < KP_X + BR0_W + KEY_GAP + BRBK_W &&
            y >= BR_Y && y < BR_Y + KEY_HEIGHT) {
            _backspace();
            return;
        }
        // Confirm
        if (x >= KP_X + BR0_W + BRBK_W + 2*KEY_GAP &&
            y >= BR_Y && y < BR_Y + KEY_HEIGHT) {
            _confirm();
            return;
        }
    }

    void _appendDigit(int d) {
        if (_digitCount >= ENTRY_MAX_DIGITS - 1) return; // buffer full
        if (_digitCount == 0 && d == 0) return;           // suppress leading zero
        _digitBuf[_digitCount++] = '0' + (char)d;
        _digitBuf[_digitCount]   = '\0';
        _valueDirty = true;
    }

    void _backspace() {
        if (_digitCount > 0) {
            _digitBuf[--_digitCount] = '\0';
            _valueDirty = true;
        }
    }

    void _confirm() {
        int val = 0;
        if (_digitCount > 0) {
            val = atoi(_digitBuf);
            val = constrain(val, _minVal, _maxVal);
        } else {
            val = _currentVal;  // no entry → keep current
        }
        close();
        if (_callback) _callback(val);
    }

    // =========================================================================
    // ENUM LIST
    // =========================================================================
    void _drawEnumList() {
        const int listY = TB_H + 2;
        const int listH = EN_BTN_Y - listY - 2;

        // Background
        _display->fillRect(0, listY, SW, listH, gTheme.bg);

        // Visible rows
        for (int r = 0; r < EN_ROWS; ++r) {
            const int idx = _scrollOffset + r;
            if (idx >= _numEnumOptions) break;

            const int16_t ry = listY + r * EN_ROW_H;
            const bool    sel = (idx == _selectedEnum);

            // Row background
            _display->fillRect(0, ry, SW, EN_ROW_H - 1,
                               sel ? gTheme.selectedBg : gTheme.bg);

            // Option text
            if (_enumLabels[idx]) {
                _display->setTextSize(2);
                _display->setTextColor(sel ? gTheme.textOnSelect : gTheme.textNormal);
                _display->setCursor(10, ry + (EN_ROW_H - 14) / 2);
                _display->print(_enumLabels[idx]);
            }
        }
    }

    void _drawEnumButtons() {
        // Confirm button (right)
        _display->fillRect(180, EN_BTN_Y, 130, 30, gTheme.keyConfirm);
        _display->setTextSize(1);
        _display->setTextColor(gTheme.buttonText);
        _display->setCursor(212, EN_BTN_Y + 11);
        _display->print("Confirm");

        // Cancel button (left)
        _display->fillRect(10, EN_BTN_Y, 130, 30, gTheme.accent);
        _display->setCursor(42, EN_BTN_Y + 11);
        _display->print("Cancel");
    }

    void _handleEnumTouch(int16_t x, int16_t y) {
        // Confirm button
        if (x >= 180 && y >= EN_BTN_Y && y < EN_BTN_Y + 30) {
            const int confirmed = _selectedEnum;
            close();
            if (_callback) _callback(confirmed);
            return;
        }
        // Cancel button
        if (x < 140 && y >= EN_BTN_Y && y < EN_BTN_Y + 30) {
            close();
            return;
        }

        // List rows
        const int listY = TB_H + 2;
        for (int r = 0; r < EN_ROWS; ++r) {
            const int16_t ry = listY + r * EN_ROW_H;
            if (y >= ry && y < ry + EN_ROW_H) {
                const int idx = _scrollOffset + r;
                if (idx < _numEnumOptions && idx != _selectedEnum) {
                    _selectedEnum = idx;
                    _valueDirty   = true;  // trigger list redraw
                    _drawEnumList();
                }
                return;
            }
        }

        // Swipe (simple: tap top 1/3 → scroll up, bottom 1/3 → scroll down)
        // This is a simple fallback; gesture engine handles it if available.
    }

    void _scrollToSelection() {
        // Ensure the selected item is within the visible window
        if (_selectedEnum < _scrollOffset) {
            _scrollOffset = _selectedEnum;
        } else if (_selectedEnum >= _scrollOffset + EN_ROWS) {
            _scrollOffset = _selectedEnum - EN_ROWS + 1;
        }
        if (_scrollOffset < 0) _scrollOffset = 0;
    }

    // =========================================================================
    // Member data
    // =========================================================================
    ILI9341_t3n*  _display;
    Mode          _mode;

    // Numeric entry
    int           _minVal, _maxVal, _currentVal;
    char          _digitBuf[ENTRY_MAX_DIGITS];   // typed digits, null-terminated
    int           _digitCount;

    // Enum entry
    int           _selectedEnum;
    int           _numEnumOptions;
    const char*   _enumLabels[ENTRY_MAX_ENUM];
    int           _scrollOffset;

    // Shared
    char          _titleBuf[ENTRY_TITLE_LEN];
    char          _unitBuf[ENTRY_UNIT_LEN];
    Callback      _callback;

    // Redraw flags
    bool          _fullRedraw;
    bool          _valueDirty;
};
