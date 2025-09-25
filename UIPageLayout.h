#pragma once
/*
 * UIPageLayout.h
 * --------------
 * UI-only configuration mapping pages/rows to CC IDs for display + editing.
 * Do not include this file from engine or patch code.
 */

#include <Arduino.h>
#include "CCDefs.h"

namespace UIPage {

static constexpr int NUM_PAGES       = 16;
static constexpr int PARAMS_PER_PAGE = 4;

static constexpr uint8_t ccMap[NUM_PAGES][PARAMS_PER_PAGE] = {
    {CC::OSC1_WAVE,        CC::OSC1_PITCH_OFFSET, CC::OSC1_FINE_TUNE,   CC::OSC1_DETUNE},    // 0
    {CC::OSC_MIX_BALANCE,  CC::OSC1_MIX,          CC::SUPERSAW1_DETUNE, CC::SUPERSAW1_MIX},  // 1
    {CC::OSC1_FREQ_DC,     CC::OSC1_SHAPE_DC,     CC::RING1_MIX,        255},                // 2

    {CC::OSC2_WAVE,        CC::OSC2_PITCH_OFFSET, CC::OSC2_FINE_TUNE,   CC::OSC2_DETUNE},    // 3
    {CC::OSC2_MIX,         CC::SUPERSAW2_DETUNE,  CC::SUPERSAW2_MIX,    CC::NOISE_MIX},      // 4
    {CC::OSC2_FREQ_DC,     CC::OSC2_SHAPE_DC,     CC::RING2_MIX,        255},                // 5

    {CC::SUB_MIX,          255,                   255,                  255},                // 6

    {CC::FILTER_CUTOFF,    CC::FILTER_RESONANCE,  CC::FILTER_ENV_AMOUNT, CC::FILTER_KEY_TRACK}, // 7
    {255,                  CC::FILTER_OCTAVE_CONTROL,                   255,                 255}, // 8

    {CC::AMP_ATTACK,       CC::AMP_DECAY,         CC::AMP_SUSTAIN,       CC::AMP_RELEASE},    // 9
    {CC::FILTER_ENV_ATTACK, CC::FILTER_ENV_DECAY, CC::FILTER_ENV_SUSTAIN, CC::FILTER_ENV_RELEASE}, // 10

    {CC::LFO1_FREQ,        CC::LFO1_DEPTH,        CC::LFO1_DESTINATION,  CC::LFO1_WAVEFORM}, // 11
    {CC::LFO2_FREQ,        CC::LFO2_DEPTH,        CC::LFO2_DESTINATION,  CC::LFO2_WAVEFORM}, // 12

    {CC::FX_REVERB_SIZE,   CC::FX_REVERB_DAMP,    CC::FX_DELAY_TIME,     CC::FX_DELAY_FEEDBACK}, // 13
    {CC::FX_DRY_MIX,       CC::FX_REVERB_MIX,     CC::FX_DELAY_MIX,      255},                // 14

    {CC::GLIDE_ENABLE,     CC::GLIDE_TIME,        CC::AMP_MOD_FIXED_LEVEL, 255}              // 15
};

static constexpr const char* ccNames[NUM_PAGES][PARAMS_PER_PAGE] = {
    {"OSC1 Wave", "OSC1 Pitch", "OSC1 Fine", "OSC1 Det"},
    {"Osc1 Mix", "SupSaw Det1", "SupSaw Mix1", "-"},
    {"Freq DC1", "Shape DC1", "Ring Mix1", "-"},

    {"OSC2 Wave", "OSC2 Pitch", "OSC2 Fine", "OSC2 Det"},
    {"Osc2 Mix", "SupSaw Det2", "SupSaw Mix2", "Noise Mix"},
    {"Freq DC2", "Shape DC2", "Ring Mix2", "-"},

    {"Sub Mix", "-", "-", "-"},

    {"Cutoff", "Resonance", "Filt Env Amt", "Key Track"},
    {"-", "Oct Ctrl", "-", "-"},

    {"Amp Att", "Amp Dec", "Amp Sus", "Amp Rel"},
    {"Filt Att", "Filt Dec", "Filt Sus", "Filt Rel"},

    {"LFO1 Freq", "LFO1 Depth", "LFO1 Dest", "LFO1 Wave"},
    {"LFO2 Freq", "LFO2 Depth", "LFO2 Dest", "LFO2 Wave"},

    {"Rev Size", "Rev Damp", "Delay Time", "Delay FB"},
    {"Dry Mix", "Rev Mix", "Delay Mix", "-"},

    {"Glide En", "Glide Time", "Amp Mod DC", "-"}
};

} // namespace UIPage
