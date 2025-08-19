// Mapping.h
// -----------------------------------------------------------------------------
// Centralized forward + inverse mappings between 7‑bit CC (0..127) and the
// internal units used by the JT‑4000 project (Hz, ms, normalized 0..1, enums).
// Keep this header-only to avoid link issues and to make UI/Synth share 1 source.
// -----------------------------------------------------------------------------
// IMPORTANT: If you retune any curve (e.g. envelope times), update BOTH forward
// and inverse here so UI and engine stay in perfect lockstep.
// -----------------------------------------------------------------------------

#pragma once
#include <math.h>
#include <Arduino.h>

namespace JT4000Map {

    // -------------------------- Helpers --------------------------
    // Clamp float into [0,1], used to protect logs and ratios.
    static inline float clamp01(float x) {
        if (x < 0.0f) return 0.0f;
        if (x > 1.0f) return 1.0f;
        return x;
    }

    // Convert [0..127] to normalized [0..1]
    static inline float cc_to_norm(uint8_t cc) {
        if (cc > 127) cc = 127;
        return ((float)cc) / 127.0f;
    }

    // Convert normalized [0..1] to [0..127] (rounded)
    static inline uint8_t norm_to_cc(float norm) {
        norm = clamp01(norm);
        return (uint8_t)constrain(lroundf(norm * 127.0f), 0, 127);
    }

    // ---------------------- Envelope Time (ms) --------------------
    // Exponential “feel” curve (t90) calibrated from JT-4000 captures.
    // CC→ms
    static inline float cc_to_time_ms(uint8_t cc) {
        // Same constants as your existing SynthEngine.cpp (don’t change lightly)
        const float a = -5.44635f;
        const float b =  0.054448f;
        const float tau = expf(a + b * (float)(cc > 127 ? 127 : cc));
        return (2.3026f * tau) * 1000.0f; // ms
    }

    // Inverse: ms→CC
    static inline uint8_t time_ms_to_cc(float ms) {
        // Guard against log of zero/negative
        if (ms <= 0.0f) return 0;
        const float a = -5.44635f;
        const float b =  0.054448f;
        float cc = (logf((ms / 1000.0f) / 2.3026f) - a) / b;
        if (!isfinite(cc)) cc = 0.0f;
        return (uint8_t)constrain(lroundf(cc), 0, 127);
    }

    // ---------------------- Filter Cutoff (Hz) --------------------
    // Log map: 20 Hz .. 10 kHz
    static inline float cc_to_cutoff_hz(uint8_t cc) {
        float norm = cc_to_norm(cc);
        return 20.0f * powf(10000.0f / 20.0f, norm);
    }

    static inline uint8_t cutoff_hz_to_cc(float hz) {
        if (hz <= 20.0f) return 0;
        if (hz >= 10000.0f) return 127;
        float norm = logf(hz / 20.0f) / logf(10000.0f / 20.0f);
        return norm_to_cc(norm);
    }

    // --------------------------- LFO (Hz) -------------------------
    // Your current spec: f = 0.03 * 1300^norm (~0.03 .. ~39 Hz)
    static inline float cc_to_lfo_hz(uint8_t cc) {
        float norm = cc_to_norm(cc);
        return 0.03f * powf(1300.0f, norm);
    }

    static inline uint8_t lfo_hz_to_cc(float hz) {
        if (hz <= 0.03f) return 0;
        // Upper bound is soft; clamp to keep UI sane
        if (hz >= 0.03f * 1300.0f) return 127;
        float norm = logf(hz / 0.03f) / logf(1300.0f);
        return norm_to_cc(norm);
    }

    // ---------------------- Linear 0..1 norms ---------------------
    // Resonance 0..1 → CC and back
    static inline uint8_t resonance_to_cc(float r) { return norm_to_cc(r); }
    static inline float  cc_to_resonance(uint8_t cc) { return cc_to_norm(cc); }

    // Generic linear 0..1 helpers, for mix, depths, etc.
    static inline uint8_t norm_to_cc_lin(float x) { return norm_to_cc(x); }
    static inline float  cc_to_norm_lin(uint8_t cc) { return cc_to_norm(cc); }

} // namespace JT4000Map
