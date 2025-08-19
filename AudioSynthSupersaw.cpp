#include "AudioSynthSupersaw.h"

AudioSynthSupersaw::AudioSynthSupersaw()
    : AudioStream(0, nullptr), freq(440.0f), detuneAmt(0.5f), mixAmt(0.5f), amp(1.0f),
      outputGain(1.0f), hpfPrevIn(0.0f), hpfPrevOut(0.0f)
{
    for (int i = 0; i < SUPERSAW_VOICES; i++) {
        phases[i] = (float)rand() / (float)RAND_MAX;
    }
    calculateIncrements();
    calculateGains();
    calculateHPF();
}

void AudioSynthSupersaw::setFrequency(float f) {
    freq = f;
    calculateIncrements();
    calculateHPF();
}

void AudioSynthSupersaw::setDetune(float amount) {
    detuneAmt = constrain(amount, 0.0f, 1.0f);
    calculateIncrements();
}

void AudioSynthSupersaw::setMix(float amount) {
    mixAmt = constrain(amount, 0.0f, 1.0f);
    calculateGains();
}

void AudioSynthSupersaw::setAmplitude(float a) {
    amp = constrain(a, 0.0f, 1.0f);
    calculateGains();
}

void AudioSynthSupersaw::setOutputGain(float gain) {
    outputGain = constrain(gain, 0.0f, 1.0f);
}

void AudioSynthSupersaw::noteOn() {
    for (int i = 0; i < SUPERSAW_VOICES; i++) {
        phases[i] = (float)rand() / (float)RAND_MAX;
    }
}

float AudioSynthSupersaw::detuneCurve(float x) {
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
        24.1878824391f * powf(x, 2) +
        0.6717417634f * x +
        0.0030115596f
    );
}

void AudioSynthSupersaw::calculateIncrements() {
    float sr = AUDIO_SAMPLE_RATE_EXACT;
    float spread = detuneCurve(detuneAmt);
    static const float offsets[SUPERSAW_VOICES] = { -3, -2, -1, 0, 1, 2, 3 };

    for (int i = 0; i < SUPERSAW_VOICES; ++i) {
        float ratio = 1.0f + offsets[i] * 0.01f * spread;
        float oscFreq = freq * ratio;
        if (oscFreq > sr * 0.5f) oscFreq = sr * 0.5f;
        phaseInc[i] = oscFreq / sr;
    }
}

void AudioSynthSupersaw::calculateGains() {
    float centerGain = -0.55366f * mixAmt + 0.99785f;
    float sideGain   = -0.73764f * mixAmt * mixAmt + 1.2841f * mixAmt + 0.044372f;

    for (int i = 0; i < SUPERSAW_VOICES; ++i) {
        if (i == SUPERSAW_VOICES / 2)
            gains[i] = amp * centerGain;
        else
            gains[i] = amp * (sideGain / (SUPERSAW_VOICES - 1));
    }
}

void AudioSynthSupersaw::calculateHPF() {
    float rc = 1.0f / (6.2831853f * freq);
    float dt = 1.0f / AUDIO_SAMPLE_RATE_EXACT;
    hpfAlpha = rc / (rc + dt);
}

void AudioSynthSupersaw::update(void) {
    audio_block_t *block = allocate();
    if (!block) return;

    for (int n = 0; n < AUDIO_BLOCK_SAMPLES; ++n) {
        float sample = 0.0f;

        for (int i = 0; i < SUPERSAW_VOICES; ++i) {
            float s = 2.0f * phases[i] - 1.0f;
            sample += s * gains[i];
            phases[i] += phaseInc[i];
            if (phases[i] >= 1.0f) phases[i] -= 1.0f;
        }

        float hpOut = hpfAlpha * (hpfPrevOut + sample - hpfPrevIn);
        hpfPrevIn = sample;
        hpfPrevOut = hpOut;

        hpOut = fmaxf(-1.0f, fminf(1.0f, hpOut));
        block->data[n] = (int16_t)(hpOut * outputGain * 32767.0f);
    }

    transmit(block);
    release(block);
}
