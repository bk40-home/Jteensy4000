#pragma once
// SynthEngine.h
// JT-4000-style engine wrapper around VoiceBlock, LFOBlock, and FXChainBlock.
// Keeps parameter names CC-aligned and exposes getters for UI/presets.

#include <Arduino.h>          // for byte
#include "VoiceBlock.h"
#include "LFOBlock.h"
#include "FXChainBlock.h"
#include "Mapping.h"          // shared CC mappings (forward/inverse)
#include "Waveforms.h"        // waveform names + CC mapping helpers
#include "DebugTrace.h"   // << add near other includes
#include "AKWF_All.h"      // AKWF bank catalog for arbitrary waveforms

using namespace JT4000Map;

#define MAX_VOICES 4

class SynthEngine {
public:
    // --- Lifecycle
    SynthEngine();
    void noteOn(byte note, float velocity);
    void noteOff(byte note);
    void update();
    static constexpr uint8_t VOICE_NONE = 255;  

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
    void setSubMix(float mix);   // 0..1
    void setNoiseMix(float mix);
    void setSupersawDetune(uint8_t oscIndex, float amount); // 0..1
    void setSupersawMix(uint8_t oscIndex, float amount);    // 0..1
    void setOsc1FrequencyDcAmp(float amp);                  // 0..1
    void setOsc2FrequencyDcAmp(float amp);                  // 0..1
    void setOsc1ShapeDcAmp(float amp);                      // 0..1
    void setOsc2ShapeDcAmp(float amp);                      // 0..1
    void setRing1Mix(float level);                          // 0..1
    void setRing2Mix(float level);                          // 0..1

    // --- Arbitrary waveform bank + index selection (per oscillator) ---
    // When an oscillator waveform is WAVEFORM_ARBITRARY, these functions
    // specify which AKWF bank and which table within that bank are used.
    // The index is clamped to the number of tables in the selected bank.
    void setOsc1ArbBank(ArbBank b);
    void setOsc2ArbBank(ArbBank b);
    void setOsc1ArbIndex(uint16_t idx);
    void setOsc2ArbIndex(uint16_t idx);
    ArbBank  getOsc1ArbBank() const { return _osc1ArbBank; }
    ArbBank  getOsc2ArbBank() const { return _osc2ArbBank; }
    uint16_t getOsc1ArbIndex() const { return _osc1ArbIndex; }
    uint16_t getOsc2ArbIndex() const { return _osc2ArbIndex; }

    // Amp modulation DC
    void SetAmpModFixedLevel(float level);
    float GetAmpModFixedLevel() const;
    float getAmpModFixedLevel() const; // duplicate convenience, kept for compatibility

    // --- LFO 1
    void setLFO1Frequency(float hz);
    void setLFO1Amount(float amt);
    void setLFO1Waveform(int type);
    void setLFO1Destination(LFODestination dest);

    float getLFO1Frequency() const;
    float getLFO1Amount() const;
    int   getLFO1Waveform() const;
    LFODestination getLFO1Destination() const;
    // UI name helpers
    const char* getLFO1WaveformName() const;
    const char* getLFO1DestinationName() const;

    // --- LFO 2
    void setLFO2Frequency(float hz);
    void setLFO2Amount(float amt);
    void setLFO2Waveform(int type);
    void setLFO2Destination(LFODestination dest);

    float getLFO2Frequency() const;
    float getLFO2Amount() const;
    int   getLFO2Waveform() const;
    LFODestination getLFO2Destination() const;
    const char* getLFO2WaveformName() const;
    const char* getLFO2DestinationName() const;

    // --- Envelopes / Filter
    void setFilterEnvAmount(float amt);   // -1..+1 (bipolar depth)
    void setFilterCutoff(float value);    // Hz
    void setFilterResonance(float value); // 0..1
    void setFilterKeyTrackAmount(float amt);  // -1..+1
    void setFilterOctaveControl(float octaves);
    void setFilterMultimode(float _multimode);        // 0..1 (when xpander4Pole=false)   void setMultimode(float _multimode);        // 0..1 (when xpander4Pole=false)
    void setFilterTwoPole(bool enabled);
    void setFilterXpander4Pole(bool enabled);
    void setFilterXpanderMode(uint8_t mode);   // 0..14
    void setFilterBPBlend2Pole(bool enabled);
    void setFilterPush2Pole(bool enabled);
    void setFilterResonanceModDepth(float depth01);

    // Getters used by UI/Patch
    float getFilterCutoff() const;
    float getFilterResonance() const;
    float getFilterEnvAmount() const;
    float getFilterKeyTrackAmount() const;
    float getFilterOctaveControl() const;
    float getFilterMultimode() const { return _filterMultimode; }
    bool getFilterTwoPole() const { return _filterUseTwoPole; }
    bool getFilterXpander4Pole() const { return _filterXpander4Pole; }
    uint8_t getFilterXpanderMode() const { return _filterXpanderMode; }
    bool getFilterBPBlend2Pole() const { return _filterBpBlend2Pole; } 
    bool getFilterPush2Pole() const { return _filterPush2Pole; }
    float getFilterResonanceModDepth() const { return _filterResonaceModDepth; }   

    float getAmpAttack() const;
    float getAmpDecay() const;
    float getAmpSustain() const;
    float getAmpRelease() const;

    float getFilterEnvAttack() const;
    float getFilterEnvDecay() const;
    float getFilterEnvSustain() const;
    float getFilterEnvRelease() const;

    // --- JPFX Effects ---
 * // Tone control
   void setFXBassGain(float dB);      // -12..+12 dB
   void setFXTrebleGain(float dB);    // -12..+12 dB
   float getFXBassGain() const;
   float getFXTrebleGain() const;
 *
 * // Modulation effects
   void setFXModEffect(int8_t variation);  // -1=off, 0..10=preset
   void setFXModMix(float mix);            // 0..1
   void setFXModRate(float hz);            // 0..20 Hz (0=use preset)
   void setFXModFeedback(float fb);        // -1=preset, 0..0.99=override
   int8_t getFXModEffect() const;
   float getFXModMix() const;
   float getFXModRate() const;
   float getFXModFeedback() const;
   const char* getFXModEffectName() const;

   // Delay effects
   void setFXDelayEffect(int8_t variation); // -1=off, 0..4=preset
   void setFXDelayMix(float mix);           // 0..1
   void setFXDelayFeedback(float fb);       // -1=preset, 0..0.99=override
   void setFXDelayTime(float ms);           // 0=preset, 5..1500ms=override
   int8_t getFXDelayEffect() const;
   float getFXDelayMix() const;
   float getFXDelayFeedback() const;
   float getFXDelayTime() const;
   const char* getFXDelayEffectName() const;
 
   // Global mix
   void setFXDryMix(float level);  // 0..1
   float getFXDryMix() const;

    // --- UI helpers
    int getOsc1Waveform() const;
    int getOsc2Waveform() const;
    const char* getOsc1WaveformName() const;
    const char* getOsc2WaveformName() const;

    // Mix getters for presets/UI
    float getSupersawDetune(uint8_t oscIndex) const; // 0..1
    float getSupersawMix(uint8_t oscIndex) const;    // 0..1
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
    float getRing1Mix() const;
    float getRing2Mix() const;
    float getOsc1FrequencyDc() const;
    float getOsc2FrequencyDc() const;
    float getOsc1ShapeDc() const;
    float getOsc2ShapeDc() const;

    bool  getGlideEnabled() const;
    float getGlideTimeMs() const;

    // --- MIDI
    void handleControlChange(byte channel, byte control, byte value);

    using NotifyFn = void(*)(uint8_t cc, uint8_t val);
    void setNotifier(NotifyFn fn);

    // --- Outputs
    AudioMixer4& getVoiceMixer() { return _voiceMixer; }
    AudioMixer4& getFXOutL()     { return _fxChain.getOutputLeft(); }
    AudioMixer4& getFXOutR()     { return _fxChain.getOutputRight(); }

private:
    // Voices / routing
    VoiceBlock _voices[MAX_VOICES];
    bool  _activeNotes[MAX_VOICES];
    byte  _noteToVoice[128];
    uint32_t _noteTimestamps[MAX_VOICES];
    uint32_t _clock = 0;

    // Global modulation
    LFOBlock _lfo1;
    LFOBlock _lfo2;

    // Amp mod DC + mix
    float _ampModFixedLevel = 1.0f;
    AudioSynthWaveformDc _ampModFixedDc;
    AudioSynthWaveformDc _ampModLimitFixedDc;
    AudioEffectMultiply  _ampMultiply;
    AudioMixer4          _ampModMixer;
    AudioMixer4          _ampModLimiterMixer;

    // Voice mix
    AudioMixer4 _voiceMixer;

    // FX
    FXChainBlock _fxChain;

    // Audio connections (allocated in ctor)
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

    // --- Cached parameter state for UI/presets readback (kept in sync by setters)
    // Osc/waves
    int   _osc1Wave = 0, _osc2Wave = 0;
    float _osc1PitchSemi = 0.0f, _osc2PitchSemi = 0.0f;
    float _osc1DetuneSemi = 0.0f, _osc2DetuneSemi = 0.0f;
    float _osc1FineCents = 0.0f, _osc2FineCents = 0.0f;
    float _osc1Mix = 1.0f, _osc2Mix = 1.0f, _subMix = 0.0f, _noiseMix = 0.0f;
    float _ring1Mix = 0.0f, _ring2Mix = 0.0f;
    float _supersawDetune[2] = {0.0f, 0.0f};
    float _supersawMix   [2] = {0.0f, 0.0f};
    float _osc1FreqDc = 0.0f, _osc2FreqDc = 0.0f;
    float _osc1ShapeDc = 0.0f, _osc2ShapeDc = 0.0f;

    // LFO state mirrors
    float _lfo1Frequency = 0.0f, _lfo2Frequency = 0.0f;
    float _lfo1Amount = 0.0f,    _lfo2Amount = 0.0f;
    int   _lfo1Type = 0,         _lfo2Type = 0;
    LFODestination _lfo1Dest = (LFODestination)0;
    LFODestination _lfo2Dest = (LFODestination)0;

    // Filter/env cached
    float _filterCutoffHz = 20000.0f;
    float _filterResonance = 0.0f;
    float _filterEnvAmount = 0.0f;     // -1..+1
    float _filterKeyTrack = 0.0f;      // -1..+1
    float _filterOctaves = 0.0f;
    float _filterMultimode = 0.0f;
    
    bool    _filterUseTwoPole   = false;
    bool    _filterXpander4Pole = false;
    uint8_t _filterXpanderMode  = 0;
    bool    _filterBpBlend2Pole = false;
    bool    _filterPush2Pole    = false;
    float _filterResModDepth = 0.0f;
    float _filterResonaceModDepth = 0.0f;

    // Glide
    bool  _glideEnabled = false;
    float _glideTimeMs = 0.0f;
    float _lastNoteFreq = 0.0f;

    // --- Arbitrary waveform selection state per oscillator ---
    // These store the current AKWF bank and table index for each oscillator.  They
    // are used by setOsc1ArbBank()/setOsc2ArbBank() and setOsc1ArbIndex()/
    // setOsc2ArbIndex() to propagate to each voice.
    ArbBank  _osc1ArbBank = ArbBank::BwBlended;
    ArbBank  _osc2ArbBank = ArbBank::BwBlended;
    uint16_t _osc1ArbIndex = 0;
    uint16_t _osc2ArbIndex = 0;

   // Cached JPFX state for UI/preset readback
   float _fxBassGain = 0.0f;
   float _fxTrebleGain = 0.0f;
   int8_t _fxModEffect = -1;
   float _fxModMix = 0.5f;
   float _fxModRate = 0.0f;
   float _fxModFeedback = -1.0f;
   int8_t _fxDelayEffect = -1;
   float _fxDelayMix = 0.5f;
   float _fxDelayFeedback = -1.0f;
   float _fxDelayTime = 0.0f;
   float _fxDryMix = 1.0f;

    NotifyFn _notify = nullptr;
};
