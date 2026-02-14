#pragma once

#include <Audio.h>
#include "Waveforms.h"
#include "AKWF_All.h"
#include "AudioSynthSupersaw.h"

/**
 * @brief Oscillator block with JP-8000 style feedback oscillation
 * 
 * Features:
 * - Dual oscillator (main + optional supersaw)
 * - Resonant feedback comb (JP-8000 simulation)
 * - Arbitrary waveform support (AKWF)
 * - Modulation inputs for frequency and shape
 * - Null-safe supersaw (OSC1 only, OSC2 fallback)
 * - CPU-efficient dirty flag system
 */
class OscillatorBlock {
public:
    // =========================================================================
    // LIFECYCLE
    // =========================================================================
    
    /**
     * @brief Constructor with optional supersaw capability
     * @param enableSupersaw true for OSC1 (supersaw capable), false for OSC2
     */
    OscillatorBlock(bool enableSupersaw = false);
    
    /**
     * @brief Update frequency and pitch parameters (called from voice update)
     * Only recalculates when _freqDirty flag is set for CPU efficiency
     */
    void update();
    
    /**
     * @brief Trigger note on with frequency and velocity
     * @param freq Target frequency in Hz
     * @param velocity MIDI velocity (0-127)
     */
    void noteOn(float freq, float velocity);
    
    /**
     * @brief Trigger note off (silence oscillators)
     */
    void noteOff();

    // =========================================================================
    // WAVEFORM & AMPLITUDE CONTROL
    // =========================================================================
    
    void setWaveformType(int type);
    void setAmplitude(float amp);
    void setBaseFrequency(float freq);
    
    // =========================================================================
    // PITCH CONTROL
    // =========================================================================
    
    void setPitchOffset(float semis);      // Coarse pitch (-24 to +24 semitones)
    void setPitchModulation(float semis);  // Modulation pitch offset
    void setDetune(float hz);              // Fine detune in Hz
    void setFineTune(float cents);         // Fine tune in cents
    
    // =========================================================================
    // SUPERSAW CONTROL
    // =========================================================================
    
    void setSupersawDetune(float amount);  // Supersaw detune amount (0-1)
    void setSupersawMix(float mix);        // Supersaw voice mix (0-1)
    
    // =========================================================================
    // GLIDE (PORTAMENTO)
    // =========================================================================
    
    void setGlideEnabled(bool enabled);
    void setGlideTime(float ms);
    
    // =========================================================================
    // DC MODULATION SOURCES
    // =========================================================================
    
    void setFrequencyDcAmp(float amp);
    void setShapeDcAmp(float amp);
    
    // =========================================================================
    // ARBITRARY WAVEFORM SELECTION
    // =========================================================================
    
    void setArbBank(ArbBank b);
    void setArbTableIndex(uint16_t idx);
    ArbBank getArbBank() const { return _arbBank; }
    uint16_t getArbTableIndex() const { return _arbIndex; }

    // =========================================================================
    // FEEDBACK OSCILLATION (JP-8000 STYLE)
    // =========================================================================
    
    /**
     * @brief Enable/disable JP-8000 style feedback oscillation
     * @param enable true to route oscillator through resonant comb filter
     * 
     * When enabled, the oscillator output is routed through a short delay
     * line with feedback, creating a resonant comb filter effect similar
     * to the JP-8000's feedback oscillator feature.
     */
    void setFeedbackEnabled(bool enable);
    
    /**
     * @brief Set feedback amount (resonance intensity)
     * @param amount Feedback gain 0.0 (none) to 0.99 (max resonance)
     * 
     * Higher values create more pronounced resonance. Values above 0.99
     * can cause runaway oscillation, so they are clamped.
     */
    void setFeedbackAmount(float amount);
    
    /**
     * @brief Set feedback mix level (wet/dry blend)
     * @param mix Feedback comb output level 0.0 (silent) to 1.0 (full)
     * 
     * Controls how much of the resonant comb filter output is mixed
     * with the normal oscillator output. Allows blending dry + wet.
     */
    void setFeedbackMix(float mix);
    
    bool getFeedbackEnabled() const { return _feedbackEnabled; }
    float getFeedbackAmount() const { return _feedbackGain; }
    float getFeedbackMix() const { return _feedbackMixLevel; }

    // =========================================================================
    // PARAMETER GETTERS
    // =========================================================================
    
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

    // =========================================================================
    // AUDIO OUTPUTS
    // =========================================================================
    
    AudioStream& output();
    AudioMixer4& frequencyModMixer();
    AudioMixer4& shapeModMixer();

private:
    // =========================================================================
    // AUDIO COMPONENTS - MAIN OSCILLATOR PATH
    // =========================================================================
    AudioSynthWaveformDc _frequencyDc;
    AudioSynthWaveformDc _shapeDc;
    AudioMixer4 _frequencyModMixer;
    AudioMixer4 _shapeModMixer;
    AudioSynthWaveformModulated _mainOsc;
    AudioSynthSupersaw* _supersaw;  // Pointer: nullptr if disabled (OSC2)
    AudioMixer4 _outputMix;
    
    // Audio connections - main path
    AudioConnection* _patchfrequencyDc;
    AudioConnection* _patchshapeDc;
    AudioConnection* _patchfrequency;
    AudioConnection* _patchshape;
    AudioConnection* _patchMainOsc;
    AudioConnection* _patchSupersaw;  // Conditional: only if supersaw enabled

    // =========================================================================
    // FEEDBACK COMB NETWORK (JP-8000 SIMULATION)
    // =========================================================================
    
    // Feedback state
    bool _feedbackEnabled = false;
    float _feedbackGain = 0.6f;
    float _feedbackMixLevel = 0.9f;  // How much comb output to mix in
    
    // Delay configuration (AudioEffectDelay manages buffer internally)
    static constexpr float FEEDBACK_DELAY_MS = 5.0f;
    
    // Comb filter components
    AudioMixer4 _combMixer;          // Mixes exciter + feedback
    AudioEffectDelay _combDelay;     // Short resonant delay
    
    // Audio connections - feedback path
    AudioConnection* _patchMainToComb;      // Main osc → comb mixer
    AudioConnection* _patchSupersawToComb;  // Supersaw → comb mixer (may be nullptr)
    AudioConnection* _patchDelayToComb;     // Delay out → comb mixer (feedback)
    AudioConnection* _patchCombToDelay;     // Comb mixer → delay in
    AudioConnection* _patchDelayToOut;      // Delay out → output mixer

    // =========================================================================
    // OSCILLATOR STATE
    // =========================================================================
    
    bool _supersawEnabled;     // True if this osc can use supersaw (OSC1 only)
    bool _freqDirty = true;    // Frequency needs recalculation (CPU optimization)
    
    int _currentType = 1;
    float _baseFreq = 440.0f;
    float _pitchOffset = 0.0f;
    float _pitchModulation = 0.0f;
    float _detune = 0.0f;
    float _fineTune = 0.0f;
    float _lastVelocity = 1.0f;
    float _supersawDetune = 0.0f;
    float _supersawMix = 0.5f;
    float _lastFreq = -1.0f;
    
    // Glide
    bool _glideEnabled = false;
    float _glideTimeMs = 0.0f;
    float _glideRate = 0.0f;
    float _targetFreq = 0.0f;
    bool _glideActive = false;
    
    // DC modulation
    float _frequencyDcAmp = 0.0f;
    float _shapeDcAmp = 0.0f;
    
    // Arbitrary waveforms
    ArbBank  _arbBank  = ArbBank::BwBlended;
    uint16_t _arbIndex = 0;
    
    // =========================================================================
    // PRIVATE HELPERS
    // =========================================================================
    void _applyArbWave();  // Load arbitrary waveform table
};