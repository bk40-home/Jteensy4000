#include "AudioSynthSupersaw.h"
#include <math.h>   // powf, fminf, fmaxf
#include <stdlib.h>  // rand

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
      hpfAlpha(0.0f),
      oversample2x(false),
      driftAmt(0.0f),
      hpfCutoff(30.0f)
{
    // Fixed initial phase (matches hardware-like repeatability)
    for (int i = 0; i < SUPERSAW_VOICES; ++i) {
        phases[i] = kPhaseOffsets[i];

        // Initialise per‑voice drift LFO phases randomly between 0 and 1
        lfoPhases[i] = (float)rand() / (float)RAND_MAX;
        // Give each voice its own slow LFO rate between 0.02 Hz and 0.08 Hz
        float lfoHz = 0.02f + ((float)rand() / (float)RAND_MAX) * 0.06f;
        lfoInc[i] = lfoHz / AUDIO_SAMPLE_RATE_EXACT;
    }

    calculateIncrements();
    calculateGains();
    calculateHPF();
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

void AudioSynthSupersaw::setDriftAmount(float amount) {
    driftAmt = clampf(amount, 0.0f, 1.0f);
}

void AudioSynthSupersaw::setHpfCutoff(float cutoff) {
    // Clamp cutoff to a reasonable range and apply immediately
    hpfCutoff = clampf(cutoff, 1.0f, 2000.0f);
    calculateHPF();
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
    // Your existing gain law (kept intact)
    const float centerGain = -0.55366f * mixAmt + 0.99785f;
    const float sideGain   = -0.73764f * mixAmt * mixAmt + 1.2841f * mixAmt + 0.044372f;

    for (int i = 0; i < SUPERSAW_VOICES; ++i) {
        if (i == (SUPERSAW_VOICES / 2)) {
            gains[i] = amp * centerGain;
        } else {
            gains[i] = amp * (sideGain / (SUPERSAW_VOICES - 1));
        }
    }
}

void AudioSynthSupersaw::calculateHPF() {
    // 1-pole high‑pass filter. The cutoff is fixed via hpfCutoff (in Hz)
    // rather than tied to the oscillator frequency. This preserves the
    // fundamental and low beating frequencies instead of high‑passing
    // everything below the note pitch.
    const float fc = fmaxf(hpfCutoff, 1.0f);
    const float rc = 1.0f / (6.2831853f * fc);
    const float dt = 1.0f / AUDIO_SAMPLE_RATE_EXACT;
    hpfAlpha = rc / (rc + dt);
}

void AudioSynthSupersaw::update(void) {
    audio_block_t *block = allocate();
    if (!block) return;
    if (!oversample2x) {
        // Standard 1× rendering
        for (int n = 0; n < AUDIO_BLOCK_SAMPLES; ++n) {
            float sample = 0.0f;

            for (int i = 0; i < SUPERSAW_VOICES; ++i) {
                // Compute a small drift modulation for this voice. The drift
                // amount is scaled by ±0.5 % of the base increment and then
                // further scaled by the user‑controlled driftAmt. The sine
                // function produces smooth LFO movement.
                float driftScale = 1.0f + (driftAmt * 0.005f) * sinf(2.0f * 3.14159265f * lfoPhases[i]);
                float inc = phaseInc[i] * driftScale;

                // naive saw: -1..+1
                const float s = 2.0f * phases[i] - 1.0f;
                sample += s * gains[i];

                phases[i] += inc;
                if (phases[i] >= 1.0f) phases[i] -= 1.0f;

                // Advance the drift LFO phase and wrap
                lfoPhases[i] += lfoInc[i];
                if (lfoPhases[i] >= 1.0f) lfoPhases[i] -= 1.0f;
            }

            // HPF
            float hpOut = hpfAlpha * (hpfPrevOut + sample - hpfPrevIn);
            hpfPrevIn = sample;
            hpfPrevOut = hpOut;

            // Clamp to avoid int16 wrap, then apply outputGain
            hpOut = fmaxf(-1.0f, fminf(1.0f, hpOut));

            const float out = hpOut * outputGain;
            block->data[n] = (int16_t)(fmaxf(-1.0f, fminf(1.0f, out)) * 32767.0f);
        }
    } else {
        // 2× oversampled rendering. Two internal sub‑samples are generated per
        // output sample, low‑passed and then decimated. Drift modulation is
        // applied on each internal tick.
        for (int n = 0; n < AUDIO_BLOCK_SAMPLES; ++n) {
            // Accumulate two internal samples
            float accum = 0.0f;
            for (int os = 0; os < 2; ++os) {
                float sample = 0.0f;
                for (int i = 0; i < SUPERSAW_VOICES; ++i) {
                    float driftScale = 1.0f + (driftAmt * 0.005f) * sinf(2.0f * 3.14159265f * lfoPhases[i]);
                    float inc = phaseInc[i] * driftScale * 0.5f; // half step for 2× OS
                    const float s = 2.0f * phases[i] - 1.0f;
                    sample += s * gains[i];
                    phases[i] += inc;
                    if (phases[i] >= 1.0f) phases[i] -= 1.0f;
                    // update drift
                    lfoPhases[i] += lfoInc[i] * 0.5f;
                    if (lfoPhases[i] >= 1.0f) lfoPhases[i] -= 1.0f;
                }
                accum += sample;
            }
            // Simple 2-tap FIR low-pass for decimation
            float sample = 0.5f * accum;

            float hpOut = hpfAlpha * (hpfPrevOut + sample - hpfPrevIn);
            hpfPrevIn = sample;
            hpfPrevOut = hpOut;
            hpOut = fmaxf(-1.0f, fminf(1.0f, hpOut));
            const float out = hpOut * outputGain;
            block->data[n] = (int16_t)(fmaxf(-1.0f, fminf(1.0f, out)) * 32767.0f);
        }
    }

    transmit(block);
    release(block);
}