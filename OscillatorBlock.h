#pragma once

#include <Audio.h>
#include "Waveforms.h"  // canonical wave IDs + Supersaw
#include "AKWF_All.h"   // AKWF bank definitions and helpers
#include "AudioSynthSupersaw.h"


// OscillatorBlock manages a hybrid waveform oscillator with modulation and tuning
class OscillatorBlock {
public:
    // --- Lifecycle
    OscillatorBlock();
    void update();
    void noteOn(float freq, float velocity);
    void noteOff();

    // --- Parameter Setters
    void setWaveformType(int type);
    void setFrequency(float freq);
    void setAmplitude(float amp);
    void setBaseFrequency(float freq);
    void setPitchOffset(float semis);
    void setPitchModulation(float semis);
    void setDetune(float hz);
    void setFineTune(float cents);
    void setSupersawDetune(float amount); // NEW
    void setSupersawMix(float mix);       // NEW
    void setGlideEnabled(bool enabled);
    void setGlideTime(float ms);
    void setFrequencyDcAmp(float amp);
    void setShapeDcAmp(float amp);
    // Set the current AKWF bank and table index.  When the waveform type is
    // WAVEFORM_ARBITRARY, these select which bank and which single-cycle
    // waveform from that bank to use.  Bank defaults to ArbBank::BwBlended.
    void setArbBank(ArbBank b);
    void setArbTableIndex(uint16_t idx);
    ArbBank getArbBank() const { return _arbBank; }
    uint16_t getArbTableIndex() const { return _arbIndex; }

    // Parameter Getters
    int getWaveform() const;
    float getPitchOffset() const;
    float getDetune() const;
    float getFineTune() const;
    float getSupersawDetune() const;
    float getSupersawMix() const;
    bool getGlideEnabled() const;
    float getGlideTime() const;
    float getFrequencyDcAmp() const;
    float getShapeDcAmp() const;

    // --- Outputs
    AudioStream& output();
    AudioMixer4& frequencyModMixer();
    AudioMixer4& shapeModMixer();

private:
    AudioSynthWaveformDc _frequencyDc;
    AudioSynthWaveformDc _shapeDc;
    AudioMixer4 _frequencyModMixer;
    AudioMixer4 _shapeModMixer;
    AudioSynthWaveformModulated _mainOsc;
    AudioSynthSupersaw _supersaw;
    AudioMixer4 _outputMix;
    AudioConnection* _patchfrequencyDc;
    AudioConnection* _patchshapeDc;
    AudioConnection* _patchfrequency;
    AudioConnection* _patchshape;
    AudioConnection* _patchMainOsc;  // NEW: connects _mainOsc to _outputMix
    AudioConnection* _patchSupersaw;


    int _currentType = 1;
    float _baseFreq = 440.0f;
    float _pitchOffset = 0.0f;
    float _pitchModulation = 0.0f;
    float _detune = 0.0f;
    float _fineTune = 0.0f;
    float _lastVelocity = 1.0f;
    float _supersawDetune = 0.0f;
    float _supersawMix = 0.5f;
    bool _glideEnabled = false;
    float _glideTimeMs = 0.0f;
    float _glideRate = 0.0f;
    float _targetFreq = 0.0f;
    bool _glideActive = false;
    float _frequencyDcAmp = 0.0f;
    float _shapeDcAmp = 0.0f;
    ArbBank  _arbBank  = ArbBank::BwBlended; // Current arbitrary waveform bank
    uint16_t _arbIndex = 0;                   // Current table index within the bank
    void _applyArbWave();  // helper to (re)load table when ARB or index changes

    // Per-instance last frequency to avoid cross-talk between instances
    float _lastFreq = -1.0f;



};
