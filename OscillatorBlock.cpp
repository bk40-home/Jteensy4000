#include "OscillatorBlock.h"
// Bring AKWF catalog into scope (bank definitions and lookup functions)
#include "AKWF_All.h"

// ============================================================================
// CONSTRUCTOR - Conditional Supersaw Creation
// ============================================================================
// enableSupersaw: true=OSC1 (supersaw capable), false=OSC2 (no supersaw)
// CPU savings: 50% fewer supersaw instances + no processing when inactive
OscillatorBlock::OscillatorBlock(bool enableSupersaw)
    : _supersawEnabled(enableSupersaw),
      _supersaw(nullptr),  // Initially null, conditionally created below
      _patchfrequencyDc(new AudioConnection(_frequencyDc, 0, _frequencyModMixer, 0)),
      _patchshapeDc(new AudioConnection(_shapeDc, 0, _shapeModMixer, 0)),
      _patchfrequency(new AudioConnection(_frequencyModMixer, 0, _mainOsc, 0)),
      _patchshape(new AudioConnection(_shapeModMixer, 0, _mainOsc, 1)),
      _patchMainOsc(new AudioConnection(_mainOsc, 0, _outputMix, 0)),
      _patchSupersaw(nullptr),  // Conditionally created below if enabled
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

    _outputMix.gain(0, 0.9f);  // Main oscillator
    _outputMix.gain(1, 0.0f);  // Supersaw (if enabled)
    
    // ========================================
    // CONDITIONAL SUPERSAW CREATION
    // Only create supersaw if enabled (OSC1 = true, OSC2 = false)
    // This reduces CPU by 50% by eliminating 4 unnecessary supersaw instances
    // ========================================
    if (_supersawEnabled) {
        _supersaw = new AudioSynthSupersaw();
        _patchSupersaw = new AudioConnection(*_supersaw, 0, _outputMix, 1);
        
        // Configure supersaw defaults
        _supersaw->setOversample(false);
        _supersaw->setMixCompensation(true);
        _supersaw->setCompensationMaxGain(1.5f);
        _supersaw->setBandLimited(false);
    }
    // else: _supersaw stays nullptr, no AudioConnection created, no CPU waste
}


// AKWF_All.h included above for arbitrary waveform access

// Load the current arbitrary waveform table into the main oscillator when
// using an ARBITRARY waveform.  Looks up the table based on the current
// bank and index via akwf_get(), and computes the maximum usable frequency
// from the table length.
void OscillatorBlock::_applyArbWave() {
    uint16_t len = 0;
    const int16_t* table = akwf_get(_arbBank, _arbIndex, len);
    if (table && len > 0) {
        const float maxFreq = AUDIO_SAMPLE_RATE_EXACT / (float)len;
        _mainOsc.arbitraryWaveform(table, maxFreq);
    }
}

// Set the current bank used for arbitrary waveforms.  If the oscillator
// waveform type is ARBITRARY, reapply the waveform with the new bank.
void OscillatorBlock::setArbBank(ArbBank b) {
    _arbBank = b;
    // Clamp index to new bank size
    uint16_t count = akwf_bankCount(b);
    if (count > 0 && _arbIndex >= count) {
        _arbIndex = count - 1;
    }
    if (_currentType == WAVEFORM_ARBITRARY) {
        _applyArbWave();
        _mainOsc.begin(WAVEFORM_ARBITRARY);
        _outputMix.gain(0, 0.7f);
        _outputMix.gain(1, 0.0f);
    }
}

// Set the current table index within the selected bank.  Indexes
// outside the range are clamped to the last available table.  If the
// waveform is ARBITRARY, reapply the waveform immediately.
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
        _outputMix.gain(0, 0.7f);
        _outputMix.gain(1, 0.0f);
    }
}

// --- existing constructor etc. ---

// ============================================================================
// setWaveformType - Null-Pointer Guard Added
// ============================================================================
void OscillatorBlock::setWaveformType(int type) {
    _currentType = type;
    _freqDirty = true;

    if (type == WAVEFORM_SUPERSAW) {
        // Supersaw path: check if available
        if (_supersawEnabled && _supersaw) {
            // Supersaw is available, use it
            _outputMix.gain(0, 0.0f);
            _outputMix.gain(1, 0.9f);
        } else {
            // Fallback to sawtooth if supersaw requested but not available (OSC2)
            _mainOsc.begin(WAVEFORM_SAWTOOTH);
            _outputMix.gain(0, 0.7f);
            _outputMix.gain(1, 0.0f);
        }
    } else if (type == WAVEFORM_ARBITRARY) {
        // Arbitrary waveform path: apply table based on current bank/index
        _applyArbWave();
        _mainOsc.begin(WAVEFORM_ARBITRARY);
        _outputMix.gain(0, 0.7f);
        _outputMix.gain(1, 0.0f);
    } else {
        // Standard Teensy waveform
        _mainOsc.begin((uint8_t)type);
        _outputMix.gain(0, 0.7f);
        _outputMix.gain(1, 0.0f);
    }
}


// ============================================================================
// setAmplitude - Null-Pointer Guard Added
// ============================================================================
void OscillatorBlock::setAmplitude(float amp) {
    _freqDirty = true;
    _mainOsc.amplitude(amp);
    
    // Only call supersaw if it exists
    if (_supersaw) {
        _supersaw->setAmplitude(amp);
    }
}

void OscillatorBlock::setFrequencyDcAmp(float amp){
    _frequencyDcAmp = amp;
    _frequencyDc.amplitude(amp);
}

void OscillatorBlock::setShapeDcAmp(float amp){
    _shapeDcAmp = amp;
    _shapeDc.amplitude(amp);
}

// ============================================================================
// noteOn - Null-Pointer Guard Added
// ============================================================================
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
        // Supersaw is active and available
        _mainOsc.amplitude(0);
        _supersaw->setAmplitude(amp);
    } else {
        // Main oscillator
        _mainOsc.amplitude(amp);
        if (_supersaw) {
            _supersaw->setAmplitude(0);
        }
    }
    AudioInterrupts();

    _lastVelocity = velocity;
}


// ============================================================================
// noteOff - Null-Pointer Guard Added
// ============================================================================
void OscillatorBlock::noteOff() {
    _mainOsc.amplitude(0.0f);
    
    // Only call supersaw if it exists
    if (_supersaw) {
        _supersaw->setAmplitude(0.0f);
    }
}

// ============================================================================
// setBaseFrequency - Null-Pointer Guard Added
// ============================================================================
void OscillatorBlock::setBaseFrequency(float freq) {
    _baseFreq = freq;
    Serial.printf("setBaseFrequency : %.2f \n", _baseFreq);
    _mainOsc.frequency(freq);
    
    // Only call supersaw if it exists
    if (_supersaw) {
        _supersaw->setFrequency(freq);
    }
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

// ============================================================================
// setSupersawDetune - Null-Pointer Guard Added
// ============================================================================
void OscillatorBlock::setSupersawDetune(float amount) {
    _supersawDetune = amount;
    
    // Only call supersaw if it exists
    if (_supersaw) {
        _supersaw->setDetune(amount);
    }
    _freqDirty = true;
}

// ============================================================================
// setSupersawMix - Null-Pointer Guard Added
// ============================================================================
void OscillatorBlock::setSupersawMix(float mix) {
    _supersawMix = mix;
    
    // Only call supersaw if it exists
    if (_supersaw) {
        _supersaw->setMix(mix);
    }
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

// ============================================================================
// update - Null-Pointer Guard Added
// ============================================================================
// Only recalculates frequency when _freqDirty flag is set
// This prevents wasteful recalculation on every audio block
void OscillatorBlock::update() {
    // Early out if we have no valid target
    if (_targetFreq <= 0.0f) return;

    bool updateRequired = false;

    // --- Glide logic (operates on _baseFreq) ---
    if (_glideActive) {
        float delta = _targetFreq - _baseFreq;
        if (fabsf(delta) < 0.01f) {
            _baseFreq = _targetFreq;
            _glideActive = false;
        } else {
            _baseFreq += delta * _glideRate;  // simple one-pole glide
        }
        updateRequired = true;
    } else if (fabsf(_baseFreq - _targetFreq) > 0.01f) {
        _baseFreq = _targetFreq;
        updateRequired = true;
    }

    // --- Snapshot modulation sources to avoid mid-calculation races ---
    const float pitchOffset   = _pitchOffset;        // semitones
    const float pitchMod      = _pitchModulation;    // semitones (latched)
    const float fine          = _fineTune / 100.0f;  // semitones
    const float detuneHz      = _detune;             // Hz additive

    // Optional: clamp to a sane musical range (prevents wild jumps)
    float semitoneShift = pitchOffset + pitchMod + fine;
    if (semitoneShift > 48.0f)  semitoneShift = 48.0f;
    if (semitoneShift < -48.0f) semitoneShift = -48.0f;

    // Convert semitone shift to frequency
    const float pitchAdjusted = _baseFreq * powf(2.0f, semitoneShift / 12.0f);
    const float finalFreq     = fmaxf(0.0f, pitchAdjusted + detuneHz);

    // Only touch the Audio objects if we actually need to change frequency
    if (updateRequired || fabsf(finalFreq - _lastFreq) > 0.01f) {

        AudioNoInterrupts();
        _mainOsc.frequency(finalFreq);
        
        // Only call supersaw if it exists
        if (_supersaw) {
            _supersaw->setFrequency(finalFreq);
        }
        AudioInterrupts();

        _lastFreq = finalFreq;
    }

    // IMPORTANT: don't auto-clear _pitchModulation here.
    // If you intended one-shot behavior, clear it in the setter AFTER update() consumes it.
    // _pitchModulation = 0;
}


AudioStream& OscillatorBlock::output() {
    return _outputMix;
}


    AudioMixer4& OscillatorBlock::frequencyModMixer() {
    return _frequencyModMixer ;
}
    AudioMixer4& OscillatorBlock::shapeModMixer() {
    return _shapeModMixer;
}



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