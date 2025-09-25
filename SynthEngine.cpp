// SynthEngine.cpp
#include "SynthEngine.h"
#include "Mapping.h"
#include "CCDefs.h"
#include "Waveforms.h"   // ensure waveformFromCC + names are available

using namespace CC;

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

    _ampModFixedDc.amplitude(_ampModFixedLevel);
    _ampModMixer.gain(0, 1.0f); // fixed DC
    _ampModMixer.gain(1, 0.0f); // LFO1
    _ampModMixer.gain(2, 0.0f); // LFO2
    _ampModMixer.gain(3, 0.0f);

    _patchAmpModFixedDcToAmpModMixer = new AudioConnection(_ampModFixedDc, 0, _ampModMixer, 0);
    _patchLFO1ToAmpModMixer          = new AudioConnection(_lfo1.output(), 0, _ampModMixer, 1);
    _patchLFO2ToAmpModMixer          = new AudioConnection(_lfo2.output(), 0, _ampModMixer, 2);

    _patchVoiceMixerToAmpMultiply    = new AudioConnection(_voiceMixer, 0, _ampMultiply, 0);
    _patchAmpModMixerToAmpMultiply   = new AudioConnection(_ampModMixer, 0, _ampMultiply, 1);
    _fxPatchIn                       = new AudioConnection(_ampMultiply, 0, _fxChain._ampIn, 0);
}

static inline float CCtoTime(uint8_t cc) { return JT4000Map::cc_to_time_ms(cc); }

// Safe CC-name lookup for logs (avoids nullptr)
static inline const char* ccname(uint8_t cc) {
  const char* n = CC::name(cc);
  return n ? n : "?";
}

void SynthEngine::setNotifier(NotifyFn fn) { _notify = fn; }


void SynthEngine::noteOn(byte note, float velocity) {
    float freq = 440.0f * powf(2.0f, (note - 69) / 12.0f);
    _lastNoteFreq = freq;

    if (_noteToVoice[note] != 255) {
        int v = _noteToVoice[note];
        _voices[v].noteOn(freq, velocity);
        _noteTimestamps[v] = _clock++;
        return;
    }
    // free voice
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (!_activeNotes[i]) {
            _voices[i].noteOn(freq, velocity);
            _activeNotes[i] = true;
            _noteToVoice[note] = i;
            _noteTimestamps[i] = _clock++;
            return;
        }
    }
    // steal oldest
    int oldest = 0;
    for (int i = 1; i < MAX_VOICES; ++i)
        if (_noteTimestamps[i] < _noteTimestamps[oldest]) oldest = i;

    for (int n = 0; n < 128; ++n)
        if (_noteToVoice[n] == oldest) { _noteToVoice[n] = 255; break; }

    _voices[oldest].noteOn(freq, velocity);
    _activeNotes[oldest] = true;
    _noteToVoice[note] = oldest;
    _noteTimestamps[oldest] = _clock++;
}

void SynthEngine::noteOff(byte note) {
    if (_noteToVoice[note] != 255) {
        int v = _noteToVoice[note];
        _voices[v].noteOff();
        _activeNotes[v] = false;
        _noteToVoice[note] = 255;
    }
}

void SynthEngine::update() {
    for (int i = 0; i < MAX_VOICES; ++i) _voices[i].update();
}

// ---- Filter / Env ----
void SynthEngine::setFilterCutoff(float value) {
    _filterCutoffHz = value;
    for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setFilterCutoff(value);
}
void SynthEngine::setFilterResonance(float value) {
    _filterResonance = value;
    for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setFilterResonance(value);
}
void SynthEngine::setFilterEnvAmount(float amt) {
    _filterEnvAmount = amt;
    for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setFilterEnvAmount(amt);
}
void SynthEngine::setFilterKeyTrackAmount(float amt) {
    _filterKeyTrack = amt;
    for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setFilterKeyTrackAmount(amt);
}
void SynthEngine::setFilterOctaveControl(float octaves) {
    _filterOctaves = octaves;
    for (int i = 0; i < MAX_VOICES; ++i) _voices[i].setFilterOctaveControl(octaves);
}

float SynthEngine::getFilterCutoff() const         { return _filterCutoffHz; }
float SynthEngine::getFilterResonance() const      { return _filterResonance; }
float SynthEngine::getFilterEnvAmount() const      { return _filterEnvAmount; }
float SynthEngine::getFilterKeyTrackAmount() const { return _filterKeyTrack; }
float SynthEngine::getFilterOctaveControl() const  { return _filterOctaves; }

// ---- Envelopes (read-through to voices; uses voice 0 as representative) ----
float SynthEngine::getAmpAttack()  const { return MAX_VOICES ? _voices[0].getAmpAttack()  : 0.0f; }
float SynthEngine::getAmpDecay()   const { return MAX_VOICES ? _voices[0].getAmpDecay()   : 0.0f; }
float SynthEngine::getAmpSustain() const { return MAX_VOICES ? _voices[0].getAmpSustain() : 0.0f; }
float SynthEngine::getAmpRelease() const { return MAX_VOICES ? _voices[0].getAmpRelease() : 0.0f; }

float SynthEngine::getFilterEnvAttack()  const { return MAX_VOICES ? _voices[0].getFilterEnvAttack()  : 0.0f; }
float SynthEngine::getFilterEnvDecay()   const { return MAX_VOICES ? _voices[0].getFilterEnvDecay()   : 0.0f; }
float SynthEngine::getFilterEnvSustain() const { return MAX_VOICES ? _voices[0].getFilterEnvSustain() : 0.0f; }
float SynthEngine::getFilterEnvRelease() const { return MAX_VOICES ? _voices[0].getFilterEnvRelease() : 0.0f; }

// ---- Oscillators / mixes ----
void SynthEngine::setOscWaveforms(int wave1, int wave2) { setOsc1Waveform(wave1); setOsc2Waveform(wave2); }
void SynthEngine::setOsc1Waveform(int wave) { _osc1Wave = wave; for (int i=0;i<MAX_VOICES;++i) _voices[i].setOsc1Waveform(wave); }
void SynthEngine::setOsc2Waveform(int wave) { _osc2Wave = wave; for (int i=0;i<MAX_VOICES;++i) _voices[i].setOsc2Waveform(wave); }

void SynthEngine::setOsc1PitchOffset(float semis) { _osc1PitchSemi = semis; for (int i=0;i<MAX_VOICES;++i) _voices[i].setOsc1PitchOffset(semis); }
void SynthEngine::setOsc2PitchOffset(float semis) { _osc2PitchSemi = semis; for (int i=0;i<MAX_VOICES;++i) _voices[i].setOsc2PitchOffset(semis); }

void SynthEngine::setOsc1Detune(float semis) { _osc1DetuneSemi = semis; for (int i=0;i<MAX_VOICES;++i) _voices[i].setOsc1Detune(semis); }
void SynthEngine::setOsc2Detune(float semis) { _osc2DetuneSemi = semis; for (int i=0;i<MAX_VOICES;++i) _voices[i].setOsc2Detune(semis); }

void SynthEngine::setOsc1FineTune(float cents) { _osc1FineCents = cents; for (int i=0;i<MAX_VOICES;++i) _voices[i].setOsc1FineTune(cents); }
void SynthEngine::setOsc2FineTune(float cents) { _osc2FineCents = cents; for (int i=0;i<MAX_VOICES;++i) _voices[i].setOsc2FineTune(cents); }

void SynthEngine::setOscMix(float osc1Level, float osc2Level) {
    _osc1Mix = osc1Level; _osc2Mix = osc2Level;
    for (int i=0;i<MAX_VOICES;++i) _voices[i].setOscMix(osc1Level, osc2Level);
}
void SynthEngine::setOsc1Mix(float oscLevel) { _osc1Mix = oscLevel; for (int i=0;i<MAX_VOICES;++i) _voices[i].setOsc1Mix(oscLevel); }
void SynthEngine::setOsc2Mix(float oscLevel) { _osc2Mix = oscLevel; for (int i=0;i<MAX_VOICES;++i) _voices[i].setOsc2Mix(oscLevel); }
void SynthEngine::setSubMix(float mix)  { _subMix = mix;  for (int i=0;i<MAX_VOICES;++i) _voices[i].setSubMix(mix); }
void SynthEngine::setNoiseMix(float mix){ _noiseMix = mix;for (int i=0;i<MAX_VOICES;++i) _voices[i].setNoiseMix(mix); }

void SynthEngine::setSupersawDetune(uint8_t oscIndex, float amount) {
    if (oscIndex > 1) return;
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (oscIndex == 0) _voices[i].setOsc1SupersawDetune(amount);
        else                _voices[i].setOsc2SupersawDetune(amount);
    }
}

void SynthEngine::setSupersawMix(uint8_t oscIndex, float amount) {
    if (oscIndex > 1) return;
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (oscIndex == 0) _voices[i].setOsc1SupersawMix(amount);
        else                _voices[i].setOsc2SupersawMix(amount);
    }
}

void SynthEngine::setOsc1FrequencyDcAmp(float amp) { _osc1FreqDc = amp; for (int i=0;i<MAX_VOICES;++i) _voices[i].setOsc1FrequencyDcAmp(amp); }
void SynthEngine::setOsc2FrequencyDcAmp(float amp) { _osc2FreqDc = amp; for (int i=0;i<MAX_VOICES;++i) _voices[i].setOsc2FrequencyDcAmp(amp); }
void SynthEngine::setOsc1ShapeDcAmp(float amp)     { _osc1ShapeDc = amp; for (int i=0;i<MAX_VOICES;++i) _voices[i].setOsc1ShapeDcAmp(amp); }
void SynthEngine::setOsc2ShapeDcAmp(float amp)     { _osc2ShapeDc = amp; for (int i=0;i<MAX_VOICES;++i) _voices[i].setOsc2ShapeDcAmp(amp); }

void SynthEngine::setRing1Mix(float level) { _ring1Mix = level; for (int i=0;i<MAX_VOICES;++i) _voices[i].setRing1Mix(level); }
void SynthEngine::setRing2Mix(float level) { _ring2Mix = level; for (int i=0;i<MAX_VOICES;++i) _voices[i].setRing2Mix(level); }

// ---- Amp mod DC ----
void SynthEngine::SetAmpModFixedLevel(float level) {
    _ampModFixedLevel = level;
    _ampModFixedDc.amplitude(level);
}
float SynthEngine::GetAmpModFixedLevel() const { return _ampModFixedLevel; }
float SynthEngine::getAmpModFixedLevel() const { return _ampModFixedLevel; }

// ---- LFOs ----
void SynthEngine::setLFO1Frequency(float hz) { _lfo1Frequency = hz; _lfo1.setFrequency(hz); }
void SynthEngine::setLFO2Frequency(float hz) { _lfo2Frequency = hz; _lfo2.setFrequency(hz); }
void SynthEngine::setLFO1Amount(float amt)   { _lfo1Amount = amt;   _lfo1.setAmplitude(amt); }
void SynthEngine::setLFO2Amount(float amt)   { _lfo2Amount = amt;   _lfo2.setAmplitude(amt); }

void SynthEngine::setLFO1Waveform(int type) { _lfo1Type = type; _lfo1.setWaveformType(type); }
void SynthEngine::setLFO2Waveform(int type) { _lfo2Type = type; _lfo2.setWaveformType(type); }

void SynthEngine::setLFO1Destination(LFODestination dest) {
    _lfo1Dest = dest; _lfo1.setDestination(dest);
    // Reset routing gains for LFO1 lanes
    for (int i=0;i<MAX_VOICES;++i) {
        _voices[i].frequencyModMixerOsc1().gain(1, 0.0f);
        _voices[i].frequencyModMixerOsc2().gain(1, 0.0f);
        _voices[i].shapeModMixerOsc1().gain(1, 0.0f);
        _voices[i].shapeModMixerOsc2().gain(1, 0.0f);
        _voices[i].filterModMixer().gain(2, 0.0f);
        _ampModMixer.gain(1, 0.0f);
    }
    // Apply
    if (dest == 1) { // frequency (both oscs)
        for (int i=0;i<MAX_VOICES;++i) {
            _voices[i].frequencyModMixerOsc1().gain(1, 1.0f);
            _voices[i].frequencyModMixerOsc2().gain(1, 1.0f);
        }
    } else if (dest == 2) { // filter
        for (int i=0;i<MAX_VOICES;++i) _voices[i].filterModMixer().gain(2, 1.0f);
    } else if (dest == 3) { // shape (both oscs)
        for (int i=0;i<MAX_VOICES;++i) {
            _voices[i].shapeModMixerOsc1().gain(1, 1.0f);
            _voices[i].shapeModMixerOsc2().gain(1, 1.0f);
        }
    } else if (dest == 4) { // amp
        _ampModMixer.gain(1, 1.0f);
    }
}

void SynthEngine::setLFO2Destination(LFODestination dest) {
    _lfo2Dest = dest; _lfo2.setDestination(dest);
    for (int i=0;i<MAX_VOICES;++i) {
        _voices[i].frequencyModMixerOsc1().gain(2, 0.0f);
        _voices[i].frequencyModMixerOsc2().gain(2, 0.0f);
        _voices[i].shapeModMixerOsc1().gain(2, 0.0f);
        _voices[i].shapeModMixerOsc2().gain(2, 0.0f);
        _voices[i].filterModMixer().gain(3, 0.0f);
        _ampModMixer.gain(2, 0.0f);
    }
    if (dest == 1) {
        for (int i=0;i<MAX_VOICES;++i) {
            _voices[i].frequencyModMixerOsc1().gain(2, 1.0f);
            _voices[i].frequencyModMixerOsc2().gain(2, 1.0f);
        }
    } else if (dest == 2) {
        for (int i=0;i<MAX_VOICES;++i) _voices[i].filterModMixer().gain(3, 1.0f);
    } else if (dest == 3) {
        for (int i=0;i<MAX_VOICES;++i) {
            _voices[i].shapeModMixerOsc1().gain(2, 1.0f);
            _voices[i].shapeModMixerOsc2().gain(2, 1.0f);
        }
    } else if (dest == 4) {
        _ampModMixer.gain(2, 1.0f);
    }
}

float SynthEngine::getLFO1Frequency() const { return _lfo1Frequency; }
float SynthEngine::getLFO2Frequency() const { return _lfo2Frequency; }
float SynthEngine::getLFO1Amount() const    { return _lfo1Amount; }
float SynthEngine::getLFO2Amount() const    { return _lfo2Amount; }
int   SynthEngine::getLFO1Waveform() const  { return _lfo1Type; }
int   SynthEngine::getLFO2Waveform() const  { return _lfo2Type; }
LFODestination SynthEngine::getLFO1Destination() const { return _lfo1Dest; }
LFODestination SynthEngine::getLFO2Destination() const { return _lfo2Dest; }

const char* SynthEngine::getLFO1WaveformName() const {
    return waveformShortName((WaveformType)_lfo1Type);
}
const char* SynthEngine::getLFO2WaveformName() const {
    return waveformShortName((WaveformType)_lfo2Type);
}
const char* SynthEngine::getLFO1DestinationName() const {
    int d = (int)_lfo1Dest;
    return (d >= 0 && d < NUM_LFO_DESTS) ? LFODestNames[d] : "Unknown";
}
const char* SynthEngine::getLFO2DestinationName() const {
    int d = (int)_lfo2Dest;
    return (d >= 0 && d < NUM_LFO_DESTS) ? LFODestNames[d] : "Unknown";
}


// ---- FX (store + hand off to FXChainBlock as needed) ----
void SynthEngine::setFXReverbRoomSize(float v) { _fxChain.setReverbRoomSize(v); }
void SynthEngine::setFXReverbDamping(float v)  { _fxChain.setReverbDamping(v); }   // name fix
void SynthEngine::setFXDelayBaseTime(float ms) { _fxChain.setBaseDelayTime(ms); }   // name fix
void SynthEngine::setFXDelayFeedback(float v)  { _fxChain.setDelayFeedback(v); }

void SynthEngine::setFXDryMix(float v)    { _fxChain.setDryMix(v, v); }            // 2 args
void SynthEngine::setFXReverbMix(float v) { _fxChain.setReverbMix(v, v); }         // 2 args
void SynthEngine::setFXDelayMix(float v)  { _fxChain.setDelayMix(v, v); }          // 2 args


// ---- UI helper getters ----
int SynthEngine::getOsc1Waveform() const { return _osc1Wave; }
int SynthEngine::getOsc2Waveform() const { return _osc2Wave; }
const char* SynthEngine::getOsc1WaveformName() const {
    return waveformShortName((WaveformType)_osc1Wave);
}
const char* SynthEngine::getOsc2WaveformName() const {
    return waveformShortName((WaveformType)_osc2Wave);
}


float SynthEngine::getSupersawDetune(uint8_t osc) const { return (osc<2)?_supersawDetune[osc]:0.0f; }
float SynthEngine::getSupersawMix(uint8_t osc)    const { return (osc<2)?_supersawMix[osc]:0.0f; }
float SynthEngine::getOsc1PitchOffset() const { return _osc1PitchSemi; }
float SynthEngine::getOsc2PitchOffset() const { return _osc2PitchSemi; }
float SynthEngine::getOsc1Detune() const { return _osc1DetuneSemi; }
float SynthEngine::getOsc2Detune() const { return _osc2DetuneSemi; }
float SynthEngine::getOsc1FineTune() const { return _osc1FineCents; }
float SynthEngine::getOsc2FineTune() const { return _osc2FineCents; }
float SynthEngine::getOscMix1() const { return _osc1Mix; }
float SynthEngine::getOscMix2() const { return _osc2Mix; }
float SynthEngine::getSubMix() const { return _subMix; }
float SynthEngine::getNoiseMix() const { return _noiseMix; }
float SynthEngine::getRing1Mix() const { return _ring1Mix; }
float SynthEngine::getRing2Mix() const { return _ring2Mix; }
float SynthEngine::getOsc1FrequencyDc() const { return _osc1FreqDc; }
float SynthEngine::getOsc2FrequencyDc() const { return _osc2FreqDc; }
float SynthEngine::getOsc1ShapeDc() const     { return _osc1ShapeDc; }
float SynthEngine::getOsc2ShapeDc() const     { return _osc2ShapeDc; }

bool  SynthEngine::getGlideEnabled() const { return _glideEnabled; }
float SynthEngine::getGlideTimeMs()  const { return _glideTimeMs; }

// ---- MIDI CC dispatcher with JT_LOGF tracing --------------------------------
void SynthEngine::handleControlChange(byte /*channel*/, byte control, byte value) {
    // Helpful in logs
    const char* ccName = CC::name(control);
    if (!ccName) ccName = "?";

    const float norm = value / 127.0f;

    switch (control) {
        // ------------------- OSC waveforms -------------------
        case 21: { // OSC1_WAVE
            WaveformType t = waveformFromCC(value);
            setOsc1Waveform((int)t);
            JT_LOGF("[CC %u:%s] OSC1 Waveform -> %s (%d)\n", control, ccName, waveformShortName(t), (int)t);
        } break;

        case 22: { // OSC2_WAVE
            WaveformType t = waveformFromCC(value);
            setOsc2Waveform((int)t);
            JT_LOGF("[CC %u:%s] OSC2 Waveform -> %s (%d)\n", control, ccName, waveformShortName(t), (int)t);
        } break;

        // ------------------- Mod wheel → LFO1 freq (example) -------------------
        case 1: {
            float hz = JT4000Map::cc_to_lfo_hz(value);
            setLFO1Frequency(hz);
            JT_LOGF("[CC %u:ModWheel] LFO1 Freq = %.4f Hz\n", control, hz);
        } break;

        // ------------------- Filter main -------------------
        case 23: { // FILTER_CUTOFF
            float hz = JT4000Map::cc_to_cutoff_hz(value);
            hz = fminf(fmaxf(hz, CUTOFF_MIN_HZ), CUTOFF_MAX_HZ);
            setFilterCutoff(hz);
            JT_LOGF("[CC %u:%s] Cutoff = %.2f Hz\n", control, ccName, hz);
        } break;

        case 24: { // FILTER_RESONANCE
            float r = JT4000Map::cc_to_res_k(value);  // your project’s resonance mapping
            setFilterResonance(r);
            JT_LOGF("[CC %u:%s] Resonance = %.4f\n", control, ccName, r);
        } break;

        // ------------------- Amp envelope -------------------
        case 25: { // AMP_ATTACK
            float ms = CCtoTime(value);
            for (int i=0;i<MAX_VOICES;++i) _voices[i].setAmpAttack(ms);
            JT_LOGF("[CC %u:%s] Amp Attack = %.2f ms\n", control, ccName, ms);
        } break;
        case 26: { // AMP_DECAY
            float ms = CCtoTime(value);
            for (int i=0;i<MAX_VOICES;++i) _voices[i].setAmpDecay(ms);
            JT_LOGF("[CC %u:%s] Amp Decay = %.2f ms\n", control, ccName, ms);
        } break;
        case 27: { // AMP_SUSTAIN
            for (int i=0;i<MAX_VOICES;++i) _voices[i].setAmpSustain(norm);
            JT_LOGF("[CC %u:%s] Amp Sustain = %.3f\n", control, ccName, norm);
        } break;
        case 28: { // AMP_RELEASE
            float ms = CCtoTime(value);
            for (int i=0;i<MAX_VOICES;++i) _voices[i].setAmpRelease(ms);
            JT_LOGF("[CC %u:%s] Amp Release = %.2f ms\n", control, ccName, ms);
        } break;

        // ------------------- Filter envelope -------------------
        case 29: { // FILTER_ENV_ATTACK
            float ms = CCtoTime(value);
            for (int i=0;i<MAX_VOICES;++i) _voices[i].setFilterAttack(ms);
            JT_LOGF("[CC %u:%s] Filt Env Attack = %.2f ms\n", control, ccName, ms);
        } break;
        case 30: { // FILTER_ENV_DECAY
            float ms = CCtoTime(value);
            for (int i=0;i<MAX_VOICES;++i) _voices[i].setFilterDecay(ms);
            JT_LOGF("[CC %u:%s] Filt Env Decay = %.2f ms\n", control, ccName, ms);
        } break;
        case 31: { // FILTER_ENV_SUSTAIN
            for (int i=0;i<MAX_VOICES;++i) _voices[i].setFilterSustain(norm);
            JT_LOGF("[CC %u:%s] Filt Env Sustain = %.3f\n", control, ccName, norm);
        } break;
        case 32: { // FILTER_ENV_RELEASE
            float ms = CCtoTime(value);
            for (int i=0;i<MAX_VOICES;++i) _voices[i].setFilterRelease(ms);
            JT_LOGF("[CC %u:%s] Filt Env Release = %.2f ms\n", control, ccName, ms);
        } break;

        // ------------------- Coarse pitch (stepped) -------------------
        case 41: { // OSC1_PITCH_OFFSET
            float semis;
            if (value <= 25)       semis = -24.0f;
            else if (value <= 51)  semis = -12.0f;
            else if (value <= 76)  semis =   0.0f;
            else if (value <= 101) semis = +12.0f;
            else                   semis = +24.0f;
            setOsc1PitchOffset(semis);
            JT_LOGF("[CC %u:%s] OSC1 Coarse = %.1f semitones\n", control, ccName, semis);
        } break;

        case 42: { // OSC2_PITCH_OFFSET
            float semis;
            if (value <= 25)       semis = -24.0f;
            else if (value <= 51)  semis = -12.0f;
            else if (value <= 76)  semis =   0.0f;
            else if (value <= 101) semis = +12.0f;
            else                   semis = +24.0f;
            setOsc2PitchOffset(semis);
            JT_LOGF("[CC %u:%s] OSC2 Coarse = %.1f semitones\n", control, ccName, semis);
        } break;

        // ------------------- Detune / fine -------------------
        case 43: { float d = norm * 2.0f - 1.0f; setOsc1Detune(d);    JT_LOGF("[CC %u:%s] OSC1 Detune = %.3f\n", control, ccName, d); } break;
        case 44: { float d = norm * 2.0f - 1.0f; setOsc2Detune(d);    JT_LOGF("[CC %u:%s] OSC2 Detune = %.3f\n", control, ccName, d); } break;
        case 45: { float c = norm * 200.0f - 100.0f; setOsc1FineTune(c); JT_LOGF("[CC %u:%s] OSC1 Fine = %.1f cents\n", control, ccName, c); } break;
        case 46: { float c = norm * 200.0f - 100.0f; setOsc2FineTune(c); JT_LOGF("[CC %u:%s] OSC2 Fine = %.1f cents\n", control, ccName, c); } break;

        // ------------------- Osc mix + taps -------------------
        case 47: { // balance
            float l = 1.0f - norm, r = norm;
            setOscMix(l, r);
            JT_LOGF("[CC %u:%s] Osc Mix balance L=%.3f R=%.3f\n", control, ccName, l, r);
        } break;
        case 60: { for (int i=0;i<MAX_VOICES;++i) _voices[i].setOsc1Mix(norm); _osc1Mix = norm; JT_LOGF("[CC %u:%s] OSC1 Mix = %.3f\n", control, ccName, norm); } break;
        case 61: { for (int i=0;i<MAX_VOICES;++i) _voices[i].setOsc2Mix(norm); _osc2Mix = norm; JT_LOGF("[CC %u:%s] OSC2 Mix = %.3f\n", control, ccName, norm); } break;
        case 58: { setSubMix(norm);   JT_LOGF("[CC %u:%s] Sub Mix = %.3f\n",  control, ccName, norm); } break;
        case 59: { setNoiseMix(norm); JT_LOGF("[CC %u:%s] Noise Mix = %.3f\n", control, ccName, norm); } break;

        // ------------------- Filter mods -------------------
        case 48: { float a = norm * 2.0f - 1.0f; setFilterEnvAmount(a);       JT_LOGF("[CC %u:%s] Filt Env Amount = %.3f\n", control, ccName, a); } break;
        case 50: { float k = norm * 2.0f - 1.0f; setFilterKeyTrackAmount(k);  JT_LOGF("[CC %u:%s] KeyTrack = %.3f\n",      control, ccName, k); } break;
        case 84: { float o = norm * 10.0f;       setFilterOctaveControl(o);   JT_LOGF("[CC %u:%s] Filter Octave = %.3f\n",  control, ccName, o); } break;

        // ------------------- LFO1 -------------------
        case 54: { float hz = JT4000Map::cc_to_lfo_hz(value); setLFO1Frequency(hz); JT_LOGF("[CC %u:%s] LFO1 Freq = %.4f Hz\n", control, ccName, hz); } break;
        case 55: { setLFO1Amount(norm); JT_LOGF("[CC %u:%s] LFO1 Depth = %.3f\n", control, ccName, norm); } break;
        case 56: { int d = JT4000Map::lfoDestFromCC(value); setLFO1Destination((LFODestination)d); JT_LOGF("[CC %u:%s] LFO1 Dest = %d\n", control, ccName, d); } break;
        case 62: { WaveformType t = waveformFromCC(value); setLFO1Waveform((int)t); JT_LOGF("[CC %u:%s] LFO1 Wave -> %s (%d)\n", control, ccName, waveformShortName(t), (int)t); } break;

        // ------------------- LFO2 -------------------
        case 51: { float hz = JT4000Map::cc_to_lfo_hz(value); setLFO2Frequency(hz); JT_LOGF("[CC %u:%s] LFO2 Freq = %.4f Hz\n", control, ccName, hz); } break;
        case 52: { setLFO2Amount(norm); JT_LOGF("[CC %u:%s] LFO2 Depth = %.3f\n", control, ccName, norm); } break;
        case 53: { int d = JT4000Map::lfoDestFromCC(value); setLFO2Destination((LFODestination)d); JT_LOGF("[CC %u:%s] LFO2 Dest = %d\n", control, ccName, d); } break;
        case 63: { WaveformType t = waveformFromCC(value); setLFO2Waveform((int)t); JT_LOGF("[CC %u:%s] LFO2 Wave -> %s (%d)\n", control, ccName, waveformShortName(t), (int)t); } break;

        // ------------------- FX -------------------
        case 70: { setFXReverbRoomSize(norm); JT_LOGF("[CC %u:%s] Reverb RoomSize = %.3f\n", control, ccName, norm); } break;
        case 71: { setFXReverbDamping(norm);  JT_LOGF("[CC %u:%s] Reverb Damping  = %.3f\n", control, ccName, norm); } break;
        case 72: { float ms = norm * 1000.0f; setFXDelayBaseTime(ms); JT_LOGF("[CC %u:%s] Delay BaseTime = %.1f ms\n", control, ccName, ms); } break;
        case 73: { setFXDelayFeedback(norm);  JT_LOGF("[CC %u:%s] Delay Feedback  = %.3f\n", control, ccName, norm); } break;
        case 74: { setFXDryMix(norm);         JT_LOGF("[CC %u:%s] Dry Mix         = %.3f\n", control, ccName, norm); } break;
        case 75: { setFXReverbMix(norm);      JT_LOGF("[CC %u:%s] Reverb Mix      = %.3f\n", control, ccName, norm); } break;
        case 76: { setFXDelayMix(norm);       JT_LOGF("[CC %u:%s] Delay Mix       = %.3f\n", control, ccName, norm); } break;

        // ------------------- Supersaw / DC / Ring -------------------
        case 77: { setSupersawDetune(0, norm); JT_LOGF("[CC %u:%s] Supersaw1 Detune = %.3f\n", control, ccName, norm); } break;
        case 78: { setSupersawMix(0, norm);    JT_LOGF("[CC %u:%s] Supersaw1 Mix    = %.3f\n", control, ccName, norm); } break;
        case 79: { setSupersawDetune(1, norm); JT_LOGF("[CC %u:%s] Supersaw2 Detune = %.3f\n", control, ccName, norm); } break;
        case 80: { setSupersawMix(1, norm);    JT_LOGF("[CC %u:%s] Supersaw2 Mix    = %.3f\n", control, ccName, norm); } break;

        case 86: { setOsc1FrequencyDcAmp(norm); JT_LOGF("[CC %u:%s] Osc1 Freq DC = %.3f\n", control, ccName, norm); } break;
        case 87: { setOsc1ShapeDcAmp(norm);     JT_LOGF("[CC %u:%s] Osc1 Shape DC = %.3f\n", control, ccName, norm); } break;
        case 88: { setOsc2FrequencyDcAmp(norm); JT_LOGF("[CC %u:%s] Osc2 Freq DC = %.3f\n", control, ccName, norm); } break;
        case 89: { setOsc2ShapeDcAmp(norm);     JT_LOGF("[CC %u:%s] Osc2 Shape DC = %.3f\n", control, ccName, norm); } break;

        case 91: { setRing1Mix(norm); JT_LOGF("[CC %u:%s] Ring1 Mix = %.3f\n", control, ccName, norm); } break;
        case 92: { setRing2Mix(norm); JT_LOGF("[CC %u:%s] Ring2 Mix = %.3f\n", control, ccName, norm); } break;

        // ------------------- Glide -------------------
        case 81: {
            _glideEnabled = (value >= 1);
            for (int i=0;i<MAX_VOICES;++i) _voices[i].setGlideEnabled(_glideEnabled);
            JT_LOGF("[CC %u:%s] Glide Enabled = %d\n", control, ccName, (int)_glideEnabled);
        } break;

        case 82: {
            float ms = CCtoTime(value);
            _glideTimeMs = ms;
            for (int i=0;i<MAX_VOICES;++i) _voices[i].setGlideTime(ms);
            JT_LOGF("[CC %u:%s] Glide Time = %.2f ms\n", control, ccName, ms);
        } break;

        default:
            JT_LOGF("[CC %u:%s] Unmapped value=%u\n", control, ccName, value);
            break;
    }
}



