/*
 * FXChainBlock.h - JPFX VERSION
 *
 * JP-8000/8080 style effects chain using AudioEffectJPFX.
 * Provides tone control, modulation (chorus/flanger/phaser), and delay.
 *
 * ARCHITECTURE:
 *   Input → AudioEffectJPFX (stereo) → Outputs
 *
 * JPFX handles internally:
 *   - Bass/Treble tone control (±12dB shelving)
 *   - 11 Modulation variations (Chorus 1-3, Flanger 1-3, Phaser 1-4, Deep Chorus)
 *   - 5 Delay variations (Short, Long, Ping-Pong 1-3)
 *   - Dry/wet mixing for each effect stage
 */

#pragma once

#include <Arduino.h>
#include <Audio.h>
#include "AudioEffectJPFX.h"

class FXChainBlock {
public:
    // Constructor - sets up JPFX with default parameters
    FXChainBlock();
    
    // Destructor - cleans up audio connections
    ~FXChainBlock();

    // =========================================================================
    // TONE CONTROL (±12dB shelving filters)
    // =========================================================================
    void setBassGain(float dB);      // -12..+12 dB
    void setTrebleGain(float dB);    // -12..+12 dB
    float getBassGain() const;
    float getTrebleGain() const;

    // =========================================================================
    // MODULATION EFFECTS (11 variations)
    // =========================================================================
    // Variations: 0-10 = presets, -1 = off
    // 0=Chorus1, 1=Chorus2, 2=Chorus3, 3=Flanger1, 4=Flanger2, 5=Flanger3,
    // 6=Phaser1, 7=Phaser2, 8=Phaser3, 9=Phaser4, 10=ChorusDeep
    void setModEffect(int8_t variation);
    void setModMix(float mix);           // 0..1
    void setModRate(float hz);           // 0..20 Hz (0=use preset)
    void setModFeedback(float fb);       // -1=preset, 0..0.99=override
    
    int8_t getModEffect() const;
    float getModMix() const;
    float getModRate() const;
    float getModFeedback() const;
    const char* getModEffectName() const;

    // =========================================================================
    // DELAY EFFECTS (5 variations)
    // =========================================================================
    // Variations: 0-4 = presets, -1 = off
    // 0=Short, 1=Long, 2=PingPong1, 3=PingPong2, 4=PingPong3
    void setDelayEffect(int8_t variation);
    void setDelayMix(float mix);         // 0..1
    void setDelayFeedback(float fb);     // -1=preset, 0..0.99=override
    void setDelayTime(float ms);         // 0=preset, 5..1500ms=override
    
    int8_t getDelayEffect() const;
    float getDelayMix() const;
    float getDelayFeedback() const;
    float getDelayTime() const;
    const char* getDelayEffectName() const;

    // =========================================================================
    // DRY MIX (global output level control)
    // =========================================================================
    void setDryMix(float left, float right);  // 0..1 for each channel
    float getDryMixL() const;
    float getDryMixR() const;

    // =========================================================================
    // AUDIO INTERFACE
    // =========================================================================
    // Get output mixers for connecting to main output
    AudioMixer4& getOutputLeft()  { return _mixerOutL; }
    AudioMixer4& getOutputRight() { return _mixerOutR; }
    
    // Get JPFX input for connecting from amp output
    AudioEffectJPFX& getJPFXInput() { return _jpfx; }

private:
    // JPFX engine (handles all effects internally)
    AudioEffectJPFX _jpfx;
    
    // Output mixers (currently just pass-through from JPFX)
    AudioMixer4 _mixerOutL;
    AudioMixer4 _mixerOutR;
    
    // Audio patch cords
    AudioConnection* _patchJPFXL;    // JPFX left → mixer left
    AudioConnection* _patchJPFXR;    // JPFX right → mixer right
    
    // Cached parameters for getters
    float _bassGain = 0.0f;
    float _trebleGain = 0.0f;
    int8_t _modEffect = -1;
    float _modMix = 0.5f;
    float _modRate = 0.0f;
    float _modFeedback = -1.0f;
    int8_t _delayEffect = -1;
    float _delayMix = 0.5f;
    float _delayFeedback = -1.0f;
    float _delayTime = 0.0f;
    float _dryMixL = 1.0f;
    float _dryMixR = 1.0f;
};