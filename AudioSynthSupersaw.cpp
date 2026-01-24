#include "AudioSynthSupersaw.h"
#include <math.h>   // powf, fminf, fmaxf

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

    if (!oversample2x) {
        // Standard (44.1 kHz) rendering
        for (int n = 0; n < AUDIO_BLOCK_SAMPLES; ++n) {
            float sample = 0.0f;
            for (int i = 0; i < SUPERSAW_VOICES; ++i) {
                // naive saw: -1..+1
                const float s = 2.0f * phases[i] - 1.0f;
                sample += s * gains[i];
                phases[i] += phaseInc[i];
                if (phases[i] >= 1.0f) phases[i] -= 1.0f;
            }
            // high-pass filter
            float hpOut = hpfAlpha * (hpfPrevOut + sample - hpfPrevIn);
            hpfPrevIn = sample;
            hpfPrevOut = hpOut;
            // clip and apply output gain
            hpOut = fmaxf(-1.0f, fminf(1.0f, hpOut));
            float out = hpOut * outputGain;
            out = fmaxf(-1.0f, fminf(1.0f, out));
            block->data[n] = (int16_t)(out * 32767.0f);
        }
    } else {
        // 2× oversampled rendering. Two internal sub-samples per output sample
        for (int n = 0; n < AUDIO_BLOCK_SAMPLES; ++n) {
            float accum = 0.0f;
            // Generate two sub-samples and average them
            for (int os = 0; os < 2; ++os) {
                float sample = 0.0f;
                for (int i = 0; i < SUPERSAW_VOICES; ++i) {
                    const float s = 2.0f * phases[i] - 1.0f;
                    sample += s * gains[i];
                    // advance phase at half rate for oversampling
                    phases[i] += phaseInc[i] * 0.5f;
                    if (phases[i] >= 1.0f) phases[i] -= 1.0f;
                }
                accum += sample;
            }
            float sample = accum * 0.5f;
            // high-pass filter
            float hpOut = hpfAlpha * (hpfPrevOut + sample - hpfPrevIn);
            hpfPrevIn = sample;
            hpfPrevOut = hpOut;
            hpOut = fmaxf(-1.0f, fminf(1.0f, hpOut));
            float out = hpOut * outputGain;
            out = fmaxf(-1.0f, fminf(1.0f, out));
            block->data[n] = (int16_t)(out * 32767.0f);
        }
    }

    transmit(block);
    release(block);
}