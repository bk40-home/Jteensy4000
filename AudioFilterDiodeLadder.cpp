#include "AudioFilterDiodeLadder.h"
#include <arm_math.h>

AudioFilterDiodeLadder::AudioFilterDiodeLadder()
    : AudioStream(2, inputQueueArray), _sampleRate(AUDIO_SAMPLE_RATE_EXACT),
      _cutoff(1000.0f), _resonance(0.0f), _cutoffMod(0.0f), _saturation(0.0f) {
    for(int ch = 0; ch < 2; ++ch)
        for(int i = 0; i < 4; ++i)
            _z1[ch][i] = 0.0f;
    updateCoefficients(_cutoff);
}

void AudioFilterDiodeLadder::setCutoff(float cutoff) {
    _cutoff = constrain(cutoff, 5.0f, _sampleRate / 2.0f);
}
void AudioFilterDiodeLadder::setResonance(float resonance) {
    _resonance = constrain(resonance, 0.0f, 2.0f);
}
void AudioFilterDiodeLadder::setCutoffMod(float mod) {
    _cutoffMod = mod;

}
void AudioFilterDiodeLadder::setSaturation(float sat) {
    _saturation = sat;
}

void AudioFilterDiodeLadder::updateCoefficients(float freq) {
    float wd = 2.0f * 3.14159265f * freq;
    float T = 1.0f / _sampleRate;
    float wa = (2.0f/T) * tanf(wd * T / 2.0f);
    float g = wa * T / 2.0f;
    for(int i = 0; i < 4; ++i) {
        _alpha[i] = g / (1.0f + g);
        _a0[i] = _alpha[i];
        _beta[i] = 1.0f - _alpha[i];
        _G[i] = 1.0f;
    }
    for(int i = 0; i < 4; ++i) _G[i] = 1.0f;
    _gamma_sum = 0.0f;
    for(int i = 0; i < 4; ++i) _gamma_sum += _G[i] * _a0[i];
}

inline float AudioFilterDiodeLadder::processOnePole(int stage, float input, int ch) {
    float output = _a0[stage] * input + _beta[stage] * _z1[ch][stage];
    _z1[ch][stage] = output;
    return output;
}

void AudioFilterDiodeLadder::update() {
    audio_block_t *blockL = receiveReadOnly(0);
    audio_block_t *blockR = receiveReadOnly(1);

    if (!blockL && !blockR) return;

    audio_block_t *outL = allocate();
    audio_block_t *outR = allocate();
    if(!outL || !outR) {
        if(outL) release(outL);
        if(outR) release(outR);
        if(blockL) release(blockL);
        if(blockR) release(blockR);
        return;
    }
    for(int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
        float inL = blockL ? blockL->data[i] / 32768.0f : 0.0f;
        float inR = blockR ? blockR->data[i] / 32768.0f : 0.0f;
        if(_saturation > 0.001f) {
            inL = tanhf(inL * (1.0f + _saturation));
            inR = tanhf(inR * (1.0f + _saturation));
        }

        float currentCutoff = constrain(_cutoff + _cutoffMod, 20.0f, _sampleRate / 2.0f);
        updateCoefficients(currentCutoff);
        // Resonance shaping to reduce instability
// float K = _resonance * 4.0f;

// float sigmaL = 0.0f, sigmaR = 0.0f;
// for(int j = 0; j < 4; ++j) {
//     sigmaL += _G[j] * _z1[0][j];
//     sigmaR += _G[j] * _z1[1][j];
// }
// float denom = 1.0f + K * _gamma_sum;
// float gainBoost = 1.0f + powf(_resonance, 3.0f) * 12.0f;
// gainBoost = fminf(gainBoost, 30.0f);
// float inBoostL = inL * gainBoost;
// float inBoostR = inR * gainBoost;
// float uL = (inBoostL - K * sigmaL) / denom;
// float uR = (inBoostR - K * sigmaR) / denom;

// Feedback gain shaped for smoother resonance rise
float K = _resonance * (4.0f + _resonance);

// Accumulate feedback contributions from each pole stage
float sigmaL = 0.0f, sigmaR = 0.0f;
for (int j = 0; j < 4; ++j) {
    sigmaL += _G[j] * _z1[0][j];
    sigmaR += _G[j] * _z1[1][j];
}

// Denominator clamping to avoid instability
float denom = fmaxf(1.0f + K * (_gamma_sum + 0.0005f), 1e-5f);

// Input gain boost to compensate for feedback signal reduction
float gainBoost = 1.0f + _resonance * (5.0f + 20.0f * _resonance);
gainBoost = fminf(gainBoost, 32.0f);

float inBoostL = inL * gainBoost;
float inBoostR = inR * gainBoost;

// Support true self-oscillation with minimal DC input if needed
if (fabsf(inBoostL) < 1e-6f && _resonance > 1.0f)
    inBoostL += sinf(i * 0.15f) * 1e-4f;
if (fabsf(inBoostR) < 1e-6f && _resonance > 1.0f)
    inBoostR += sinf(i * 0.17f) * 1e-4f;

// Nonlinear feedback stage
float uL = (inBoostL - K * sigmaL) / denom;
float uR = (inBoostR - K * sigmaR) / denom;

// Ladder stages with internal damping for anti-glitch protection
float vL = uL;
float vR = uR;
for (int stage = 0; stage < 4; ++stage) {
    vL = tanhf(processOnePole(stage, vL, 0));  // soft saturation per stage
    vR = tanhf(processOnePole(stage, vR, 1));
}

// Clamp output to avoid wraparound
vL = fmaxf(-1.0f, fminf(1.0f, vL));
vR = fmaxf(-1.0f, fminf(1.0f, vR));
           


        outL->data[i] = (int16_t)(vL * 32767.0f);
        outR->data[i] = (int16_t)(vR * 32767.0f);

    }
    transmit(outL, 0);
    transmit(outR, 1);
    release(outL);
    release(outR);
    if(blockL) release(blockL);
    if(blockR) release(blockR);
}
