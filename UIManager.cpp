#include "WireIMXRT.h"      // ✅ requested include goes first

// UIManager.cpp
// -----------------------------------------------------------------------------
// UI reflects engine state by converting internal units back to 0..127,
// using the exact inverse curves in Mapping.h so values are consistent.
// -----------------------------------------------------------------------------
// IMPORTANT: Stay aligned with Teensy Audio & JT-4000 scope; no feature creep.
// -----------------------------------------------------------------------------

#include "UIManager.h"
#include "Mapping.h"
#include "CCMap.h"
#include "HardwareInterface.h"
#include "Presets.h"         // ✅ ensure this stays included
#include "AudioScopeTap.h"
#include "Waveforms.h"  // for future text lookups if needed (optional)

extern AudioScopeTap scopeTap;  // provided by Jteensy4000.ino

// NEW: global preset index across ALL presets (templates + bank).
// We keep this at file scope to avoid touching class members.
static int gPresetIdx = 0;

// Utility: wrap an index into [0..max-1]
static inline int wrapIndex(int i, int max) {
  if (max <= 0) return 0;
  while (i < 0)   i += max;
  while (i >= max) i -= max;
  return i;
}


// Convert a CC to a text label for display (or return nullptr if none)
const char* UIManager::ccToDisplayText(byte cc, SynthEngine& synth) {
    switch (cc) {
        // --- OSC1/OSC2 waveform names ---
        case 21: return synth.getOsc1WaveformName();
        case 22: return synth.getOsc2WaveformName();

        // LFO waveforms
        case 62: return synth.getLFO1WaveformName();
        case 63: return synth.getLFO2WaveformName();

        // LFO destinations
        case 56: return synth.getLFO1DestinationName();
        case 53: return synth.getLFO2DestinationName();

        default: return nullptr; // fall back to numeric via ccToDisplayValue()
    }
}


void UIManager::pollInputs(HardwareInterface& hw, SynthEngine& synth) {

    // Optional: define (or replace with your CC names if you have them in CCMap.h)
    static constexpr uint8_t CC_FILTER_CUTOFF    = 23;
    static constexpr uint8_t CC_FILTER_RESONANCE = 24;

    // -------- Button: toggle scope on/off --------
    if (hw.isButtonPressed()) {
        _scopeOn = !_scopeOn;

        // If we just LEFT scope (entered EDIT), refresh page labels/values
        if (!_scopeOn) {
            // Ensure page labels are up-to-date for the current page
            for (int i = 0; i < 4; ++i) {
                setParameterLabel(i, ccNames[_currentPage][i]);
            }
            // Pull live values from the engine so the page shows the truth
            syncFromEngine(synth);
        }
    }

    // (You had this here already)
    AudioProcessorUsageMaxReset();

    const bool scopeVisible = _scopeOn;

    // -------- Encoder behavior --------
    int delta = hw.getEncoderDelta();

    if (scopeVisible) {
        // ===================== SCOPE MODE ======================
        // Encoder cycles through *all* presets (templates + bank).
        if (delta != 0) {
            const int total = Presets::presets_totalCount();           // templates + 32 bank
            gPresetIdx = wrapIndex(gPresetIdx + (delta > 0 ? 1 : -1), total);

            // Load immediately while scoping (original behavior: immediate apply)
            Presets::presets_loadByGlobalIndex(synth, gPresetIdx, /*midiCh=*/1);

            // Keep UI values in sync so when you exit scope, page shows latest
            syncFromEngine(synth);

            // Mark footer stats dirty and reset CPU peak for the new patch
            AudioProcessorUsageMaxReset();
            markStatsDirty();
        }

        // --- While in SCOPE mode, map Pot0/1 exactly like Filter page ---
        // Cutoff + Resonance via the same CCs your engine expects.
        // Engine will apply Mapping.h curves (log cutoff, etc.).
        if (hw.potChanged(0, 1)) {
            int ccVal = (hw.readPot(0) >> 3);
            synth.handleControlChange(1, CC_FILTER_CUTOFF, (uint8_t)ccVal);
            _lastCCSent[CC_FILTER_CUTOFF] = (uint8_t)ccVal;
            _hasCCSent[CC_FILTER_CUTOFF]  = true;
            setParameterValue(0, ccVal);
            _valueText[0] = nullptr;                    // 🔧 force numeric draw
        }
        if (hw.potChanged(1, 1)) {
            int ccVal = (hw.readPot(1) >> 3);
            synth.handleControlChange(1, CC_FILTER_RESONANCE, (uint8_t)ccVal);
            _lastCCSent[CC_FILTER_RESONANCE] = (uint8_t)ccVal;
            _hasCCSent[CC_FILTER_RESONANCE]  = true;
            setParameterValue(1, ccVal);
            _valueText[1] = nullptr;                    // 🔧 force numeric draw
        }


        // (Leave pots 2/3 ignored in scope mode for now; easy to add later)
        return; // exit: in scope mode we don't do page nav / page-mapped pots
    }
    else {
        // ====================== EDIT MODE =======================
        // Encoder navigates pages (unchanged behavior).
        if (delta > 0) {
            int newPage = (_currentPage + 1) % NUM_PAGES;
            setPage(newPage);
            for (int i = 0; i < 4; ++i) setParameterLabel(i, ccNames[newPage][i]);
            syncFromEngine(synth);
        } else if (delta < 0) {
            int newPage = (_currentPage == 0) ? NUM_PAGES - 1 : _currentPage - 1;
            setPage(newPage);
            for (int i = 0; i < 4; ++i) setParameterLabel(i, ccNames[newPage][i]);
            syncFromEngine(synth);
        }
    }

    // -------- Pots: only in EDIT mode (page-mapped) --------
    if (!scopeVisible) {
        for (int i = 0; i < 4; ++i) {
            if (hw.potChanged(i, 1)) {                 // CC-step hysteresis
            int ccVal = (hw.readPot(i) >> 3);      // smoothed 0..127
            byte cc = ccMap[_currentPage][i];

            synth.handleControlChange(1, cc, (uint8_t)ccVal);

            _lastCCSent[cc] = (uint8_t)ccVal;
            _hasCCSent[cc]  = true;

            // Update numeric cache (fallback)
            setParameterValue(i, ccVal);

            // 🔑 Update text cache NOW so renderPage() shows the right thing immediately
            _valueText[i] = ccToDisplayText(cc, synth);   // nullptr for numeric params
        }
        }
    }
}


UIManager::UIManager() : _display(128, 64, &Wire2, -1), _currentPage(0), _highlightIndex(0) {
    for (int i = 0; i < 4; ++i) {
        _labels[i] = "";
        _values[i] = 0;
        _valueText[i] = nullptr;  // <- init
    }
}

void UIManager::begin() {
    Wire2.begin();
    _display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);
    _display.display();

    // 🆕 Start the scope recorder but don't block if not patched yet
    for (int i = 0; i < SCOPE_RING; ++i) _scopeRing[i] = 0;
    _scopeWrite = 0;
    _scopeQueue.begin();

    // --- initialize first page labels and real values ---
    for (int i = 0; i < 4; ++i) {
        setParameterLabel(i, ccNames[_currentPage][i]);
    }
    _statsNextMs = 0;
    _statsDirty  = true;
    _cpuNowDisp  = AudioProcessorUsage();
    _blkNowDisp  = AudioMemoryUsage();

    // NEW: initialize selection to first bank item if you prefer:
    // gPresetIdx = presets_templateCount(); // uncomment to start at bank
}

void UIManager::setPage(int page) { _currentPage = page; }
int  UIManager::getCurrentPage() const { return _currentPage; }
void UIManager::highlightParameter(int index) { _highlightIndex = index; }

void UIManager::setParameterLabel(int index, const char* label) {
    if (index >= 0 && index < 4) _labels[index] = label;
}

void UIManager::setParameterValue(int index, int value) {
    if (index >= 0 && index < 4) _values[index] = value;
}

int UIManager::getParameterValue(int index) const {
    return (index >= 0 && index < 4) ? _values[index] : 0;
}

// --- Decide which screen to show ---------------------------------------------
void UIManager::updateDisplay(SynthEngine& synth) {
    if (_scopeOn) renderScope();
    else          renderPage();
}


// --- Existing page render extracted into a helper (no behavior change) -------
void UIManager::renderPage() {
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);

    const int rowHeight = 16;
    const int labelX = 0;

    for (int i = 0; i < 4; ++i) {
        int y = i * rowHeight;

        // Left: label
        _display.setCursor(labelX, y);
        _display.print(_labels[i]);

        // Right: prefer text (e.g., "SSAW"); else numeric (0..127)
        if (_valueText[i]) {
            drawRightAligned(String(_valueText[i]), y);
        } else {
            char valueBuf[12];
            snprintf(valueBuf, sizeof(valueBuf), "%3d", _values[i]);
            drawRightAligned(valueBuf, y);
        }
    }
    _display.display();
}


// --- Utility: right align text on 128px width --------------------------------
void UIManager::drawRightAligned(const String& text, int y) {
    int16_t x1, y1; uint16_t w, h;
    _display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    _display.setCursor(128 - w, y);
    _display.print(text);
}

// --- Scope renderer: draw waveform from AudioRecordQueue ---------------------
void UIManager::renderScope() {
    // ---------------------------------------------------------------------
    // Layout: 128x64 OLED
    //  - Top header (8 px):  P:<num> <name>
    //  - Wave area (48 px):  y = 8 .. 55
    //  - Bottom footer (8 px): CPU (current) + Audio blocks in use (current)
    // ---------------------------------------------------------------------
    const int TOP_BAR = 8;
    const int BOT_BAR = 8;
    const int DRAW_TOP = TOP_BAR;
    const int DRAW_BOT = 63 - BOT_BAR;

    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);

    // --- Header: preset number + name (GLOBAL index + global name) ---
    {
        _display.setCursor(0, 0);
        _display.print("P:");
        _display.print(gPresetIdx);   // show global index

        _display.print(" ");
        const char* fullName = Presets::presets_nameByGlobalIndex(gPresetIdx);
        char nameBuf[22];
        size_t room = sizeof(nameBuf) - 1;
        strncpy(nameBuf, fullName ? fullName : "", room);
        nameBuf[room] = '\0';
        _display.print(nameBuf);
    }

    // --- Pull a recent window from the always-on scope tap ---
    static int16_t snap[512];
    const uint16_t nAvail = scopeTap.snapshot(snap, (uint16_t)512);
    if (nAvail < 128) {
        _display.setCursor(0, DRAW_TOP + 8);
        _display.print("SCOPE (arming)");

        const uint32_t now = millis();
        // refresh if time elapsed or an event happened
        if (_statsDirty || now >= _statsNextMs) {
            _cpuNowDisp = AudioProcessorUsage();  // current CPU %
            _blkNowDisp = AudioMemoryUsage();     // current blocks in use
            _statsDirty = false;
            _statsNextMs = now + 250;             // throttle to every 250 ms
        }

        _display.fillRect(0, 56, 128, 8, SSD1306_BLACK);
        _display.setCursor(0, 56);
        _display.print("CPU ");
        _display.print(_cpuNowDisp, 1);
        _display.print("%  Blk ");
        _display.print(_blkNowDisp);
        _display.display();
        return;
    }

    // Helper: newest sample by offset (0=newest, k=older)
    auto newest = [&](int k)->int16_t {
        if (k < 0) k = 0;
        if (k >= (int)nAvail) k = (int)nAvail - 1;
        return snap[nAvail - 1 - k];
    };

    // --- Auto timebase via zero-crossings near tail ---
    const int Nscan = (nAvail < 3000) ? nAvail : 3000;
    int anchor = -1;                  // last -to+ crossing
    int16_t prev = newest(1);
    for (int i = 2; i < Nscan; ++i) {
        int16_t cur = newest(i);
        if (prev < 0 && cur >= 0) { anchor = i; break; }
        prev = cur;
    }
    int period = 256; // fallback
    if (anchor > 0) {
        int16_t pprev = newest(anchor + 1);
        for (int i = anchor + 1; i < Nscan; ++i) {
            int16_t c = newest(i);
            if (pprev < 0 && c >= 0) { period = i - anchor; break; }
            pprev = c;
        }
    }

    // Window ≈ 1.5 cycles, clamp [128..nAvail]
    int win = period + period / 2;
    if (win < 128) win = 128;
    if (win > (int)nAvail) win = (int)nAvail;
    const int startIdx = (int)nAvail - win;

    // --- Downsample window to 128 columns with a small box filter ---
    int16_t col[128];
    const int box = (win / 128) < 1 ? 1 : (win / 128);
    const int stepQ16 = (win << 16) / 128;
    int32_t posQ16 = 0;

    for (int x = 0; x < 128; ++x) {
        int base = startIdx + (posQ16 >> 16);
        int acc = 0, cnt = 0;
        for (int k = 0; k < box; ++k) {
            int idx = base + k;
            if (idx < 0) idx = 0;
            if (idx >= (int)nAvail) idx = (int)nAvail - 1;
            acc += snap[idx];
            cnt++;
        }
        col[x] = (int16_t)(acc / (cnt ? cnt : 1));
        posQ16 += stepQ16;
    }

    // --- Vertical auto-gain (same thresholds as before) ---
    int16_t peak = 0;
    for (int x = 0; x < 128; ++x) {
        int16_t a = col[x] < 0 ? -col[x] : col[x];
        if (a > peak) peak = a;
    }
    int shift = 10;
    if      (peak < 1024) shift = 6;
    else if (peak < 2048) shift = 7;
    else if (peak < 4096) shift = 8;
    else if (peak < 8192) shift = 9;
    if (shift < 6)  shift = 6;
    if (shift > 11) shift = 11;

    // --- Draw waveform in band (8..55) ---
    const int midY = (DRAW_TOP + DRAW_BOT) / 2;
    int prevX = 0, prevY = midY;
    for (int x = 0; x < 128; ++x) {
        int y = midY - (col[x] >> shift);
        if (y < DRAW_TOP) y = DRAW_TOP;
        if (y > DRAW_BOT) y = DRAW_BOT;
        if (x > 0) _display.drawLine(prevX, prevY, x, y, SSD1306_WHITE);
        prevX = x; prevY = y;
    }

    // --- Footer: CPU(current) + Audio blocks used (current) ---
    _display.fillRect(0, 56, 128, 8, SSD1306_BLACK);
    _display.setCursor(0, 56);
    _display.print("CPU ");
    _display.print(AudioProcessorUsage(), 1);
    _display.print("%  Blk ");
    _display.print(AudioMemoryUsage());

    _display.display();
}


// Sync the 4 on-screen values from the *engine* using the inverse mapping.
// This ensures Edit mode shows the true current values even if they came
// from presets, MIDI, or other non-UI changes.
void UIManager::syncFromEngine(SynthEngine& synth) {
    for (int i = 0; i < 4; ++i) {
        const byte cc = ccMap[_currentPage][i];
        if (cc == 255) {
            _valueText[i] = nullptr;
            setParameterValue(i, 0);
            continue;
        }

        // Always compute numeric (0..127) for fallback and CC cache
        const int val = ccToDisplayValue(cc, synth);
        setParameterValue(i, val);

        // Prefer a text label for discrete enums (e.g., waveform names)
        const char* t = ccToDisplayText(cc, synth);
        _valueText[i] = t;   // nullptr if none

        // Keep CC cache aligned so pot moves don’t jump
        _lastCCSent[cc] = (uint8_t)val;
        _hasCCSent[cc]  = true;
    }
}


AudioRecordQueue& UIManager::scopeIn() {
  return _scopeQueue;
}

// --- Converts internal SynthEngine values to display-friendly 0–127 CC values
int UIManager::ccToDisplayValue(byte cc, SynthEngine& synth) {
    using namespace JT4000Map;

    switch (cc) {

        // -------------------- OSC waveforms -----------------------
        // -------------------- OSC waveforms -----------------------
        case 21: {
            // Return the *bin-centered CC* for the current waveform so values are 0..127 aligned
            const int w = synth.getOsc1Waveform();           // Teensy ID or Supersaw(100)
            return ccFromWaveform((WaveformType)w);
        }
        case 22: {
            const int w = synth.getOsc2Waveform();
            return ccFromWaveform((WaveformType)w);
        }

        // -------------------- Filter main -------------------------
        case 23: { // Cutoff Hz -> CC via log inverse
            float hz = synth.getFilterCutoff();
            return cutoff_hz_to_cc(hz);
        }
        case 24: { // Resonance 0..1 -> CC
            float r = synth.getFilterResonance();
            return resonance_to_cc(r);
        }

        // -------------------- Amp Env (ms, norm) ------------------
        case 25: return time_ms_to_cc(synth.getAmpAttack());   // ms -> CC
        case 26: return time_ms_to_cc(synth.getAmpDecay());
        case 27: return norm_to_cc_lin(synth.getAmpSustain());
        case 28: return time_ms_to_cc(synth.getAmpRelease());

        // -------------------- OSC pitch/detune/fine ---------------
        case 41: { // -24, -12, 0, +12, +24 mapped by UI as 0..127 buckets
            float semis = synth.getOsc1PitchOffset();
            int bucket = 64; // default ~0
            if      (semis <= -18.0f) bucket =  12;
            else if (semis <=  -6.0f) bucket =  38;
            else if (semis <=   6.0f) bucket =  64;
            else if (semis <=  18.0f) bucket =  90;
            else                       bucket = 116;
            return bucket;
        }
        case 42: {
            float semis = synth.getOsc2PitchOffset();
            int bucket = 64;
            if      (semis <= -18.0f) bucket =  12;
            else if (semis <=  -6.0f) bucket =  38;
            else if (semis <=   6.0f) bucket =  64;
            else if (semis <=  18.0f) bucket =  90;
            else                       bucket = 116;
            return bucket;
        }
        case 43: return norm_to_cc_lin((synth.getOsc1Detune() + 1.0f) * 0.5f);     // -1..1 -> 0..1
        case 44: return norm_to_cc_lin((synth.getOsc2Detune() + 1.0f) * 0.5f);
        case 45: return (uint8_t)constrain(lroundf((synth.getOsc1FineTune() + 100.0f) * 127.0f / 200.0f), 0, 127);
        case 46: return (uint8_t)constrain(lroundf((synth.getOsc2FineTune() + 100.0f) * 127.0f / 200.0f), 0, 127);

        // -------------------- Mixes (per CCMap.h) -----------------
        // CC 47 is "Osc1 Mix" in ccNames -> show osc1 level (engine sets inverse pair on this CC)
        case 47: return norm_to_cc_lin(synth.getOscMix1());
        // Dedicated mixer taps:
        case 60: return norm_to_cc_lin(synth.getOscMix1());
        case 61: return norm_to_cc_lin(synth.getOscMix2());
        case 58: return norm_to_cc_lin(synth.getSubMix());
        case 59: return norm_to_cc_lin(synth.getNoiseMix());

        // -------------------- Filter Env (per CCMap.h) ------------
        case 29: return time_ms_to_cc(synth.getFilterEnvAttack());
        case 30: return time_ms_to_cc(synth.getFilterEnvDecay());
        case 31: return norm_to_cc_lin(synth.getFilterEnvSustain());
        case 32: return time_ms_to_cc(synth.getFilterEnvRelease());
        case 48: return norm_to_cc_lin((synth.getFilterEnvAmount() + 1.0f) * 0.5f); // -1..1 -> 0..1
        case 50: return norm_to_cc_lin((synth.getFilterEnvAmount() + 1.0f) * 0.5f); // If UI shows KeyTrack here later, swap to that getter.

        // -------------------- LFO2 (Page 12) ----------------------
        case 51: return lfo_hz_to_cc(synth.getLFO2Frequency());
        case 52: return norm_to_cc_lin(synth.getLFO2Amount());
        case 53: {
                    int d = (int)synth.getLFO2Destination();
                    return JT4000Map::ccFromLfoDest(d);
                }

        
        // -------------------- LFO1 (Page 11) ----------------------
        case 54: return lfo_hz_to_cc(synth.getLFO1Frequency());
        case 55: return norm_to_cc_lin(synth.getLFO1Amount());
        // LFO destinations -> CC bin-centre (Mapping.h helper)
        case 56: {
            int d = (int)synth.getLFO1Destination();
            return JT4000Map::ccFromLfoDest(d);       // Mapping.h
        }

        // LFO waveforms -> CC bin-centre (same helper as OSC)
        case 62: {
            int w = synth.getLFO1Waveform();
            return ccFromWaveform((WaveformType)w);   // Waveforms.h
        }
        case 63: {
            int w = synth.getLFO2Waveform();
            return ccFromWaveform((WaveformType)w);
        }
        // -------------------- Filter Ext (Page 8) -----------------
        //case 83: return norm_to_cc_lin(synth.getFilterDrive() / 5.0f);       // 0..5 -> 0..1
        case 84: return norm_to_cc_lin(synth.getFilterOctaveControl() / 8.0f); // 0..8 -> 0..1
        //case 85: return norm_to_cc_lin(synth.getFilterPassbandGain() / 3.0f);  // 0..3 -> 0..1

        // -------------------- Supersaw/DC/Ring --------------------
        case 77: return norm_to_cc_lin(synth.getSupersawDetune(0));
        case 78: return norm_to_cc_lin(synth.getSupersawMix(0));
        case 79: return norm_to_cc_lin(synth.getSupersawDetune(1));
        case 80: return norm_to_cc_lin(synth.getSupersawMix(1));
        case 86: return norm_to_cc_lin(synth.getOsc1FrequencyDc());
        case 87: return norm_to_cc_lin(synth.getOsc1ShapeDc());
        case 88: return norm_to_cc_lin(synth.getOsc2FrequencyDc());
        case 89: return norm_to_cc_lin(synth.getOsc2ShapeDc());
        case 91: return norm_to_cc_lin(synth.getRing1Mix());
        case 92: return norm_to_cc_lin(synth.getRing2Mix());

        // -------------------- Glide & AmpMod (Page 15) ------------
        case 81: return synth.getGlideEnabled() ? 127 : 0;
        case 82: return (uint8_t)constrain(lroundf((synth.getGlideTimeMs() / 500.0f) * 127.0f), 0, 127);
        case 90: return norm_to_cc_lin(synth.getAmpModFixedLevel());

        default: return 0;
    }
}
