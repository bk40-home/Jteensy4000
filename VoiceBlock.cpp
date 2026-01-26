#include "synth_waveform.h"
//#include "usb_serial.h"
#include "VoiceBlock.h"

VoiceBlock::VoiceBlock() {
    _patchCables[0] = new AudioConnection(_osc1.output(), 0, _oscMixer, 0);
    _patchCables[1] = new AudioConnection(_osc2.output(), 0, _oscMixer, 1);
    _patchCables[2] = new AudioConnection(_osc1.output(), 0, _ring1, 0);
    _patchCables[3] = new AudioConnection(_osc2.output(), 0, _ring1, 1);
    _patchCables[4] = new AudioConnection(_osc1.output(), 0, _ring2, 0);
    _patchCables[5] = new AudioConnection(_osc2.output(), 0, _ring2, 0);
    _patchCables[6] = new AudioConnection(_ring1, 0, _oscMixer, 2);
    _patchCables[7] = new AudioConnection(_ring2, 0, _oscMixer, 3);
    _patchCables[8] = new AudioConnection(_oscMixer, 0, _voiceMixer, 0);
    _patchCables[9] = new AudioConnection(_subOsc.output(), 0, _voiceMixer, 2);
    _patchCables[10] = new AudioConnection(_noise, 0, _voiceMixer, 3); 
    _patchCables[11] = new AudioConnection(_voiceMixer, 0, _filter.input(), 0);    
    _patchCables[12] = new AudioConnection(_filter.output(), 0, _ampEnvelope.input(), 0);
    //_patchCables[12] = new AudioConnection(_voiceMixer, 0, _ampEnvelope.input(), 0);
    
    _patchCables[13] = new AudioConnection(_filter.envmod(), 0 , _filterEnvelope.input(), 0); 
    _patchCables[14] = new AudioConnection(_filterEnvelope.output(), 0, _filter.modMixer(), 1);

    _oscMixer.gain(0, _on);
    _oscMixer.gain(1, _on);
    _oscMixer.gain(2, 0.0f);
    _oscMixer.gain(3, 0.0f);

    _voiceMixer.gain(0, _on);
    _voiceMixer.gain(1, 0.0f);
    _voiceMixer.gain(2, 0.0f);
    _voiceMixer.gain(3, 0.0f);

    _subOsc.setWaveform(WAVEFORM_SINE);
    _subOsc.setAmplitude(0.0f);
    _subOsc.setFrequency(110.0f);
    _noise.amplitude(0.0f);

    _osc1.setWaveformType(WAVEFORM_SAWTOOTH);
    _osc2.setWaveformType(WAVEFORM_SAWTOOTH);
    
    
}

void VoiceBlock::noteOn(float freq, float velocity) {
    
    Serial.printf("noteOn voice: freq %.1f velocity %.1f\n", freq, velocity);
    setAmplitude(_on);
    _osc1.noteOn(freq, velocity);
    _osc2.noteOn(freq, velocity);
    _subOsc.setFrequency(freq);
    _filterEnvelope.noteOn();
    _ampEnvelope.noteOn();
    //_filter.setKeyTrackAmount(log2f(freq / 440.0f));
    // Compute key tracking modulation.  deltaOct is the number of
    // octaves above or below the reference (A4=440 Hz).  We normalise
    // this by the filter’s octave-control setting, scale it by the
    // user’s key‑tracking depth (0–1), and clamp to ±1.

    float deltaOct   = log2f(freq / 440.0f);
    float octaveCtrl = _filter.getOctaveControl();
    float norm       = (octaveCtrl > 0.0f) ? (deltaOct / octaveCtrl) : 0.0f;
    norm *= _filterKeyTrackAmount;
    if (norm >  1.0f) norm =  1.0f;
    if (norm < -1.0f) norm = -1.0f;
    _filter.setKeyTrackAmount(norm);

    // Cache the current frequency so changes to the key‑tracking
    // amount during a held note can update correctly.
    _currentFreq = freq;
}

void VoiceBlock::noteOff() {
    _filterEnvelope.noteOff();
    _ampEnvelope.noteOff();
}

void VoiceBlock::setOsc1Waveform(int wave) { _osc1.setWaveformType(wave); }
void VoiceBlock::setOsc2Waveform(int wave) { _osc2.setWaveformType(wave); }

void VoiceBlock::setBaseFrequency(float freq) {
    _osc1.setBaseFrequency(freq);
    _osc2.setBaseFrequency(freq);
    _subOsc.setFrequency(freq);
}

void VoiceBlock::setAmplitude(float amp) {
    _osc1.setAmplitude(amp);
    _osc2.setAmplitude(amp);
    _subOsc.setAmplitude(amp);
    _noise.amplitude(amp);
}

float VoiceBlock::_clampedLevel(float level){
    if (level > _on) {
        level = _on;
    }
    return level;
}

void VoiceBlock::setOscMix(float _osc1Lvl, float _osc2Lvl) {
    
    _osc1Level = _osc1Lvl;
    _osc2Level = _osc2Lvl;
    _oscMixer.gain(0, _clampedLevel(_osc1Level));
    _oscMixer.gain(1, _clampedLevel(_osc2Level));
   
}

void VoiceBlock::setOsc1Mix(float _oscLvl) {
    _osc1Level = _oscLvl;
    _oscMixer.gain(0, _clampedLevel(_osc1Level));
}

void VoiceBlock::setOsc2Mix(float _oscLvl) {
    _osc2Level = _oscLvl;
    _oscMixer.gain(1, _clampedLevel(_osc2Level));
}

void VoiceBlock::setRing1Mix(float level) {
    _ring1Level = level;
    _oscMixer.gain(2, _clampedLevel(_ring1Level));    
}

void VoiceBlock::setRing2Mix(float level) {
    _ring2Level = level;
    _oscMixer.gain(3, _clampedLevel(_ring2Level));    
}

void VoiceBlock::setSubMix(float level) {
    _subMix = level;
    _voiceMixer.gain(2, _clampedLevel(_subMix));

}

void VoiceBlock::setNoiseMix(float level) {
    _noiseMix = level;
    _voiceMixer.gain(3, _clampedLevel(_noiseMix));
}

void VoiceBlock::setOsc1SupersawDetune(float amount) {
    _osc1.setSupersawDetune(amount);
}

void VoiceBlock::setOsc2SupersawDetune(float amount) {
    _osc2.setSupersawDetune(amount);
}

void VoiceBlock::setOsc1SupersawMix(float amount) {
    _osc1.setSupersawMix(amount);
}

void VoiceBlock::setOsc2SupersawMix(float amount) {
    _osc2.setSupersawMix(amount);
}

void VoiceBlock::setGlideEnabled(bool enabled) {
    _osc1.setGlideEnabled(enabled);
    _osc2.setGlideEnabled(enabled);
}

void VoiceBlock::setGlideTime(float ms) {
    _osc1.setGlideTime(ms);
    _osc2.setGlideTime(ms);
}

void VoiceBlock::setOsc1FrequencyDcAmp(float amp){ _osc1.setFrequencyDcAmp(amp);}
void VoiceBlock::setOsc2FrequencyDcAmp(float amp){ _osc2.setFrequencyDcAmp(amp);}
void VoiceBlock::setOsc1ShapeDcAmp(float amp){ _osc1.setShapeDcAmp(amp);}
void VoiceBlock::setOsc2ShapeDcAmp(float amp){ _osc2.setShapeDcAmp(amp);}

void VoiceBlock::setFilterCutoff(float value) {
    _baseCutoff = value;
    _filter.setCutoff(value);
}

void VoiceBlock::setFilterResonance(float value) {
    _filter.setResonance(value);
}

// void VoiceBlock::setFilterDrive(float value) {
//     _filter.setDrive(value);
// }

void VoiceBlock::setFilterOctaveControl(float value) {
    _filter.setOctaveControl(value);
}

// void VoiceBlock::setFilterPassbandGain(float value) {
//     _filter.setPassbandGain(value);
// }

void VoiceBlock::setFilterEnvAmount(float amt) {
    _filterEnvAmount = amt;
    _filter.setEnvModAmount(amt);
}

void VoiceBlock::setFilterKeyTrackAmount(float amt) {
    // _filterKeyTrackAmount = amt;
    // _filter.setKeyTrackAmount(amt);
    // Store the user’s desired key‑tracking depth (0–1).
    _filterKeyTrackAmount = amt;
     // If a note has been played (i.e. _currentFreq > 0), recompute the
     // modulation using the last frequency.  Otherwise, the next call to
      // noteOn() will set it.
    if (_currentFreq > 0.0f) {
        float deltaOct   = log2f(_currentFreq / 440.0f);
        float octaveCtrl = _filter.getOctaveControl();
        float norm       = (octaveCtrl > 0.0f) ? (deltaOct / octaveCtrl) : 0.0f;
        norm *= _filterKeyTrackAmount;
        if (norm >  1.0f) norm =  1.0f;
        if (norm < -1.0f) norm = -1.0f;
        _filter.setKeyTrackAmount(norm);
    }
}

void VoiceBlock::setMultimode(float amount) {
    _multimode = amount;
    _filter.setMultimode(amount);

}

void VoiceBlock::setTwoPole(bool enabled) {
    _useTwoPole = enabled;
    _filter.setTwoPole(enabled);
}

void VoiceBlock::setXpander4Pole(bool enabled) {
    _xpander4Pole = enabled;
    _filter.setXpander4Pole(enabled);
}

void VoiceBlock::setXpanderMode(uint8_t amount) {
    _xpanderMode = amount;
    _filter.setXpanderMode(amount);
}

void VoiceBlock::setBPBlend2Pole(bool enabled) {
    _bpBlend2Pole = enabled;
    _filter.setBPBlend2Pole(enabled);
}

void VoiceBlock::setPush2Pole(bool enabled) {
    _push2Pole = enabled;
    _filter.setPush2Pole(enabled);
}

void VoiceBlock::setResonanceModDepth(float amount) {
    _resonanceModDepth = amount;
    _filter.setResonanceModDepth(amount);
}



// --- Amp Envelope ---
void VoiceBlock::setAmpAttack(float a) { _ampEnvelope.setAttackTime(a); }
void VoiceBlock::setAmpDecay(float d) { _ampEnvelope.setDecayTime(d); }
void VoiceBlock::setAmpSustain(float s) { _ampEnvelope.setSustainLevel(s); }
void VoiceBlock::setAmpRelease(float r) { _ampEnvelope.setReleaseTime(r); }
void VoiceBlock::setAmpADSR(float a, float d, float s, float r) {
    _ampEnvelope.setADSR(a,d,s,r);
}

// --- Filter Envelope ---
void VoiceBlock::setFilterAttack(float a) { _filterEnvelope.setAttackTime(a); }
void VoiceBlock::setFilterDecay(float d) { _filterEnvelope.setDecayTime(d); }
void VoiceBlock::setFilterSustain(float s) { _filterEnvelope.setSustainLevel(s); }
void VoiceBlock::setFilterRelease(float r) { _filterEnvelope.setReleaseTime(r); }
void VoiceBlock::setFilterADSR(float a, float d, float s, float r) {
    _filterEnvelope.setADSR(a,d,s,r);
}

void VoiceBlock::setOsc1PitchOffset(float semis) { _osc1.setPitchOffset(semis); }
void VoiceBlock::setOsc2PitchOffset(float semis) { _osc2.setPitchOffset(semis); }
void VoiceBlock::setOsc1Detune(float hz) { _osc1.setDetune(hz); }
void VoiceBlock::setOsc2Detune(float hz) { _osc2.setDetune(hz); }
void VoiceBlock::setOsc1FineTune(float cents) { _osc1.setFineTune(cents); }
void VoiceBlock::setOsc2FineTune(float cents) { _osc2.setFineTune(cents); }

void VoiceBlock::update() {

    _osc1.update();
    _osc2.update();

}

AudioStream& VoiceBlock::output() {
    return _ampEnvelope.output();
}

AudioMixer4& VoiceBlock::frequencyModMixerOsc1(){
    return _osc1.frequencyModMixer();
}
AudioMixer4& VoiceBlock::shapeModMixerOsc1(){
    return _osc1.shapeModMixer();
}

AudioMixer4& VoiceBlock::frequencyModMixerOsc2(){
    return _osc2.frequencyModMixer();
}
AudioMixer4& VoiceBlock::shapeModMixerOsc2(){
    return _osc2.shapeModMixer();
}
AudioMixer4& VoiceBlock::filterModMixer(){
    return _filter.modMixer();
}

// Getters
int VoiceBlock::getOsc1Waveform() const { return _osc1.getWaveform(); }
int VoiceBlock::getOsc2Waveform() const { return _osc2.getWaveform(); }
float VoiceBlock::getOsc1PitchOffset() const { return _osc1.getPitchOffset(); }
float VoiceBlock::getOsc2PitchOffset() const { return _osc2.getPitchOffset(); }
float VoiceBlock::getOsc1Detune() const { return _osc1.getDetune(); }
float VoiceBlock::getOsc2Detune() const { return _osc2.getDetune(); }
float VoiceBlock::getOsc1FineTune() const { return _osc1.getFineTune(); }
float VoiceBlock::getOsc2FineTune() const { return _osc2.getFineTune(); }
float VoiceBlock::getOscMix1() const { return _osc1Level; }
float VoiceBlock::getOscMix2() const { return _osc2Level; }
float VoiceBlock::getSubMix() const { return _subMix; }
float VoiceBlock::getNoiseMix() const { return _noiseMix; }
float VoiceBlock::getOsc1SupersawDetune() const { return _osc1.getSupersawDetune(); }
float VoiceBlock::getOsc2SupersawDetune() const { return _osc2.getSupersawDetune(); }
float VoiceBlock::getOsc1SupersawMix() const { return _osc1.getSupersawMix(); }
float VoiceBlock::getOsc2SupersawMix() const { return _osc2.getSupersawMix(); }
float VoiceBlock::getOsc1FrequencyDc() const { return _osc1.getFrequencyDcAmp(); }
float VoiceBlock::getOsc2FrequencyDc() const { return _osc2.getFrequencyDcAmp(); }
float VoiceBlock::getOsc1ShapeDc() const { return _osc1.getShapeDcAmp(); }
float VoiceBlock::getOsc2ShapeDc() const { return _osc2.getShapeDcAmp(); }

bool VoiceBlock::getGlideEnabled() const { return _osc1.getGlideEnabled(); }
float VoiceBlock::getGlideTime() const { return _osc1.getGlideTime(); }

float VoiceBlock::getFilterCutoff() const { return _baseCutoff; }
float VoiceBlock::getFilterResonance() const { return _filter.getResonance(); }
//float VoiceBlock::getFilterDrive() const { return _filter.getDrive(); }
float VoiceBlock::getFilterOctaveControl() const { return _filter.getOctaveControl(); }
//float VoiceBlock::getFilterPassbandGain() const { return _filter.getPassbandGain(); }
float VoiceBlock::getFilterEnvAmount() const { return _filterEnvAmount; }
float VoiceBlock::getFilterKeyTrackAmount() const { return _filterKeyTrackAmount; }

float VoiceBlock::getRing1Mix() const { return _ring1Level; }
float VoiceBlock::getRing2Mix() const { return _ring2Level; }


float VoiceBlock::getAmpAttack() const { return _ampEnvelope.getAttackTime(); }
float VoiceBlock::getAmpDecay() const { return _ampEnvelope.getDecayTime(); }
float VoiceBlock::getAmpSustain() const { return _ampEnvelope.getSustainLevel(); }
float VoiceBlock::getAmpRelease() const { return _ampEnvelope.getReleaseTime(); }

float VoiceBlock::getFilterEnvAttack() const { return _filterEnvelope.getAttackTime(); }
float VoiceBlock::getFilterEnvDecay() const { return _filterEnvelope.getDecayTime(); }
float VoiceBlock::getFilterEnvSustain() const { return _filterEnvelope.getSustainLevel(); }
float VoiceBlock::getFilterEnvRelease() const { return _filterEnvelope.getReleaseTime(); }

// === Arbitrary waveform bank/index forwarding ===
// These functions allow the SynthEngine to set the current AKWF bank and
// table index for each oscillator.  When the oscillator waveform type is
// WAVEFORM_ARBITRARY, the OscillatorBlock will call akwf_get() to fetch
// the correct table from AKWF_All.h based on the bank and index.  See
// OscillatorBlock::setArbBank() and setArbTableIndex().

void VoiceBlock::setOsc1ArbBank(ArbBank bank) {
    _osc1.setArbBank(bank);
}

void VoiceBlock::setOsc2ArbBank(ArbBank bank) {
    _osc2.setArbBank(bank);
}

void VoiceBlock::setOsc1ArbIndex(uint16_t idx) {
    _osc1.setArbTableIndex(idx);
}

void VoiceBlock::setOsc2ArbIndex(uint16_t idx) {
    _osc2.setArbTableIndex(idx);
}
