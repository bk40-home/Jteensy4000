#include "synth_pinknoise.h"
#include "effect_multiply.h"
#pragma once

#include <Audio.h>

#include "OscillatorBlock.h"
#include "EnvelopeBlock.h"
#include "FilterBlock.h"
#include "AmpBlock.h"
#include "LFOBlock.h"
#include "SubOscillatorBlock.h"

/**
 * @brief Complete voice with dual oscillators, filter, envelopes, and feedback
 * 
 * VoiceBlock combines:
 * - 2 oscillators (OSC1 with supersaw, OSC2 standard)
 * - Sub oscillator
 * - Pink noise generator
 * - Ring modulators
 * - Resonant filter
 * - Amp & filter envelopes
 * - Feedback oscillation support (NEW)
 */
class VoiceBlock {
public:
    // =========================================================================
    // LIFECYCLE
    // =========================================================================
    VoiceBlock();
    void update();
    void noteOn(float freq, float velocity);
    void noteOff();
    void setAmplitude(float amp);

    // =========================================================================
    // OSCILLATOR CONFIGURATION
    // =========================================================================
    void setOsc1Waveform(int wave);
    void setOsc2Waveform(int wave);
    void setOscMix(float osc1Level, float osc2Level);
    void setOsc1Mix(float oscLevel);
    void setOsc2Mix(float oscLevel);
    void setOsc1PitchOffset(float semis);
    void setOsc2PitchOffset(float semis);
    void setOsc1Detune(float hz);
    void setOsc2Detune(float hz);
    void setOsc1FineTune(float cents);
    void setOsc2FineTune(float cents);
    void setSubMix(float mix);
    void setNoiseMix(float mix);
    void setOsc1SupersawDetune(float amount);
    void setOsc2SupersawDetune(float amount);
    void setOsc1SupersawMix(float amount);
    void setOsc2SupersawMix(float amount);
    void setOsc1FrequencyDcAmp(float amp);
    void setOsc2FrequencyDcAmp(float amp);
    void setOsc1ShapeDcAmp(float amp);
    void setOsc2ShapeDcAmp(float amp);
    void setRing1Mix(float level);
    void setRing2Mix(float level); 
    void setBaseFrequency(float freq);   

    // =========================================================================
    // ARBITRARY WAVEFORM SELECTION
    // =========================================================================
    void setOsc1ArbBank(ArbBank bank);
    void setOsc2ArbBank(ArbBank bank);
    void setOsc1ArbIndex(uint16_t idx);
    void setOsc2ArbIndex(uint16_t idx);

    // =========================================================================
    // GLIDE (PORTAMENTO)
    // =========================================================================
    void setGlideEnabled(bool enabled);
    void setGlideTime(float ms);

    // =========================================================================
    // FEEDBACK OSCILLATION (NEW)
    // =========================================================================

    void setOsc1FeedbackAmount(float amount);
    void setOsc2FeedbackAmount(float amount);
    void setOsc1FeedbackMix(float mix);
    void setOsc2FeedbackMix(float mix);
   
    float getOsc1FeedbackMix() const;
    float getOsc2FeedbackMix() const;

    float getOsc1FeedbackAmount() const;
    float getOsc2FeedbackAmount() const;

    // =========================================================================
    // FILTER
    // =========================================================================
    void setFilterCutoff(float value);
    void setFilterResonance(float value);
    void setFilterOctaveControl(float octaves);
    void setFilterEnvAmount(float amt);
    void setFilterKeyTrackAmount(float amt);
    void setMultimode(float _multimode);
    void setTwoPole(bool enabled);
    void setXpander4Pole(bool enabled);
    void setXpanderMode(uint8_t mode);
    void setBPBlend2Pole(bool enabled);
    void setPush2Pole(bool enabled);
    void setResonanceModDepth(float depth01);

    // =========================================================================
    // ENVELOPES
    // =========================================================================
    void setAmpAttack(float attack);
    void setAmpDecay(float decay);
    void setAmpSustain(float sustain);
    void setAmpRelease(float release);
    void setAmpADSR(float a, float d, float s, float r);

    void setFilterAttack(float attack);
    void setFilterDecay(float decay);
    void setFilterSustain(float sustain);
    void setFilterRelease(float release);
    void setFilterADSR(float a, float d, float s, float r);

    // =========================================================================
    // NEW: PITCH ENVELOPE
    // Separate ADSR that modulates oscillator pitch in semitones.
    // Depth=0 causes noteOn to skip triggering (no CPU spent).
    // =========================================================================
    void setPitchEnvAttack(float ms);
    void setPitchEnvDecay(float ms);
    void setPitchEnvSustain(float level);
    void setPitchEnvRelease(float ms);
    void setPitchEnvDepth(float semitones);   // ±24 semitones
    float getPitchEnvDepth() const { return _pitchEnvDepth; }

    // Pitch envelope audio output — wired by SynthEngine to frequencyModMixer
    AudioStream& pitchEnvOutput();

    // =========================================================================
    // NEW: VELOCITY SENSITIVITY
    // Three 0..1 scalars applied on noteOn.
    //   0 = no velocity effect  |  1 = full velocity control
    // =========================================================================
    void setVelocityAmpSens(float s)    { _velAmpSens    = s; }
    void setVelocityFilterSens(float s) { _velFilterSens = s; }
    void setVelocityEnvSens(float s)    { _velEnvSens    = s; }

    // =========================================================================
    // GETTERS (UI/STATE QUERY)
    // =========================================================================
    int getOsc1Waveform() const;
    int getOsc2Waveform() const;
    float getOsc1PitchOffset() const;
    float getOsc2PitchOffset() const;
    float getOsc1Detune() const;
    float getOsc2Detune() const;
    float getOsc1FineTune() const;
    float getOsc2FineTune() const;
    float getOscMix1() const;
    float getOscMix2() const;
    float getSubMix() const;
    float getNoiseMix() const;
    float getOsc1SupersawDetune() const;
    float getOsc2SupersawDetune() const;
    float getOsc1SupersawMix() const;
    float getOsc2SupersawMix() const;
    bool  getGlideEnabled() const;
    float getGlideTime() const;
    float getOsc1FrequencyDc() const;
    float getOsc2FrequencyDc() const;
    float getOsc1ShapeDc() const;
    float getOsc2ShapeDc() const;
    float getRing1Mix() const;
    float getRing2Mix() const;   

    float getFilterCutoff() const;
    float getFilterResonance() const; 
    // float getFilterDrive() const; 
    float getFilterOctaveControl() const;
    // float getFilterPassbandGain() const;
    float getFilterEnvAmount() const; 
    float getFilterKeyTrackAmount() const; 

    float getMultimode() const { return _multimode; }
    bool getTwoPole() const { return _useTwoPole; }
    bool getXpander4Pole() const { return _xpander4Pole; }
    uint8_t getXpanderMode() const { return _xpanderMode; }
    bool getBPBlend2Pole() const { return _bpBlend2Pole; } 
    bool getPush2Pole() const { return _push2Pole; }
    float getResonanceModDepth() const { return _resonanceModDepth; }   
    
    float getAmpAttack() const;
    float getAmpDecay() const;
    float getAmpSustain() const;
    float getAmpRelease() const;

    float getFilterEnvAttack() const;
    float getFilterEnvDecay() const;
    float getFilterEnvSustain() const;
    float getFilterEnvRelease() const;

    // =========================================================================
    // AUDIO OUTPUTS & MODULATION MIXERS
    // =========================================================================
    AudioStream& output();
    AudioMixer4& frequencyModMixerOsc1();
    AudioMixer4& frequencyModMixerOsc2();
    AudioMixer4& shapeModMixerOsc1();
    AudioMixer4& shapeModMixerOsc2();
    AudioMixer4& filterModMixer();

    // --- Modulation
    void setModInputs(audio_block_t** modSources);

    // SynthEngine needs to access _pitchEnvPatch1/2 and pitchEnvOutput()
    // to wire the pitch envelope into the audio graph at construction time.
    friend class SynthEngine;

private:
    // Audio components
    OscillatorBlock _osc1{true};   // OSC1 has supersaw capability
    OscillatorBlock _osc2{false};  // OSC2 no supersaw
    AudioEffectMultiply _ring1, _ring2;
    SubOscillatorBlock _subOsc;
    AudioSynthNoisePink _noise;
    AudioMixer4 _oscMixer;
    AudioMixer4 _voiceMixer;

    FilterBlock _filter;

    EnvelopeBlock _filterEnvelope;
    EnvelopeBlock _ampEnvelope;

    // State variables
    float _osc1Level = 1.0f;
    float _osc2Level = 0.0f;

    float _ring1Level = 0.0f;
    float _ring2Level = 0.0f;
    float _osc1FeedbackMixLevel = 0.0f;
    float _osc2FeedbackMixLevel = 0.0f;
    
    float _baseCutoff = 10000.0f;
    float _keyTrackVal = 0.0f;
    float _filterEnvAmount = 0.0f;
    float _filterLfoAmount = 0.0f;
    float _filterKeyTrackAmount = 0.5f;
    float _multimode = 0.0f;
    float _resonanceModDepth = 0.0f;
    bool    _useTwoPole   = false;
    bool    _xpander4Pole = false;
    uint8_t _xpanderMode  = 0;
    bool    _bpBlend2Pole = false;
    bool    _push2Pole    = false;

    bool _isActive = false;

    float _currentFreq = 0.0f;
    float _osc1PitchOffset = 0.0f;
    float _osc2PitchOffset = 0.0f;
    float _subMixLevel = 0.0f;
    float _noiseMixLevel = 0.0f;

    float _subMix = 0.0f;
    float _noiseMix = 0.0f;

    // Limit mixers for headroom
    float _on = 0.9f;
    float _clampedLevel(float level);

    AudioConnection* _patchCables[18];  // +1 for pitch envelope DC source

    // -----------------------------------------------------------------------
    // NEW: Pitch envelope
    // AudioEffectEnvelope is a through-type: it needs an input signal (DC=1.0)
    // which it multiplies by the ADSR shape.  _pitchEnvDc provides that 1.0 feed.
    // The output (0..1) flows to frequencyModMixer input 3 via SynthEngine wiring.
    // -----------------------------------------------------------------------
    AudioSynthWaveformDc _pitchEnvDc;   // constant 1.0 feed into pitch envelope
    EnvelopeBlock _pitchEnvelope;



    // Pitch env depth in semitones (signed, ±24)
    float _pitchEnvDepth = 0.0f;

    // -----------------------------------------------------------------------
    // NEW: Velocity sensitivity scalars
    // -----------------------------------------------------------------------
    float _velAmpSens    = 0.0f;
    float _velFilterSens = 0.0f;
    float _velEnvSens    = 0.0f;

    // Base filter env amount (before velocity scaling)
    float _baseFilterEnvAmount = 0.0f;
};