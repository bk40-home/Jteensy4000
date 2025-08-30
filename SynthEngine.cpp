#include "SynthEngine.h"
#include "Mapping.h"  // <-- NEW: shared CC mappings for forward/inverse UI use

SynthEngine::SynthEngine() {
    for (int i = 0; i < MAX_VOICES; ++i) {
        _activeNotes[i] = false;
        _voicePatch[i] = new AudioConnection(_voices[i].output(), 0, _voiceMixer, i);
        _voicePatchLFO1ShapeOsc1[i] = new AudioConnection(_lfo1.output(), 0, _voices[i].shapeModMixerOsc1(), 1);
        _voicePatchLFO1ShapeOsc2[i] = new AudioConnection(_lfo1.output(), 0, _voices[i].shapeModMixerOsc2(), 1);
        _voicePatchLFO1FrequencyOsc1[i] = new AudioConnection(_lfo1.output(), 0, _voices[i].frequencyModMixerOsc1(), 1);
        _voicePatchLFO1FrequencyOsc2[i] = new AudioConnection(_lfo1.output(), 0, _voices[i].frequencyModMixerOsc2(), 1);
        _voicePatchLFO1Filter[i] = new AudioConnection(_lfo1.output(), 0, _voices[i].filterModMixer(), 2);
        _voicePatchLFO2ShapeOsc1[i] = new AudioConnection(_lfo2.output(), 0, _voices[i].shapeModMixerOsc1(), 2);
        _voicePatchLFO2ShapeOsc2[i] = new AudioConnection(_lfo2.output(), 0, _voices[i].shapeModMixerOsc2(), 2);
        _voicePatchLFO2FrequencyOsc1[i] = new AudioConnection(_lfo2.output(), 0, _voices[i].frequencyModMixerOsc1(), 2);
        _voicePatchLFO2FrequencyOsc2[i] = new AudioConnection(_lfo2.output(), 0, _voices[i].frequencyModMixerOsc2(), 2);
        _voicePatchLFO2Filter[i] = new AudioConnection(_lfo2.output(), 0, _voices[i].filterModMixer(), 3);
        _voiceMixer.gain(i, 1.0f / MAX_VOICES);
    }
    for (int i = 0; i < 128; ++i) _noteToVoice[i] = 255;
    for (int i = 0; i < MAX_VOICES; ++i) _noteTimestamps[i] = 0;
    


    _ampModFixedDc.amplitude(1.0f);

    _ampModMixer.gain(0, 1.0f);
    _ampModMixer.gain(1, 0.0f);
    _ampModMixer.gain(2, 0.0f);
    _ampModMixer.gain(3, 0.0f);

    _patchAmpModFixedDcToAmpModMixer= new AudioConnection(_ampModFixedDc, 0, _ampModMixer, 0);
    _patchLFO1ToAmpModMixer= new AudioConnection(_lfo1.output(), 0, _ampModMixer, 1);
    _patchLFO2ToAmpModMixer= new AudioConnection(_lfo2.output(), 0, _ampModMixer, 2);

    _patchVoiceMixerToAmpMultiply= new AudioConnection(_voiceMixer, 0, _ampMultiply, 0);
    _patchAmpModMixerToAmpMultiply= new AudioConnection(_ampModMixer, 0, _ampMultiply, 1);
    _fxPatchIn = new AudioConnection(_ampMultiply, 0, _fxChain._ampIn, 0);
}

static inline float CCtoTime(uint8_t cc)
{
    // Keep this for compatibility, but it now defers to Mapping.h so
    // we have only one place to retune if needed.
    return JT4000Map::cc_to_time_ms(cc);  // ms
}

void SynthEngine::noteOn(byte note, float velocity) {
    float freq = 440.0f * powf(2.0f, (note - 69) / 12.0f);

    Serial.printf("noteOn se: note %d, freq %.2f, velocity %.1f\n", note, freq, velocity);

    // Already assigned?
    if (_noteToVoice[note] != 255) {
        int voiceIndex = _noteToVoice[note];
        _voices[voiceIndex].noteOn(freq, velocity);
        _noteTimestamps[voiceIndex] = _clock++;
        return;
    }

    // Free voice?
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (!_activeNotes[i]) {
            _voices[i].noteOn(freq, velocity);
            _activeNotes[i] = true;
            _noteToVoice[note] = i;
            _noteTimestamps[i] = _clock++;
            return;
        }
    }

    // Steal oldest
    int oldest = 0;
    for (int i = 1; i < MAX_VOICES; ++i) {
        if (_noteTimestamps[i] < _noteTimestamps[oldest]) {
            oldest = i;
        }
    }

    // Remove any note assigned to the voice
    for (int n = 0; n < 128; ++n) {
        if (_noteToVoice[n] == oldest) {
            _noteToVoice[n] = 255;
            break;
        }
    }

    _voices[oldest].noteOn(freq, velocity);
    _activeNotes[oldest] = true;
    _noteToVoice[note] = oldest;
    _noteTimestamps[oldest] = _clock++;
}


void SynthEngine::noteOff(byte note) {
    if (_noteToVoice[note] != 255) {
        int voiceIndex = _noteToVoice[note];
        _voices[voiceIndex].noteOff();
        _activeNotes[voiceIndex] = false;
        _noteToVoice[note] = 255;
    }
}

void SynthEngine::update() {
    for (int i = 0; i < MAX_VOICES; ++i) {
        _voices[i].update();
    }
}



void SynthEngine::setFilterCutoff(float value) {
    for (int i = 0; i < MAX_VOICES; ++i) {
        _voices[i].setFilterCutoff(value);
    }
}

void SynthEngine::setFilterResonance(float value) {
    for (int i = 0; i < MAX_VOICES; ++i) {
        _voices[i].setFilterResonance(value);
    }
}

void SynthEngine::setOsc1Waveform(int wave){
    for (int i = 0; i < MAX_VOICES; ++i) {
        _voices[i].setOsc1Waveform(wave);
    }
}

void SynthEngine::setOsc2Waveform(int wave){
    for (int i = 0; i < MAX_VOICES; ++i) {
        _voices[i].setOsc2Waveform(wave);
    }
}



//float GetAmpModFixedLevel() const{return _ampModFixedLevel}

void SynthEngine::setLFO1Frequency(float hz) { _lfo1.setFrequency(hz); }
void SynthEngine::setLFO2Frequency(float hz) {  _lfo2.setFrequency(hz); }
void SynthEngine::setLFO1Amount(float amt) { _lfo1.setAmplitude(amt); }
void SynthEngine::setLFO2Amount(float amt) { _lfo2.setAmplitude(amt); }
void SynthEngine::setLFO1Destination(LFODestination dest){
    _lfo1.setDestination(dest);
    for (int i = 0; i < MAX_VOICES; ++i) {
        _voices[i].frequencyModMixerOsc1().gain(1, 0.0f);
        _voices[i].frequencyModMixerOsc2().gain(1, 0.0f);
        _voices[i].shapeModMixerOsc1().gain(1, 0.0f);
        _voices[i].shapeModMixerOsc2().gain(1, 0.0f);
        _voices[i].filterModMixer().gain(2, 0.0f);
        _ampModMixer.gain(1, 0.0f);
    }
    if (dest == 1){
        for (int i = 0; i < MAX_VOICES; ++i) {
          _voices[i].frequencyModMixerOsc1().gain(1, 1.0f);
          _voices[i].frequencyModMixerOsc2().gain(1, 1.0f);
        }
    } else if (dest == 2) {
        for (int i = 0; i < MAX_VOICES; ++i) {
            _voices[i].filterModMixer().gain(2, 1.0f);
        }
    }  else if (dest == 3) {
        for (int i = 0; i < MAX_VOICES; ++i) {
            _voices[i].shapeModMixerOsc1().gain(1, 1.0f);
            _voices[i].shapeModMixerOsc2().gain(1, 1.0f);
        }
    }   else if (dest == 4) {
        for (int i = 0; i < MAX_VOICES; ++i) {
            _ampModMixer.gain(1, 1.0f);
        }
    }
}
void SynthEngine::setLFO2Destination(LFODestination dest){
    _lfo2.setDestination(dest);
    for (int i = 0; i < MAX_VOICES; ++i) {
        _voices[i].frequencyModMixerOsc1().gain(2, 0.0f);
        _voices[i].frequencyModMixerOsc2().gain(2, 0.0f);
        _voices[i].shapeModMixerOsc1().gain(2, 0.0f);
        _voices[i].shapeModMixerOsc2().gain(2, 0.0f);
        _voices[i].filterModMixer().gain(3, 0.0f);
        _ampModMixer.gain(2, 0.0f);
    }
    if (dest == 1){
        for (int i = 0; i < MAX_VOICES; ++i) {
          _voices[i].frequencyModMixerOsc1().gain(2, 1.0f);
          _voices[i].frequencyModMixerOsc2().gain(2, 1.0f);
        }
    } else if (dest == 2) {
        for (int i = 0; i < MAX_VOICES; ++i) {
            _voices[i].filterModMixer().gain(3, 1.0f);
        }
    }  else if (dest == 3) {
        for (int i = 0; i < MAX_VOICES; ++i) {
            _voices[i].shapeModMixerOsc1().gain(2, 1.0f);
            _voices[i].shapeModMixerOsc2().gain(2, 1.0f);
        }
    }   else if (dest == 4) {
        for (int i = 0; i < MAX_VOICES; ++i) {
            _ampModMixer.gain(2, 1.0f);
        }
    }
}

void SynthEngine::setOsc1PitchOffset(float semis) {
    Serial.printf("setOsc1PitchOffset  to %.3f semis\n", semis);
    for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setOsc1PitchOffset(semis);
}

void SynthEngine::setOsc2PitchOffset(float semis) {
    Serial.printf("setOsc2PitchOffset  to %.3f semis\n", semis);
    for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setOsc2PitchOffset(semis);
}

void SynthEngine::setOsc1Detune(float semis) {
    Serial.printf("setOsc1Detune  to %.3f semis\n", semis);
    for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setOsc1Detune(semis);
}

void SynthEngine::setOsc2Detune(float semis) {
    Serial.printf("setOsc2Detune  to %.3f semis\n", semis);
    for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setOsc2Detune(semis);
}

void SynthEngine::setOsc1FineTune(float cents) {
        Serial.printf("setOsc1FineTune  to %.3f cents\n", cents);
    for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setOsc1FineTune(cents);
}

void SynthEngine::setOsc2FineTune(float cents) {
     Serial.printf("setOsc2FineTune  to %.3f cents\n", cents);
    for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setOsc2FineTune(cents);
}

void SynthEngine::setOscMix(float osc1Level, float osc2Level) {
    Serial.printf("setOscMix  osc1Level to %.3f and osc2Level to %.3f\n", osc1Level, osc2Level);
    for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setOscMix(osc1Level, osc2Level);
}

void SynthEngine::setOsc1Mix(float oscLevel) {
    Serial.printf("setOsc1Mix to %.3f\n", oscLevel);
    for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setOsc1Mix(oscLevel);
}

void SynthEngine::setOsc2Mix(float oscLevel) {
    Serial.printf("setOsc2Mix to %.3f\n", oscLevel);
    for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setOsc2Mix(oscLevel);
}

void SynthEngine::setOsc1FrequencyDcAmp(float amp){
    Serial.printf("setOsc1FrequencyDcAmp to %.3f\n", amp);
    for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setOsc1FrequencyDcAmp(amp);
}
void SynthEngine::setOsc2FrequencyDcAmp(float amp){
    Serial.printf("setOsc2FrequencyDcAmp to %.3f\n", amp);
    for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setOsc2FrequencyDcAmp(amp);
}
void SynthEngine::setOsc1ShapeDcAmp(float amp){
    Serial.printf("setOsc1ShapeDcAmp to %.3f\n", amp);
    for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setOsc1ShapeDcAmp(amp);
}
void SynthEngine::setOsc2ShapeDcAmp(float amp){
    Serial.printf("setOsc2ShapeDcAmp to %.3f\n", amp);
    for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setOsc2ShapeDcAmp(amp);
}

void SynthEngine::setRing1Mix(float level){
    Serial.printf("setRing1Mix to %.3f\n", level);
    for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setRing1Mix(level);
}
void SynthEngine::setRing2Mix(float level){
    Serial.printf("setRing2Mix to %.3f\n", level);
    for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setRing2Mix(level);
}


void SynthEngine::handleControlChange(byte channel, byte control, byte value) {
    float normValue = value / 127.0f;

    switch (control) {
        // --- OSCILLATOR CONTROLS ---
        case 21: {
            int safetype = constrain(value, 0, 9);
            Serial.printf("[CC %d] Set Oscillator 1 waveform to(%d)\n", control, safetype);
            setOsc1Waveform(safetype);
            break;
        }
        case 22: {
            int safetype = constrain(value, 0, 9);
            Serial.printf("[CC %d] Set Oscillator 2 waveform to(%d)\n", control, safetype);
            setOsc2Waveform(safetype);
            break;
        }
        
        // ---------------- LFO1 (mod wheel was case 1) ----------------
        case 1: {
            // If you want MOD WHEEL to drive LFO1 freq, use the shared curve:
            // (Or keep your special-case, but then also add its inverse to Mapping.h.)
            float lfoHz = cc_to_lfo_hz(value);                 // Mapping.h
            Serial.printf("[CC %d] LFO1 freq = %.3f Hz\n", control, lfoHz);
            setLFO1Frequency(lfoHz);
            break;
        }

        // ---------------- FILTER CUTOFF (20..20k, log + optional taper) ----
        case 23: {
            float cutoffHz = cc_to_cutoff_hz(value);           // Mapping.h (taper respected)
            // Keep engine-side safe clamp identical to Mapping.h constants
            cutoffHz = fminf(fmaxf(cutoffHz, CUTOFF_MIN_HZ), CUTOFF_MAX_HZ);
            Serial.printf("[CC %d] Filter cutoff = %.1f Hz\n", control, cutoffHz);
            setFilterCutoff(cutoffHz);
            break;
        }

        // ---------------- FILTER RESONANCE ------------------------------
        case 24: {
        float k = JT4000Map::cc_to_res_k(value); // value 0..127 → k 0..20
            setFilterResonance(k);           // or setResonance(k) in your block
        break;
        }


        // --- AMP ENVELOPE ---
        case 25: {
            // BUGFIX: previously we computed 'attack' (ms) then called setAmpAttack(CCtoTime(attack))
            // which converted ms as-if it were a CC again. Use the ms value directly.
            float attack = CCtoTime(value); // ms
            Serial.printf("[CC %d] Set Amp Attack to %.1f ms\n", control, attack);
            for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setAmpAttack(attack); // <-- FIXED
            break;
        }
        case 26: {
            float decay = CCtoTime(value);
            Serial.printf("[CC %d] Set Amp Decay to %.1f ms\n", control, decay);
            for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setAmpDecay(decay);
            break;
        }
        case 27: {
            float sustain = normValue;
            Serial.printf("[CC %d] Set Amp Sustain to %.1f\n", control, sustain);
            for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setAmpSustain(sustain);
            break;
        }
        case 28: {
            float release = CCtoTime(value);
            Serial.printf("[CC %d] Set Amp Release to %.1f ms\n", control, release);
            for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setAmpRelease(release);
            break;
        }
        // --- FILTER ENVELOPE ---
        case 29: {
            float attack = CCtoTime(value);
            Serial.printf("[CC %d] Set Filter Env Attack to %.3f ms\n", control, attack);
            for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setFilterAttack(attack);
            break;
        }
        case 30: {
            float decay = CCtoTime(value);
            Serial.printf("[CC %d] Set Filter Env Decay to %.3f ms\n", control, decay);
            for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setFilterDecay(decay);
            break;
        }
        case 31: {
            float sustain = normValue;
            Serial.printf("[CC %d] Set Filter Env Sustain to %.3f\n", control, sustain);
            for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setFilterSustain(sustain);
            break;
        }
        case 32: {
            float release = CCtoTime(value);
            Serial.printf("[CC %d] Set Filter Env Release to %.3f ms\n", control, release);
            for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setFilterRelease(release);
            break;
        }
            case 41: {  // OSC1 Pitch Offset in fixed steps
                float semis;
                if (value <= 25) semis = -24.0f;
                else if (value <= 51) semis = -12.0f;
                else if (value <= 76) semis = 0.0f;
                else if (value <= 101) semis = 12.0f;
                else semis = 24.0f;

                setOsc1PitchOffset(semis);
                Serial.printf("[CC %d] Osc1 pitch offset = %.1f semitones\n", control, semis);
                break;
            }

            case 42: {  // OSC2 Pitch Offset
                float semis;
                if (value <= 25) semis = -24.0f;
                else if (value <= 51) semis = -12.0f;
                else if (value <= 76) semis = 0.0f;
                else if (value <= 101) semis = 12.0f;
                else semis = 24.0f;

                setOsc2PitchOffset(semis);
                Serial.printf("[CC %d] Osc2 pitch offset = %.1f semitones\n", control, semis);
                break;
            }

        case 43: setOsc1Detune(normValue * 2.0f - 1.0f); break;
        case 44: setOsc2Detune(normValue * 2.0f - 1.0f); break;
        case 45: setOsc1FineTune(normValue * 200.0f - 100.0f); break;
        case 46: setOsc2FineTune(normValue * 200.0f - 100.0f); break;
        case 47: setOscMix(1.0f - normValue, normValue); break;
        // --- FILTER ENVELOPE AMOUNT ---
        case 48: {
            float amount = normValue * 2.0f - 1.0f; // -1 to +1
            Serial.printf("[CC %d] Set Filter Env Amount to %.2f\n", control, amount);
            setFilterEnvAmount(amount);
            break;
        }
        // case 49: {
        //     float amount = normValue * 2.0f - 1.0f;
        //     setFilterLfoAmount(amount);
        //     Serial.printf("[CC %d] Set Filter LFO Amount to %.2f\n", control, amount);
        //     break;
        // }
        case 50: {
            float amount = normValue * 2.0f - 1.0f;
            setFilterKeyTrackAmount(amount);
            Serial.printf("[CC %d] Set Filter Key Track Amount to %.2f\n", control, amount);
            break;
        }

        // --- LFO2 CONFIG ---
        case 51: {
            float freq = cc_to_lfo_hz(value);                  // Mapping.h
            Serial.printf("[CC %d] LFO2 freq = %.3f Hz\n", control, freq);
            setLFO2Frequency(freq);
            break;
        }
        case 52: {
            float depth = normValue;
            Serial.printf("[CC %d] Set LFO2 Depth to %.2f\n", control, depth);
            setLFO2Amount(depth);
            break;
        }
        case 53: {
            LFODestination dest = static_cast<LFODestination>(value % NUM_LFO_DESTS);
            Serial.printf("[CC %d] Set LFO2 Destination to %d\n", control, dest);
            setLFO2Destination(dest);
            break;
        }
        // --- LFO1 CONFIG ---
        case 54: {
            float freq = cc_to_lfo_hz(value);                  // Mapping.h
            Serial.printf("[CC %d] LFO1 freq = %.3f Hz\n", control, freq);
            setLFO1Frequency(freq);
            break;
        }
        case 55: {
            float depth = normValue;
            Serial.printf("[CC %d] Set LFO1 Depth to %.2f\n", control, depth);
            setLFO1Amount(depth);
            break;
        }
        case 56: {
            LFODestination dest = static_cast<LFODestination>(value % NUM_LFO_DESTS);
            Serial.printf("[CC %d] Set LFO1 Destination to %d\n", control, dest);
            setLFO1Destination(dest);
            break;
        }
        case 57: {break;}
        //Sub osc and noise
        case 58: {
            float level = normValue;
            Serial.printf("[CC %d] Sub Mix = %.2f\n", control, level);
            for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setSubMix(level);
            break;
        }
        case 59: {
            float level = normValue;
            Serial.printf("[CC %d] Noise Mix = %.2f\n", control, level);
            for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setNoiseMix(level);
            break;
        }
        case 60: {
            float level = normValue;
            Serial.printf("[CC %d] Osc 1 level = %.2f\n", control, level);
            for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setOsc1Mix(level);
            break;
        }
        case 61: {
            float level = normValue;
            Serial.printf("[CC %d] Osc 2 level = %.2f\n", control, level);
            for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setOsc2Mix(level);
            break;
        }

        case 62: {
            int safetype = constrain(value, 0, 8);
            Serial.printf("[CC %d] setLfo1Waveform  = %d\n", control, safetype);
            _lfo1.setWaveformType(safetype);
            break;
        }
        case 63: {
            int safetype = constrain(value, 0, 8);
            Serial.printf("[CC %d] setLfo1Waveform  = %d\n", control, safetype);
            _lfo2.setWaveformType(safetype);
            break;
        }
        // --- GLOBAL FX CHAIN CONTROLS ---
        case 70: {
            float val = normValue;
            Serial.printf("[CC %d] Set Reverb Room Size = %.2f\n", control, val);
            setFXReverbRoomSize(val);
            break;
        }
        case 71: {
            float val = normValue;
            Serial.printf("[CC %d] Set Reverb Damping = %.2f\n", control, val);
            setFXReverbDamping(val);
            break;
        }
        case 72: {
            float baseMs = normValue * 1000.0f;  // 0–1000 ms
            Serial.printf("[CC %d] Set Delay Base Tap Time = %.1f ms\n", control, baseMs);
            setFXDelayBaseTime(baseMs);
            break;
        }
        case 73: {

            float feedback = normValue;
            Serial.printf("[CC %d] Set Delay feedback = %.2f\n", control, feedback);
            setFXDelayFeedback(feedback);
            break;
        }
        case 74: {
            float level = normValue;
            Serial.printf("[CC %d] Set Dry Mix = %.2f\n", control, level);
            setFXDryMix(level);
            break;
        }
        case 75: {
            float level = normValue;
            Serial.printf("[CC %d] Set Reverb Mix = %.2f\n", control, level);
            setFXReverbMix(level);
            break;
        }
        case 76: {
            float level = normValue;
            Serial.printf("[CC %d] Set Delay Mix = %.2f\n", control, level);
            setFXDelayMix(level);
            break;
        }
        case 77: {  // Supersaw Detune for osc1
            float norm = value / 127.0f;
            setSupersawDetune(0, norm);
            break;
        }
        case 78: {  // Supersaw Mix for osc1
            float norm = value / 127.0f;
            setSupersawMix(0, norm);
            break;
        }
        case 79: {  // Supersaw Detune for osc2
            float norm = value / 127.0f;
            setSupersawDetune(1, norm);
            break;
        }
        case 80: {  // Supersaw Mix for osc2
            float norm = value / 127.0f;
            setSupersawMix(1, norm);
            break;
        }

        case 81:  // Glide Enable
            _glideEnabled = (value >= 1);
            for (int i = 0; i < MAX_VOICES; ++i){
                _voices[i].setGlideEnabled(_glideEnabled);
            }
            Serial.printf("[CC %d] Glide Enabled = %d\n", control, _glideEnabled);
            break;

        // ---------------- Glide time (optional: reuse time curve) ----------
        case 82: {
            float ms = CCtoTime(value);                        // same 0..127 → ms curve
            _glideTimeMs = ms;
            for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setGlideTime(ms);
            Serial.printf("[CC %d] Glide Time = %.1f ms\n", control, ms);
            break;
        }

        // spare
        // case 83: {
        //     float drive = normValue * 4.0f;
        //     setFilterDrive(drive);
        //     Serial.printf("[CC %d] Set Filter Drive to %.2f\n", control, drive);
        //     break;
        // }
        case 84: {
            float oct = normValue * 10.0f;
            setFilterOctaveControl(oct);
            Serial.printf("[CC %d] Set Filter Octave Control to %.2f octaves\n", control, oct);
            break;
        }
        
        // spare
        // case 85: {
        //     float pbg = normValue * 0.5f;
        //     setFilterPassbandGain(pbg);
        //     Serial.printf("[CC %d] Set Filter Passband Gain to %.2f\n", control, pbg);
        //     break;
        // }
		case 86: {  
            float value = normValue;
            Serial.printf("[CC %d] Set Osc1 frequencyDc = %.2f\n", control, value);
            setOsc1FrequencyDcAmp(value);
            break;
        }
        case 87: {  
            float value = normValue;
            Serial.printf("[CC %d] Set Osc1 shapeDc = %.2f\n", control, value);
            setOsc1ShapeDcAmp(value);
            break;
        }
        case 88: {  
            float value = normValue;
            Serial.printf("[CC %d] Set Osc2 frequencyDc = %.2f\n", control, value);
            setOsc2FrequencyDcAmp(value);
            break;
        }
        case 89: {  
            float value = normValue;
            Serial.printf("[CC %d] Set Osc2 shapeDc = %.2f\n", control, value);
            setOsc2ShapeDcAmp(value);
            break;
        }
        case 90: {  
            float value = normValue;
            _ampModFixedLevel = value;
            Serial.printf("[CC %d] Set AmpModFixedLevel  = %.2f\n", control, value);
            _ampModFixedDc.amplitude(value);

            break;
        }

        case 91: {  
            float value = normValue;
            Serial.printf("[CC %d] Set setRing1Mix  = %.2f\n", control, value);
            setRing1Mix(value);
            break;
        }
        case 92: {  
            float value = normValue;
            Serial.printf("[CC %d] Set setRing2Mix = %.2f\n", control, value);
            setRing2Mix(value);
            break;
        }        

        default:
                Serial.printf("Unmapped CC: %d = %d\n", control, value);
                break;
        }
}

void SynthEngine::setFilterKeyTrackAmount(float amt) {
    for (int i = 0; i < MAX_VOICES; ++i)
        _voices[i].setFilterKeyTrackAmount(amt);
}

// void SynthEngine::setFilterDrive(float drive) {
//     for (int i = 0; i < MAX_VOICES; ++i)
//         _voices[i].setFilterDrive(drive);
// }

void SynthEngine::setFilterOctaveControl(float octaves) {
    for (int i = 0; i < MAX_VOICES; ++i)
        _voices[i].setFilterOctaveControl(octaves);
}

// void SynthEngine::setFilterPassbandGain(float gain) {
//     for (int i = 0; i < MAX_VOICES; ++i)
//         _voices[i].setFilterPassbandGain(gain);
// }

void SynthEngine::setFilterEnvAmount(float amt) {
    for (int i = 0; i < MAX_VOICES; ++i)
        _voices[i].setFilterEnvAmount(amt);
}


void SynthEngine::setSupersawDetune(uint8_t oscIndex, float amount) {
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (oscIndex == 0)
            _voices[i].setOsc1SupersawDetune(amount);
        else
            _voices[i].setOsc2SupersawDetune(amount);
    }
}

void SynthEngine::setSupersawMix(uint8_t oscIndex, float amount) {
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (oscIndex == 0)
            _voices[i].setOsc1SupersawMix(amount);
        else
            _voices[i].setOsc2SupersawMix(amount);
    }
}

int SynthEngine::getOsc1Waveform() const { return _voices[0].getOsc1Waveform(); }
int SynthEngine::getOsc2Waveform() const { return _voices[0].getOsc2Waveform(); }
float SynthEngine::getOsc1PitchOffset() const { return _voices[0].getOsc1PitchOffset(); }
float SynthEngine::getOsc2PitchOffset() const { return _voices[0].getOsc2PitchOffset(); }
float SynthEngine::getOsc1Detune() const { return _voices[0].getOsc1Detune(); }
float SynthEngine::getOsc2Detune() const { return _voices[0].getOsc2Detune(); }
float SynthEngine::getOsc1FineTune() const { return _voices[0].getOsc1FineTune(); }
float SynthEngine::getOsc2FineTune() const { return _voices[0].getOsc2FineTune(); }
float SynthEngine::getOscMix1() const { return _voices[0].getOscMix1(); }
float SynthEngine::getOscMix2() const { return _voices[0].getOscMix2(); }
float SynthEngine::getNoiseMix() const { return _voices[0].getNoiseMix(); }
float SynthEngine::getSubMix() const { return _voices[0].getSubMix(); }
float SynthEngine::getSupersawDetune(uint8_t oscIndex) const {
    return (oscIndex == 0)
        ? _voices[0].getOsc1SupersawDetune()
        : _voices[0].getOsc2SupersawDetune();
}

float SynthEngine::getSupersawMix(uint8_t oscIndex) const {
    return (oscIndex == 0)
        ? _voices[0].getOsc1SupersawMix()
        : _voices[0].getOsc2SupersawMix();
}
float SynthEngine::getOsc1FrequencyDc() const { return _voices[0].getOsc1FrequencyDc(); }
float SynthEngine::getOsc2FrequencyDc() const { return _voices[0].getOsc2FrequencyDc(); }
float SynthEngine::getOsc1ShapeDc() const { return _voices[0].getOsc1ShapeDc(); }
float SynthEngine::getOsc2ShapeDc() const { return _voices[0].getOsc2ShapeDc(); }
float SynthEngine::getRing1Mix() const { return _voices[0].getRing1Mix(); }
float SynthEngine::getRing2Mix() const { return _voices[0].getRing2Mix(); }

float SynthEngine::getFilterCutoff() const { return _voices[0].getFilterCutoff(); }
float SynthEngine::getFilterResonance() const { return _voices[0].getFilterResonance(); }
//float SynthEngine::getFilterDrive() const{ return _voices[0].getFilterDrive(); }

float SynthEngine::getFilterKeyTrackAmount() const{ return _voices[0].getFilterKeyTrackAmount(); }
float SynthEngine::getFilterOctaveControl() const{ return _voices[0].getFilterOctaveControl(); }
//float SynthEngine::getFilterPassbandGain() const{ return _voices[0].getFilterPassbandGain(); }




float SynthEngine::getAmpAttack() const { return _voices[0].getAmpAttack(); }
float SynthEngine::getAmpDecay() const { return _voices[0].getAmpDecay(); }
float SynthEngine::getAmpSustain() const { return _voices[0].getAmpSustain(); }
float SynthEngine::getAmpRelease() const { return _voices[0].getAmpRelease(); }
float SynthEngine::getFilterEnvAttack() const { return _voices[0].getFilterEnvAttack(); }
float SynthEngine::getFilterEnvDecay() const { return _voices[0].getFilterEnvDecay(); }
float SynthEngine::getFilterEnvSustain() const { return _voices[0].getFilterEnvSustain(); }
float SynthEngine::getFilterEnvRelease() const { return _voices[0].getFilterEnvRelease(); }
float SynthEngine::getFilterEnvAmount() const { return _voices[0].getFilterEnvAmount(); }

float SynthEngine::getLFO1Frequency() const {return _lfo1.getFrequency();}
float SynthEngine::getLFO2Frequency() const {return _lfo2.getFrequency();}
float SynthEngine::getLFO1Amount() const { return _lfo1.getAmplitude(); }
float SynthEngine::getLFO2Amount() const { return _lfo2.getAmplitude(); }


// LFO destination/waveform (avoid recursion)
LFODestination SynthEngine::getLFO1Destination() const { return _lfo1.getDestination(); }
LFODestination SynthEngine::getLFO2Destination() const { return _lfo2.getDestination(); }
int  SynthEngine::getLFO1Waveform() const { return _lfo1.getWaveform(); }
int  SynthEngine::getLFO2Waveform() const { return _lfo2.getWaveform(); }

// Glide and AmpMod DC level (for UI reflection)
bool  SynthEngine::getGlideEnabled()   const { return _glideEnabled; }
float SynthEngine::getGlideTimeMs()    const { return _glideTimeMs; }
float SynthEngine::getAmpModFixedLevel() const { return _ampModFixedLevel; }


void SynthEngine::setFXReverbRoomSize(float size) {
    _fxChain.setReverbRoomSize(size);
}
void SynthEngine::setFXReverbDamping(float damp) {
    _fxChain.setReverbDamping(damp);
}
void SynthEngine::setFXDelayBaseTime(float baseMs){
    _fxChain.setBaseDelayTime(baseMs);
}
void SynthEngine::setFXDryMix(float level) {
    _fxChain.setDryMix(level, level);
}
void SynthEngine::setFXReverbMix(float level) {
    _fxChain.setReverbMix(level, level);
}
void SynthEngine::setFXDelayMix(float level) {
    _fxChain.setDelayMix(level, level);
}
void SynthEngine::setFXDelayFeedback(float feedback) {
    _fxChain.setDelayFeedback(feedback);
}


