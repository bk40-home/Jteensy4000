/*
 * FXChainBlock.cpp - JPFX VERSION
 *
 * Implementation of JP-8000 style effects using AudioEffectJPFX.
 * All effects processing happens inside AudioEffectJPFX.
 * This class just wraps it with proper audio connections and caching.
 */

#include "FXChainBlock.h"

// Effect name arrays for display
static const char* modEffectNames[] = {
    "Chorus 1",
    "Chorus 2",
    "Chorus 3",
    "Flanger 1",
    "Flanger 2",
    "Flanger 3",
    "Phaser 1",
    "Phaser 2",
    "Phaser 3",
    "Phaser 4",
    "Chorus Deep"
};

static const char* delayEffectNames[] = {
    "Short",
    "Long",
    "PingPong 1",
    "PingPong 2",
    "PingPong 3"
};

// ============================================================================
// CONSTRUCTOR
// ============================================================================

FXChainBlock::FXChainBlock()
    : _jpfx()
{
    // Connect JPFX outputs to mixers (input connected from SynthEngine)
    _patchJPFXL = new AudioConnection(_jpfx, 0, _mixerOutL, 0);
    _patchJPFXR = new AudioConnection(_jpfx, 1, _mixerOutR, 0);
    
    // Set default mixer gains (full wet from JPFX since it handles dry/wet internally)
    _mixerOutL.gain(0, 1.0f);
    _mixerOutR.gain(0, 1.0f);
    
    // Initialize JPFX to defaults (all effects off)
    // Note: JPFX handles dry/wet internally via modulation and delay mix
    _jpfx.setBassGain(0.0f);
    _jpfx.setTrebleGain(0.0f);
    _jpfx.setModEffect(AudioEffectJPFX::JPFX_MOD_OFF);
    _jpfx.setModMix(0.5f);
    _jpfx.setDelayEffect(AudioEffectJPFX::JPFX_DELAY_OFF);
    _jpfx.setDelayMix(0.5f);
    // No setDryMix - JPFX doesn't have this method
}

// ============================================================================
// DESTRUCTOR
// ============================================================================

FXChainBlock::~FXChainBlock()
{
    // Clean up audio connections
    if (_patchJPFXL) delete _patchJPFXL;
    if (_patchJPFXR) delete _patchJPFXR;
}

// ============================================================================
// TONE CONTROL
// ============================================================================

void FXChainBlock::setBassGain(float dB)
{
    _bassGain = dB;
    _jpfx.setBassGain(dB);
}

void FXChainBlock::setTrebleGain(float dB)
{
    _trebleGain = dB;
    _jpfx.setTrebleGain(dB);
}

float FXChainBlock::getBassGain() const
{
    return _bassGain;
}

float FXChainBlock::getTrebleGain() const
{
    return _trebleGain;
}

// ============================================================================
// MODULATION EFFECTS
// ============================================================================

void FXChainBlock::setModEffect(int8_t variation)
{
    _modEffect = variation;
    
    // Map to AudioEffectJPFX enum
    AudioEffectJPFX::ModEffectType type;
    if (variation < 0) {
        type = AudioEffectJPFX::JPFX_MOD_OFF;
    } else {
        // Clamp to valid range
        if (variation > 10) variation = 10;
        type = (AudioEffectJPFX::ModEffectType)variation;
    }
    
    _jpfx.setModEffect(type);
}

void FXChainBlock::setModMix(float mix)
{
    _modMix = mix;
    _jpfx.setModMix(mix);
}

void FXChainBlock::setModRate(float hz)
{
    _modRate = hz;
    _jpfx.setModRate(hz);
}

void FXChainBlock::setModFeedback(float fb)
{
    _modFeedback = fb;
    _jpfx.setModFeedback(fb);
}

int8_t FXChainBlock::getModEffect() const
{
    return _modEffect;
}

float FXChainBlock::getModMix() const
{
    return _modMix;
}

float FXChainBlock::getModRate() const
{
    return _modRate;
}

float FXChainBlock::getModFeedback() const
{
    return _modFeedback;
}

const char* FXChainBlock::getModEffectName() const
{
    if (_modEffect < 0) return "Off";
    if (_modEffect > 10) return "Unknown";
    return modEffectNames[_modEffect];
}

// ============================================================================
// DELAY EFFECTS
// ============================================================================

void FXChainBlock::setDelayEffect(int8_t variation)
{
    _delayEffect = variation;
    
    // Map to AudioEffectJPFX enum
    AudioEffectJPFX::DelayEffectType type;
    if (variation < 0) {
        type = AudioEffectJPFX::JPFX_DELAY_OFF;
    } else {
        // Clamp to valid range
        if (variation > 4) variation = 4;
        type = (AudioEffectJPFX::DelayEffectType)variation;
    }
    
    _jpfx.setDelayEffect(type);
}

void FXChainBlock::setDelayMix(float mix)
{
    _delayMix = mix;
    _jpfx.setDelayMix(mix);
}

void FXChainBlock::setDelayFeedback(float fb)
{
    _delayFeedback = fb;
    _jpfx.setDelayFeedback(fb);
}

void FXChainBlock::setDelayTime(float ms)
{
    _delayTime = ms;
    _jpfx.setDelayTime(ms);
}

int8_t FXChainBlock::getDelayEffect() const
{
    return _delayEffect;
}

float FXChainBlock::getDelayMix() const
{
    return _delayMix;
}

float FXChainBlock::getDelayFeedback() const
{
    return _delayFeedback;
}

float FXChainBlock::getDelayTime() const
{
    return _delayTime;
}

const char* FXChainBlock::getDelayEffectName() const
{
    if (_delayEffect < 0) return "Off";
    if (_delayEffect > 4) return "Unknown";
    return delayEffectNames[_delayEffect];
}

// ============================================================================
// DRY MIX (Note: JPFX handles dry/wet internally, no external control)
// ============================================================================

void FXChainBlock::setDryMix(float left, float right)
{
    // Cache for compatibility but JPFX doesn't expose dry mix control
    // Dry/wet is handled internally via modulation and delay mix parameters
    _dryMixL = left;
    _dryMixR = right;
    // No _jpfx.setDryMix() - this method doesn't exist in AudioEffectJPFX
}

float FXChainBlock::getDryMixL() const
{
    return _dryMixL;
}

float FXChainBlock::getDryMixR() const
{
    return _dryMixR;
}