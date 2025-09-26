#include "Presets.h"
#include "Mapping.h"
#include "CCDefs.h"
#include "Presets_Microsphere.h"

namespace {
inline void sendCC(SynthEngine& synth, uint8_t cc, uint8_t val, uint8_t ch = 1) {
    synth.handleControlChange(ch, cc, val);
}
}

namespace Presets {

static const int kTEMPLATE_COUNT = 9;

const char* templateName(uint8_t idx) {
    static const char* names[9] = {
        "Init Wave 0","Init Wave 1","Init Wave 2","Init Wave 3","Init Wave 4",
        "Init Wave 5","Init Wave 6","Init Wave 7","Init Wave 8"
    };
    return (idx < 9) ? names[idx] : "Init";
}

int presets_templateCount() { return kTEMPLATE_COUNT; }
int presets_totalCount()    { return kTEMPLATE_COUNT + JT4000_PRESET_COUNT; }

const char* presets_nameByGlobalIndex(int idx) {
    if (idx < kTEMPLATE_COUNT) return templateName((uint8_t)idx);
    int bankIdx = idx - kTEMPLATE_COUNT;
    return (bankIdx >= 0 && bankIdx < JT4000_PRESET_COUNT) ? JT4000_Presets[bankIdx].name : "—";
}

void presets_loadByGlobalIndex(SynthEngine& synth, int globalIdx, uint8_t midiCh) {
    int total = presets_totalCount(); if (total <= 0) return;
    while (globalIdx < 0) globalIdx += total;
    while (globalIdx >= total) globalIdx -= total;

    if (globalIdx < kTEMPLATE_COUNT) {
        loadInitTemplateByWave(synth, (uint8_t)globalIdx);
    } else {
        int bankIdx = globalIdx - kTEMPLATE_COUNT;
        loadMicrospherePreset(synth, bankIdx, midiCh);
    }
}

void loadInitTemplateByWave(SynthEngine& synth, uint8_t waveIndex) {
    if (waveIndex > 8) waveIndex = 0;
    AudioNoInterrupts();

    sendCC(synth, CC::OSC1_WAVE, waveIndex);
    sendCC(synth, CC::OSC2_WAVE, waveIndex);

    sendCC(synth, CC::OSC1_MIX, 0);
    sendCC(synth, CC::OSC2_MIX, 0);
    sendCC(synth, CC::SUB_MIX, 0);
    sendCC(synth, CC::NOISE_MIX, 0);

    sendCC(synth, CC::OSC1_PITCH_OFFSET, 65);
    sendCC(synth, CC::OSC1_FINE_TUNE, 64);
    sendCC(synth, CC::OSC1_DETUNE, 65);
    sendCC(synth, CC::OSC2_PITCH_OFFSET, 65);
    sendCC(synth, CC::OSC2_FINE_TUNE, 64);
    sendCC(synth, CC::OSC2_DETUNE, 65);

    sendCC(synth, CC::FILTER_CUTOFF,    127);
    sendCC(synth, CC::FILTER_RESONANCE, 0);
    sendCC(synth, CC::FILTER_ENV_AMOUNT, 65);
    sendCC(synth, CC::FILTER_KEY_TRACK,  65);
    sendCC(synth, CC::FILTER_OCTAVE_CONTROL, 0);

    sendCC(synth, CC::AMP_ATTACK,  0);
    sendCC(synth, CC::AMP_DECAY,   0);
    sendCC(synth, CC::AMP_SUSTAIN, 127);
    sendCC(synth, CC::AMP_RELEASE, 0);

    sendCC(synth, CC::FILTER_ENV_ATTACK,  0);
    sendCC(synth, CC::FILTER_ENV_DECAY,   0);
    sendCC(synth, CC::FILTER_ENV_SUSTAIN, 127);
    sendCC(synth, CC::FILTER_ENV_RELEASE, 0);

    sendCC(synth, CC::LFO1_DEPTH, 0);
    sendCC(synth, CC::LFO2_DEPTH, 0);

    sendCC(synth, CC::FX_REVERB_SIZE,    0);
    sendCC(synth, CC::FX_REVERB_DAMP,    0);
    sendCC(synth, CC::FX_DELAY_TIME,     0);
    sendCC(synth, CC::FX_DELAY_FEEDBACK, 0);
    sendCC(synth, CC::FX_DRY_MIX,    127);
    sendCC(synth, CC::FX_REVERB_MIX, 0);
    sendCC(synth, CC::FX_DELAY_MIX,  0);

    sendCC(synth, CC::GLIDE_ENABLE, 0);
    sendCC(synth, CC::GLIDE_TIME,   0);
    sendCC(synth, CC::AMP_MOD_FIXED_LEVEL, 127);

    AudioInterrupts();
}

void loadRawPatchViaCC(SynthEngine& synth, const uint8_t data[64], uint8_t midiCh) {
    AudioNoInterrupts();
    for (const auto& row : JT4000Map::kSlots) {
        uint8_t idx0 = (row.byte1 >= 1) ? (row.byte1 - 1) : 0;
        if (idx0 >= 64) continue;
        uint8_t raw = data[idx0];
        uint8_t val = JT4000Map::toCC(raw, row.xf);
        synth.handleControlChange(midiCh, row.cc, val);
    }
    AudioInterrupts();
}

void loadMicrospherePreset(SynthEngine& synth, int index, uint8_t midiCh) {
    if (index < 0 || index >= JT4000_PRESET_COUNT) return;
    loadRawPatchViaCC(synth, JT4000_Presets[index].data, midiCh);
}

} // namespace Presets
