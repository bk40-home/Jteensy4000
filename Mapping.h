// Mapping.h
//
// Centralized forward + inverse mappings between 7-bit CC (0..127) and
// internal units (Hz, ms, normalized) used by the JT-4000 project.

#pragma once
#include <math.h>
#include <Arduino.h>
#include "CCDefs.h"
#include "LFOBlock.h"  // for NUM_LFO_DESTS used in LFO destination binning

namespace JT4000Map {

    static constexpr float CUTOFF_MIN_HZ = 20.0f;
    static constexpr float CUTOFF_MAX_HZ = 20000.0f;

    // ----- Optional JT byte -> CC helpers (kept for preset import paths) -----
    enum class Xform : uint8_t {
        Raw_0_127,
        Bool_0_127,
        Enum_Direct,
        Scale_0_99_to_127,
        EnumMap_Osc1Wave,
        EnumMap_Osc2Wave,
        EnumMap_LFOWave,
        EnumMap_LFO1Dest,
    };

    // Minimal example LUTs used by your preset loader path (unchanged)
    inline uint8_t toCC(uint8_t raw, Xform xf) {
        switch (xf) {
            case Xform::Bool_0_127:        return raw ? 127 : 0;
            case Xform::Enum_Direct:       return raw;
            case Xform::Scale_0_99_to_127: return (raw > 99) ? 127 : (raw * 127) / 99;
            default:                       return raw;
        }
    }

 
    struct Slot { uint8_t byte1; uint8_t cc; Xform xf; };

    // Example subset; keep your existing table content as needed.
    inline constexpr Slot kSlots[] = {
        {  1, CC::OSC1_WAVE, Xform::Enum_Direct },
        {  2, CC::OSC2_WAVE, Xform::Enum_Direct },
        {  5, CC::OSC1_PITCH_OFFSET, Xform::Raw_0_127 },
        {  6, CC::OSC1_FINE_TUNE,    Xform::Raw_0_127 },
        {  7, CC::OSC2_PITCH_OFFSET, Xform::Raw_0_127 },
        {  8, CC::OSC2_FINE_TUNE,    Xform::Raw_0_127 },
        {  9, CC::OSC1_MIX,          Xform::Raw_0_127 },
        // ... keep the rest of your slots as before ...
    };

    // ----- Shared curves -----
    enum CutoffTaper { TAPER_NEUTRAL = 0, TAPER_LOW, TAPER_HIGH };
    static CutoffTaper cutoffTaperMode = TAPER_LOW;

    inline float clamp01(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }
    inline float cc_to_norm(uint8_t cc) { if (cc>127) cc=127; return cc/127.0f; }
    inline uint8_t norm_to_cc(float n) { n = clamp01(n); return (uint8_t)constrain(lroundf(n*127.0f),0,127); }

    inline float applyTaper(float t) {
        switch (cutoffTaperMode) { case TAPER_LOW: return powf(t,0.5f);
                                   case TAPER_HIGH:return powf(t,2.0f);
                                   default:        return t; }
    }

    inline float cc_to_cutoff_hz(uint8_t cc) {
        float t = applyTaper(cc_to_norm(cc));
        return CUTOFF_MIN_HZ * powf(CUTOFF_MAX_HZ / CUTOFF_MIN_HZ, t);
    }
    inline uint8_t cutoff_hz_to_cc(float hz) {
        hz = fmaxf(CUTOFF_MIN_HZ, fminf(hz, CUTOFF_MAX_HZ));
        float t = logf(hz / CUTOFF_MIN_HZ) / logf(CUTOFF_MAX_HZ / CUTOFF_MIN_HZ);
        if (cutoffTaperMode==TAPER_LOW)  t = powf(t,2.0f);
        if (cutoffTaperMode==TAPER_HIGH) t = powf(t,0.5f);
        return (uint8_t)constrain(lroundf(t*127.0f),0,127);
    }

    inline float cc_to_time_ms(uint8_t cc) {
        const float a=-5.44635f, b=0.054448f;
        const float tau = expf(a + b * (float)(cc>127?127:cc));
        return (2.3026f * tau) * 1000.0f;
    }
    inline uint8_t time_ms_to_cc(float ms) {
        if (ms<=0) return 0;
        const float a=-5.44635f, b=0.054448f;
        float cc = (logf((ms/1000.0f)/2.3026f) - a)/b;
        if (!isfinite(cc)) cc = 0;
        return (uint8_t)constrain(lroundf(cc),0,127);
    }

    inline float cc_to_lfo_hz(uint8_t cc) { return 0.03f * powf(1300.0f, cc_to_norm(cc)); }
    inline uint8_t lfo_hz_to_cc(float hz) {
        if (hz <= 0.03f) return 0;
        if (hz >= 0.03f*1300.0f) return 127;
        float n = logf(hz/0.03f)/logf(1300.0f);
        return norm_to_cc(n);
    }

    inline uint8_t ccFromLfoDest(int dest) {
        if (dest < 0) dest = 0;
        if (dest >= NUM_LFO_DESTS) dest = NUM_LFO_DESTS - 1;
        const uint16_t start = (dest    * 128u) / NUM_LFO_DESTS;
        const uint16_t end   = ((dest+1)* 128u) / NUM_LFO_DESTS;
        return (uint8_t)((start+end)/2);
    }
    inline int lfoDestFromCC(uint8_t cc) {
        uint8_t idx = (uint16_t(cc) * NUM_LFO_DESTS) / 128;
        if (idx >= NUM_LFO_DESTS) idx = NUM_LFO_DESTS - 1;
        return (int)idx;
    }

    inline uint8_t resonance_to_cc(float r) { return norm_to_cc(r); }
    inline float  cc_to_resonance(uint8_t cc){ return cc_to_norm(cc); }

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

<<<<<<< Updated upstream
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
=======
constexpr float RES_K_MIN = 0.0f;
constexpr float RES_K_MAX = 20.0f;

inline float clamp_res_k(float k) {
    if (k < RES_K_MIN) return RES_K_MIN;
    if (k > RES_K_MAX) return RES_K_MAX;
    return k;
}

namespace _res_internal {

// Map x in [x0..x1] linearly to y in [y0..y1].
// Clamps outside the range (useful for piecewise zones).
inline float zone_map(float x, float x0, float x1, float y0, float y1) {
    if (x <= x0) return y0;
    if (x >= x1) return y1;
    const float t = (x - x0) / (x1 - x0);
    return y0 + t * (y1 - y0);
}

// Inverse of the above: given y in [y0..y1], return x in [x0..x1].
inline float zone_map_inv(float y, float x0, float x1, float y0, float y1) {
    if (y <= y0) return x0;
    if (y >= y1) return x1;
    const float t = (y - y0) / (y1 - y0);
    return x0 + t * (x1 - x0);
}

} // namespace _res_internal

// CC (0..127) -> resonance k (0..20)  [linear by default]
inline float cc_to_res_k(uint8_t cc) {
    const float n = static_cast<float>(cc) / 127.0f;   // 0..1
    return clamp_res_k(n * RES_K_MAX);                 // 0..20
}

// resonance k (0..20) -> CC (0..127)
inline uint8_t res_k_to_cc(float k) {
    const float n = clamp_res_k(k) / RES_K_MAX;        // 0..1
    int v = static_cast<int>(lroundf(n * 127.0f));
    if (v < 0)   v = 0;
    if (v > 127) v = 127;
    return static_cast<uint8_t>(v);
}
>>>>>>> Stashed changes

} // namespace JT4000Map
