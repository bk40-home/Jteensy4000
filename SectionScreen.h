// SectionScreen.h
// =============================================================================
// Section sub-screen — shows all parameters for one synth section.
//
// Screen layout (320 × 240 px):
//
//   ┌───────────────────────────────────────────────────────────────────┐  y=0
//   │  <  OSC 1                                         CPU: 34%        │  ← header 24 px
//   ├───────────────────────────────────────────────────────────────────┤  y=24
//   │  [Core] [Mix] [DC] [FB]                                           │  ← page tabs 18 px
//   ├───────────────────────────────────────────────────────────────────┤  y=42
//   │  OSC1 Wave                            SSAW   >                    │
//   │  ████████████░░░░░░░░░░░░░░  ← 3 px bar                         │  ← row 0  43 px
//   │  OSC1 Pitch                              0 st   >                 │
//   │  ████████░░░░░░░░░░░░░░░░░░                                      │  ← row 1
//   │  OSC1 Fine                             -12 ct   >                 │  ← row 2
//   │  OSC1 Det                               64      >                 │  ← row 3
//   ├───────────────────────────────────────────────────────────────────┤  y=214
//   │  L:<Back  R:Value  Long-R:Edit                                    │  ← footer 26 px
//   └───────────────────────────────────────────────────────────────────┘
//
// Navigation:
//   Tap page tab      → switch active page (rows rebuild inline)
//   Tap row           → open TFTNumericEntry overlay (numeric or enum)
//   Left enc delta    → move selected-row highlight (0-3)
//   Right enc delta   → adjust selected CC value ±1
//   Left enc short    → back to home (fires _onBack)
//   Right enc long    → open entry for selected row
//   Cancel in entry   → close entry, restore section screen
//
// Sync:
//   syncFromEngine() reads current CC values and updates rows — repaints only
//   rows where the value actually changed.
//
// Entry overlay:
//   TFTNumericEntry is embedded here (not on a push/pop stack) so it can
//   close cleanly and return focus to this screen without a full redraw.
//   While the entry is open, draw() delegates entirely to the entry.
//
// No singletons, no static instance pointers.
// The callbacks from TFTParamRow are wired via a thin context struct so the
// tap handler can call back into this screen without global state.
// =============================================================================

#pragma once
#include <Arduino.h>
#include "ILI9341_t3n.h"
#include "SynthEngine.h"
#include "UIPageLayout.h"
#include "JT4000_Sections.h"
#include "JT4000_Colours.h"
#include "TFTParamRow.h"
#include "TFTNumericEntry.h"
#include "Mapping.h"
#include "CCDefs.h"

using BackCallback = void (*)();

class SectionScreen {
public:
    // ---- Layout ----
    static constexpr int16_t SW        = 320;
    static constexpr int16_t SH        = 240;
    static constexpr int16_t HEADER_H  = 24;
    static constexpr int16_t TABS_H    = 18;
    static constexpr int16_t FOOTER_H  = 26;
    static constexpr int16_t PARAMS_Y  = HEADER_H + TABS_H;     // y=42
    static constexpr int16_t PARAMS_H  = SH - PARAMS_Y - FOOTER_H;  // 172 px
    // 4 rows; last row gets the remainder pixel
    static constexpr int16_t ROW_H     = PARAMS_H / 4;          // 43 px

    // =========================================================================
    // Constructor — wires row tap-callbacks via a Context pointer to *this
    // =========================================================================
    SectionScreen()
        : _display(nullptr)
        , _section(nullptr)
        , _synth(nullptr)
        , _activePage(0)
        , _selectedRow(0)
        , _onBack(nullptr)
        , _needsFullRedraw(true)
        , _needsTabRedraw(false)
        // Pre-build four rows at fixed Y positions; CC/name/colour updated in open()
        , _row0(0, PARAMS_Y + 0*ROW_H, SW, ROW_H, 255, "---", COLOUR_GLOBAL)
        , _row1(0, PARAMS_Y + 1*ROW_H, SW, ROW_H, 255, "---", COLOUR_GLOBAL)
        , _row2(0, PARAMS_Y + 2*ROW_H, SW, ROW_H, 255, "---", COLOUR_GLOBAL)
        , _row3(0, PARAMS_Y + 3*ROW_H, SW, ROW_H, 255, "---", COLOUR_GLOBAL)
    {
        _rows[0] = &_row0;
        _rows[1] = &_row1;
        _rows[2] = &_row2;
        _rows[3] = &_row3;

        // Row tap callbacks — use C++11 non-capturing lambdas are not possible
        // with member context.  We use a small per-screen static Context trick:
        // _ctx is set in begin(), lambdas capture _ctx by value via a static ptr.
        // SAFE because only one SectionScreen is ever active at a time.
        _ctx = nullptr;   // set in begin()
    }

    // =========================================================================
    // begin() — call once after construction
    // =========================================================================
    void begin(ILI9341_t3n* display) {
        _display = display;
        _entry.setDisplay(display);
        for (int i = 0; i < 4; ++i) _rows[i]->setDisplay(display);

        // Register this instance as the static context for row callbacks
        _ctx = this;

        // Wire tap callbacks — each row passes its CC to _onRowTap()
        _row0.setCallback([](uint8_t cc){ if (SectionScreen::_ctx) SectionScreen::_ctx->_onRowTap(cc); });
        _row1.setCallback([](uint8_t cc){ if (SectionScreen::_ctx) SectionScreen::_ctx->_onRowTap(cc); });
        _row2.setCallback([](uint8_t cc){ if (SectionScreen::_ctx) SectionScreen::_ctx->_onRowTap(cc); });
        _row3.setCallback([](uint8_t cc){ if (SectionScreen::_ctx) SectionScreen::_ctx->_onRowTap(cc); });
    }

    void setBackCallback(BackCallback cb) { _onBack = cb; }

    // =========================================================================
    // open() — activate this screen for a given section
    // Call whenever the home screen pushes into this section.
    // =========================================================================
    void open(const SectionDef& section, SynthEngine& synth) {
        _section      = &section;
        _synth        = &synth;
        _activePage   = 0;
        _selectedRow  = 0;
        _entry.close();       // discard any stale entry overlay
        _rebuildRows();       // assign CC / name / colour for page 0
        _needsFullRedraw = true;
        _needsTabRedraw  = false;
    }

    // =========================================================================
    // draw() — call every frame while this screen is visible
    // =========================================================================
    void draw() {
        if (!_display || !_section) return;

        // Entry overlay takes full screen — suppress normal drawing
        if (_entry.isOpen()) {
            _entry.draw();
            return;
        }

        if (_needsFullRedraw) {
            _display->fillScreen(COLOUR_BACKGROUND);
            _drawHeader();
            _drawTabs();
            for (int i = 0; i < 4; ++i) {
                _rows[i]->markDirty();
                _rows[i]->draw();
            }
            _drawFooter();
            _needsFullRedraw = false;
            _needsTabRedraw  = false;
            return;
        }

        if (_needsTabRedraw) {
            _drawTabs();
            _needsTabRedraw = false;
        }

        // Let rows repaint themselves only when their dirty flag is set
        for (int i = 0; i < 4; ++i) _rows[i]->draw();
    }

    // =========================================================================
    // syncFromEngine() — re-read all 4 CC values for the current page.
    // Only repaints rows where value or text changed.
    // =========================================================================
    void syncFromEngine() {
        if (!_section || !_synth) return;
        const int pageIdx = _section->pages[_activePage];
        if (pageIdx >= UIPage::NUM_PAGES) return;

        for (int r = 0; r < 4; ++r) {
            const uint8_t cc = UIPage::ccMap[pageIdx][r];
            if (cc == 255) {
                _rows[r]->setValue(0, nullptr);
                continue;
            }
            const uint8_t rawCC = _synth->getCC(cc);
            _rows[r]->setValue(rawCC, _enumText(cc));
        }
    }

    // =========================================================================
    // Touch routing
    // =========================================================================
    bool onTouch(int16_t x, int16_t y) {
        // Entry overlay consumes everything while open
        if (_entry.isOpen()) {
            _entry.onTouch(x, y);
            if (!_entry.isOpen()) {
                // Entry just closed — full screen repaint needed
                _needsFullRedraw = true;
            }
            return true;
        }

        // Header back-arrow region (< in leftmost 20 px)
        if (y < HEADER_H && x < 20) {
            if (_onBack) _onBack();
            return true;
        }

        // Tab bar
        if (y >= HEADER_H && y < HEADER_H + TABS_H) {
            _onTabTouch(x);
            return true;
        }

        // Param rows — also update selected row to the tapped one
        for (int i = 0; i < 4; ++i) {
            if (_rows[i]->onTouch(x, y)) {
                _setSelectedRow(i);
                return true;
            }
        }
        return false;
    }

    // =========================================================================
    // Encoder input — called from UIManager_TFT::pollInputs()
    // =========================================================================

    // Left encoder: scroll row selection
    void onEncoderLeft(int delta) {
        if (_entry.isOpen() || delta == 0) return;
        _setSelectedRow((_selectedRow + delta + 4) % 4);
    }

    // Right encoder: nudge selected CC value ±1
    void onEncoderRight(int delta) {
        if (_entry.isOpen() || delta == 0 || !_synth || !_section) return;
        const int     pageIdx = _section->pages[_activePage];
        if (pageIdx >= UIPage::NUM_PAGES) return;
        const uint8_t cc      = UIPage::ccMap[pageIdx][_selectedRow];
        if (cc == 255) return;
        const int     newVal  = constrain((int)_synth->getCC(cc) + delta, 0, 127);
        _synth->setCC(cc, (uint8_t)newVal);
        syncFromEngine();
    }

    // Left enc short press — back to home (or close entry if open)
    void onBackPress() {
        if (_entry.isOpen()) {
            _entry.close();
            _needsFullRedraw = true;
            return;
        }
        if (_onBack) _onBack();
    }

    // Right enc long press — open entry for selected row
    void onEditPress() {
        if (_entry.isOpen() || !_synth || !_section) return;
        const int     pageIdx = _section->pages[_activePage];
        if (pageIdx >= UIPage::NUM_PAGES) return;
        const uint8_t cc      = UIPage::ccMap[pageIdx][_selectedRow];
        if (cc == 255) return;
        _openEntry(cc);
    }

    bool isEntryOpen() const { return _entry.isOpen(); }

private:
    // ---- Static context pointer for row tap lambdas ----
    // Only ever one SectionScreen active — safe.
    static SectionScreen* _ctx;

    // =========================================================================
    // HEADER
    // =========================================================================
    void _drawHeader() {
        if (!_section) return;
        _display->fillRect(0, 0, SW, HEADER_H, COLOUR_HEADER_BG);
        // Bottom border in section colour
        _display->drawFastHLine(0, HEADER_H - 1, SW, _section->colour);

        // Back arrow
        _display->setTextSize(1);
        _display->setTextColor(COLOUR_TEXT_DIM);
        _display->setCursor(4, 8);
        _display->print("<");

        // Section name
        _display->setTextColor(_section->colour);
        _display->setCursor(14, 8);
        _display->print(_section->label);

        // CPU right-aligned
        char buf[14];
        snprintf(buf, sizeof(buf), "CPU:%d%%", (int)AudioProcessorUsageMax());
        const int16_t bw = (int16_t)(strlen(buf) * 6);
        _display->setTextColor(COLOUR_TEXT_DIM);
        _display->setCursor(SW - bw - 4, 8);
        _display->print(buf);
    }

    // =========================================================================
    // PAGE TABS
    // =========================================================================
    void _drawTabs() {
        if (!_section) return;
        // Clear tab strip
        _display->fillRect(0, HEADER_H, SW, TABS_H, 0x0821);

        const int     tabCount = _section->pageCount;
        if (tabCount == 0) return;
        const int16_t tabW = SW / tabCount;

        for (int t = 0; t < tabCount; ++t) {
            const int pageIdx = _section->pages[t];
            if (pageIdx >= UIPage::NUM_PAGES) continue;

            const int16_t tx     = t * tabW;
            const bool    active = (t == _activePage);
            const uint16_t bg    = active ? _section->colour : COLOUR_HEADER_BG;

            _display->fillRect(tx, HEADER_H, tabW - 1, TABS_H, bg);

            // First word of page title (≤7 chars)
            const char* title = UIPage::pageTitle[pageIdx];
            char shortTitle[8];
            int  c = 0;
            while (title[c] && title[c] != ' ' && c < 7) {
                shortTitle[c] = title[c];
                c++;
            }
            shortTitle[c] = '\0';

            _display->setTextSize(1);
            _display->setTextColor(active ? COLOUR_BACKGROUND : COLOUR_TEXT_DIM);
            const int16_t tw = (int16_t)(strlen(shortTitle) * 6);
            _display->setCursor(tx + (tabW - 1 - tw) / 2, HEADER_H + 5);
            _display->print(shortTitle);
        }

        // Separator under tabs
        _display->drawFastHLine(0, HEADER_H + TABS_H - 1, SW, _section->colour);
    }

    // =========================================================================
    // FOOTER
    // =========================================================================
    void _drawFooter() {
        const int16_t fy = SH - FOOTER_H;
        _display->fillRect(0, fy, SW, FOOTER_H, COLOUR_HEADER_BG);
        _display->drawFastHLine(0, fy, SW, COLOUR_BORDER);
        _display->setTextSize(1);
        _display->setTextColor(COLOUR_TEXT_DIM);
        _display->setCursor(4, fy + 9);
        _display->print("L:<Back  R:Adjust  Hold-R:Edit");
    }

    // =========================================================================
    // ROW REBUILD — call after page change or section open
    // Reconfigures each TFTParamRow for the new page's CCs without allocating.
    // =========================================================================
    void _rebuildRows() {
        if (!_section) return;
        const int pageIdx = _section->pages[_activePage];
        if (pageIdx >= UIPage::NUM_PAGES) return;

        for (int r = 0; r < 4; ++r) {
            const uint8_t  cc     = UIPage::ccMap[pageIdx][r];
            const char*    name   = UIPage::ccNames[pageIdx][r];
            const uint16_t colour = _ccColour(cc);

            // Reinitialise the row widget in-place via helper
            _configRow(*_rows[r], cc, name ? name : "---", colour);
        }

        // Apply selection highlight
        for (int i = 0; i < 4; ++i) _rows[i]->setSelected(i == _selectedRow);

        syncFromEngine();
    }

static void _configRow(TFTParamRow& row,
                       uint8_t cc,
                       const char* name,
                       uint16_t colour)
{
    row.configure(cc, name, colour);
}

    // =========================================================================
    // TAB TOUCH
    // =========================================================================
    void _onTabTouch(int16_t x) {
        if (!_section) return;
        const int tabCount = _section->pageCount;
        const int tabW     = SW / tabCount;
        const int tapped   = x / tabW;
        if (tapped < 0 || tapped >= tabCount) return;
        if (tapped == _activePage) return;

        _activePage     = tapped;
        _needsTabRedraw = true;
        _rebuildRows();
        _needsFullRedraw = true;   // rows changed — need full repaint
    }

    // =========================================================================
    // ROW SELECTION
    // =========================================================================
    void _setSelectedRow(int row) {
        if (row == _selectedRow) return;
        const int prev = _selectedRow;
        _selectedRow   = row;
        _rows[prev]->setSelected(false);
        _rows[row]->setSelected(true);
    }

    // =========================================================================
    // ROW TAP → open entry overlay
    // =========================================================================
    void _onRowTap(uint8_t cc) {
        if (cc == 255 || !_synth) return;
        _openEntry(cc);
    }

    // =========================================================================
    // OPEN ENTRY OVERLAY
    // =========================================================================
    void _openEntry(uint8_t cc) {
        if (!_synth) return;

        const char* name = _ccName(cc);

        if (_isEnumCC(cc)) {
            _openEnumEntry(cc, name);
        } else {
            _openNumericEntry(cc, name);
        }
    }

    // ---- Enum entry ----
    void _openEnumEntry(uint8_t cc, const char* title) {
        static const char* kWave[]   = { "SINE","TRI","SQR","SAW","RSAW","SSAW","ARB" };
        static const char* kLFOwave[]= { "SINE","TRI","SQR","SAW" };
        static const char* kLFOdest[]= { "PITCH","FILTER","SHAPE","AMP" };
        static const char* kSync[]   = { "Free","4bar","2bar","1bar",
                                         "1/2","1/4","1/8","1/16",
                                         "1/4T","1/8T","1/16T","1/32T" };
        static const char* kClkSrc[] = { "Internal","External" };
        static const char* kOnOff[]  = { "Off","On" };
        static const char* kBypass[] = { "Active","Bypass" };

        const char* const* opts  = kOnOff;
        int                count = 2;

        switch (cc) {
            case CC::OSC1_WAVE:
            case CC::OSC2_WAVE:          opts = kWave;    count = 7;  break;
            case CC::LFO1_WAVEFORM:
            case CC::LFO2_WAVEFORM:      opts = kLFOwave; count = 4;  break;
            case CC::LFO1_DESTINATION:
            case CC::LFO2_DESTINATION:   opts = kLFOdest; count = 4;  break;
            case CC::LFO1_TIMING_MODE:
            case CC::LFO2_TIMING_MODE:
            case CC::DELAY_TIMING_MODE:  opts = kSync;    count = 12; break;
            case CC::BPM_CLOCK_SOURCE:   opts = kClkSrc;  count = 2;  break;
            case CC::FX_REVERB_BYPASS:   opts = kBypass;  count = 2;  break;
            default:                     opts = kOnOff;   count = 2;  break;
        }

        // Current index = scale CC into 0..(count-1) bucket
        const int curIdx = (int)_synth->getCC(cc) * count / 128;

        // Store CC for the confirm callback
        _pendingCC    = cc;
        _pendingCount = count;

        _entry.openEnum(title, opts, count, constrain(curIdx, 0, count - 1),
            [](int idx) {
                if (!SectionScreen::_ctx || !SectionScreen::_ctx->_synth) return;
                SectionScreen* s = SectionScreen::_ctx;
                // Map selected index back to CC midpoint
                const uint8_t ccVal = (uint8_t)
                    ((idx * 128 + (128 / s->_pendingCount) / 2) / s->_pendingCount);
                s->_synth->setCC(s->_pendingCC, ccVal);
                s->syncFromEngine();
            }
        );
    }

    // ---- Numeric entry ----
    void _openNumericEntry(uint8_t cc, const char* title) {
        using namespace JT4000Map;

        // Defaults — raw 0-127
        const char* unit = "";
        int minV = 0, maxV = 127, curV = (int)_synth->getCC(cc);

        switch (cc) {
            case CC::FILTER_CUTOFF:
                unit = "Hz";  minV = 20;   maxV = 18000;
                curV = (int)_synth->getFilterCutoff();
                break;
            case CC::FILTER_RESONANCE:
                unit = "";    minV = 0;    maxV = 127;
                break;
            case CC::AMP_ATTACK:    case CC::AMP_DECAY:    case CC::AMP_RELEASE:
            case CC::FILTER_ENV_ATTACK: case CC::FILTER_ENV_DECAY: case CC::FILTER_ENV_RELEASE:
                unit = "ms";  minV = 1;    maxV = 11880;
                curV = (int)cc_to_time_ms(_synth->getCC(cc));
                break;
            case CC::AMP_SUSTAIN:   case CC::FILTER_ENV_SUSTAIN:
                unit = "%";   minV = 0;    maxV = 100;
                curV = (int)(_synth->getCC(cc) * 100 / 127);
                break;
            case CC::LFO1_FREQ:     case CC::LFO2_FREQ:
                unit = "Hz";  minV = 0;    maxV = 39;
                curV = (int)cc_to_lfo_hz(_synth->getCC(cc));
                break;
            case CC::FX_BASS_GAIN:  case CC::FX_TREBLE_GAIN:
                unit = "dB";  minV = -12;  maxV = 12;
                curV = (int)((_synth->getCC(cc) / 127.0f) * 24.0f - 12.0f);
                break;
            case CC::BPM_INTERNAL_TEMPO:
                unit = "BPM"; minV = 20;   maxV = 300;
                curV = (int)(20.0f + (_synth->getCC(cc) / 127.0f) * 280.0f);
                break;
            case CC::OSC1_PITCH_OFFSET: case CC::OSC2_PITCH_OFFSET:
                unit = "st";  minV = -24;  maxV = 24;
                curV = (int)(_synth->getCC(cc) * 48 / 127 - 24);
                break;
            case CC::OSC1_FINE_TUNE:    case CC::OSC2_FINE_TUNE:
                unit = "ct";  minV = -100; maxV = 100;
                curV = (int)(_synth->getCC(cc) * 200 / 127 - 100);
                break;
            default:
                break;
        }

        _pendingCC = cc;

        _entry.openNumeric(title, unit, minV, maxV, curV,
            [](int humanVal) {
                if (!SectionScreen::_ctx || !SectionScreen::_ctx->_synth) return;
                SectionScreen* s   = SectionScreen::_ctx;
                const uint8_t  tcc = s->_pendingCC;
                uint8_t        ccVal = 0;
                using namespace JT4000Map;

                switch (tcc) {
                    case CC::FILTER_CUTOFF:
                        ccVal = obxa_cutoff_hz_to_cc((float)humanVal);  break;
                    case CC::AMP_ATTACK:    case CC::AMP_DECAY:    case CC::AMP_RELEASE:
                    case CC::FILTER_ENV_ATTACK: case CC::FILTER_ENV_DECAY: case CC::FILTER_ENV_RELEASE:
                        ccVal = time_ms_to_cc((float)humanVal);          break;
                    case CC::AMP_SUSTAIN:   case CC::FILTER_ENV_SUSTAIN:
                        ccVal = (uint8_t)(humanVal * 127 / 100);         break;
                    case CC::LFO1_FREQ:     case CC::LFO2_FREQ:
                        ccVal = lfo_hz_to_cc((float)humanVal);           break;
                    case CC::FX_BASS_GAIN:  case CC::FX_TREBLE_GAIN:
                        ccVal = (uint8_t)((humanVal + 12) * 127 / 24);  break;
                    case CC::BPM_INTERNAL_TEMPO:
                        ccVal = (uint8_t)((humanVal - 20) * 127 / 280); break;
                    case CC::OSC1_PITCH_OFFSET: case CC::OSC2_PITCH_OFFSET:
                        ccVal = (uint8_t)((humanVal + 24) * 127 / 48);  break;
                    case CC::OSC1_FINE_TUNE:    case CC::OSC2_FINE_TUNE:
                        ccVal = (uint8_t)((humanVal + 100) * 127 / 200); break;
                    default:
                        ccVal = (uint8_t)constrain(humanVal, 0, 127);   break;
                }
                s->_synth->setCC(tcc, ccVal);
                s->syncFromEngine();
            }
        );
    }

    // =========================================================================
    // HELPERS
    // =========================================================================

    // Returns an enum display string for known enum CCs, nullptr for numeric CCs
    const char* _enumText(uint8_t cc) const {
        if (!_synth) return nullptr;
        switch (cc) {
            case CC::OSC1_WAVE:          return _synth->getOsc1WaveformName();
            case CC::OSC2_WAVE:          return _synth->getOsc2WaveformName();
            case CC::LFO1_WAVEFORM:      return _synth->getLFO1WaveformName();
            case CC::LFO2_WAVEFORM:      return _synth->getLFO2WaveformName();
            case CC::LFO1_DESTINATION:   return _synth->getLFO1DestinationName();
            case CC::LFO2_DESTINATION:   return _synth->getLFO2DestinationName();
            case CC::GLIDE_ENABLE:       return _synth->getGlideEnabled() ? "On" : "Off";
            case CC::FX_REVERB_BYPASS:   return _synth->getFXReverbBypass() ? "Bypass" : "Active";
            case CC::FILTER_OBXA_TWO_POLE: return _synth->getFilterTwoPole() ? "On" : "Off";
            default:                     return nullptr;
        }
    }

    // Look up the display name for a CC from the current page
    const char* _ccName(uint8_t cc) const {
        if (!_section) return "?";
        const int pageIdx = _section->pages[_activePage];
        if (pageIdx >= UIPage::NUM_PAGES) return "?";
        for (int r = 0; r < 4; ++r) {
            if (UIPage::ccMap[pageIdx][r] == cc) return UIPage::ccNames[pageIdx][r];
        }
        return "?";
    }

    // Returns true if this CC should use the list picker instead of the keypad
    static bool _isEnumCC(uint8_t cc) {
        return (cc == CC::OSC1_WAVE         || cc == CC::OSC2_WAVE          ||
                cc == CC::LFO1_WAVEFORM     || cc == CC::LFO2_WAVEFORM      ||
                cc == CC::LFO1_DESTINATION  || cc == CC::LFO2_DESTINATION   ||
                cc == CC::LFO1_TIMING_MODE  || cc == CC::LFO2_TIMING_MODE   ||
                cc == CC::DELAY_TIMING_MODE || cc == CC::BPM_CLOCK_SOURCE    ||
                cc == CC::GLIDE_ENABLE      || cc == CC::FX_REVERB_BYPASS    ||
                cc == CC::FILTER_OBXA_TWO_POLE ||
                cc == CC::FILTER_OBXA_BP_BLEND_2_POLE ||
                cc == CC::FILTER_OBXA_PUSH_2_POLE      ||
                cc == CC::FILTER_OBXA_XPANDER_4_POLE);
    }

    // Maps a CC to its section category colour
    static uint16_t _ccColour(uint8_t cc) {
        if (cc == 255) return COLOUR_GLOBAL;
        if (cc >= CC::OSC1_WAVE        && cc <= CC::OSC2_FEEDBACK_MIX)           return COLOUR_OSC;
        if (cc >= CC::FILTER_CUTOFF    && cc <= CC::FILTER_OBXA_RES_MOD_DEPTH)   return COLOUR_FILTER;
        if (cc >= CC::AMP_ATTACK       && cc <= CC::FILTER_ENV_RELEASE)          return COLOUR_ENV;
        if (cc >= CC::LFO1_FREQ        && cc <= CC::LFO2_TIMING_MODE)            return COLOUR_LFO;
        if (cc >= CC::FX_BASS_GAIN     && cc <= CC::FX_REVERB_BYPASS)            return COLOUR_FX;
        return COLOUR_GLOBAL;
    }

    // ---- Members ----
    ILI9341_t3n*       _display;
    const SectionDef*  _section;
    SynthEngine*       _synth;
    int                _activePage;
    int                _selectedRow;
    BackCallback       _onBack;
    bool               _needsFullRedraw;
    bool               _needsTabRedraw;

    // Pending entry CC/count stored for static lambda access via _ctx
    uint8_t  _pendingCC    = 255;
    int      _pendingCount = 2;

    TFTNumericEntry _entry;    // embedded entry overlay

    TFTParamRow _row0, _row1, _row2, _row3;
    TFTParamRow* _rows[4];

    // Allow _configRow to access private members of TFTParamRow
    // (or we add a public reconfigure() to TFTParamRow)
    friend void _configRowFriend(TFTParamRow&, uint8_t, const char*, uint16_t);
};

// Static context pointer — one SectionScreen active at a time
SectionScreen* SectionScreen::_ctx = nullptr;
