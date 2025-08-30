// Mapping.h
// -----------------------------------------------------------------------------
// Centralized forward + inverse mappings between 7-bit CC (0..127) and the
// internal units used by the JT-4000 project (Hz, ms, normalized 0..1, enums).
// Keep this header-only to avoid link issues and to make UI/Synth share 1 source.
// -----------------------------------------------------------------------------
// IMPORTANT: If you retune any curve (e.g. envelope times), update BOTH forward
// and inverse here so UI and engine stay in perfect lockstep.
// -----------------------------------------------------------------------------

#pragma once
#include <math.h>
#include <Arduino.h>

namespace JT4000Map {
    static constexpr float CUTOFF_MIN_HZ = 20.0f;
    static constexpr float CUTOFF_MAX_HZ = 20000.0f;

    // Choose taper mode
    enum CutoffTaper {
        TAPER_NEUTRAL = 0,  // pure log (geometric mean ~632 Hz at CC=64)
        TAPER_LOW,          // bias resolution to low frequencies
        TAPER_HIGH          // bias resolution to high frequencies
    };

    // You can set this externally (SynthEngine or FilterBlock)
    static CutoffTaper cutoffTaperMode = TAPER_LOW;

    // Apply taper to normalized CC (0..1)
    inline float applyTaper(float t) {
        switch (cutoffTaperMode) {
            case TAPER_LOW:
                // sqrt(t) -> expands low end
                return powf(t, 0.5f);
            case TAPER_HIGH:
                // square(t) -> expands high end
                return powf(t, 2.0f);
            default:
                return t; // neutral
        }
    }

    // CC (0..127) → Hz
    inline float cc_to_cutoff_hz(uint8_t cc) {
        float t = cc / 127.0f;
        t = applyTaper(t);
        return CUTOFF_MIN_HZ * powf(CUTOFF_MAX_HZ / CUTOFF_MIN_HZ, t);
    }

    // Hz → CC
    inline uint8_t cutoff_hz_to_cc(float hz) {
        hz = fmaxf(CUTOFF_MIN_HZ, fminf(hz, CUTOFF_MAX_HZ));
        float t = logf(hz / CUTOFF_MIN_HZ) / logf(CUTOFF_MAX_HZ / CUTOFF_MIN_HZ);

        // Undo taper for display/CC mapping
        switch (cutoffTaperMode) {
            case TAPER_LOW:  t = powf(t, 2.0f);  break; // inverse of sqrt
            case TAPER_HIGH: t = powf(t, 0.5f);  break; // inverse of square
            default: break;
        }

        return (uint8_t)lrintf(t * 127.0f);
    }    

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
    // Keep legacy linear resonance helpers (for modules that expect 0..1)
    static inline uint8_t resonance_to_cc(float r) { return norm_to_cc(r); }
    static inline float  cc_to_resonance(uint8_t cc) { return cc_to_norm(cc); }

    // Generic linear 0..1 helpers, for mix, depths, etc.
    static inline uint8_t norm_to_cc_lin(float x) { return norm_to_cc(x); }
    static inline float  cc_to_norm_lin(uint8_t cc) { return cc_to_norm(cc); }

    // =================== MoogLinear Resonance (k) ===================
    // Extended 3-zone mapping for AudioFilterMoogLinear 'k' parameter.
    // Goal: precise control in 0..1.5, reach 1.5..4 “interesting” region,
    // and a short tail to extreme 4..20.
    //
    // Use these in your filter block instead of the linear 0..1 helpers above:
    //   float k = JT4000Map::cc_to_res_k(cc);
    //   uint8_t cc = JT4000Map::res_k_to_cc(k);

    // Zone bounds (in "k")
    static constexpr float RES_MIN_K  = 0.0f;
    static constexpr float RES_Z1_MAX = 1.5f;   // normal operating sweet spot
    static constexpr float RES_Z2_MAX = 4.0f;   // extended “interesting” region
    static constexpr float RES_MAX_K  = 20.0f;  // extreme tail

    // Zone weights over CC travel (must sum to 1.0)
    // Tune these to taste without refactoring engine/UI.
    static constexpr float RES_W1 = 0.75f;   // 75% of CC span in 0..1.5
    static constexpr float RES_W2 = 0.20f;   // 20% in 1.5..4
    static constexpr float RES_W3 = 0.05f;   // 5%  in 4..20

    // Curves within each zone (>1.0 biases more resolution to lower end of the zone)
    static constexpr float RES_CURVE_Z1 = 1.60f;  // more detail near 0..1.5
    static constexpr float RES_CURVE_Z2 = 1.20f;  // gentle skew 1.5..4
    static constexpr float RES_CURVE_Z3 = 2.20f;  // strong skew 4..20

    // Clamp for safety
    static inline float clamp_res_k(float k) {
        if (k < RES_MIN_K) return RES_MIN_K;
        if (k > RES_MAX_K) return RES_MAX_K;
        return k;
    }

    // Internal easing helpers (kept inline for speed)
    namespace _res_internal {
        inline float zone_map(float t, float a, float b, float curve) {
            if (t <= 0.0f) return a;
            if (t >= 1.0f) return b;
            float u = powf(t, curve);
            return a + (b - a) * u;
        }
        inline float zone_map_inv(float v, float a, float b, float curve) {
            if (v <= a) return 0.0f;
            if (v >= b) return 1.0f;
            float u = (v - a) / (b - a);
            if (curve <= 0.0f) return u; // defensive; we only use >1.0
            return powf(u, 1.0f / curve);
        }
    } // namespace _res_internal

    // CC (0..127) → k (0..20) with 3-zone response
    static inline float cc_to_res_k(uint8_t cc) {
        const float n = clamp01(cc / 127.0f);
        const float W1 = RES_W1;
        const float W2 = RES_W2;
        const float W3 = RES_W3; // = 1 - W1 - W2

        if (n <= W1) {
            // Zone 1: 0 .. 1.5
            const float t = (W1 > 0.0f) ? (n / W1) : 0.0f;
            return clamp_res_k(_res_internal::zone_map(t, RES_MIN_K, RES_Z1_MAX, RES_CURVE_Z1));
        } else if (n <= (W1 + W2)) {
            // Zone 2: 1.5 .. 4
            const float t = (W2 > 0.0f) ? ((n - W1) / W2) : 0.0f;
            return clamp_res_k(_res_internal::zone_map(t, RES_Z1_MAX, RES_Z2_MAX, RES_CURVE_Z2));
        } else {
            // Zone 3: 4 .. 20
            const float t = (W3 > 0.0f) ? ((n - W1 - W2) / W3) : 0.0f;
            return clamp_res_k(_res_internal::zone_map(t, RES_Z2_MAX, RES_MAX_K, RES_CURVE_Z3));
        }
    }

    // k (0..20) → CC (0..127), inverse of above for UI round-trip stability
    static inline uint8_t res_k_to_cc(float k) {
        k = clamp_res_k(k);

        const float W1 = RES_W1;
        const float W2 = RES_W2;
        const float W3 = RES_W3;

        float n = 0.0f; // normalized [0..1]

        if (k <= RES_Z1_MAX) {
            const float t = _res_internal::zone_map_inv(k, RES_MIN_K, RES_Z1_MAX, RES_CURVE_Z1);
            n = t * W1;
        } else if (k <= RES_Z2_MAX) {
            const float t = _res_internal::zone_map_inv(k, RES_Z1_MAX, RES_Z2_MAX, RES_CURVE_Z2);
            n = W1 + t * W2;
        } else {
            const float t = _res_internal::zone_map_inv(k, RES_Z2_MAX, RES_MAX_K, RES_CURVE_Z3);
            n = W1 + W2 + t * W3;
        }

        return (uint8_t)constrain(lroundf(n * 127.0f), 0, 127);
    }

} // namespace JT4000Map
