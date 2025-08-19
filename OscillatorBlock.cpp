#include "OscillatorBlock.h"

OscillatorBlock::OscillatorBlock()
    : 
        _patchfrequencyDc(new AudioConnection(_frequencyDc, 0, _frequencyModMixer, 0)),
        _patchshapeDc(new AudioConnection(_shapeDc, 0, _shapeModMixer, 0)),
        _patchfrequency(new AudioConnection(_frequencyModMixer, 0, _mainOsc, 0)),
        _patchshape(new AudioConnection(_shapeModMixer, 0, _mainOsc, 1)),
        _patchMainOsc(new AudioConnection(_mainOsc, 0, _outputMix, 0)),
        _patchSupersaw(new AudioConnection(_supersaw, 0, _outputMix, 1)),
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

    _outputMix.gain(0, 1.0f);  // Main oscillator
    _outputMix.gain(1, 0.0f);  // Supersaw off by default
}


void OscillatorBlock::setWaveformType(int type) {
    _currentType = type;

    if (type == 9) {  // Supersaw
        _outputMix.gain(0, 0.0f);
        _outputMix.gain(1, 1.0f);
    } else {
        _mainOsc.begin(type);
        _outputMix.gain(0, 1.0f);
        _outputMix.gain(1, 0.0f);
    }
}
void OscillatorBlock::setAmplitude(float amp) {
    _mainOsc.amplitude(amp);
    _supersaw.setAmplitude(amp);
}
void OscillatorBlock::setFrequencyDcAmp(float amp){
    _frequencyDcAmp = amp;
    _frequencyDc.amplitude(amp);
}

void OscillatorBlock::setShapeDcAmp(float amp){
    _shapeDcAmp = amp;
    _shapeDc.amplitude(amp);
}

// void OscillatorBlock::noteOn(float freq, float velocity) {

//     _targetFreq = freq;
//     _glideActive = _glideEnabled && (_glideTimeMs > 0.0f);
//     setBaseFrequency(freq);
    
//     if (_currentType == 9){
//         _mainOsc.amplitude(0);
        
//         _supersaw.setAmplitude(velocity);
//         Serial.printf("Note ON ss: %.2f vel %.2f\n", freq, velocity);
//     }else{
//         _mainOsc.amplitude(velocity);
//         _supersaw.setAmplitude(0);
//         Serial.printf("Note ON osc: %.2f vel %.2f\n", freq, velocity);
//     }
//     _lastVelocity = velocity;
//        Serial.printf("_mainOsc.amplitude last velocity : %.2f \n", _lastVelocity);
// }
void OscillatorBlock::noteOn(float freq, float velocity) {
    _targetFreq = freq;
    float amp = velocity / 127.0f;

    if (_glideEnabled && _glideTimeMs > 0.0f) {
        _glideActive = true;
    } else {
        _baseFreq = _targetFreq;
        _glideActive = false;
    }
    // _mainOsc.frequency(_targetFreq);
    // _supersaw.setFrequency(_targetFreq);
    
    AudioNoInterrupts();
    if (_currentType == 9) {
        _mainOsc.amplitude(0);
        _supersaw.setAmplitude(amp);
    } else {
        _mainOsc.amplitude(amp);
        _supersaw.setAmplitude(0);
    }
    AudioInterrupts();

    _lastVelocity = velocity;
}



void OscillatorBlock::noteOff() {
    _mainOsc.amplitude(0.0f);
    _supersaw.setAmplitude(0.0f);
}

void OscillatorBlock::setBaseFrequency(float freq) {
    _baseFreq = freq;
    Serial.printf("setBaseFrequency : %.2f \n", _baseFreq);
    _mainOsc.frequency(freq);
    _supersaw.setFrequency(freq);
}

void OscillatorBlock::setPitchOffset(float semis) {
    _pitchOffset = semis;
}

void OscillatorBlock::setPitchModulation(float semis) {
    _pitchModulation = semis;
}

void OscillatorBlock::setDetune(float hz) {
    _detune = hz;
}

void OscillatorBlock::setFineTune(float cents) {
    _fineTune = cents;
}

void OscillatorBlock::setSupersawDetune(float amount) {
    _supersawDetune = amount;
    _supersaw.setDetune(amount);
}

void OscillatorBlock::setSupersawMix(float mix) {
    _supersawMix = mix;
    _supersaw.setMix(mix);
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


// void OscillatorBlock::update() {
//     if (_targetFreq <= 0.0f) return;

//     bool updateRequired = false;

//     // Glide logic
//     if (_glideActive) {
//         float delta = _targetFreq - _baseFreq;
//         if (fabsf(delta) < 0.01f) {
//             _baseFreq = _targetFreq;
//             _glideActive = false;
//         } else {
//             _baseFreq += delta * _glideRate;
//         }
//         updateRequired = true;
//     } 
//     // If no glide, but target freq changed externally
//     else if (fabsf(_baseFreq - _targetFreq) > 0.01f) {
//         _baseFreq = _targetFreq;
//         updateRequired = true;
//     }

//     // Detect other mod sources affecting pitch
//     float semitoneShift = _pitchOffset + _pitchModulation + (_fineTune / 100.0f);
//     float pitchAdjusted = _baseFreq * powf(2.0f, semitoneShift / 12.0f);
//     float finalFreq = pitchAdjusted + _detune;

//     static float lastFreq = -1.0f;

//     if (updateRequired || fabsf(finalFreq - lastFreq) > 0.01f) {
//         AudioNoInterrupts();
//         Serial.printf("updateRequired %d, lastFreq %.2f, finalFreq %.2f\n", updateRequired, lastFreq, finalFreq );
//         _mainOsc.frequency(finalFreq);
//         _supersaw.setFrequency(finalFreq);

//         AudioInterrupts();

//         lastFreq = finalFreq;
//     }
//         _pitchModulation = 0; 
// }
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
    // If these can be updated from ISR/UI, consider declaring them volatile and/or
    // copying under AudioNoInterrupts() before using.
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

        // Do NOT Serial.printf while interrupts are disabled
        AudioNoInterrupts();
        _mainOsc.frequency(finalFreq);
        _supersaw.setFrequency(finalFreq);
        AudioInterrupts();

        // Now it’s safe to log
        Serial.printf("updateRequired %d, lastFreq %.2f, finalFreq %.2f\n",
                      (int)updateRequired, _lastFreq, finalFreq);

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
    return _shapeModMixer;
}
    AudioMixer4& OscillatorBlock::shapeModMixer() {
    return _frequencyModMixer;
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

