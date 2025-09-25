#pragma once
/*
 * PatchSchema.h
 * -------------
 * Defines which CCs are captured/restored by patches.
 * This intentionally does NOT depend on any UI layout.
 */

#include <Arduino.h>
#include "CCDefs.h"

namespace PatchSchema {

static constexpr uint8_t kPatchableCCs[] = {
    // Oscillator
    CC::OSC1_WAVE, CC::OSC2_WAVE,
    CC::OSC1_PITCH_OFFSET, CC::OSC2_PITCH_OFFSET,
    CC::OSC1_FINE_TUNE, CC::OSC2_FINE_TUNE,
    CC::OSC1_DETUNE, CC::OSC2_DETUNE,

    // Mix
    CC::OSC_MIX_BALANCE, CC::OSC1_MIX, CC::OSC2_MIX,
    CC::SUB_MIX, CC::NOISE_MIX,

    // Supersaw
    CC::SUPERSAW1_DETUNE, CC::SUPERSAW1_MIX,
    CC::SUPERSAW2_DETUNE, CC::SUPERSAW2_MIX,

    // Filter core
    CC::FILTER_CUTOFF, CC::FILTER_RESONANCE,
    CC::FILTER_ENV_AMOUNT, CC::FILTER_KEY_TRACK,
    CC::FILTER_OCTAVE_CONTROL,

    // Envelopes
    CC::AMP_ATTACK, CC::AMP_DECAY, CC::AMP_SUSTAIN, CC::AMP_RELEASE,
    CC::FILTER_ENV_ATTACK, CC::FILTER_ENV_DECAY, CC::FILTER_ENV_SUSTAIN, CC::FILTER_ENV_RELEASE,

    // LFOs
    CC::LFO1_FREQ, CC::LFO1_DEPTH, CC::LFO1_DESTINATION, CC::LFO1_WAVEFORM,
    CC::LFO2_FREQ, CC::LFO2_DEPTH, CC::LFO2_DESTINATION, CC::LFO2_WAVEFORM,

    // FX
    CC::FX_REVERB_SIZE, CC::FX_REVERB_DAMP,
    CC::FX_DELAY_TIME, CC::FX_DELAY_FEEDBACK,
    CC::FX_DRY_MIX, CC::FX_REVERB_MIX, CC::FX_DELAY_MIX,

    // Glide / global
    CC::GLIDE_ENABLE, CC::GLIDE_TIME,
    CC::AMP_MOD_FIXED_LEVEL,

    // Ring / DC
    CC::RING1_MIX, CC::RING2_MIX,
    CC::OSC1_FREQ_DC, CC::OSC1_SHAPE_DC,
    CC::OSC2_FREQ_DC, CC::OSC2_SHAPE_DC
};

static constexpr int kPatchableCount = sizeof(kPatchableCCs) / sizeof(kPatchableCCs[0]);

} // namespace PatchSchema
