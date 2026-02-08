/*
 * FXChainBlock.cpp
 *
 * Implements JP-8000 style FX chain using AudioEffectJPFX.
 * All audio processing happens in the JPFX module; this class manages
 * parameter routing and final wet/dry mix.
 */

#include "FXChainBlock.h"

FXChainBlock::FXChainBlock()
    : _jpfx()
{
    // Initialize JPFX with defaults
    _jpfx.setBassGain(0.0f);
    _jpfx.setTrebleGain(0.0f);
    _jpfx.setModEffect(AudioEffectJPFX::JPFX_MOD_OFF);
    _jpfx.setDelayEffect(AudioEffectJPFX::JPFX_DELAY_OFF);

    // Create patch cords
    // Amplifier → JPFX (mono in, stereo out)
    _patchAmpToJPFX   = new AudioConnection(_ampIn, _jpfx);
    
    // Dry path: amp → mixers (channel 0 = dry)
    _patchAmpToMixL   = new AudioConnection(_ampIn, 0, _mixerOutL, 0);
    _patchAmpToMixR   = new AudioConnection(_ampIn, 0, _mixerOutR, 0);
    
    // Wet path: JPFX → mixers (channel 1 = wet)
    _patchJPFXToMixL  = new AudioConnection(_jpfx, 0, _mixerOutL, 1);
    _patchJPFXToMixR  = new AudioConnection(_jpfx, 0, _mixerOutR, 1);

    // Default mixer gains: dry = 1, wet controlled by effect mix params
    setDryMix(1.0f, 1.0f);
    
    // JPFX handles wet/dry internally; we just pass the signal through
    // Set mixer channel 1 (wet) to full level - JPFX controls actual mix
    _mixerOutL.gain(1, 1.0f);
    _mixerOutR.gain(1, 1.0f);
}

// -----------------------------------------------------------------------------
// Tone control
// -----------------------------------------------------------------------------
void FXChainBlock::setBassGain(float dB) {
    // Constrain to ±12dB
    if (dB < -12.0f) dB = -12.0f;
    if (dB > 12.0f) dB = 12.0f;
    _bassGain = dB;
    _jpfx.setBassGain(dB);
}

void FXChainBlock::setTrebleGain(float dB) {
    // Constrain to ±12dB
    if (dB < -12.0f) dB = -12.0f;
    if (dB > 12.0f) dB = 12.0f;
    _trebleGain = dB;
    _jpfx.setTrebleGain(dB);
}

float FXChainBlock::getBassGain() const { return _bassGain; }
float FXChainBlock::getTrebleGain() const { return _trebleGain; }

// -----------------------------------------------------------------------------
// Modulation effects
// -----------------------------------------------------------------------------
void FXChainBlock::setModEffect(int8_t variation) {
    // Constrain: -1=off, 0..10=valid variations
    if (variation < -1) variation = -1;
    if (variation > 10) variation = 10;
    
    _modEffect = variation;
    
    if (variation < 0) {
        _jpfx.setModEffect(AudioEffectJPFX::JPFX_MOD_OFF);
    } else {
        _jpfx.setModEffect((AudioEffectJPFX::ModEffectType)variation);
    }
}

void FXChainBlock::setModMix(float mix) {
    if (mix < 0.0f) mix = 0.0f;
    if (mix > 1.0f) mix = 1.0f;
    _modMix = mix;
    _jpfx.setModMix(mix);
}

void FXChainBlock::setModRate(float hz) {
    // 0 = use preset, >0 = override
    if (hz < 0.0f) hz = 0.0f;
    if (hz > 20.0f) hz = 20.0f;  // Cap at 20Hz to avoid metallic artifacts
    _modRate = hz;
    _jpfx.setModRate(hz);
}

void FXChainBlock::setModFeedback(float fb) {
    // -1 = use preset, 0..0.99 = override
    if (fb < -1.0f) fb = -1.0f;
    if (fb > 0.99f) fb = 0.99f;
    _modFeedback = fb;
    _jpfx.setModFeedback(fb);
}

int8_t FXChainBlock::getModEffect() const { return _modEffect; }
float FXChainBlock::getModMix() const { return _modMix; }
float FXChainBlock::getModRate() const { return _modRate; }
float FXChainBlock::getModFeedback() const { return _modFeedback; }

const char* FXChainBlock::getModEffectName() const {
    if (_modEffect < 0 || _modEffect > 10) return "Off";
    return MOD_EFFECT_NAMES[_modEffect];
}

// -----------------------------------------------------------------------------
// Delay effects
// -----------------------------------------------------------------------------
void FXChainBlock::setDelayEffect(int8_t variation) {
    // Constrain: -1=off, 0..4=valid variations
    if (variation < -1) variation = -1;
    if (variation > 4) variation = 4;
    
    _delayEffect = variation;
    
    if (variation < 0) {
        _jpfx.setDelayEffect(AudioEffectJPFX::JPFX_DELAY_OFF);
    } else {
        _jpfx.setDelayEffect((AudioEffectJPFX::DelayEffectType)variation);
    }
}

void FXChainBlock::setDelayMix(float mix) {
    if (mix < 0.0f) mix = 0.0f;
    if (mix > 1.0f) mix = 1.0f;
    _delayMix = mix;
    _jpfx.setDelayMix(mix);
}

void FXChainBlock::setDelayFeedback(float fb) {
    // -1 = use preset, 0..0.99 = override
    if (fb < -1.0f) fb = -1.0f;
    if (fb > 0.99f) fb = 0.99f;
    _delayFeedback = fb;
    _jpfx.setDelayFeedback(fb);
}

void FXChainBlock::setDelayTime(float ms) {
    // 0 = use preset, 5..1500ms = override
    if (ms < 0.0f) ms = 0.0f;
    if (ms > 1500.0f) ms = 1500.0f;
    _delayTime = ms;
    _jpfx.setDelayTime(ms);
}

int8_t FXChainBlock::getDelayEffect() const { return _delayEffect; }
float FXChainBlock::getDelayMix() const { return _delayMix; }
float FXChainBlock::getDelayFeedback() const { return _delayFeedback; }
float FXChainBlock::getDelayTime() const { return _delayTime; }

const char* FXChainBlock::getDelayEffectName() const {
    if (_delayEffect < 0 || _delayEffect > 4) return "Off";
    return DELAY_EFFECT_NAMES[_delayEffect];
}

// -----------------------------------------------------------------------------
// Global mix control
// -----------------------------------------------------------------------------
void FXChainBlock::setDryMix(float levelL, float levelR) {
    if (levelL < 0.0f) levelL = 0.0f;
    if (levelL > 1.0f) levelL = 1.0f;
    if (levelR < 0.0f) levelR = 0.0f;
    if (levelR > 1.0f) levelR = 1.0f;
    
    _dryMixL = levelL;
    _dryMixR = levelR;
    
    _mixerOutL.gain(0, levelL);
    _mixerOutR.gain(0, levelR);
}

// -----------------------------------------------------------------------------
// Output access
// -----------------------------------------------------------------------------
AudioMixer4& FXChainBlock::getOutputLeft()  { return _mixerOutL; }
AudioMixer4& FXChainBlock::getOutputRight() { return _mixerOutR; }