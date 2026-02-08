/*
 * CCDispatch.h
 *
 * Table-driven MIDI CC dispatch system for JT-4000.
 * Replaces giant switch statement with O(1) lookup table.
 * 
 * PERFORMANCE BENEFITS:
 * - Direct indexed lookup vs linear switch scanning
 * - Better instruction cache utilization (smaller code)
 * - Easier to maintain (add CC = add table entry)
 * - Compile-time verification of handler types
 *
 * USAGE:
 *   CCDispatch::handle(control, value, *synthEngine);
 */

#pragma once

#include <Arduino.h>
#include "CCDefs.h"
#include "Mapping.h"
#include "WaveForms.h"
#include "DebugTrace.h"

// Forward declare SynthEngine to avoid circular dependency
class SynthEngine;

namespace CCDispatch {

// -------------------------------------------------------------------------
// CC handler function types
// -------------------------------------------------------------------------
// Each handler receives the raw CC value (0..127) and SynthEngine pointer
using HandlerFn = void (*)(uint8_t ccVal, SynthEngine* synth);

// -------------------------------------------------------------------------
// Handler implementations
// -------------------------------------------------------------------------
// Keep handlers small and inline for best performance.
// Complex logic belongs in SynthEngine methods.

// --- Oscillator waveforms ---
inline void handleOsc1Wave(uint8_t cc, SynthEngine* s) {
    WaveformType t = waveformFromCC(cc);
    s->setOsc1Waveform((int)t);
    JT_LOGF("[CC %u] OSC1 Wave -> %s\n", CC::OSC1_WAVE, waveformShortName(t));
}

inline void handleOsc2Wave(uint8_t cc, SynthEngine* s) {
    WaveformType t = waveformFromCC(cc);
    s->setOsc2Waveform((int)t);
    JT_LOGF("[CC %u] OSC2 Wave -> %s\n", CC::OSC2_WAVE, waveformShortName(t));
}

// --- Filter ---
inline void handleFilterCutoff(uint8_t cc, SynthEngine* s) {
    float hz = JT4000Map::cc_to_obxa_cutoff_hz(cc);
    s->setFilterCutoff(hz);
    JT_LOGF("[CC %u] Cutoff = %.2f Hz\n", CC::FILTER_CUTOFF, hz);
}

inline void handleFilterResonance(uint8_t cc, SynthEngine* s) {
    float r = JT4000Map::cc_to_obxa_res01(cc);
    s->setFilterResonance(r);
    JT_LOGF("[CC %u] Resonance = %.4f\n", CC::FILTER_RESONANCE, r);
}

// --- JPFX Effects ---
inline void handleFXBassGain(uint8_t cc, SynthEngine* s) {
    // Map 0..127 to -12..+12 dB
    float dB = (cc / 127.0f) * 24.0f - 12.0f;
    s->setFXBassGain(dB);
    JT_LOGF("[CC %u] Bass = %.1f dB\n", CC::FX_BASS_GAIN, dB);
}

inline void handleFXTrebleGain(uint8_t cc, SynthEngine* s) {
    // Map 0..127 to -12..+12 dB
    float dB = (cc / 127.0f) * 24.0f - 12.0f;
    s->setFXTrebleGain(dB);
    JT_LOGF("[CC %u] Treble = %.1f dB\n", CC::FX_TREBLE_GAIN, dB);
}

inline void handleFXModEffect(uint8_t cc, SynthEngine* s) {
    // Map 0..127 to -1..10 (12 buckets: off + 11 variations)
    int8_t variation = -1;
    if (cc > 0) {
        // Map 1..127 to 0..10
        variation = ((uint16_t)(cc - 1) * 11) / 127;
        if (variation > 10) variation = 10;
    }
    s->setFXModEffect(variation);
    JT_LOGF("[CC %u] Mod Effect = %d\n", CC::FX_MOD_EFFECT, variation);
}

inline void handleFXModMix(uint8_t cc, SynthEngine* s) {
    float mix = cc / 127.0f;
    s->setFXModMix(mix);
    JT_LOGF("[CC %u] Mod Mix = %.3f\n", CC::FX_MOD_MIX, mix);
}

inline void handleFXModRate(uint8_t cc, SynthEngine* s) {
    // Map 0..127 to 0..20 Hz (0 = use preset)
    float hz = (cc / 127.0f) * 20.0f;
    s->setFXModRate(hz);
    JT_LOGF("[CC %u] Mod Rate = %.2f Hz\n", CC::FX_MOD_RATE, hz);
}

inline void handleFXModFeedback(uint8_t cc, SynthEngine* s) {
    // Map 0..127 to -1..0.99 (0 = use preset, 1..127 = 0..0.99)
    float fb = -1.0f;
    if (cc > 0) {
        fb = ((cc - 1) / 126.0f) * 0.99f;
    }
    s->setFXModFeedback(fb);
    JT_LOGF("[CC %u] Mod FB = %.3f\n", CC::FX_MOD_FEEDBACK, fb);
}

inline void handleFXDelayEffect(uint8_t cc, SynthEngine* s) {
    // Map 0..127 to -1..4 (6 buckets: off + 5 variations)
    int8_t variation = -1;
    if (cc > 0) {
        // Map 1..127 to 0..4
        variation = ((uint16_t)(cc - 1) * 5) / 127;
        if (variation > 4) variation = 4;
    }
    s->setFXDelayEffect(variation);
    JT_LOGF("[CC %u] Delay Effect = %d\n", CC::FX_DELAY_EFFECT, variation);
}

inline void handleFXDelayMix(uint8_t cc, SynthEngine* s) {
    float mix = cc / 127.0f;
    s->setFXDelayMix(mix);
    JT_LOGF("[CC %u] Delay Mix = %.3f\n", CC::FX_DELAY_MIX, mix);
}

inline void handleFXDelayFeedback(uint8_t cc, SynthEngine* s) {
    // Map 0..127 to -1..0.99 (0 = use preset)
    float fb = -1.0f;
    if (cc > 0) {
        fb = ((cc - 1) / 126.0f) * 0.99f;
    }
    s->setFXDelayFeedback(fb);
    JT_LOGF("[CC %u] Delay FB = %.3f\n", CC::FX_DELAY_FEEDBACK, fb);
}

inline void handleFXDelayTime(uint8_t cc, SynthEngine* s) {
    // Map 0..127 to 0..1500ms (0 = use preset)
    float ms = (cc / 127.0f) * 1500.0f;
    s->setFXDelayTime(ms);
    JT_LOGF("[CC %u] Delay Time = %.1f ms\n", CC::FX_DELAY_TIME, ms);
}

inline void handleFXDryMix(uint8_t cc, SynthEngine* s) {
    float mix = cc / 127.0f;
    s->setFXDryMix(mix);
    JT_LOGF("[CC %u] Dry Mix = %.3f\n", CC::FX_DRY_MIX, mix);
}

// --- Envelopes (using time mapping) ---
inline void handleAmpAttack(uint8_t cc, SynthEngine* s) {
    float ms = JT4000Map::cc_to_time_ms(cc);
    s->setAmpAttack(ms);
    JT_LOGF("[CC %u] Amp Attack = %.2f ms\n", CC::AMP_ATTACK, ms);
}

inline void handleAmpDecay(uint8_t cc, SynthEngine* s) {
    float ms = JT4000Map::cc_to_time_ms(cc);
    s->setAmpDecay(ms);
    JT_LOGF("[CC %u] Amp Decay = %.2f ms\n", CC::AMP_DECAY, ms);
}

inline void handleAmpSustain(uint8_t cc, SynthEngine* s) {
    float norm = cc / 127.0f;
    s->setAmpSustain(norm);
    JT_LOGF("[CC %u] Amp Sustain = %.3f\n", CC::AMP_SUSTAIN, norm);
}

inline void handleAmpRelease(uint8_t cc, SynthEngine* s) {
    float ms = JT4000Map::cc_to_time_ms(cc);
    s->setAmpRelease(ms);
    JT_LOGF("[CC %u] Amp Release = %.2f ms\n", CC::AMP_RELEASE, ms);
}

// Additional handlers would follow same pattern...
// For brevity, showing key examples. Full implementation would cover all CCs.

// -------------------------------------------------------------------------
// Dispatch table
// -------------------------------------------------------------------------
// Indexed by CC number. nullptr = unmapped CC.
// This is a sparse array - only ~40 of 128 entries are used.
// Memory cost: 128 * sizeof(void*) = 512 bytes on Teensy (32-bit pointers)
//
// PERFORMANCE: Direct lookup is O(1) vs O(N) switch statement scan.
// Branch predictor loves this approach.

constexpr HandlerFn HANDLER_TABLE[128] = {
    // 0-20: Standard MIDI (unmapped)
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr,
    
    // 21-32: Oscillators and envelopes
    handleOsc1Wave,           // 21
    handleOsc2Wave,           // 22
    handleFilterCutoff,       // 23
    handleFilterResonance,    // 24
    handleAmpAttack,          // 25
    handleAmpDecay,           // 26
    handleAmpSustain,         // 27
    handleAmpRelease,         // 28
    nullptr,                  // 29 - FILTER_ENV_ATTACK (add handler)
    nullptr,                  // 30 - FILTER_ENV_DECAY
    nullptr,                  // 31 - FILTER_ENV_SUSTAIN
    nullptr,                  // 32 - FILTER_ENV_RELEASE
    
    // 33-69: Various synth params (would be filled in)
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, // 33-40
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, // 41-48
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, // 49-56
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, // 57-64
    nullptr, nullptr, nullptr, nullptr, nullptr,                             // 65-69
    
    // 70-80: JPFX Effects
    handleFXBassGain,         // 70
    handleFXTrebleGain,       // 71
    handleFXModEffect,        // 72
    handleFXModMix,           // 73
    handleFXModRate,          // 74
    handleFXModFeedback,      // 75
    handleFXDelayEffect,      // 76
    handleFXDelayMix,         // 77
    handleFXDelayFeedback,    // 78
    handleFXDelayTime,        // 79
    handleFXDryMix,           // 80
    
    // 81-127: Remaining params (would be filled in)
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, // 81-88
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, // 89-96
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, // 97-104
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, // 105-112
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, // 113-120
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr           // 121-127
};

// -------------------------------------------------------------------------
// Main dispatch function
// -------------------------------------------------------------------------
inline void handle(uint8_t cc, uint8_t value, SynthEngine* synth) {
    if (cc >= 128) return; // Safety check
    
    HandlerFn handler = HANDLER_TABLE[cc];
    if (handler) {
        handler(value, synth);
    } else {
        // Unmapped CC - could log or ignore
        JT_LOGF("[CC %u] Unmapped (value=%u)\n", cc, value);
    }
}

} // namespace CCDispatch
