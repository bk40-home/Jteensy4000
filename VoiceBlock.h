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


class VoiceBlock {
public:
    void setAmplitude(float amp);
    // --- Lifecycle
    VoiceBlock();
    void update();
    void noteOn(float freq, float velocity);
    void noteOff();

    // --- Oscillator Configuration
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
    void setSubMix(float mix);      // e.g. 0–1
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

    void setGlideEnabled(bool enabled);
    void setGlideTime(float ms);

    // --- Filter
    void setFilterCutoff(float value) ;
    void setFilterResonance(float value);
    // void setFilterDrive(float value) ;
    void setFilterOctaveControl(float octaves);
    // void setFilterPassbandGain(float gain);
    void setFilterEnvAmount(float amt) ;
    void setFilterKeyTrackAmount(float amt);


 
    // --- Envelopes
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

    // --- UI Getters
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
    
    float getAmpAttack() const;
    float getAmpDecay() const;
    float getAmpSustain() const;
    float getAmpRelease() const;
    float getFilterEnvAttack() const;
    float getFilterEnvDecay() const;
    float getFilterEnvSustain() const;
    float getFilterEnvRelease() const;

    // --- Outputs
    AudioStream& output();
    AudioMixer4& frequencyModMixerOsc1();
    AudioMixer4& shapeModMixerOsc1();    
    AudioMixer4& frequencyModMixerOsc2();
    AudioMixer4& shapeModMixerOsc2();
    AudioMixer4& filterModMixer();

    // --- Modulation
    void setModInputs(audio_block_t** modSources);

private:

    OscillatorBlock _osc1, _osc2;
    AudioEffectMultiply _ring1, _ring2;
    SubOscillatorBlock _subOsc;
    AudioSynthNoisePink _noise;
    AudioMixer4 _oscMixer;
    AudioMixer4 _voiceMixer;  // come back

    FilterBlock _filter;

    EnvelopeBlock _filterEnvelope;
    EnvelopeBlock _ampEnvelope; // updating this to take the audio input 
    //AmpBlock _ampBlock; // will I still need this?

    float _osc1Level = 1.0f;
    float _osc2Level = 0.0f;

    float _ring1Level = 0.0f;
    float _ring2Level = 0.0f;
    
    float _baseCutoff = 10000.0f;
    float _keyTrackVal = 0.0f;
    float _filterEnvAmount = 0.0f;
    float _filterLfoAmount = 0.0f;
    float _filterKeyTrackAmount = 0.5f;


    float _currentFreq = 0.0f;
    float _osc1PitchOffset = 0.0f;        // unmodulated offsets
    float _osc2PitchOffset = 0.0f;
    float _subMixLevel = 0.0f;
    float _noiseMixLevel = 0.0f;

    float _subMix = 0.0f;
    float _noiseMix = 0.0f;

    // limit mixers for head room to avoid clipping
    float _on = 0.8f;
    float _clampedLevel(float level);


    AudioConnection* _patchCables[15];
};
