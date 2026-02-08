/*
 * FXChainBlock.h
 *
 * JP-8000 style FX chain for the JT-4000 using AudioEffectJPFX.
 * Provides tone control (bass/treble), modulation effects (chorus/flanger/phaser)
 * and delay effects with preset variations.
 *
 * ARCHITECTURE:
 * - Audio flows: amplifier → JPFX → dry/wet mixer → stereo outputs
 * - JPFX internally processes tone → modulation → delay
 * - When an effect's mix is zero, JPFX bypasses that stage (CPU efficient)
 * - Single mono input, stereo output for wider spatial effects
 *
 * PERFORMANCE NOTES:
 * - JPFX shares a single delay buffer for modulation effects (CPU efficient)
 * - Separate delay buffer for delay effects (max 1500ms)
 * - All processing in single update() call (no multiple AudioStream hops)
 * - Bypass unnecessary processing when mix = 0
 */

#pragma once

#include <Audio.h>
#include "AudioEffectJPFX.h"  // JP-8000 FX engine

class FXChainBlock {
public:
    FXChainBlock();

    // --- Tone controls (±12dB shelving filters) ---
    // Positive values boost, negative values cut
    void setBassGain(float dB);          // -12..+12 dB
    void setTrebleGain(float dB);        // -12..+12 dB
    float getBassGain() const;
    float getTrebleGain() const;

    // --- Modulation effects (11 JP-8000 variations) ---
    // Variations: CHORUS1-3, FLANGER1-3, PHASER1-4, CHORUS_DEEP
    // Pass -1 to disable modulation
    void setModEffect(int8_t variation);  // -1=off, 0..10=preset
    void setModMix(float mix);            // 0..1 wet/dry
    void setModRate(float hz);            // LFO rate override (0=use preset)
    void setModFeedback(float fb);        // 0..0.99 feedback override (-1=use preset)
    
    int8_t getModEffect() const;
    float getModMix() const;
    float getModRate() const;
    float getModFeedback() const;
    const char* getModEffectName() const; // Returns human-readable name

    // --- Delay effects (5 JP-8000 variations) ---
    // Variations: SHORT, LONG, PINGPONG1-3
    // Pass -1 to disable delay
    void setDelayEffect(int8_t variation); // -1=off, 0..4=preset
    void setDelayMix(float mix);           // 0..1 wet/dry
    void setDelayFeedback(float fb);       // 0..0.99 feedback override (-1=use preset)
    void setDelayTime(float ms);           // Time override (0=use preset, 5..1500ms)
    
    int8_t getDelayEffect() const;
    float getDelayMix() const;
    float getDelayFeedback() const;
    float getDelayTime() const;
    const char* getDelayEffectName() const; // Returns human-readable name

    // --- Global mix ---
    void setDryMix(float levelL, float levelR); // 0..1 per channel
    float getDryMixL() const { return _dryMixL; }
    float getDryMixR() const { return _dryMixR; }

    // --- Output access for patching ---
    AudioMixer4& getOutputLeft();
    AudioMixer4& getOutputRight();

    // --- Amplifier input for summed voices ---
    AudioAmplifier _ampIn;

private:
    // JP-8000 effect engine
    AudioEffectJPFX _jpfx;

    // Final stereo mix
    AudioMixer4 _mixerOutL;
    AudioMixer4 _mixerOutR;

    // Patch cords
    AudioConnection* _patchAmpToJPFX;
    AudioConnection* _patchAmpToMixL;
    AudioConnection* _patchAmpToMixR;
    AudioConnection* _patchJPFXToMixL;
    AudioConnection* _patchJPFXToMixR;

    // Cached parameters for UI/preset readback
    float _bassGain = 0.0f;
    float _trebleGain = 0.0f;
    
    int8_t _modEffect = -1;       // -1 = off
    float _modMix = 0.5f;
    float _modRate = 0.0f;        // 0 = use preset
    float _modFeedback = -1.0f;   // -1 = use preset
    
    int8_t _delayEffect = -1;     // -1 = off
    float _delayMix = 0.5f;
    float _delayFeedback = -1.0f; // -1 = use preset
    float _delayTime = 0.0f;      // 0 = use preset
    
    float _dryMixL = 1.0f;
    float _dryMixR = 1.0f;

    // Effect variation name lookup tables
    static constexpr const char* MOD_EFFECT_NAMES[] = {
        "Chorus 1", "Chorus 2", "Chorus 3",
        "Flanger 1", "Flanger 2", "Flanger 3",
        "Phaser 1", "Phaser 2", "Phaser 3", "Phaser 4",
        "Chorus Deep"
    };
    
    static constexpr const char* DELAY_EFFECT_NAMES[] = {
        "Short", "Long",
        "PingPong 1", "PingPong 2", "PingPong 3"
    };
};