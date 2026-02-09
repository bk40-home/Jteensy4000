/*
 * FXChainBlock.h - HYBRID VERSION (JPFX + hexefx Reverb)
 *
 * This combines:
 * - JPFX for tone control, modulation (chorus/flanger/phaser), and delay
 * - hexefx PlateReverb for reverb
 *
 * SIGNAL FLOW:
 *   Input (stereo) → JPFX → Reverb → Mix (dry + JPFX + reverb) → Output (stereo)
 *
 * This gives you the complete JP-8080 effects suite:
 * - Tone control (JPFX)
 * - 11 modulation variations (JPFX)
 * - 5 delay variations (JPFX)
 * - High-quality reverb (hexefx)
 */

#pragma once

#include <Arduino.h>
#include <Audio.h>
#include "AudioEffectJPFX.h"
#include "effect_platereverb_i16.h"  // hexefx reverb

class FXChainBlock {
public:
    FXChainBlock();
    ~FXChainBlock();

    // =========================================================================
    // JPFX CONTROLS (Tone, Modulation, Delay)
    // =========================================================================
    
    // Tone control
    void setBassGain(float dB);
    void setTrebleGain(float dB);
    float getBassGain() const;
    float getTrebleGain() const;

    // Modulation effects (11 variations)
    void setModEffect(int8_t variation);     // -1=off, 0-10=preset
    void setModMix(float mix);               // 0..1
    void setModRate(float hz);               // 0..20 Hz
    void setModFeedback(float fb);           // -1=preset, 0..0.99
    int8_t getModEffect() const;
    float getModMix() const;
    float getModRate() const;
    float getModFeedback() const;
    const char* getModEffectName() const;

    // Delay effects (5 variations)
    void setDelayEffect(int8_t variation);   // -1=off, 0-4=preset
    void setDelayMix(float mix);             // 0..1
    void setDelayFeedback(float fb);         // -1=preset, 0..0.99
    void setDelayTime(float ms);             // 0=preset, 5..1500ms
    int8_t getDelayEffect() const;
    float getDelayMix() const;
    float getDelayFeedback() const;
    float getDelayTime() const;
    const char* getDelayEffectName() const;

    // =========================================================================
    // HEXEFX REVERB CONTROLS
    // =========================================================================
    
    void setReverbRoomSize(float size);      // 0..1
    void setReverbHiDamping(float damp);     // 0..1
    void setReverbLoDamping(float damp);     // 0..1
    float getReverbRoomSize() const;
    float getReverbHiDamping() const;
    float getReverbLoDamping() const;

    // =========================================================================
    // MIX CONTROLS (dry + JPFX + reverb)
    // =========================================================================
    
    void setDryMix(float left, float right);       // 0..1 per channel
    void setJPFXMix(float left, float right);      // 0..1 per channel (JPFX wet)
    void setReverbMix(float left, float right);    // 0..1 per channel
    
    float getDryMixL() const;
    float getDryMixR() const;
    float getJPFXMixL() const;
    float getJPFXMixR() const;
    float getReverbMixL() const;
    float getReverbMixR() const;

    // =========================================================================
    // AUDIO INTERFACE
    // =========================================================================
    
    AudioMixer4& getOutputLeft()  { return _mixerOutL; }
    AudioMixer4& getOutputRight() { return _mixerOutR; }
    AudioEffectJPFX& getJPFXInput() { return _jpfx; }

private:
    // Effects engines
    AudioEffectJPFX _jpfx;                    // JP-8000 tone/mod/delay
    AudioEffectPlateReverb_i16 _plateReverb;  // High-quality reverb

    // Output mixers (4 channels each: dry, JPFX wet, reverb wet, unused)
    AudioMixer4 _mixerOutL;
    AudioMixer4 _mixerOutR;

    // Audio connections
    // JPFX outputs → reverb input
    AudioConnection* _patchJPFXtoReverbL;
    AudioConnection* _patchJPFXtoReverbR;
    
    // JPFX outputs → mixer (channel 1 = JPFX wet)
    AudioConnection* _patchJPFXtoMixerL;
    AudioConnection* _patchJPFXtoMixerR;
    
    // Reverb outputs → mixer (channel 2 = reverb wet)
    AudioConnection* _patchReverbToMixerL;
    AudioConnection* _patchReverbToMixerR;
    
    // Note: Dry signal is mixed in SynthEngine before JPFX
    // Channel 0 of mixer is for dry (connected from amp in SynthEngine)

    // Cached parameters
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
    
    float _reverbRoomSize = 0.5f;
    float _reverbHiDamp = 0.5f;
    float _reverbLoDamp = 0.5f;
    
    float _dryMixL = 1.0f;
    float _dryMixR = 1.0f;
    float _jpfxMixL = 0.0f;
    float _jpfxMixR = 0.0f;
    float _reverbMixL = 0.0f;
    float _reverbMixR = 0.0f;
};