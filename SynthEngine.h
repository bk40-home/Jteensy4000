#pragma once

#include "VoiceBlock.h"
#include "LFOBlock.h"
#include "FXChainBlock.h"
#include "Mapping.h"  // Keep: shared CC mappings for forward/inverse UI use

// Optional: pull names local for brevity (or prefix with JT4000Map:: each time)
using namespace JT4000Map;

#define MAX_VOICES 4

class SynthEngine {
public:
    // --- Lifecycle
    SynthEngine();
    void noteOn(byte note, float velocity);
    void noteOff(byte note);
    void update();

    // --- Oscillator
    void setOscWaveforms(int wave1, int wave2);
    void setOsc1Waveform(int wave);
    void setOsc2Waveform(int wave);
    void setOsc1PitchOffset(float semis);
    void setOsc2PitchOffset(float semis);
    void setOsc1Detune(float semis);
    void setOsc2Detune(float semis);
    void setOsc1FineTune(float cents);
    void setOsc2FineTune(float cents);
    void setOscMix(float osc1Level, float osc2Level);
    void setOsc1Mix(float oscLevel);
    void setOsc2Mix(float oscLevel);
    void setSubMix(float mix);      // e.g. 0–1
    void setNoiseMix(float mix);
    void setSupersawDetune(uint8_t oscIndex, float amount);
    void setSupersawMix(uint8_t oscIndex, float amount);
    void setOsc1FrequencyDcAmp(float amp);
    void setOsc2FrequencyDcAmp(float amp);
    void setOsc1ShapeDcAmp(float amp);
    void setOsc2ShapeDcAmp(float amp);
    void setRing1Mix(float level);
    void setRing2Mix(float level);   

    //Amp modulation
    void SetAmpModFixedLevel(float level);
    float GetAmpModFixedLevel() const;

    // --- LFO 1
    void setLFO1Frequency(float hz);
    void setLFO1Amount(float amt);
    void setLFO1Waveform(int type);  
    void setLFO1Destination(LFODestination dest);

    float getLFO1Frequency() const;
    float getLFO1Amount() const;
    int getLFO1Waveform() const;
    LFODestination getLFO1Destination() const;
    

    // --- LFO 2
    void setLFO2Frequency(float hz);
    void setLFO2Amount(float amt);
    void setLFO2Waveform(int type); 
    void setLFO2Destination(LFODestination dest);

    float getLFO2Frequency() const;
    int getLFO2Waveform() const;
    float getLFO2Amount() const;
    LFODestination getLFO2Destination() const;
   

    // --- Envelopes
    void setFilterEnvAmount(float amt);

    float getAmpAttack() const;
    float getAmpDecay() const;
    float getAmpSustain() const;
    float getAmpRelease() const;

    float getFilterEnvAttack() const;
    float getFilterEnvDecay() const;
    float getFilterEnvSustain() const;
    float getFilterEnvRelease() const;

    // FX
    void setFXReverbRoomSize(float size);
    void setFXReverbDamping(float damp);
    void setFXDelayBaseTime(float baseMs);
    void setFXDryMix(float level);
    void setFXReverbMix(float level);
    void setFXDelayMix(float level);
    void setFXDelayFeedback(float feedback);

    // --- UI Getters
    int getOsc1Waveform() const;
    int getOsc2Waveform() const;

    // Supersaw parameter getters
    float getSupersawDetune(uint8_t oscIndex) const;
    float getSupersawMix(uint8_t oscIndex) const;


    float getOsc1PitchOffset() const;
    float getOsc2PitchOffset() const;

    float getOsc1Detune() const;
    float getOsc2Detune() const;

    float getOsc1FineTune() const;
    float getOsc2FineTune() const;

    float getOscMix1() const;
    float getOscMix2() const;

    float getOsc1FrequencyDc() const;
    float getOsc2FrequencyDc() const;

    float getOsc1ShapeDc() const;
    float getOsc2ShapeDc() const;

    const char* getOsc1WaveformName() const;
    const char* getOsc2WaveformName() const;

    float getSubMix() const;
    float getNoiseMix() const;
    
    float getRing1Mix() const;
    float getRing2Mix() const;  

// --- Filter
    void setFilterCutoff(float value);
    void setFilterResonance(float value);
    //void setFilterLfoAmount(float amt);
    void setFilterKeyTrackAmount(float amt);
    void setFilterDrive(float drive);
    void setFilterOctaveControl(float octaves);
    void setFilterPassbandGain(float gain);

    float getFilterCutoff() const;
    float getFilterResonance() const;
    float getFilterDrive() const;
    float getFilterEnvAmount() const;
    //float getFilterLfoAmount() const;
    float getFilterKeyTrackAmount() const;
    float getFilterOctaveControl() const;
    float getFilterPassbandGain() const;

    bool  getGlideEnabled()const;
    float getGlideTimeMs()const;
    float getAmpModFixedLevel()const;

    // --- MIDI
    void handleControlChange(byte channel, byte control, byte value);

    // --- Outputs
    AudioMixer4& getVoiceMixer() { return _voiceMixer; }
    AudioMixer4& getFXOutL() { return _fxChain.getOutputLeft(); }
    AudioMixer4& getFXOutR() { return _fxChain.getOutputRight(); }



private:
    VoiceBlock _voices[MAX_VOICES];
    bool _activeNotes[MAX_VOICES];
    byte _noteToVoice[128];              // MIDI note to voice index
    uint32_t _noteTimestamps[MAX_VOICES]; // For voice stealing
    uint32_t _clock = 0;                 // Incremented per noteOn
    FXChainBlock _fxChain;

    bool _glideEnabled = false;
    float _glideTimeMs = 0.0f;
    float _lastNoteFreq = 0.0f;


    LFOBlock _lfo1;
    LFOBlock _lfo2;

    float _ampModFixedLevel = 1.0f;
    AudioSynthWaveformDc _ampModFixedDc;
    AudioSynthWaveformDc _ampModLimitFixedDc;

    AudioEffectMultiply _ampMultiply;
    AudioMixer4 _ampModMixer;
    AudioMixer4 _ampModLimiterMixer; // limit to never go below or above 1
    


    float _lfo1Frequency = 0.0f;
    float _lfo2Frequency = 0.0f;
    int _lfo1Type = 0;
    float _lfo1Amount = 0.0f;
    float _lfo2Amount = 0.0f;
    int _lfo2Type = 0;

    


    AudioMixer4 _voiceMixer;

    AudioConnection* _voicePatch[MAX_VOICES];
    AudioConnection* _voicePatchLFO1ShapeOsc1[MAX_VOICES];
    AudioConnection* _voicePatchLFO1ShapeOsc2[MAX_VOICES];
    AudioConnection* _voicePatchLFO1FrequencyOsc1[MAX_VOICES];
    AudioConnection* _voicePatchLFO1FrequencyOsc2[MAX_VOICES];
    AudioConnection* _voicePatchLFO1Filter[MAX_VOICES];
    AudioConnection* _voicePatchLFO2ShapeOsc1[MAX_VOICES];
    AudioConnection* _voicePatchLFO2ShapeOsc2[MAX_VOICES];
    AudioConnection* _voicePatchLFO2FrequencyOsc1[MAX_VOICES];
    AudioConnection* _voicePatchLFO2FrequencyOsc2[MAX_VOICES];
    AudioConnection* _voicePatchLFO2Filter[MAX_VOICES];

    AudioConnection* _patchAmpModFixedDcToAmpModMixer;
    AudioConnection* _patchLFO1ToAmpModMixer;
    AudioConnection* _patchLFO2ToAmpModMixer;
    AudioConnection* _patchAmpModMixerToAmpMultiply;
    AudioConnection* _patchVoiceMixerToAmpMultiply;

    AudioConnection* _fxPatchIn;
};
