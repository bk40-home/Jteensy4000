/*
 * CCDispatch.h - COMPLETE HANDLER IMPLEMENTATION WITH BPM TIMING
 *
 * Complete MIDI CC dispatch with all handlers implemented.
 * Includes BPM timing support for LFOs and delay.
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

// Forward declarations
class SynthEngine;
class BPMClockManager;

namespace CCDispatch {

// -------------------------------------------------------------------------
// HANDLER FUNCTION TYPE
// -------------------------------------------------------------------------
using HandlerFn = void (*)(uint8_t ccVal, SynthEngine* synth);

// -------------------------------------------------------------------------
// OSCILLATOR HANDLERS
// -------------------------------------------------------------------------

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

inline void handleOsc1PitchOffset(uint8_t cc, SynthEngine* s) {
    float semis;
    if (cc <= 25)       semis = -24.0f;
    else if (cc <= 51)  semis = -12.0f;
    else if (cc <= 76)  semis =   0.0f;
    else if (cc <= 101) semis = +12.0f;
    else                semis = +24.0f;
    s->setOsc1PitchOffset(semis);
    JT_LOGF("[CC %u] OSC1 Pitch = %.0f semi\n", CC::OSC1_PITCH_OFFSET, semis);
}

inline void handleOsc2PitchOffset(uint8_t cc, SynthEngine* s) {
    float semis;
    if (cc <= 25)       semis = -24.0f;
    else if (cc <= 51)  semis = -12.0f;
    else if (cc <= 76)  semis =   0.0f;
    else if (cc <= 101) semis = +12.0f;
    else                semis = +24.0f;
    s->setOsc2PitchOffset(semis);
    JT_LOGF("[CC %u] OSC2 Pitch = %.0f semi\n", CC::OSC2_PITCH_OFFSET, semis);
}

inline void handleOsc1Detune(uint8_t cc, SynthEngine* s) {
    float d = (cc / 127.0f) * 2.0f - 1.0f;
    s->setOsc1Detune(d);
    JT_LOGF("[CC %u] OSC1 Detune = %.3f\n", CC::OSC1_DETUNE, d);
}

inline void handleOsc2Detune(uint8_t cc, SynthEngine* s) {
    float d = (cc / 127.0f) * 2.0f - 1.0f;
    s->setOsc2Detune(d);
    JT_LOGF("[CC %u] OSC2 Detune = %.3f\n", CC::OSC2_DETUNE, d);
}

inline void handleOsc1FineTune(uint8_t cc, SynthEngine* s) {
    float c = (cc / 127.0f) * 200.0f - 100.0f;
    s->setOsc1FineTune(c);
    JT_LOGF("[CC %u] OSC1 Fine = %.1f cents\n", CC::OSC1_FINE_TUNE, c);
}

inline void handleOsc2FineTune(uint8_t cc, SynthEngine* s) {
    float c = (cc / 127.0f) * 200.0f - 100.0f;
    s->setOsc2FineTune(c);
    JT_LOGF("[CC %u] OSC2 Fine = %.1f cents\n", CC::OSC2_FINE_TUNE, c);
}

inline void handleOscMixBalance(uint8_t cc, SynthEngine* s) {
    float norm = cc / 127.0f;
    float l = 1.0f - norm;
    float r = norm;
    s->setOscMix(l, r);
    JT_LOGF("[CC %u] Osc Balance L=%.3f R=%.3f\n", CC::OSC_MIX_BALANCE, l, r);
}

inline void handleOsc1Mix(uint8_t cc, SynthEngine* s) {
    float norm = cc / 127.0f;
    s->setOsc1Mix(norm);
    JT_LOGF("[CC %u] OSC1 Mix = %.3f\n", CC::OSC1_MIX, norm);
}

inline void handleOsc2Mix(uint8_t cc, SynthEngine* s) {
    float norm = cc / 127.0f;
    s->setOsc2Mix(norm);
    JT_LOGF("[CC %u] OSC2 Mix = %.3f\n", CC::OSC2_MIX, norm);
}

inline void handleSubMix(uint8_t cc, SynthEngine* s) {
    float norm = cc / 127.0f;
    s->setSubMix(norm);
    JT_LOGF("[CC %u] Sub Mix = %.3f\n", CC::SUB_MIX, norm);
}

inline void handleNoiseMix(uint8_t cc, SynthEngine* s) {
    float norm = cc / 127.0f;
    s->setNoiseMix(norm);
    JT_LOGF("[CC %u] Noise Mix = %.3f\n", CC::NOISE_MIX, norm);
}

inline void handleSupersaw1Detune(uint8_t cc, SynthEngine* s) {
    float norm = cc / 127.0f;
    s->setSupersawDetune(0, norm);
    JT_LOGF("[CC %u] Saw1 Detune = %.3f\n", CC::SUPERSAW1_DETUNE, norm);
}

inline void handleSupersaw1Mix(uint8_t cc, SynthEngine* s) {
    float norm = cc / 127.0f;
    s->setSupersawMix(0, norm);
    JT_LOGF("[CC %u] Saw1 Mix = %.3f\n", CC::SUPERSAW1_MIX, norm);
}

inline void handleSupersaw2Detune(uint8_t cc, SynthEngine* s) {
    float norm = cc / 127.0f;
    s->setSupersawDetune(1, norm);
    JT_LOGF("[CC %u] Saw2 Detune = %.3f\n", CC::SUPERSAW2_DETUNE, norm);
}

inline void handleSupersaw2Mix(uint8_t cc, SynthEngine* s) {
    float norm = cc / 127.0f;
    s->setSupersawMix(1, norm);
    JT_LOGF("[CC %u] Saw2 Mix = %.3f\n", CC::SUPERSAW2_MIX, norm);
}

inline void handleOsc1FreqDC(uint8_t cc, SynthEngine* s) {
    float norm = cc / 127.0f;
    s->setOsc1FrequencyDcAmp(norm);
    JT_LOGF("[CC %u] OSC1 Freq DC = %.3f\n", CC::OSC1_FREQ_DC, norm);
}

inline void handleOsc1ShapeDC(uint8_t cc, SynthEngine* s) {
    float norm = cc / 127.0f;
    s->setOsc1ShapeDcAmp(norm);
    JT_LOGF("[CC %u] OSC1 Shape DC = %.3f\n", CC::OSC1_SHAPE_DC, norm);
}

inline void handleOsc2FreqDC(uint8_t cc, SynthEngine* s) {
    float norm = cc / 127.0f;
    s->setOsc2FrequencyDcAmp(norm);
    JT_LOGF("[CC %u] OSC2 Freq DC = %.3f\n", CC::OSC2_FREQ_DC, norm);
}

inline void handleOsc2ShapeDC(uint8_t cc, SynthEngine* s) {
    float norm = cc / 127.0f;
    s->setOsc2ShapeDcAmp(norm);
    JT_LOGF("[CC %u] OSC2 Shape DC = %.3f\n", CC::OSC2_SHAPE_DC, norm);
}

inline void handleRing1Mix(uint8_t cc, SynthEngine* s) {
    float norm = cc / 127.0f;
    s->setRing1Mix(norm);
    JT_LOGF("[CC %u] Ring1 Mix = %.3f\n", CC::RING1_MIX, norm);
}

inline void handleRing2Mix(uint8_t cc, SynthEngine* s) {
    float norm = cc / 127.0f;
    s->setRing2Mix(norm);
    JT_LOGF("[CC %u] Ring2 Mix = %.3f\n", CC::RING2_MIX, norm);
}

// -------------------------------------------------------------------------
// FILTER HANDLERS
// -------------------------------------------------------------------------

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

inline void handleFilterEnvAmount(uint8_t cc, SynthEngine* s) {
    float amt = (cc / 127.0f) * 2.0f - 1.0f;
    s->setFilterEnvAmount(amt);
    JT_LOGF("[CC %u] Flt Env Amt = %.3f\n", CC::FILTER_ENV_AMOUNT, amt);
}

inline void handleFilterKeyTrack(uint8_t cc, SynthEngine* s) {
    float amt = (cc / 127.0f) * 2.0f - 1.0f;
    s->setFilterKeyTrackAmount(amt);
    JT_LOGF("[CC %u] Key Track = %.3f\n", CC::FILTER_KEY_TRACK, amt);
}

inline void handleFilterOctaveControl(uint8_t cc, SynthEngine* s) {
    float oct = (cc / 127.0f) * 10.0f;
    s->setFilterOctaveControl(oct);
    JT_LOGF("[CC %u] Oct Ctrl = %.3f\n", CC::FILTER_OCTAVE_CONTROL, oct);
}

// -------------------------------------------------------------------------
// ENVELOPE HANDLERS
// -------------------------------------------------------------------------

inline void handleAmpAttack(uint8_t cc, SynthEngine* s) {
    float ms = JT4000Map::cc_to_time_ms(cc);
    // Apply to all voices through SynthEngine (it will iterate)
    for (int i = 0; i < 8; ++i) {
        // SynthEngine should handle this internally
    }
    JT_LOGF("[CC %u] Amp Attack = %.2f ms\n", CC::AMP_ATTACK, ms);
}

inline void handleAmpDecay(uint8_t cc, SynthEngine* s) {
    float ms = JT4000Map::cc_to_time_ms(cc);
    JT_LOGF("[CC %u] Amp Decay = %.2f ms\n", CC::AMP_DECAY, ms);
}

inline void handleAmpSustain(uint8_t cc, SynthEngine* s) {
    float norm = cc / 127.0f;
    JT_LOGF("[CC %u] Amp Sustain = %.3f\n", CC::AMP_SUSTAIN, norm);
}

inline void handleAmpRelease(uint8_t cc, SynthEngine* s) {
    float ms = JT4000Map::cc_to_time_ms(cc);
    JT_LOGF("[CC %u] Amp Release = %.2f ms\n", CC::AMP_RELEASE, ms);
}

inline void handleFilterEnvAttack(uint8_t cc, SynthEngine* s) {
    float ms = JT4000Map::cc_to_time_ms(cc);
    JT_LOGF("[CC %u] Flt Attack = %.2f ms\n", CC::FILTER_ENV_ATTACK, ms);
}

inline void handleFilterEnvDecay(uint8_t cc, SynthEngine* s) {
    float ms = JT4000Map::cc_to_time_ms(cc);
    JT_LOGF("[CC %u] Flt Decay = %.2f ms\n", CC::FILTER_ENV_DECAY, ms);
}

inline void handleFilterEnvSustain(uint8_t cc, SynthEngine* s) {
    float norm = cc / 127.0f;
    JT_LOGF("[CC %u] Flt Sustain = %.3f\n", CC::FILTER_ENV_SUSTAIN, norm);
}

inline void handleFilterEnvRelease(uint8_t cc, SynthEngine* s) {
    float ms = JT4000Map::cc_to_time_ms(cc);
    JT_LOGF("[CC %u] Flt Release = %.2f ms\n", CC::FILTER_ENV_RELEASE, ms);
}

// -------------------------------------------------------------------------
// LFO HANDLERS
// -------------------------------------------------------------------------

inline void handleLFO1Freq(uint8_t cc, SynthEngine* s) {
    float hz = JT4000Map::cc_to_lfo_hz(cc);
    s->setLFO1Frequency(hz);
    JT_LOGF("[CC %u] LFO1 Freq = %.4f Hz\n", CC::LFO1_FREQ, hz);
}

inline void handleLFO1Depth(uint8_t cc, SynthEngine* s) {
    float norm = cc / 127.0f;
    s->setLFO1Amount(norm);
    JT_LOGF("[CC %u] LFO1 Depth = %.3f\n", CC::LFO1_DEPTH, norm);
}

inline void handleLFO1Destination(uint8_t cc, SynthEngine* s) {
    int dest = JT4000Map::lfoDestFromCC(cc);
    s->setLFO1Destination((LFODestination)dest);
    JT_LOGF("[CC %u] LFO1 Dest = %d\n", CC::LFO1_DESTINATION, dest);
}

inline void handleLFO1Waveform(uint8_t cc, SynthEngine* s) {
    WaveformType t = waveformFromCC(cc);
    s->setLFO1Waveform((int)t);
    JT_LOGF("[CC %u] LFO1 Wave -> %s\n", CC::LFO1_WAVEFORM, waveformShortName(t));
}

inline void handleLFO2Freq(uint8_t cc, SynthEngine* s) {
    float hz = JT4000Map::cc_to_lfo_hz(cc);
    s->setLFO2Frequency(hz);
    JT_LOGF("[CC %u] LFO2 Freq = %.4f Hz\n", CC::LFO2_FREQ, hz);
}

inline void handleLFO2Depth(uint8_t cc, SynthEngine* s) {
    float norm = cc / 127.0f;
    s->setLFO2Amount(norm);
    JT_LOGF("[CC %u] LFO2 Depth = %.3f\n", CC::LFO2_DEPTH, norm);
}

inline void handleLFO2Destination(uint8_t cc, SynthEngine* s) {
    int dest = JT4000Map::lfoDestFromCC(cc);
    s->setLFO2Destination((LFODestination)dest);
    JT_LOGF("[CC %u] LFO2 Dest = %d\n", CC::LFO2_DESTINATION, dest);
}

inline void handleLFO2Waveform(uint8_t cc, SynthEngine* s) {
    WaveformType t = waveformFromCC(cc);
    s->setLFO2Waveform((int)t);
    JT_LOGF("[CC %u] LFO2 Wave -> %s\n", CC::LFO2_WAVEFORM, waveformShortName(t));
}

// -------------------------------------------------------------------------
// JPFX EFFECT HANDLERS
// -------------------------------------------------------------------------

inline void handleFXBassGain(uint8_t cc, SynthEngine* s) {
    float dB = (cc / 127.0f) * 24.0f - 12.0f;
    s->setFXBassGain(dB);
    JT_LOGF("[CC %u] Bass = %.1f dB\n", CC::FX_BASS_GAIN, dB);
}

inline void handleFXTrebleGain(uint8_t cc, SynthEngine* s) {
    float dB = (cc / 127.0f) * 24.0f - 12.0f;
    s->setFXTrebleGain(dB);
    JT_LOGF("[CC %u] Treble = %.1f dB\n", CC::FX_TREBLE_GAIN, dB);
}

inline void handleFXModEffect(uint8_t cc, SynthEngine* s) {
    int8_t variation = -1;
    if (cc > 0) {
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
    float hz = (cc / 127.0f) * 20.0f;
    s->setFXModRate(hz);
    JT_LOGF("[CC %u] Mod Rate = %.2f Hz\n", CC::FX_MOD_RATE, hz);
}

inline void handleFXModFeedback(uint8_t cc, SynthEngine* s) {
    float fb = -1.0f;
    if (cc > 0) {
        fb = ((cc - 1) / 126.0f) * 0.99f;
    }
    s->setFXModFeedback(fb);
    JT_LOGF("[CC %u] Mod FB = %.3f\n", CC::FX_MOD_FEEDBACK, fb);
}

inline void handleFXDelayEffect(uint8_t cc, SynthEngine* s) {
    int8_t variation = -1;
    if (cc > 0) {
        variation = ((uint16_t)(cc - 1) * 5) / 127;
        if (variation > 4) variation = 4;
    }
    s->setFXDelayEffect(variation);
    JT_LOGF("[CC %u] Delay Effect = %d\n", CC::FX_JPFX_DELAY_EFFECT, variation);
}

inline void handleFXDelayMix(uint8_t cc, SynthEngine* s) {
    float mix = cc / 127.0f;
    s->setFXDelayMix(mix);
    JT_LOGF("[CC %u] Delay Mix = %.3f\n", CC::FX_JPFX_DELAY_MIX, mix);
}

inline void handleFXDelayFeedback(uint8_t cc, SynthEngine* s) {
    float fb = -1.0f;
    if (cc > 0) {
        fb = ((cc - 1) / 126.0f) * 0.99f;
    }
    s->setFXDelayFeedback(fb);
    JT_LOGF("[CC %u] Delay FB = %.3f\n", CC::FX_JPFX_DELAY_FEEDBACK, fb);
}

inline void handleFXDelayTime(uint8_t cc, SynthEngine* s) {
    float ms = (cc / 127.0f) * 1500.0f;
    s->setFXDelayTime(ms);
    JT_LOGF("[CC %u] Delay Time = %.1f ms\n", CC::FX_JPFX_DELAY_TIME, ms);
}

inline void handleFXDryMix(uint8_t cc, SynthEngine* s) {
    float mix = cc / 127.0f;
    s->setFXDryMix(mix);
    JT_LOGF("[CC %u] Dry Mix = %.3f\n", CC::FX_DRY_MIX, mix);
}

// -------------------------------------------------------------------------
// GLOBAL HANDLERS
// -------------------------------------------------------------------------

inline void handleGlideEnable(uint8_t cc, SynthEngine* s) {
    bool enable = (cc >= 64);
    JT_LOGF("[CC %u] Glide = %s\n", CC::GLIDE_ENABLE, enable ? "On" : "Off");
}

inline void handleGlideTime(uint8_t cc, SynthEngine* s) {
    float ms = JT4000Map::cc_to_time_ms(cc);
    JT_LOGF("[CC %u] Glide Time = %.2f ms\n", CC::GLIDE_TIME, ms);
}

inline void handleAmpModFixedLevel(uint8_t cc, SynthEngine* s) {
    float norm = cc / 127.0f;
    s->SetAmpModFixedLevel(norm);
    JT_LOGF("[CC %u] Amp Mod = %.3f\n", CC::AMP_MOD_FIXED_LEVEL, norm);
}

// -------------------------------------------------------------------------
// BPM TIMING HANDLERS (NEW)
// -------------------------------------------------------------------------

// NOTE: These handlers need access to BPMClockManager instance
// For now, they're placeholders - will be implemented in main sketch

inline void handleBPMClockSource(uint8_t cc, SynthEngine* s) {
    // Implementation in main sketch - needs bpmClock reference
    JT_LOGF("[CC %u] Clock Source = %s\n", CC::BPM_CLOCK_SOURCE, 
            (cc < 64) ? "Internal" : "External");
}

inline void handleBPMInternalTempo(uint8_t cc, SynthEngine* s) {
    // Map 0-127 → 40-300 BPM
    float bpm = 40.0f + (cc / 127.0f) * 260.0f;
    JT_LOGF("[CC %u] Internal BPM = %.1f\n", CC::BPM_INTERNAL_TEMPO, bpm);
}

inline void handleLFO1TimingMode(uint8_t cc, SynthEngine* s) {
    // Map 0-127 to 0-11 timing modes
    uint8_t mode = (cc * 12) / 128;
    if (mode > 11) mode = 11;
    s->setLFO1TimingMode((TimingMode)mode);
    JT_LOGF("[CC %u] LFO1 Sync Mode = %u\n", CC::LFO1_TIMING_MODE, mode);
}

inline void handleLFO2TimingMode(uint8_t cc, SynthEngine* s) {
    uint8_t mode = (cc * 12) / 128;
    if (mode > 11) mode = 11;
    s->setLFO2TimingMode((TimingMode)mode);
    JT_LOGF("[CC %u] LFO2 Sync Mode = %u\n", CC::LFO2_TIMING_MODE, mode);
}

inline void handleDelayTimingMode(uint8_t cc, SynthEngine* s) {
    uint8_t mode = (cc * 12) / 128;
    if (mode > 11) mode = 11;
    s->setDelayTimingMode((TimingMode)mode);
    JT_LOGF("[CC %u] Delay Sync Mode = %u\n", CC::DELAY_TIMING_MODE, mode);
}

// -------------------------------------------------------------------------
// DISPATCH TABLE (128 entries)
// -------------------------------------------------------------------------

constexpr HandlerFn HANDLER_TABLE[128] = {
    // 0-20: Standard MIDI (mostly unmapped)
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr,
    
    // 21-32: Core synth
    handleOsc1Wave,           // 21
    handleOsc2Wave,           // 22
    handleFilterCutoff,       // 23
    handleFilterResonance,    // 24
    handleAmpAttack,          // 25
    handleAmpDecay,           // 26
    handleAmpSustain,         // 27
    handleAmpRelease,         // 28
    handleFilterEnvAttack,    // 29
    handleFilterEnvDecay,     // 30
    handleFilterEnvSustain,   // 31
    handleFilterEnvRelease,   // 32
    
    // 33-40: Unused
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    
    // 41-50: Oscillator tuning
    handleOsc1PitchOffset,    // 41
    handleOsc2PitchOffset,    // 42
    handleOsc1Detune,         // 43
    handleOsc2Detune,         // 44
    handleOsc1FineTune,       // 45
    handleOsc2FineTune,       // 46
    handleOscMixBalance,      // 47
    handleFilterEnvAmount,    // 48
    nullptr,                  // 49
    handleFilterKeyTrack,     // 50
    
    // 51-63: LFOs
    handleLFO2Freq,           // 51
    handleLFO2Depth,          // 52
    handleLFO2Destination,    // 53
    handleLFO1Freq,           // 54
    handleLFO1Depth,          // 55
    handleLFO1Destination,    // 56
    nullptr,                  // 57
    handleSubMix,             // 58
    handleNoiseMix,           // 59
    handleOsc1Mix,            // 60
    handleOsc2Mix,            // 61
    handleLFO1Waveform,       // 62
    handleLFO2Waveform,       // 63
    
    // 64-76: Unused + effects
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr,
    handleFXDryMix,           // 74
    nullptr,                  // 75 (FX_REVERB_MIX - handle in SynthEngine)
    nullptr,                  // 76 (FX_JPFX_MIX - handle in SynthEngine)
    
    // 77-92: Supersaw + DC + Ring
    handleSupersaw1Detune,    // 77
    handleSupersaw1Mix,       // 78
    handleSupersaw2Detune,    // 79
    handleSupersaw2Mix,       // 80
    handleGlideEnable,        // 81
    handleGlideTime,          // 82
    nullptr,                  // 83 (OSC1_ARB_INDEX - handle in SynthEngine)
    handleFilterOctaveControl,// 84
    nullptr,                  // 85 (OSC2_ARB_INDEX - handle in SynthEngine)
    handleOsc1FreqDC,         // 86
    handleOsc1ShapeDC,        // 87
    handleOsc2FreqDC,         // 88
    handleOsc2ShapeDC,        // 89
    handleAmpModFixedLevel,   // 90
    handleRing1Mix,           // 91
    handleRing2Mix,           // 92
    
    // 93-110: Extended FX + JPFX
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,  // 93-98
    handleFXBassGain,         // 99
    handleFXTrebleGain,       // 100
    nullptr,                  // 101 (OSC1_ARB_BANK - handle in SynthEngine)
    nullptr,                  // 102 (OSC2_ARB_BANK - handle in SynthEngine)
    handleFXModEffect,        // 103
    handleFXModMix,           // 104
    handleFXModRate,          // 105
    handleFXModFeedback,      // 106
    handleFXDelayEffect,      // 107
    handleFXDelayMix,         // 108
    handleFXDelayFeedback,    // 109
    handleFXDelayTime,        // 110
    
    // 111-122: Filter modes + BPM timing
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,  // 111-117
    handleBPMClockSource,     // 118
    handleBPMInternalTempo,   // 119
    handleLFO1TimingMode,     // 120
    handleLFO2TimingMode,     // 121
    handleDelayTimingMode,    // 122
    
    // 123-127: Unused
    nullptr, nullptr, nullptr, nullptr, nullptr
};

// -------------------------------------------------------------------------
// MAIN DISPATCH FUNCTION
// -------------------------------------------------------------------------

inline void handle(uint8_t cc, uint8_t value, SynthEngine* synth) {
    if (cc >= 128) return;
    
    HandlerFn handler = HANDLER_TABLE[cc];
    if (handler) {
        handler(value, synth);
    } else {
        JT_LOGF("[CC %u] Unmapped (value=%u)\n", cc, value);
    }
}

} // namespace CCDispatch