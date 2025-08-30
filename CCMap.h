#pragma once

const int NUM_PAGES = 16;
const int PARAMS_PER_PAGE = 4;

const byte ccMap[NUM_PAGES][PARAMS_PER_PAGE] = {
    {21, 41, 45, 43},    // Page 0: OSC1 Core - Waveform, Pitch, Fine, Detune
    {47, 60, 77, 78},    // Page 1: OSC1 Mix - Mix, Supersaw Detune, Supersaw Mix
    {86, 87, 91, 255},   // Page 2: OSC1 Mod - Freq DC, Shape DC, Ring Mix

    {22, 42, 46, 44},    // Page 3: OSC2 Core - Waveform, Pitch, Fine, Detune
    {61, 79, 80, 59},    // Page 4: OSC2 Mix - Mix, Supersaw Detune, Supersaw Mix, Noise Mix
    {88, 89, 92, 255},   // Page 5: OSC2 Mod - Freq DC, Shape DC, Ring Mix

    {58, 255, 255, 255}, // Page 6: Sub Osc & Spare

    {23, 24, 48, 50},    // Page 7: Filter - Cutoff, Res, Env Amt, KeyTrack
    {255, 84, 255, 255},   // Page 8: Filter Ext - Drive, Oct Ctrl, Passband Gain

    {25, 26, 27, 28},    // Page 9: Amp Env - A, D, S, R
    {29, 30, 31, 32},    // Page 10: Filter Env - A, D, S, R

    {54, 55, 56, 62},    // Page 11: LFO1 - Freq, Depth, Dest, Waveform
    {51, 52, 53, 63},    // Page 12: LFO2 - Freq, Depth, Dest, Waveform

    {70, 71, 72, 73},    // Page 13: FX - Reverb Size, Damping, Delay Time, Feedback
    {74, 75, 76, 255},   // Page 14: FX Mix - Dry, Reverb, Delay

    {81, 82, 90, 255}    // Page 15: Glide & Global Mod - Glide En, Glide Time, Amp Mod DC
};

const char* const ccNames[NUM_PAGES][PARAMS_PER_PAGE] = {
    {"OSC1 Wave", "OSC1 Pitch", "OSC1 Fine", "OSC1 Det"},
    {"Osc1 Mix", "SupSaw Det1", "SupSaw Mix1", "-"},
    {"Freq DC1", "Shape DC1", "Ring Mix1", "-"},

    {"OSC2 Wave", "OSC2 Pitch", "OSC2 Fine", "OSC2 Det"},
    {"Osc2 Mix", "SupSaw Det2", "SupSaw Mix2", "Noise Mix"},
    {"Freq DC2", "Shape DC2", "Ring Mix2", "-"},

    {"Sub Mix", "-", "-", "-"},

    {"Cutoff", "Resonance", "Filt Env Amt", "Key Track"},
    {"-", "Oct Ctrl", "", "-"},

    {"Amp Att", "Amp Dec", "Amp Sus", "Amp Rel"},
    {"Filt Att", "Filt Dec", "Filt Sus", "Filt Rel"},

    {"LFO1 Freq", "LFO1 Depth", "LFO1 Dest", "LFO1 Wave"},
    {"LFO2 Freq", "LFO2 Depth", "LFO2 Dest", "LFO2 Wave"},

    {"Rev Size", "Rev Damp", "Delay Time", "Delay FB"},
    {"Dry Mix", "Rev Mix", "Delay Mix", "-"},

    {"Glide En", "Glide Time", "Amp Mod DC", "-"}
};
