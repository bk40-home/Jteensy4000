#pragma once
// -----------------------------------------------------------------------------
// Waveforms.h — Teensy Audio waveforms + JT-4000 Supersaw extension
// -----------------------------------------------------------------------------
// - Mirrors Teensy Audio Library waveform IDs exactly.
// - Adds WAVEFORM_SUPERSAW (100) as a project-local extra.
// - Use Pulse + pulseWidth() for duty control (no separate PWM type).
// - Header-only utilities: names, CC mapping, and helpers.
// -----------------------------------------------------------------------------

#include <Arduino.h>
#include <Audio.h>

// Teensy canonical IDs for reference (already provided by Audio.h):
// 0  SINE
// 1  SAWTOOTH
// 2  SQUARE
// 3  TRIANGLE
// 4  ARBITRARY
// 5  PULSE
// 6  SAWTOOTH_REVERSE
// 7  SAMPLE_HOLD
// 8  TRIANGLE_VARIABLE
// 9  BANDLIMIT_SAWTOOTH
// 10 BANDLIMIT_SAWTOOTH_REVERSE
// 11 BANDLIMIT_SQUARE
// 12 BANDLIMIT_PULSE

// Project-local extension (keep outside 0..12 range)
static constexpr uint8_t WAVEFORM_SUPERSAW = 100;

// Unified enum (explicit values keep 1:1 mapping with Teensy)
enum WaveformType : uint8_t {
    WAVE_SINE                         = WAVEFORM_SINE,
    WAVE_SAW                          = WAVEFORM_SAWTOOTH,
    WAVE_SQUARE                       = WAVEFORM_SQUARE,
    WAVE_TRIANGLE                     = WAVEFORM_TRIANGLE,
    WAVE_ARBITRARY                    = WAVEFORM_ARBITRARY,
    WAVE_PULSE                        = WAVEFORM_PULSE,              // adjust via pulseWidth()
    WAVE_SAW_REVERSE                  = WAVEFORM_SAWTOOTH_REVERSE,
    WAVE_SAMPLE_HOLD                  = WAVEFORM_SAMPLE_HOLD,
    WAVE_TRIANGLE_VARIABLE            = WAVEFORM_TRIANGLE_VARIABLE,
    WAVE_BANDLIMIT_SAW                = WAVEFORM_BANDLIMIT_SAWTOOTH,
    WAVE_BANDLIMIT_SAW_REVERSE        = WAVEFORM_BANDLIMIT_SAWTOOTH_REVERSE,
    WAVE_BANDLIMIT_SQUARE             = WAVEFORM_BANDLIMIT_SQUARE,
    WAVE_BANDLIMIT_PULSE              = WAVEFORM_BANDLIMIT_PULSE,
    // JT-4000 custom:
    WAVE_SUPERSAW                     = WAVEFORM_SUPERSAW
};

// Core Teensy shapes (documented order)
static constexpr WaveformType waveformListCore[] = {
    WAVE_SINE,
    WAVE_SAW,
    WAVE_SQUARE,
    WAVE_TRIANGLE,
    WAVE_ARBITRARY,
    WAVE_PULSE,
    WAVE_SAW_REVERSE,
    WAVE_SAMPLE_HOLD,
    WAVE_TRIANGLE_VARIABLE,
    WAVE_BANDLIMIT_SAW,
    WAVE_BANDLIMIT_SAW_REVERSE,
    WAVE_BANDLIMIT_SQUARE,
    WAVE_BANDLIMIT_PULSE
};
static constexpr size_t numWaveformsCore = sizeof(waveformListCore) / sizeof(waveformListCore[0]);

// Full list incl. Supersaw (placed last)
static constexpr WaveformType waveformListAll[] = {
    WAVE_SINE,
    WAVE_SAW,
    WAVE_SQUARE,
    WAVE_TRIANGLE,
    WAVE_ARBITRARY,
    WAVE_PULSE,
    WAVE_SAW_REVERSE,
    WAVE_SAMPLE_HOLD,
    WAVE_TRIANGLE_VARIABLE,
    WAVE_BANDLIMIT_SAW,
    WAVE_BANDLIMIT_SAW_REVERSE,
    WAVE_BANDLIMIT_SQUARE,
    WAVE_BANDLIMIT_PULSE,
    WAVE_SUPERSAW
};
static constexpr size_t numWaveformsAll = sizeof(waveformListAll) / sizeof(waveformListAll[0]);

// UI names (short)
static constexpr const char* waveShortNames[numWaveformsAll] = {
    "SIN", "SAW", "SQR", "TRI", "ARB", "PLS", "rSAW", "S&H", "vTRI",
    "BLS", "rBLS", "BLSQ", "BLP", "SSAW"
};
// UI names (long)
static constexpr const char* waveLongNames[numWaveformsAll] = {
    "Sine", "Sawtooth", "Square", "Triangle", "Arbitrary", "Pulse",
    "Saw Reverse", "Sample & Hold", "Triangle Variable",
    "Bandlimited Saw", "Bandlimited Saw Reverse", "Bandlimited Square", "Bandlimited Pulse",
    "Supersaw"
};

// Name helpers
inline const char* waveformShortName(WaveformType t) {
    for (size_t i = 0; i < numWaveformsAll; ++i) if (waveformListAll[i] == t) return waveShortNames[i];
    return "???";
}
inline const char* waveformLongName(WaveformType t) {
    for (size_t i = 0; i < numWaveformsAll; ++i) if (waveformListAll[i] == t) return waveLongNames[i];
    return "Unknown";
}

// Capabilities
inline bool isBandlimited(WaveformType t) {
    return (t == WAVE_BANDLIMIT_SAW) || (t == WAVE_BANDLIMIT_SAW_REVERSE) ||
           (t == WAVE_BANDLIMIT_SQUARE) || (t == WAVE_BANDLIMIT_PULSE);
}
inline bool isStandardTeensyWave(WaveformType t) {
    return (t != WAVE_SUPERSAW);
}
inline bool supportsPulseWidth(WaveformType t) {
    return (t == WAVE_PULSE) || (t == WAVE_BANDLIMIT_PULSE);
}

// CC mapping (use full list including Supersaw)
inline WaveformType waveformFromCC(uint8_t cc) {
    uint8_t idx = (static_cast<uint16_t>(cc) * numWaveformsAll) / 128;
    if (idx >= numWaveformsAll) idx = numWaveformsAll - 1;
    return waveformListAll[idx];
}
inline uint8_t ccFromWaveform(WaveformType t) {
    for (size_t i = 0; i < numWaveformsAll; ++i) {
        if (waveformListAll[i] == t) {
            const uint16_t start = (i    * 128u) / numWaveformsAll;
            const uint16_t end   = ((i+1)* 128u) / numWaveformsAll;
            return static_cast<uint8_t>((start + end) / 2);
        }
    }
    return 0;
}


// Helpers to apply to Teensy oscillators
template <typename TWaveObj>
inline void setWaveformIfStandard(TWaveObj& osc, WaveformType t) {
    if (isStandardTeensyWave(t)) {
        osc.begin(static_cast<uint8_t>(t));
    }
}

inline uint8_t beginCodeOrFF(WaveformType t) {
    return isStandardTeensyWave(t) ? static_cast<uint8_t>(t) : 0xFF;
}
