#include "AudioSynthSupersaw.h"
#include <math.h>   // powf, fminf, fmaxf, fabsf
#include <stdlib.h> // rand

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
    // Initialise phase offsets and per‑voice LFOs
    for (int i = 0; i < SUPERSAW_VOICES; ++i) {
        // repeatable start phases (matches original hardware)
        phases[i] = kPhaseOffsets[i];
        // random initial drift LFO phase in [0,1)
        lfoPhases[i] = (float)rand() / (float)RAND_MAX;
        // choose a slow LFO frequency between 0.02 Hz and 0.08 Hz
        float lfoHz = 0.02f + ((float)rand() / (float)RAND_MAX) * 0.06f;
        lfoInc[i] = lfoHz / AUDIO_SAMPLE_RATE_EXACT;
    }
    // default flags
    oversample2x = false;
    driftAmt     = 0.0f;
    hpfCutoff    = 30.0f;

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
    // Clamp cutoff to reasonable range and apply immediately
    hpfCutoff = clampf(cutoff, 1.0f, 2000.0f);
    calculateHPF();
}

// “noteOn” should reset phases in a repeatable hardware-like way.
// (If you want “free-running” behaviour, make this a no-op.)
void AudioSynthSupersaw::noteOn() {
    // Randomise phases at note on to avoid cancellation when detune=0 and mix is low.
    // The JT‑4000 uses free‑running oscillators so each note should begin with
    // unpredictable phase relationships.  Using fixed offsets can cause
    // destructive interference at mix=0 when all voices share the same
    // frequency.  Random phases ensure the centre oscillator is audible.
    for (int i = 0; i < SUPERSAW_VOICES; ++i) {
        phases[i] = (float)rand() / (float)RAND_MAX;
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
    // Linearly cross‑fade the gain between the centre oscillator and the six side
    // oscillators.  When mix=0 the centre voice is full volume and the
    // side voices are silent.  When mix=1 the centre voice is silent and
    // all seven oscillators share the output evenly.  This behaviour
    // matches the JT‑4000 where the mix knob blends in the detuned voices.
    const float centerGain = 1.0f - mixAmt;
    const float sideGain   = mixAmt;

    for (int i = 0; i < SUPERSAW_VOICES; ++i) {
        if (i == (SUPERSAW_VOICES / 2)) {
            gains[i] = amp * centerGain;
        } else {
            // distribute the side gain across all non‑centre voices
            gains[i] = amp * (sideGain / (SUPERSAW_VOICES - 1));
        }
    }
}

void AudioSynthSupersaw::calculateHPF() {
    // 1-pole high-pass filter.  Use hpfCutoff (Hz) rather than note frequency
    const float fc = fmaxf(hpfCutoff, 1.0f);
    const float rc = 1.0f / (6.2831853f * fc);
    const float dt = 1.0f / AUDIO_SAMPLE_RATE_EXACT;
    hpfAlpha = rc / (rc + dt);
}

void AudioSynthSupersaw::update(void) {
    audio_block_t *block = allocate();
    if (!block) return;

    if (!oversample2x) {
        // Normal (1×) rendering
        for (int n = 0; n < AUDIO_BLOCK_SAMPLES; ++n) {
            float sample = 0.0f;

            for (int i = 0; i < SUPERSAW_VOICES; ++i) {
                // generate a triangular LFO value in [-1,1] without heavy trig
                float phase = lfoPhases[i];
                float tri = (phase < 0.5f) ? (phase * 2.0f) : (2.0f - 2.0f * phase);
                tri = tri * 2.0f - 1.0f;
                float driftScale = 1.0f + (driftAmt * 0.005f) * tri;
                float inc = phaseInc[i] * driftScale;

                // naive saw: -1..+1
                const float s = 2.0f * phases[i] - 1.0f;
                sample += s * gains[i];

                phases[i] += inc;
                if (phases[i] >= 1.0f) phases[i] -= 1.0f;

                // advance LFO phase and wrap
                phase += lfoInc[i];
                if (phase >= 1.0f) phase -= 1.0f;
                lfoPhases[i] = phase;
            }

            // High-pass filter to remove DC/low-frequency bias
            float hpOut = hpfAlpha * (hpfPrevOut + sample - hpfPrevIn);
            hpfPrevIn = sample;
            hpfPrevOut = hpOut;

            // Clamp output and apply outputGain
            hpOut = fmaxf(-1.0f, fminf(1.0f, hpOut));
            float out = hpOut * outputGain;
            out = fmaxf(-1.0f, fminf(1.0f, out));
            block->data[n] = (int16_t)(out * 32767.0f);
        }
    } else {
        // 2× oversampled rendering
        // Precompute driftScale for each voice once per output sample
        for (int n = 0; n < AUDIO_BLOCK_SAMPLES; ++n) {
            float driftScale[SUPERSAW_VOICES];
            for (int i = 0; i < SUPERSAW_VOICES; ++i) {
                float phase = lfoPhases[i];
                float tri = (phase < 0.5f) ? (phase * 2.0f) : (2.0f - 2.0f * phase);
                tri = tri * 2.0f - 1.0f;
                driftScale[i] = 1.0f + (driftAmt * 0.005f) * tri;
            }
            float accum = 0.0f;
            // two internal sub-samples
            for (int os = 0; os < 2; ++os) {
                float sample = 0.0f;
                for (int i = 0; i < SUPERSAW_VOICES; ++i) {
                    float inc = phaseInc[i] * driftScale[i] * 0.5f;
                    const float s = 2.0f * phases[i] - 1.0f;
                    sample += s * gains[i];
                    phases[i] += inc;
                    if (phases[i] >= 1.0f) phases[i] -= 1.0f;
                    // advance LFO phase for oversampled step
                    float lfoStep = lfoInc[i] * 0.5f;
                    float phase = lfoPhases[i] + lfoStep;
                    if (phase >= 1.0f) phase -= 1.0f;
                    lfoPhases[i] = phase;
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