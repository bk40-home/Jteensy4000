// UIManager.cpp  (full file with safe edits)
// -----------------------------------------------------------------------------
// UI reflects engine state by converting internal units back to 0..127,
// using the exact inverse curves in Mapping.h so values are consistent.
// -----------------------------------------------------------------------------

#include "UIManager.h"
#include "Mapping.h"
#include "CCMap.h"
#include "HardwareInterface.h"  // ✅ Ensure visibility

// ... [unchanged methods above] ...

void UIManager::pollInputs(HardwareInterface& hw, SynthEngine& synth) {
    int currentPage = getCurrentPage();

    for (int i = 0; i < 4; ++i) {
        if (hw.potChanged(i, 5)) {
            int raw = hw.readPot(i);
            int mapped = map(raw, 0, 1023, 0, 127);
            byte cc = ccMap[currentPage][i];
            setParameterValue(i, mapped);
            synth.handleControlChange(1, cc, mapped);
            Serial.printf("Pot %d → CC %d = %d\n", i, cc, mapped);
        }
    }

    int delta = hw.getEncoderDelta();
    if (delta > 0) {
        int newPage = (currentPage + 1) % NUM_PAGES;
        setPage(newPage);
        for (int i = 0; i < 4; ++i)
            setParameterLabel(i, ccNames[newPage][i]);
        syncFromEngine(synth);
        Serial.printf("Page increased to: %d\n", newPage);
    } else if (delta < 0) {
        int newPage = (currentPage == 0) ? NUM_PAGES - 1 : currentPage - 1;
        setPage(newPage);
        for (int i = 0; i < 4; ++i)
            setParameterLabel(i, ccNames[newPage][i]);
        syncFromEngine(synth);
        Serial.printf("Page decreased to: %d\n", newPage);
    }

    if (hw.isButtonPressed()) {
        // Reserved for future behavior
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

void UIManager::updateDisplay(SynthEngine& synth) {
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);

    const int rowHeight = 16;
    const int labelX = 0;
    const int valueX = 128;

    for (int i = 0; i < 4; ++i) {
        int y = i * rowHeight;
        byte cc = ccMap[_currentPage][i];

        // Left-aligned label
        _display.setCursor(labelX, y);
        _display.print(_labels[i]);

        // Right-aligned value (0..127)
        char valueBuf[12];
        snprintf(valueBuf, sizeof(valueBuf), "%3d", _values[i]);
        valueBuf[sizeof(valueBuf) - 1] = '\0'; // safety

        int16_t x1, y1;
        uint16_t w, h;
        _display.getTextBounds(valueBuf, 0, 0, &x1, &y1, &w, &h);
        _display.setCursor(valueX - w, y);
        _display.print(valueBuf);
    }	
    _display.display();
}

void UIManager::syncFromEngine(SynthEngine& synth) {
    for (int i = 0; i < 4; ++i) {
        byte cc = ccMap[_currentPage][i];
        _values[i] = constrain(ccToDisplayValue(cc, synth), 0, 127);
    }
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
        case 83: return norm_to_cc_lin(synth.getFilterDrive() / 5.0f);       // 0..5 -> 0..1
        case 84: return norm_to_cc_lin(synth.getFilterOctaveControl() / 8.0f); // 0..8 -> 0..1
        case 85: return norm_to_cc_lin(synth.getFilterPassbandGain() / 3.0f);  // 0..3 -> 0..1

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
