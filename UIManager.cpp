// UIManager.cpp  (full file with safe edits)
// -----------------------------------------------------------------------------
// UI reflects engine state by converting internal units back to 0..127,
// using the exact inverse curves in Mapping.h so values are consistent.
// -----------------------------------------------------------------------------

#include "UIManager.h"
#include "Mapping.h"
#include "CCMap.h"
#include "HardwareInterface.h"  // ✅ Ensure visibility
#include "Presets.h"   // make sure this is at the top of UIManager.cpp

void UIManager::pollInputs(HardwareInterface& hw, SynthEngine& synth) {
 

    // -------- Button: toggle scope on/off --------
    if (hw.isButtonPressed()) {
        _scopeOn = !_scopeOn;
    
    }

    const bool scopeVisible = _scopeOn ;

    // -------- Encoder behavior --------
        int delta = hw.getEncoderDelta();

    if (scopeVisible) {
        // SCOPE MODE: encoder cycles presets, pots ignored
        if (delta != 0) {
            _currentPreset += (delta > 0 ? 1 : -1);
            if (_currentPreset < 0) _currentPreset = 8;
            if (_currentPreset > 8) _currentPreset = 0;

            // Use in-project presets (no JSON here)
            Presets::loadTemplateByIndex(synth, (uint8_t)_currentPreset);
            // Keep UI values in sync so when you exit scope, page shows latest
            syncFromEngine(synth);
            // stay in scope; do not process pots
        }
        return; // exit: in scope mode we ignore pots and page nav
    } 
    else {
        // EDIT MODE (page view): encoder navigates pages
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
        // -------- Pots: only in EDIT mode --------
    if (!scopeVisible) {
        for (int i = 0; i < 4; ++i) {
            if (hw.potChanged(i, 1)) {                 // CC-step hysteresis
                int ccVal = (hw.readPot(i) >> 3);      // smoothed 0..127
                byte cc = ccMap[_currentPage][i];
                synth.handleControlChange(1, cc, (uint8_t)ccVal);
                _lastCCSent[cc] = (uint8_t)ccVal;
                _hasCCSent[cc]  = true;
                setParameterValue(i, ccVal);           // shows raw CC for now
            }
        }
    }
}




UIManager::UIManager() : _display(128, 64, &Wire1, -1), _currentPage(0), _highlightIndex(0) {
    for (int i = 0; i < 4; ++i) {
        _labels[i] = "";
        _values[i] = 0;
    }
}

void UIManager::begin() {
    Wire1.begin();
    _display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);
    _display.display();

    // 🆕 Start the scope recorder but don't block if not patched yet
    for (int i = 0; i < SCOPE_RING; ++i) _scopeRing[i] = 0;
    _scopeWrite = 0;
    _scopeQueue.begin();

}

void UIManager::setPage(int page) { _currentPage = page; }
int UIManager::getCurrentPage() const { return _currentPage; }
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
        _display.setCursor(labelX, y);
        _display.print(_labels[i]);

        // Right aligned raw CC (0..127) as before
        char valueBuf[12];
        snprintf(valueBuf, sizeof(valueBuf), "%3d", _values[i]);
        drawRightAligned(valueBuf, y);
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
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);

    int blocks = _scopeQueue.available();
    static int16_t lastBlock[128] = {0};


// --- Feed ring buffer (keep newest) ---
bool readAny = false;                 // NEW: track if we consumed audio
while (blocks-- > 0) {
    const int16_t* buf = (const int16_t*)_scopeQueue.readBuffer();
    for (int i = 0; i < 128; ++i) {
        _scopeRing[_scopeWrite] = buf[i];
        _scopeWrite = (_scopeWrite + 1) % SCOPE_RING;
    }
    memcpy(lastBlock, buf, 128 * sizeof(int16_t));
    _scopeQueue.freeBuffer();
    readAny = true;                   // we read at least one block
}

// If nothing ever arrived since boot, show hint and return
static bool everHadAudio = false;
everHadAudio |= readAny;              // ✅ only set true when we actually read
if (!everHadAudio) {
    _display.setCursor(0, 0);
    _display.print("SCOPE (no blocks)");
    _display.setCursor(0, 10);
    _display.print("Check patch to UI queue");
    _display.display();
    return;
}

    // --- Auto timebase: estimate period via zero-crossing ---
    auto ringAt = [&](int idx)->int16_t { return _scopeRing[(idx + SCOPE_RING) % SCOPE_RING]; };
    const int Nscan = min(SCOPE_RING, 3000);    // scan up to ~68 ms for period
    int w = _scopeWrite - 1;                    // start near newest
    // find last negative-to-positive crossing
    int anchor = -1;
    int16_t prev = ringAt(w-1);
    for (int i = 1; i < Nscan; ++i) {
    int16_t cur = ringAt(w - i);
    if (prev < 0 && cur >= 0) { anchor = w - i; break; }
    prev = cur;
    }
    // find previous crossing for period
    int period = 256; // fallback ~5.8ms
    if (anchor >= 0) {
    int16_t pprev = ringAt(anchor-1);
    for (int i = 1; i < Nscan; ++i) {
        int16_t c = ringAt(anchor - i);
        if (pprev < 0 && c >= 0) { period = i; break; }
        pprev = c;
    }
    }
    // target window ~1.5 cycles, clamp to [128 .. SCOPE_RING]
    int win = period + period/2;
    if (win < 128) win = 128;
    if (win > SCOPE_RING) win = SCOPE_RING;

    // --- Downsample window to 128 columns (mean) ---
    int start = (_scopeWrite - win + SCOPE_RING) % SCOPE_RING;
    int stepQ16 = (win << 16) / 128;  // fixed-point step
    int32_t pos = 0;
    int16_t col[128];
    for (int x = 0; x < 128; ++x) {
    int base = start + (pos >> 16);
    // small box filter over ~win/128 samples
    int acc = 0, cnt = 0;
    int box = max(1, win / 128);
    for (int k = 0; k < box; ++k) {
        acc += ringAt(base + k);
        cnt++;
    }
    col[x] = (int16_t)(acc / cnt);
    pos += stepQ16;
    }

    // --- Vertical auto-gain (your dynamic shift logic) ---
    int16_t peak = 0;
    for (int x = 0; x < 128; ++x) { int16_t a = abs(col[x]); if (a > peak) peak = a; }
    int shift = 10;
    if      (peak < 1024) shift = 6;
    else if (peak < 2048) shift = 7;
    else if (peak < 4096) shift = 8;
    else if (peak < 8192) shift = 9;
    if (shift < 6) shift = 6; if (shift > 11) shift = 11;

    // --- Draw ---
    const int midY = 32;
    int prevX = 0, prevY = midY;
    for (int x = 0; x < 128; ++x) {
    int y = midY - (col[x] >> shift);
    if (y < 0) y = 0; else if (y > 63) y = 63;
    if (x > 0) _display.drawLine(prevX, prevY, x, y, SSD1306_WHITE);
    prevX = x; prevY = y;
    }
    // --- Overlay: Preset number + name and current visual gain ---
    // Use a slim footer so the top of the waveform stays unobstructed
    _display.fillRect(0, 56, 128, 8, SSD1306_BLACK);   // footer bar
    _display.setCursor(0, 56);
    _display.print("P:"); 
    _display.print(_currentPreset);
    _display.print(" ");
    _display.print(Presets::templateName((uint8_t)_currentPreset));

    // Show visual gain (1<<(10-shift))
    _display.setCursor(100, 56);
    _display.print("x");
    _display.print(1 << (10 - shift));

    _display.display(); // <- update for blocks>0 branch
}



void UIManager::syncFromEngine(SynthEngine& synth) {
    for (int i = 0; i < 4; ++i) {
        byte cc = ccMap[_currentPage][i];
        if (_hasCCSent[cc]) {
            _values[i] = _lastCCSent[cc];  // show exactly what we last set
        } else {
            _values[i] = 0;                // or a per-CC default if you prefer
        }
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
        case 21: return synth.getOsc1Waveform();  // already 0..N
        case 22: return synth.getOsc2Waveform();

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
            // Map back to nearest bucket center
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
        case 53: return (int)synth.getLFO2Destination();   // enum already small int
        case 63: return synth.getLFO2Waveform();           // 0..8

        // -------------------- LFO1 (Page 11) ----------------------
        case 54: return lfo_hz_to_cc(synth.getLFO1Frequency());
        case 55: return norm_to_cc_lin(synth.getLFO1Amount());
        case 56: return (int)synth.getLFO1Destination();
        case 62: return synth.getLFO1Waveform();

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
