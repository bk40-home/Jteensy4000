#include "OscillatorBlock.h"
#include "AKWF_All.h"

// ============================================================================
// CONSTRUCTOR - Dual Signal Path (Normal + Feedback)
// ============================================================================
OscillatorBlock::OscillatorBlock(bool enableSupersaw)
    : _supersaw(nullptr),
      _supersawEnabled(enableSupersaw),
      _patchfrequencyDc(new AudioConnection(_frequencyDc, 0, _frequencyModMixer, 0)),
      _patchshapeDc(new AudioConnection(_shapeDc, 0, _shapeModMixer, 0)),
      _patchfrequency(new AudioConnection(_frequencyModMixer, 0, _mainOsc, 0)),
      _patchshape(new AudioConnection(_shapeModMixer, 0, _mainOsc, 1)),
      _patchMainOsc(new AudioConnection(_mainOsc, 0, _outputMix, 0)),
      _patchSupersaw(nullptr),
      _baseFreq(440.0f)
{
    _mainOsc.begin(_currentType);
    _mainOsc.amplitude(1.0f);
    _mainOsc.frequencyModulation(10);
    _mainOsc.phaseModulation(179);

    _frequencyDc.amplitude(0.0f);
    _shapeDc.amplitude(0.0f);

    _frequencyModMixer.gain(0, 1.0f);
    _frequencyModMixer.gain(1, 1.0f);
    _frequencyModMixer.gain(2, 1.0f);
    _frequencyModMixer.gain(3, 1.0f);

    _shapeModMixer.gain(0, 1.0f);
    _shapeModMixer.gain(1, 1.0f);
    _shapeModMixer.gain(2, 1.0f);
    _shapeModMixer.gain(3, 1.0f);

    // =========================================================================
    // OUTPUT MIXER - DUAL PATH ARCHITECTURE
    // =========================================================================
    // Channel 0: Main oscillator (stays active!)
    // Channel 1: Supersaw oscillator (if enabled)
    // Channel 2: Feedback comb output (added when feedback enabled)
    // Channel 3: Unused
    // KEY: Normal output (0/1) stays on when feedback is enabled!
    // =========================================================================
    
    _outputMix.gain(0, 0.9f);  // Normal output (default ON)
    _outputMix.gain(1, 0.0f);  // Supersaw (OFF by default)
    _outputMix.gain(2, 0.0f);  // Feedback comb (OFF by default)
    _outputMix.gain(3, 0.0f);  // Unused

    // =========================================================================
    // FEEDBACK COMB NETWORK SETUP
    // =========================================================================
    
    _combMixer.gain(0, 1.0f);  // Main osc input
    _combMixer.gain(1, 0.0f);  // Supersaw input
    _combMixer.gain(2, 0.0f);  // Feedback path (OFF until enabled)
    _combMixer.gain(3, 0.0f);  // Unused

    _combDelay.delay(0, FEEDBACK_DELAY_MS);

    _patchMainToComb     = new AudioConnection(_mainOsc, 0, _combMixer, 0);
    _patchDelayToComb    = new AudioConnection(_combDelay, 0, _combMixer, 2);
    _patchCombToDelay    = new AudioConnection(_combMixer, 0, _combDelay, 0);
    _patchDelayToOut     = new AudioConnection(_combDelay, 0, _outputMix, 2);
    
    _patchSupersawToComb = nullptr;
    
    if (_supersawEnabled) {
        _supersaw = new AudioSynthSupersaw();
        _patchSupersaw       = new AudioConnection(*_supersaw, 0, _outputMix, 1);
        _patchSupersawToComb = new AudioConnection(*_supersaw, 0, _combMixer, 1);
        
        _supersaw->setOversample(false);
        _supersaw->setMixCompensation(true);
        _supersaw->setCompensationMaxGain(1.5f);
        _supersaw->setBandLimited(false);
    }
}

// ============================================================================
// ARBITRARY WAVEFORM HELPERS
// ============================================================================

void OscillatorBlock::_applyArbWave() {
    uint16_t len = 0;
    const int16_t* table = akwf_get(_arbBank, _arbIndex, len);
    if (table && len > 0) {
        const float maxFreq = AUDIO_SAMPLE_RATE_EXACT / (float)len;
        _mainOsc.arbitraryWaveform(table, maxFreq);
    }
}

void OscillatorBlock::setArbBank(ArbBank b) {
    _arbBank = b;
    uint16_t count = akwf_bankCount(b);
    if (count > 0 && _arbIndex >= count) {
        _arbIndex = count - 1;
    }
    if (_currentType == WAVEFORM_ARBITRARY) {
        _applyArbWave();
        _mainOsc.begin(WAVEFORM_ARBITRARY);
    }
}

void OscillatorBlock::setArbTableIndex(uint16_t idx) {
    uint16_t count = akwf_bankCount(_arbBank);
    if (count == 0) {
        _arbIndex = 0;
        return;
    }
    if (idx >= count) idx = count - 1;
    _arbIndex = idx;
    if (_currentType == WAVEFORM_ARBITRARY) {
        _applyArbWave();
        _mainOsc.begin(WAVEFORM_ARBITRARY);
    }
}

// ============================================================================
// WAVEFORM TYPE - Clean Separation (No Feedback Coupling!)
// ============================================================================

void OscillatorBlock::setWaveformType(int type) {
    _currentType = type;
    _freqDirty = true;

    // ========================================================================
    // WAVEFORM ROUTING - Independent of feedback state
    // ========================================================================
    
    if (type == WAVEFORM_SUPERSAW) {
        if (_supersawEnabled && _supersaw) {
            // Route through supersaw
            _outputMix.gain(0, 0.0f);  // Mute main
            _outputMix.gain(1, 0.9f);  // Enable supersaw
            
            // Also route supersaw to comb input if feedback exists
            if (_feedbackEnabled) {
                _combMixer.gain(0, 0.0f);  // Mute main to comb
                _combMixer.gain(1, 1.0f);  // Supersaw to comb
            }
        } else {
            // Fallback to sawtooth
            _mainOsc.begin(WAVEFORM_SAWTOOTH);
            _outputMix.gain(0, 0.7f);
            _outputMix.gain(1, 0.0f);
            
            if (_feedbackEnabled) {
                _combMixer.gain(0, 1.0f);
                _combMixer.gain(1, 0.0f);
            }
        }
    } else if (type == WAVEFORM_ARBITRARY) {
        _applyArbWave();
        _mainOsc.begin(WAVEFORM_ARBITRARY);
        _outputMix.gain(0, 0.7f);
        _outputMix.gain(1, 0.0f);
        
        if (_feedbackEnabled) {
            _combMixer.gain(0, 1.0f);
            _combMixer.gain(1, 0.0f);
        }
    } else {
        _mainOsc.begin((uint8_t)type);
        _outputMix.gain(0, 0.7f);
        _outputMix.gain(1, 0.0f);
        
        if (_feedbackEnabled) {
            _combMixer.gain(0, 1.0f);
            _combMixer.gain(1, 0.0f);
        }
    }
    
    // Note: We don't touch _outputMix.gain(2) here - feedback state manages it
}

// ============================================================================
// AMPLITUDE & FREQUENCY CONTROL
// ============================================================================

void OscillatorBlock::setAmplitude(float amp) {
    _freqDirty = true;
    _mainOsc.amplitude(amp);
    if (_supersaw) _supersaw->setAmplitude(amp);
}

void OscillatorBlock::setFrequencyDcAmp(float amp){
    _frequencyDcAmp = amp;
    _frequencyDc.amplitude(amp);
}

void OscillatorBlock::setShapeDcAmp(float amp){
    _shapeDcAmp = amp;
    _shapeDc.amplitude(amp);
}

void OscillatorBlock::noteOn(float freq, float velocity) {
    _targetFreq = freq;
    float amp = velocity / 127.0f;

    if (_glideEnabled && _glideTimeMs > 0.0f) {
        _glideActive = true;
    } else {
        _baseFreq = _targetFreq;
        _glideActive = false;
    }
    
    AudioNoInterrupts();
    if (_currentType == WAVEFORM_SUPERSAW && _supersaw) {
        _mainOsc.amplitude(0);
        _supersaw->setAmplitude(amp);
    } else {
        _mainOsc.amplitude(amp);
        if (_supersaw) _supersaw->setAmplitude(0);
    }
    AudioInterrupts();

    _lastVelocity = velocity;
}

void OscillatorBlock::noteOff() {
    _mainOsc.amplitude(0.0f);
    if (_supersaw) _supersaw->setAmplitude(0.0f);
}

void OscillatorBlock::setBaseFrequency(float freq) {
    _baseFreq = freq;
    _mainOsc.frequency(freq);
    if (_supersaw) _supersaw->setFrequency(freq);
    _freqDirty = true;
}

void OscillatorBlock::setPitchOffset(float semis) {
    _pitchOffset = semis;
    _freqDirty = true;
}

void OscillatorBlock::setPitchModulation(float semis) {
    _pitchModulation = semis;
    _freqDirty = true;
}

void OscillatorBlock::setDetune(float hz) {
    _detune = hz;
    _freqDirty = true;
}

void OscillatorBlock::setFineTune(float cents) {
    _fineTune = cents;
    _freqDirty = true;
}

void OscillatorBlock::setSupersawDetune(float amount) {
    _supersawDetune = amount;
    if (_supersaw) _supersaw->setDetune(amount);
    _freqDirty = true;
}

void OscillatorBlock::setSupersawMix(float mix) {
    _supersawMix = mix;
    if (_supersaw) _supersaw->setMix(mix);
    _freqDirty = true;
}

void OscillatorBlock::setGlideEnabled(bool enabled) {
    _glideEnabled = enabled;
}

void OscillatorBlock::setGlideTime(float ms) {
    _glideTimeMs = ms;
    if (ms > 0.0f) {
        float samples = (ms / 1000.0f) * AUDIO_SAMPLE_RATE_EXACT;
        _glideRate = 1.0f / samples;
    } else {
        _glideRate = 0.0f;
    }
}

void OscillatorBlock::update() {
    if (_targetFreq <= 0.0f) return;

    bool updateRequired = false;

    if (_glideActive) {
        float delta = _targetFreq - _baseFreq;
        if (fabsf(delta) < 0.01f) {
            _baseFreq = _targetFreq;
            _glideActive = false;
        } else {
            _baseFreq += delta * _glideRate;
        }
        updateRequired = true;
    } else if (fabsf(_baseFreq - _targetFreq) > 0.01f) {
        _baseFreq = _targetFreq;
        updateRequired = true;
    }

    const float pitchOffset   = _pitchOffset;
    const float pitchMod      = _pitchModulation;
    const float fine          = _fineTune / 100.0f;
    const float detuneHz      = _detune;

    float semitoneShift = pitchOffset + pitchMod + fine;
    if (semitoneShift > 48.0f)  semitoneShift = 48.0f;
    if (semitoneShift < -48.0f) semitoneShift = -48.0f;

    const float pitchAdjusted = _baseFreq * powf(2.0f, semitoneShift / 12.0f);
    const float finalFreq     = fmaxf(0.0f, pitchAdjusted + detuneHz);

    if (updateRequired || fabsf(finalFreq - _lastFreq) > 0.01f) {
        AudioNoInterrupts();
        _mainOsc.frequency(finalFreq);
        if (_supersaw) _supersaw->setFrequency(finalFreq);
        AudioInterrupts();

        _lastFreq = finalFreq;
    }
}

// ============================================================================
// FEEDBACK OSCILLATION - FIXED SIGNAL PATH
// ============================================================================

void OscillatorBlock::setFeedbackEnabled(bool enable) {
    if (enable == _feedbackEnabled) return;
    
    _feedbackEnabled = enable;

    AudioNoInterrupts();
    
    if (enable) {
        // ====================================================================
        // ENABLE FEEDBACK - KEY FIX: Don't mute normal output!
        // ====================================================================
        
        // Determine which oscillator feeds the comb
        if (_currentType == WAVEFORM_SUPERSAW && _supersaw) {
            _combMixer.gain(0, 0.0f);  // Mute main to comb
            _combMixer.gain(1, 1.0f);  // Supersaw to comb
        } else {
            _combMixer.gain(0, 1.0f);  // Main to comb
            _combMixer.gain(1, 0.0f);  // Mute supersaw to comb
        }
        
        // Enable feedback loop
        _combMixer.gain(2, _feedbackGain);
        
        // Enable comb output (ADDS to normal output, doesn't replace!)
        _outputMix.gain(2, _feedbackMixLevel);
        
        // CRITICAL: Normal oscillator output stays active!
        // Channels 0/1 are NOT touched here - they remain as set by setWaveformType()
        
    } else {
        // ====================================================================
        // DISABLE FEEDBACK
        // ====================================================================
        
        // Disable feedback loop
        _combMixer.gain(2, 0.0f);
        
        // Disable comb output
        _outputMix.gain(2, 0.0f);
        
        // Normal oscillator stays active (no change to gain 0/1)
    }
    
    AudioInterrupts();
}

void OscillatorBlock::setFeedbackAmount(float amount) {
    // Clamp to safe range (prevents runaway oscillation)
    _feedbackGain = constrain(amount, 0.0f, 0.99f);

        
    _feedbackEnabled = _feedbackGain > 0.0f;

    AudioNoInterrupts();
    
    if (_feedbackEnabled) {
        // ====================================================================
        // ENABLE FEEDBACK - KEY FIX: Don't mute normal output!
        // ====================================================================
        
        // Determine which oscillator feeds the comb
        if (_currentType == WAVEFORM_SUPERSAW && _supersaw) {
            _combMixer.gain(0, 0.0f);  // Mute main to comb
            _combMixer.gain(1, 1.0f);  // Supersaw to comb
        } else {
            _combMixer.gain(0, 1.0f);  // Main to comb
            _combMixer.gain(1, 0.0f);  // Mute supersaw to comb
        }
        
        // Enable feedback loop
        _combMixer.gain(2, _feedbackGain);
        
        // Enable comb output (ADDS to normal output, doesn't replace!)
        _outputMix.gain(2, _feedbackMixLevel);
        
        // CRITICAL: Normal oscillator output stays active!
        // Channels 0/1 are NOT touched here - they remain as set by setWaveformType()
        
    } else {
        // ====================================================================
        // DISABLE FEEDBACK
        // ====================================================================
        
        // Disable feedback loop
        _combMixer.gain(2, 0.0f);
        
        // Disable comb output
        _outputMix.gain(2, 0.0f);
        
        // Normal oscillator stays active (no change to gain 0/1)
    }
    
}

void OscillatorBlock::setFeedbackMix(float mix) {
    // NEW METHOD: Control how much feedback comb is blended with dry signal
    _feedbackMixLevel = constrain(mix, 0.0f, 1.0f);
    
    if (_feedbackEnabled) {
        AudioNoInterrupts();
        _outputMix.gain(2, _feedbackMixLevel);
        AudioInterrupts();
    }
}

// ============================================================================
// AUDIO OUTPUTS & GETTERS
// ============================================================================

AudioStream& OscillatorBlock::output() { return _outputMix; }
AudioMixer4& OscillatorBlock::frequencyModMixer() { return _frequencyModMixer; }
AudioMixer4& OscillatorBlock::shapeModMixer() { return _shapeModMixer; }

int OscillatorBlock::getWaveform() const { return _currentType; }
float OscillatorBlock::getPitchOffset() const { return _pitchOffset; }
float OscillatorBlock::getDetune() const { return _detune; }
float OscillatorBlock::getFineTune() const { return _fineTune; }
float OscillatorBlock::getSupersawDetune() const { return _supersawDetune; }
float OscillatorBlock::getSupersawMix() const { return _supersawMix; }
bool OscillatorBlock::getGlideEnabled() const { return _glideEnabled; }
float OscillatorBlock::getGlideTime() const { return _glideTimeMs; }
float OscillatorBlock::getShapeDcAmp() const { return _shapeDcAmp; }
float OscillatorBlock::getFrequencyDcAmp() const { return _frequencyDcAmp; }