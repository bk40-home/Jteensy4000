#include "AudioSynthSupersaw.h"
#include <math.h>   // powf, fminf, fmaxf

// -----------------------------------------------------------------------------
// PolyBLEP helper functions
//
// PolyBLEP (Polynomial Band‑Limited Step) is a technique for generating
// band‑limited waveforms without oversampling.  It works by subtracting a
// small polynomial from the naive sawtooth at the discontinuities to
// suppress high‑frequency aliases.  These helpers are declared static and
// inline so the compiler can optimise them away when not used.
//
// t  = current phase [0..1)
// dt = phase increment per sample (i.e. the fractional advance per sample)
static inline float poly_blep(float t, float dt)
{
    // If t is near the beginning of the cycle, apply a correction for the
    // rising edge.  The polynomial smoothly ramps the discontinuity over
    // one sample period, effectively cancelling the high‑frequency content.
    if (t < dt) {
        float x = t / dt;
        // 2*x - x^2 - 1 yields a curve from -1 to 0 when x goes 0..1
        return x + x - x * x - 1.0f;
    }
    // If t is near the end of the cycle, apply a correction for the
    // falling edge.  Shift phase by one period so the same polynomial can
    // be reused.
    else if (t > 1.0f - dt) {
        float x = (t - 1.0f) / dt;
        // x^2 + 2*x + 1 yields a curve from 0 to 1 when x goes 0..1
        return x * x + x + x + 1.0f;
    }
    // Otherwise, no correction is needed.
    return 0.0f;
}

// Generate a single saw sample using PolyBLEP.  This function wraps
// poly_blep() and produces a conventional -1..+1 sawtooth with aliasing
// suppressed at the discontinuity.  phaseInc should be the phase
// increment per sample (dt).  This helper is used only when usePolyBLEP
// is true.
static inline float saw_polyblep(float phase, float phaseInc)
{
    // Naive saw wave from -1 to +1
    float s = 2.0f * phase - 1.0f;
    // Subtract PolyBLEP correction at the discontinuity
    s -= poly_blep(phase, phaseInc);
    return s;
}

// -----------------------------------------------------------------------------
// Reverse-engineered / Szabó-derived per-voice frequency offsets
// These are ratios relative to the center oscillator: f_i = f * (1 + offset_i)
// -----------------------------------------------------------------------------
static const float kFreqOffsetsMax[SUPERSAW_VOICES] = {
    -0.11002313f,
    -0.06288439f,
    -0.01952356f,
     0.0f,
     0.01991221f,
     0.06216538f,
     0.10745242f
};

// -----------------------------------------------------------------------------
// Hardware-measured per-voice phase offsets (0..1 cycles). Repeatable start.
// (These were posted earlier by you; they’re extremely close to the ratio set.)
// -----------------------------------------------------------------------------
static const float kPhaseOffsets[SUPERSAW_VOICES] = {
    0.10986328125f,
    0.06286621094f,
    0.01953125f,
    0.0f,
    0.01953125f,
    0.06225585938f,
    0.107421875f
};

// Small helper clamp without pulling in Arduino constrain macro requirements.
static inline float clampf(float v, float lo, float hi) {
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

AudioSynthSupersaw::AudioSynthSupersaw()
    : AudioStream(0, nullptr),
      freq(440.0f),
      detuneAmt(0.5f),
      mixAmt(0.5f),
      amp(1.0f),
      outputGain(1.0f),
      hpfPrevIn(0.0f),
      hpfPrevOut(0.0f),
      hpfAlpha(0.0f)
{
    // Fixed initial phase (matches hardware-like repeatability)
    for (int i = 0; i < SUPERSAW_VOICES; ++i) {
        phases[i] = kPhaseOffsets[i];
    }

    calculateIncrements();
    calculateGains();
    calculateHPF();

    // default oversampling off
    oversample2x = false;

    // default to naive saw generation.  When usePolyBLEP is true the
    // oscillator will generate a band‑limited saw using the PolyBLEP
    // technique.  See setBandLimited() for details.
    usePolyBLEP = false;

    // By default enable mix compensation so the overall loudness stays
    // closer to the dry signal when the detuned oscillators are mixed in.
    mixCompensationEnabled = true;
    // Maximum gain when mix=1.  1.5f was empirically found to match
    // perceived levels on the hardware: full supersaw sounds roughly
    // equivalent to a single saw with outputGain=1.0f.  You can adjust
    // this at runtime via setCompensationMaxGain().
    compensationMaxGain = 1.5f;
}

void AudioSynthSupersaw::setFrequency(float f) {
    // Prevent pathological values
    freq = (f < 0.0f) ? 0.0f : f;

    calculateIncrements();
    calculateHPF();
}

void AudioSynthSupersaw::setDetune(float amount) {
    detuneAmt = clampf(amount, 0.0f, 1.0f);
    calculateIncrements();
}

void AudioSynthSupersaw::setMix(float amount) {
    mixAmt = clampf(amount, 0.0f, 1.0f);
    calculateGains();
}

void AudioSynthSupersaw::setAmplitude(float a) {
    amp = clampf(a, 0.0f, 1.0f);
    calculateGains();
}

void AudioSynthSupersaw::setOutputGain(float gain) {
    outputGain = clampf(gain, 0.0f, 1.5f);
}

void AudioSynthSupersaw::setOversample(bool enable) {
    oversample2x = enable;
}

void AudioSynthSupersaw::setMixCompensation(bool enable) {
    mixCompensationEnabled = enable;
}

void AudioSynthSupersaw::setCompensationMaxGain(float maxGain) {
    // Clamp to a sensible range.  Values below 1.0 would attenuate the
    // full mix and values above ~3.0 are unlikely to be useful.
    if (maxGain < 1.0f) {
        maxGain = 1.0f;
    } else if (maxGain > 3.0f) {
        maxGain = 3.0f;
    }
    compensationMaxGain = maxGain;
}

void AudioSynthSupersaw::setBandLimited(bool enable) {
    // Simply store the flag.  When true the oscillator will use the
    // PolyBLEP band‑limited saw; when false it reverts to a naive saw.
    usePolyBLEP = enable;
}

// “noteOn” should reset phases in a repeatable hardware-like way.
// (If you want “free-running” behaviour, make this a no-op.)
void AudioSynthSupersaw::noteOn() {
    for (int i = 0; i < SUPERSAW_VOICES; ++i) {
        phases[i] = kPhaseOffsets[i];
    }
}

float AudioSynthSupersaw::detuneCurve(float x) {
    // Your fitted curve (kept intact)
    return (
        10028.7312891634f * powf(x, 11) -
        50818.8652045924f * powf(x, 10) +
        111363.4808729368f * powf(x, 9) -
        138150.6761080548f * powf(x, 8) +
        106649.6679158292f * powf(x, 7) -
        53046.9642751875f * powf(x, 6) +
        17019.9518580080f * powf(x, 5) -
        3425.0836591318f * powf(x, 4) +
        404.2703938388f * powf(x, 3) -
        24.1878824391f  * powf(x, 2) +
        0.6717417634f   * x +
        0.0030115596f
    );
}

void AudioSynthSupersaw::calculateIncrements() {
    const float sr = AUDIO_SAMPLE_RATE_EXACT;
    const float nyquist = 0.5f * sr;

    // detuneCurve() gives your “amount feel” in 0..~1 territory (depending on fit)
    // Clamp to keep things stable if curve overshoots.
    const float detuneDepth = clampf(detuneCurve(detuneAmt), 0.0f, 1.0f);

    for (int i = 0; i < SUPERSAW_VOICES; ++i) {
        // Apply reverse-engineered ratios directly
        float oscFreq = freq * (1.0f + (kFreqOffsetsMax[i] * detuneDepth));

        // Clamp to valid range
        if (oscFreq < 0.0f)    oscFreq = 0.0f;
        if (oscFreq > nyquist) oscFreq = nyquist;

        phaseInc[i] = oscFreq / sr;
    }
}

void AudioSynthSupersaw::calculateGains() {
    // Linear mix: cross-fade between the centre oscillator and the side
    // oscillators. When mix=0 the centre voice is at full amplitude and
    // the side voices are silent. When mix=1 the centre voice is silent and
    // all six side voices share the amplitude equally.
    const float centerGain = 1.0f - mixAmt;
    const float sideGain   = mixAmt;
    for (int i = 0; i < SUPERSAW_VOICES; ++i) {
        if (i == (SUPERSAW_VOICES / 2)) {
            gains[i] = amp * centerGain;
        } else {
            gains[i] = amp * (sideGain / (SUPERSAW_VOICES - 1));
        }
    }
}

void AudioSynthSupersaw::calculateHPF() {
    // Simple 1-pole HPF; you were tying cutoff to freq.
    // Keep as-is but clamp freq so RC doesn’t go crazy at 0Hz.
    const float f = fmaxf(freq, 1.0f);
    const float rc = 1.0f / (6.2831853f * f);
    const float dt = 1.0f / AUDIO_SAMPLE_RATE_EXACT;
    hpfAlpha = rc / (rc + dt);
}

void AudioSynthSupersaw::update(void) {
    audio_block_t *block = allocate();
    if (!block) return;

    // Compute the dynamic mix compensation factor once per audio block.
    float mixGain = 1.0f;
    if (mixCompensationEnabled) {
        mixGain = 1.0f + mixAmt * (compensationMaxGain - 1.0f);
    }

    if (!oversample2x) {
        // -----------------------------------------------------------------
        // Standard (44.1 kHz) rendering
        //
        // When oversampling is disabled we compute a single sample per
        // output sample.  Each voice can generate either a naive saw or a
        // band‑limited saw depending on the usePolyBLEP flag.  Phase
        // advances by the full phase increment per sample.
        // -----------------------------------------------------------------
        for (int n = 0; n < AUDIO_BLOCK_SAMPLES; ++n) {
            float sample = 0.0f;
            for (int i = 0; i < SUPERSAW_VOICES; ++i) {
                float s;
                if (usePolyBLEP) {
                    // PolyBLEP band‑limited saw uses phase increment as dt
                    s = saw_polyblep(phases[i], phaseInc[i]);
                } else {
                    // naive saw: -1..+1
                    s = 2.0f * phases[i] - 1.0f;
                }
                sample += s * gains[i];
                // advance phase
                phases[i] += phaseInc[i];
                if (phases[i] >= 1.0f) phases[i] -= 1.0f;
            }
            // high-pass filter
            float hpOut = hpfAlpha * (hpfPrevOut + sample - hpfPrevIn);
            hpfPrevIn = sample;
            hpfPrevOut = hpOut;
            // clip and apply output gain and optional mix compensation
            hpOut = fmaxf(-1.0f, fminf(1.0f, hpOut));
            float out = hpOut * outputGain * mixGain;
            out = fmaxf(-1.0f, fminf(1.0f, out));
            block->data[n] = (int16_t)(out * 32767.0f);
        }
    } else {
        // -----------------------------------------------------------------
        // 2× oversampled rendering
        //
        // When oversampling is enabled we generate two internal sub‑samples
        // for every output sample.  Each sub‑sample advances the phase by
        // half the normal increment.  If PolyBLEP is enabled we use the
        // half‑rate increment as dt for the band‑limited saw.  The two
        // sub‑samples are accumulated and averaged before filtering.
        // -----------------------------------------------------------------
        for (int n = 0; n < AUDIO_BLOCK_SAMPLES; ++n) {
            float accum = 0.0f;
            for (int os = 0; os < 2; ++os) {
                float sample = 0.0f;
                for (int i = 0; i < SUPERSAW_VOICES; ++i) {
                    float s;
                    // half of the phase increment for oversampling
                    float incHalf = phaseInc[i] * 0.5f;
                    if (usePolyBLEP) {
                        // band‑limited saw using half rate dt
                        s = saw_polyblep(phases[i], incHalf);
                    } else {
                        // naive saw
                        s = 2.0f * phases[i] - 1.0f;
                    }
                    sample += s * gains[i];
                    // advance phase at half rate
                    phases[i] += incHalf;
                    if (phases[i] >= 1.0f) phases[i] -= 1.0f;
                }
                accum += sample;
            }
            float sample = accum * 0.5f;
            // high-pass filter
            float hpOut = hpfAlpha * (hpfPrevOut + sample - hpfPrevIn);
            hpfPrevIn = sample;
            hpfPrevOut = hpOut;
            // clip and apply output gain and optional mix compensation
            hpOut = fmaxf(-1.0f, fminf(1.0f, hpOut));
            float out = hpOut * outputGain * mixGain;
            out = fmaxf(-1.0f, fminf(1.0f, out));
            block->data[n] = (int16_t)(out * 32767.0f);
        }
    }

    transmit(block);
    release(block);
}