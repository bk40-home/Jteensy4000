#include <Wire.h>          // portable I2C include
#include "UIManager.h"
#include "Mapping.h"
#include "CCDefs.h"
#include "UIPageLayout.h"   // UI-only page layout
#include "HardwareInterface.h"
#include "Presets.h"
#include "AudioScopeTap.h"
#include "WaveForms.h"      // waveform helpers
#include "AKWF_All.h"       // AKWF bank helpers for ARB display

// If you are patching the scope via a global tap, keep this extern.
// If not used on your build, it’s harmless.
extern AudioScopeTap scopeTap;

// ---------- small utils ----------
static inline int wrapIndex(int i, int max) {
    if (max <= 0) return 0;
    while (i < 0)    i += max;
    while (i >= max) i -= max;
    return i;
}

// Text labels for enum-like CCs. Return nullptr for numeric params so UI prints numbers.
const char* UIManager::ccToDisplayText(byte cc, SynthEngine& synth) {
    switch (cc) {
        // ---- Oscillator waveforms (short names) ----
        case CC::OSC1_WAVE: return synth.getOsc1WaveformName();   // e.g. "SQR", "SAW", "SINE", "SSAW"
        case CC::OSC2_WAVE: return synth.getOsc2WaveformName();

        // ---- LFO waveforms (short names) ----
        case CC::LFO1_WAVEFORM: return synth.getLFO1WaveformName();
        case CC::LFO2_WAVEFORM: return synth.getLFO2WaveformName();

        // ---- LFO destinations (human names) ----
        case CC::LFO1_DESTINATION: return synth.getLFO1DestinationName(); // e.g. "Pitch", "Filter", "Shape", "Amp"
        case CC::LFO2_DESTINATION: return synth.getLFO2DestinationName();

        // ---- Arbitrary waveform bank names ----
        // When selecting an ARB bank via the CC, display the human-readable bank name.
        case CC::OSC1_ARB_BANK: return akwf_bankName(synth.getOsc1ArbBank());
        case CC::OSC2_ARB_BANK: return akwf_bankName(synth.getOsc2ArbBank());
        // Table index is numeric; fall through to default (returns nullptr)

        // Everything else is numeric (0..127) → return nullptr so renderPage() prints the number
        default: return nullptr;
    }
}

int UIManager::ccToDisplayValue(byte cc, SynthEngine& synth) {
    using namespace JT4000Map;

    switch (cc) {
        // ---------------- Waveforms ----------------
        // Keep the currently selected waveform centered in a bin
        case CC::OSC1_WAVE: return ccFromWaveform((WaveformType)synth.getOsc1Waveform());
        case CC::OSC2_WAVE: return ccFromWaveform((WaveformType)synth.getOsc2Waveform());

        // ---------------- Filter core --------------
        case CC::FILTER_CUTOFF:    return obxa_cutoff_hz_to_cc(synth.getFilterCutoff());
        case CC::FILTER_RESONANCE: return obxa_res01_to_cc(synth.getFilterResonance());

        // ---------------- Amp env ------------------
        case CC::AMP_ATTACK:  return time_ms_to_cc(synth.getAmpAttack());
        case CC::AMP_DECAY:   return time_ms_to_cc(synth.getAmpDecay());
        case CC::AMP_SUSTAIN: return norm_to_cc(synth.getAmpSustain());
        case CC::AMP_RELEASE: return time_ms_to_cc(synth.getAmpRelease());

        // ---------------- Filter env ---------------
        case CC::FILTER_ENV_ATTACK:  return time_ms_to_cc(synth.getFilterEnvAttack());
        case CC::FILTER_ENV_DECAY:   return time_ms_to_cc(synth.getFilterEnvDecay());
        case CC::FILTER_ENV_SUSTAIN: return norm_to_cc(synth.getFilterEnvSustain());
        case CC::FILTER_ENV_RELEASE: return time_ms_to_cc(synth.getFilterEnvRelease());

        // ---------------- LFOs ---------------------
        case CC::LFO1_FREQ:        return lfo_hz_to_cc(synth.getLFO1Frequency());
        case CC::LFO1_DEPTH:       return norm_to_cc(synth.getLFO1Amount());
        case CC::LFO2_FREQ:        return lfo_hz_to_cc(synth.getLFO2Frequency());
        case CC::LFO2_DEPTH:       return norm_to_cc(synth.getLFO2Amount());
        case CC::LFO1_DESTINATION: return JT4000Map::ccFromLfoDest((int)synth.getLFO1Destination());
        case CC::LFO2_DESTINATION: return JT4000Map::ccFromLfoDest((int)synth.getLFO2Destination());

        // ---------------- Mixer --------------------
        // For the balance control, show OSC2 level (your UI convention)
        case CC::OSC_MIX_BALANCE: return norm_to_cc(synth.getOscMix2());
        case CC::OSC1_MIX:        return norm_to_cc(synth.getOscMix1());
        case CC::OSC2_MIX:        return norm_to_cc(synth.getOscMix2());
        case CC::SUB_MIX:         return norm_to_cc(synth.getSubMix());
        case CC::NOISE_MIX:       return norm_to_cc(synth.getNoiseMix());

        // ---------------- Pitch / Detune / Fine ----
        // Coarse pitch shows the *bucket center* values so the OLED matches the detents
        case CC::OSC1_PITCH_OFFSET: {
            float semis = synth.getOsc1PitchOffset(); // -24, -12, 0, +12, +24
            // Return the center CC of those buckets, matching your earlier UI mapping.
            if      (semis <= -18.0f) return 12;   // ~ -24
            else if (semis <=  -6.0f) return 38;   // ~ -12
            else if (semis <=   6.0f) return 64;   // ~  0
            else if (semis <=  18.0f) return 90;   // ~ +12
            else                      return 116;  // ~ +24
        }
        case CC::OSC2_PITCH_OFFSET: {
            float semis = synth.getOsc2PitchOffset();
            if      (semis <= -18.0f) return 12;
            else if (semis <=  -6.0f) return 38;
            else if (semis <=   6.0f) return 64;
            else if (semis <=  18.0f) return 90;
            else                      return 116;
        }

        // Detune stored as -1..+1 → map back to 0..127
        case CC::OSC1_DETUNE: {
            float d = synth.getOsc1Detune(); // -1..+1
            return norm_to_cc((d + 1.0f) * 0.5f);
        }
        case CC::OSC2_DETUNE: {
            float d = synth.getOsc2Detune(); // -1..+1
            return norm_to_cc((d + 1.0f) * 0.5f);
        }

        // Fine tune stored as -100..+100 cents → map back to 0..127
        case CC::OSC1_FINE_TUNE: {
            float cents = synth.getOsc1FineTune();  // -100..+100
            int v = (int)lroundf((cents + 100.0f) * 127.0f / 200.0f);
            return (uint8_t)constrain(v, 0, 127);
        }
        case CC::OSC2_FINE_TUNE: {
            float cents = synth.getOsc2FineTune();  // -100..+100
            int v = (int)lroundf((cents + 100.0f) * 127.0f / 200.0f);
            return (uint8_t)constrain(v, 0, 127);
        }

        // ---------------- Supersaw / DC / Ring ----
        case CC::SUPERSAW1_DETUNE: return norm_to_cc(synth.getSupersawDetune(0));
        case CC::SUPERSAW1_MIX:    return norm_to_cc(synth.getSupersawMix(0));
        case CC::SUPERSAW2_DETUNE: return norm_to_cc(synth.getSupersawDetune(1));
        case CC::SUPERSAW2_MIX:    return norm_to_cc(synth.getSupersawMix(1));
        case CC::OSC1_FREQ_DC:     return norm_to_cc(synth.getOsc1FrequencyDc());
        case CC::OSC1_SHAPE_DC:    return norm_to_cc(synth.getOsc1ShapeDc());
        case CC::OSC2_FREQ_DC:     return norm_to_cc(synth.getOsc2FrequencyDc());
        case CC::OSC2_SHAPE_DC:    return norm_to_cc(synth.getOsc2ShapeDc());
        case CC::RING1_MIX:        return norm_to_cc(synth.getRing1Mix());
        case CC::RING2_MIX:        return norm_to_cc(synth.getRing2Mix());

        // ---------------- Filter mods --------------
        case CC::FILTER_ENV_AMOUNT:     return norm_to_cc((synth.getFilterEnvAmount() + 1.0f) * 0.5f);
        case CC::FILTER_KEY_TRACK:      return norm_to_cc((synth.getFilterKeyTrackAmount() + 1.0f) * 0.5f);
        case CC::FILTER_OCTAVE_CONTROL: return norm_to_cc(synth.getFilterOctaveControl() / 8.0f);

        // ---------------- FX (use SynthEngine getters for display) -----------
        case CC::FX_REVERB_SIZE: {
            // Room size is 0..1 → map linearly to CC
            return norm_to_cc(synth.getFXReverbRoomSize());
        }
        case CC::FX_REVERB_DAMP: {
            // High damping (legacy mapping).  Use separate getter for hi damping.
            return norm_to_cc(synth.getFXReverbHiDamping());
        }
        case CC::FX_REVERB_LODAMP: {
            // Low damping
            return norm_to_cc(synth.getFXReverbLoDamping());
        }
        case CC::FX_DELAY_TIME: {
            // Delay time in ms (0..2000).  Map linearly to CC 0..127.
            float ms = synth.getFXDelayTimeMs();
            if (ms < 0.0f) ms = 0.0f;
            if (ms > 2000.0f) ms = 2000.0f;
            return (uint8_t)constrain(lroundf((ms / 2000.0f) * 127.0f), 0, 127);
        }
        case CC::FX_DELAY_FEEDBACK: {
            return norm_to_cc(synth.getFXDelayFeedback());
        }
        case CC::FX_DRY_MIX: {
            return norm_to_cc(synth.getFXDryMix());
        }
        case CC::FX_REVERB_MIX: {
            return norm_to_cc(synth.getFXReverbMix());
        }
        case CC::FX_DELAY_MIX: {
            return norm_to_cc(synth.getFXDelayMix());
        }
        case CC::FX_DELAY_MOD_RATE: {
            return norm_to_cc(synth.getFXDelayModRate());
        }
        case CC::FX_DELAY_MOD_DEPTH: {
            return norm_to_cc(synth.getFXDelayModDepth());
        }
        case CC::FX_DELAY_INERTIA: {
            return norm_to_cc(synth.getFXDelayInertia());
        }
        case CC::FX_DELAY_TREBLE: {
            return norm_to_cc(synth.getFXDelayTreble());
        }
        case CC::FX_DELAY_BASS: {
            return norm_to_cc(synth.getFXDelayBass());
        }

        // ---------------- Glide / AmpModDC --------------------------
        case CC::GLIDE_ENABLE:        return synth.getGlideEnabled() ? 127 : 0;
        case CC::GLIDE_TIME:          return (uint8_t)constrain(lroundf((synth.getGlideTimeMs() / 500.0f) * 127.0f), 0, 127);
        case CC::AMP_MOD_FIXED_LEVEL: return norm_to_cc(synth.getAmpModFixedLevel());

        // ---------------- Arbitrary waveform bank/table ----------------
        case CC::OSC1_ARB_BANK: {
            // Map the current bank to the midpoint of its CC bin
            uint8_t numBanks = static_cast<uint8_t>(ArbBank::BwTri) + 1;
            uint8_t bankIdx = static_cast<uint8_t>(synth.getOsc1ArbBank());
            uint16_t start = (static_cast<uint16_t>(bankIdx) * 128u) / numBanks;
            uint16_t end   = (static_cast<uint16_t>(bankIdx + 1) * 128u) / numBanks;
            return static_cast<uint8_t>((start + end) / 2);
        }
        case CC::OSC2_ARB_BANK: {
            uint8_t numBanks = static_cast<uint8_t>(ArbBank::BwTri) + 1;
            uint8_t bankIdx = static_cast<uint8_t>(synth.getOsc2ArbBank());
            uint16_t start = (static_cast<uint16_t>(bankIdx) * 128u) / numBanks;
            uint16_t end   = (static_cast<uint16_t>(bankIdx + 1) * 128u) / numBanks;
            return static_cast<uint8_t>((start + end) / 2);
        }
        case CC::OSC1_ARB_INDEX: {
            uint16_t count = akwf_bankCount(synth.getOsc1ArbBank());
            if (count == 0) return 0;
            uint16_t idx = synth.getOsc1ArbIndex();
            if (idx >= count) idx = count - 1;
            uint16_t start = (idx * 128u) / count;
            uint16_t end   = ((idx + 1) * 128u) / count;
            return static_cast<uint8_t>((start + end) / 2);
        }
        case CC::OSC2_ARB_INDEX: {
            uint16_t count = akwf_bankCount(synth.getOsc2ArbBank());
            if (count == 0) return 0;
            uint16_t idx = synth.getOsc2ArbIndex();
            if (idx >= count) idx = count - 1;
            uint16_t start = (idx * 128u) / count;
            uint16_t end   = ((idx + 1) * 128u) / count;
            return static_cast<uint8_t>((start + end) / 2);
        }

        default:
            return 0;
    }
}





// ---------- input handling (pots + encoder + scope toggle) ----------
void UIManager::pollInputs(HardwareInterface& hw, SynthEngine& synth) {
    if (hw.isButtonPressed()) {
        _scopeOn = !_scopeOn;
        if (!_scopeOn) {
            for (int i = 0; i < 4; ++i) setParameterLabel(i, UIPage::ccNames[_currentPage][i]);
            syncFromEngine(synth);
        }
    }

    const bool scopeVisible = _scopeOn;
    int delta = hw.getEncoderDelta();

    if (scopeVisible) {
        // Preset browsing in scope view
        if (delta != 0) {
            int total = Presets::presets_totalCount();
            _currentPreset = wrapIndex(_currentPreset + (delta > 0 ? 1 : -1), total);
            Presets::presets_loadByGlobalIndex(synth, _currentPreset, 1);
            syncFromEngine(synth);
        }
        // Quick cutoff/res while scoping
        if (hw.potChanged(0,1)) {
            uint8_t v = (hw.readPot(0) >> 3);
            synth.handleControlChange(1, CC::FILTER_CUTOFF, v);
            setParameterValue(0, v);
            _valueText[0] = nullptr;
        }
        if (hw.potChanged(1,1)) {
            uint8_t v = (hw.readPot(1) >> 3);
            synth.handleControlChange(1, CC::FILTER_RESONANCE, v);
            setParameterValue(1, v);
            _valueText[1] = nullptr;
        }
        return;
    }

    // Edit mode: encoder cycles pages
    if (delta > 0) {
        int newPage = (_currentPage + 1) % UIPage::NUM_PAGES;
        setPage(newPage);
        for (int i=0;i<4;++i) setParameterLabel(i, UIPage::ccNames[newPage][i]);
        syncFromEngine(synth);
    } else if (delta < 0) {
        int newPage = (_currentPage == 0) ? UIPage::NUM_PAGES - 1 : _currentPage - 1;
        setPage(newPage);
        for (int i=0;i<4;++i) setParameterLabel(i, UIPage::ccNames[newPage][i]);
        syncFromEngine(synth);
    }

    // Pots edit current page’s four parameters
    for (int i = 0; i < 4; ++i) {
        if (!hw.potChanged(i, 1)) continue;

        // FIX: use 'uint8_t', not 'U_INT8'
        byte v  = (hw.readPot(i) >> 3);
        byte cc = UIPage::ccMap[_currentPage][i];
        if (cc == 255) continue;

        synth.handleControlChange(1, cc, v);
        setParameterValue(i, v);
        _valueText[i] = ccToDisplayText(cc, synth);

        Serial.printf("[POT] page %d knob %d → CC %3u = %3u\n", _currentPage, i, cc, v);
    }
}

// ---------- lifecycle ----------
UIManager::UIManager()
: _display(128, 64, &Wire2, -1),
  _currentPage(0),
  _highlightIndex(0),
  _currentPreset(0)           // <= put after _highlightIndex to match UIManager.h
{
    for (int i = 0; i < 4; ++i) {
        _labels[i] = "";
        _values[i] = 0;
        _valueText[i] = nullptr;
    }
}


void UIManager::begin() {
    Wire2.begin();
    _display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);
    _display.display();

    // Initial labels
    for (int i=0;i<4;++i) setParameterLabel(i, UIPage::ccNames[_currentPage][i]);
}

// ---------- public UI control ----------
void UIManager::updateDisplay(SynthEngine& synth) {
    if (_scopeOn) renderScope();
    else          renderPage();
}

void UIManager::setPage(int page) {
    if (page < 0) page = 0;
    if (page >= UIPage::NUM_PAGES) page = UIPage::NUM_PAGES - 1;
    _currentPage = page;
}

int UIManager::getCurrentPage() const { return _currentPage; }
void UIManager::highlightParameter(int index) { _highlightIndex = index; }

void UIManager::setParameterLabel(int index, const char* label) {
    if (index >= 0 && index < 4) _labels[index] = label ? label : "";
}

void UIManager::setParameterValue(int index, int value) {
    if (index < 0 || index >= 4) return;
    if (value < 0) value = 0; else if (value > 127) value = 127;
    _values[index] = value;
}

int UIManager::getParameterValue(int index) const {
    return (index >= 0 && index < 4) ? _values[index] : 0;
}

AudioRecordQueue& UIManager::scopeIn() { return _scopeQueue; }

// ---------- draw routines ----------
void UIManager::renderPage() {
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);

    const int rowH = 16;
    for (int i = 0; i < 4; ++i) {
        int y = i * rowH;

        // left: label
        _display.setCursor(0, y);
        _display.print(_labels[i]);

        // right: value (text if available, else numeric 0..127)
        if (_valueText[i]) {
            drawRightAligned(String(_valueText[i]), y);
        } else {
            char buf[8];
            snprintf(buf, sizeof(buf), "%3d", _values[i]);
            drawRightAligned(buf, y);
        }
    }
    _display.display();
}

void UIManager::renderScope() {
    const int TOP_BAR = 8;
    const int BOT_BAR = 8;
    const int DRAW_TOP = TOP_BAR;
    const int DRAW_BOT = 63 - BOT_BAR;

    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);

    // --- Header: preset number (global) and name ---
    _display.setCursor(0, 0);
    _display.print("P:");
    _display.print(_currentPreset);
    _display.print(" ");
    {
        const char* fullName = Presets::presets_nameByGlobalIndex(_currentPreset);
        char nameBuf[22];
        size_t room = sizeof(nameBuf) - 1;
        strncpy(nameBuf, fullName ? fullName : "", room);
        nameBuf[room] = '\0';
        _display.print(nameBuf);
    }

    // --- Pull recent samples from the global tap ---
    static int16_t snap[512];
    const uint16_t nAvail = scopeTap.snapshot(snap, (uint16_t)512);
    if (nAvail < 128) {
        _display.setCursor(0, DRAW_TOP + 8);
        _display.print("SCOPE (arming)");

        _display.fillRect(0, 56, 128, 8, SSD1306_BLACK);
        _display.setCursor(0, 56);
        _display.print("CPU ");
        _display.print(AudioProcessorUsage(), 1);
        _display.print("%  Blk ");
        _display.print(AudioMemoryUsage());
        _display.display();
        return;
    }

    // Helper: newest sample by offset (0=newest)
    auto newest = [&](int k)->int16_t {
        if (k < 0) k = 0;
        if (k >= (int)nAvail) k = (int)nAvail - 1;
        return snap[nAvail - 1 - k];
    };

    // Auto timebase via zero crossings near tail
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

    // Downsample window to 128 columns with a small box filter
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

    // Vertical auto-gain
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

    // Draw waveform in band (8..55)
    const int midY = (DRAW_TOP + DRAW_BOT) / 2;
    int prevX = 0, prevY = midY;
    for (int x = 0; x < 128; ++x) {
        int y = midY - (col[x] >> shift);
        if (y < DRAW_TOP) y = DRAW_TOP;
        if (y > DRAW_BOT) y = DRAW_BOT;
        if (x > 0) _display.drawLine(prevX, prevY, x, y, SSD1306_WHITE);
        prevX = x; prevY = y;
    }

    // Footer: CPU + blocks
    _display.fillRect(0, 56, 128, 8, SSD1306_BLACK);
    _display.setCursor(0, 56);
    _display.print("CPU ");
    _display.print(AudioProcessorUsage(), 1);
    _display.print("%  Blk ");
    _display.print(AudioMemoryUsage());

    _display.display();
}


void UIManager::drawRightAligned(const String& text, int y) {
    int16_t x1, y1; uint16_t w, h;
    _display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    _display.setCursor(128 - w, y);
    _display.print(text);
}

// ---------- engine → UI sync (use inverse curves so UI matches engine) ----------
void UIManager::syncFromEngine(SynthEngine& synth) {
    for (int i = 0; i < 4; ++i) {
        const byte cc = UIPage::ccMap[_currentPage][i];
        if (cc == 255) {
            _valueText[i] = nullptr;
            setParameterValue(i, 0);
            continue;
        }
        const int v = ccToDisplayValue(cc, synth);
        setParameterValue(i, v);
        _valueText[i] = ccToDisplayText(cc, synth); // may be nullptr
        // (If you keep a last-CC cache for pot hysteresis, update it here.)
    }
}
