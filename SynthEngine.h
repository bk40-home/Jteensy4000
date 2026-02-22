#pragma once
// SynthEngine.h — 8-voice polyphonic synthesizer engine
// Mixer topology: Voices 0-3 → MixerA, Voices 4-7 → MixerB, A+B → MixerFinal
// CPU target: < 80% @ 44.1 kHz on Teensy 4.1

#include <Arduino.h>
#include "VoiceBlock.h"
#include "LFOBlock.h"
#include "FXChainBlock.h"
#include "Mapping.h"
#include "Waveforms.h"
#include "DebugTrace.h"
#include "AKWF_All.h"
#include "BPMClockManager.h"

using namespace JT4000Map;

#define MAX_VOICES 8   // 8-voice polyphony

class SynthEngine {
public:
    // =========================================================================
    // Lifecycle
    // =========================================================================
    SynthEngine();
    void noteOn(byte note, float velocity);
    void noteOff(byte note);
    void update();

    static constexpr uint8_t VOICE_NONE = 255;  // Sentinel: no voice assigned

    // =========================================================================
    // CC state cache — raw MIDI 0-127 values
    // =========================================================================
    // Filled by handleControlChange(); lets the UI read back any CC value
    // without needing typed getters for every parameter.
    // Not valid until the first handleControlChange() call for that CC.

    // Returns the last raw CC value received (0-127), or 0 if never set.
    inline uint8_t getCC(uint8_t cc) const {
        return _ccState[cc];
    }

    // Dispatches a CC as if received from MIDI. Also updates _ccState.
    // Use this from the UI encoder/touch handlers.
    inline void setCC(uint8_t cc, uint8_t value) {
        handleControlChange(1, cc, value);
    }

    // =========================================================================
    // Oscillator control
    // =========================================================================
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
    void setSubMix(float mix);
    void setNoiseMix(float mix);
    void setSupersawDetune(uint8_t oscIndex, float amount);
    void setSupersawMix(uint8_t oscIndex, float amount);
    void setOsc1FrequencyDcAmp(float amp);
    void setOsc2FrequencyDcAmp(float amp);
    void setOsc1ShapeDcAmp(float amp);
    void setOsc2ShapeDcAmp(float amp);
    void setRing1Mix(float level);
    void setRing2Mix(float level);
    void setOsc1FeedbackAmount(float amount);
    void setOsc2FeedbackAmount(float amount);
    void setOsc1FeedbackMix(float mix);
    void setOsc2FeedbackMix(float mix);

    float getOsc1FeedbackAmount()  const;
    float getOsc2FeedbackAmount()  const;
    float getOsc1FeedbackMix()     const;
    float getOsc2FeedbackMix()     const;

    // =========================================================================
    // Arbitrary waveform (AKWF bank) selection
    // =========================================================================
    void setOsc1ArbBank(ArbBank b);
    void setOsc2ArbBank(ArbBank b);
    void setOsc1ArbIndex(uint16_t idx);
    void setOsc2ArbIndex(uint16_t idx);
    ArbBank  getOsc1ArbBank()  const { return _osc1ArbBank; }
    ArbBank  getOsc2ArbBank()  const { return _osc2ArbBank; }
    uint16_t getOsc1ArbIndex() const { return _osc1ArbIndex; }
    uint16_t getOsc2ArbIndex() const { return _osc2ArbIndex; }

    // =========================================================================
    // Amp modulation DC offset
    // =========================================================================
    void  SetAmpModFixedLevel(float level);
    float GetAmpModFixedLevel() const;
    float getAmpModFixedLevel() const;   // alias

    // =========================================================================
    // LFO 1
    // =========================================================================
    void   setLFO1Frequency(float hz);
    void   setLFO1Amount(float amt);
    void   setLFO1Waveform(int type);
    void   setLFO1Destination(LFODestination dest);
    float  getLFO1Frequency()    const;
    float  getLFO1Amount()       const;
    int    getLFO1Waveform()     const;
    LFODestination getLFO1Destination() const;
    const char* getLFO1WaveformName()    const;
    const char* getLFO1DestinationName() const;

    // =========================================================================
    // LFO 2
    // =========================================================================
    void   setLFO2Frequency(float hz);
    void   setLFO2Amount(float amt);
    void   setLFO2Waveform(int type);
    void   setLFO2Destination(LFODestination dest);
    float  getLFO2Frequency()    const;
    float  getLFO2Amount()       const;
    int    getLFO2Waveform()     const;
    LFODestination getLFO2Destination() const;
    const char* getLFO2WaveformName()    const;
    const char* getLFO2DestinationName() const;

    // =========================================================================
    // Filter
    // =========================================================================
    void setFilterEnvAmount(float amt);
    void setFilterCutoff(float value);
    void setFilterResonance(float value);
    void setFilterKeyTrackAmount(float amt);
    void setFilterOctaveControl(float octaves);
    void setFilterMultimode(float multimode);
    void setFilterTwoPole(bool enabled);
    void setFilterXpander4Pole(bool enabled);
    void setFilterXpanderMode(uint8_t mode);
    void setFilterBPBlend2Pole(bool enabled);
    void setFilterPush2Pole(bool enabled);
    void setFilterResonanceModDepth(float depth01);

    float   getFilterCutoff()          const;
    float   getFilterResonance()       const;
    float   getFilterEnvAmount()       const;
    float   getFilterKeyTrackAmount()  const;
    float   getFilterOctaveControl()   const;
    float   getFilterMultimode()       const { return _filterMultimode; }
    bool    getFilterTwoPole()         const { return _filterUseTwoPole; }
    bool    getFilterXpander4Pole()    const { return _filterXpander4Pole; }
    uint8_t getFilterXpanderMode()     const { return _filterXpanderMode; }
    bool    getFilterBPBlend2Pole()    const { return _filterBpBlend2Pole; }
    bool    getFilterPush2Pole()       const { return _filterPush2Pole; }
    float   getFilterResonanceModDepth() const { return _filterResonaceModDepth; }

    // =========================================================================
    // Envelopes
    // =========================================================================
    float getAmpAttack()         const;
    float getAmpDecay()          const;
    float getAmpSustain()        const;
    float getAmpRelease()        const;
    float getFilterEnvAttack()   const;
    float getFilterEnvDecay()    const;
    float getFilterEnvSustain()  const;
    float getFilterEnvRelease()  const;

    // =========================================================================
    // JPFX Effects — Tone
    // =========================================================================
    void   setFXBassGain(float dB);
    void   setFXTrebleGain(float dB);
    float  getFXBassGain()   const;
    float  getFXTrebleGain() const;

    // =========================================================================
    // JPFX Effects — Modulation (chorus/flange/phase)
    // =========================================================================
    void   setFXModEffect(int8_t variation);
    void   setFXModMix(float mix);
    void   setFXModRate(float hz);
    void   setFXModFeedback(float fb);
    int8_t getFXModEffect()   const;
    float  getFXModMix()      const;
    float  getFXModRate()     const;
    float  getFXModFeedback() const;
    const char* getFXModEffectName() const;

    // =========================================================================
    // JPFX Effects — Delay
    // =========================================================================
    void   setFXDelayEffect(int8_t variation);
    void   setFXDelayMix(float mix);
    void   setFXDelayFeedback(float fb);
    void   setFXDelayTime(float ms);
    int8_t getFXDelayEffect()   const;
    float  getFXDelayMix()      const;
    float  getFXDelayFeedback() const;
    float  getFXDelayTime()     const;
    const char* getFXDelayEffectName() const;

    // =========================================================================
    // Reverb (hexefx)
    // =========================================================================
    void  setFXReverbRoomSize(float size);
    void  setFXReverbHiDamping(float damp);
    void  setFXReverbLoDamping(float damp);
    float getFXReverbRoomSize()   const;
    float getFXReverbHiDamping()  const;
    float getFXReverbLoDamping()  const;

    // Bypass reverb to save CPU when not needed
    void setFXReverbBypass(bool bypass);
    bool getFXReverbBypass()      const;

    // =========================================================================
    // Output mix levels
    // =========================================================================
    void  setFXDryMix(float level);
    void  setFXJPFXMix(float left, float right);
    void  setFXReverbMix(float left, float right);

    float getFXDryMix()     const;
    float getFXJPFXMixL()   const;
    float getFXJPFXMixR()   const;
    float getFXReverbMixL() const;
    float getFXReverbMixR() const;

    // =========================================================================
    // UI helpers — typed getters for display formatting
    // =========================================================================
    int  getOsc1Waveform() const;
    int  getOsc2Waveform() const;
    const char* getOsc1WaveformName() const;
    const char* getOsc2WaveformName() const;

    float getSupersawDetune(uint8_t oscIndex) const;
    float getSupersawMix(uint8_t oscIndex)    const;
    float getOsc1PitchOffset() const;
    float getOsc2PitchOffset() const;
    float getOsc1Detune()      const;
    float getOsc2Detune()      const;
    float getOsc1FineTune()    const;
    float getOsc2FineTune()    const;
    float getOscMix1()         const;
    float getOscMix2()         const;
    float getSubMix()          const;
    float getNoiseMix()        const;
    float getRing1Mix()        const;
    float getRing2Mix()        const;
    float getOsc1FrequencyDc() const;
    float getOsc2FrequencyDc() const;
    float getOsc1ShapeDc()     const;
    float getOsc2ShapeDc()     const;

    bool  getGlideEnabled() const;
    float getGlideTimeMs()  const;

    // =========================================================================
    // MIDI
    // =========================================================================
    void handleControlChange(byte channel, byte control, byte value);

    // Callback fired after every CC is processed; UI uses this to stay in sync
    using NotifyFn = void(*)(uint8_t cc, uint8_t val);
    void setNotifier(NotifyFn fn);

    // =========================================================================
    // Audio graph outputs
    // =========================================================================
    AudioMixer4& getVoiceMixer() { return _voiceMixerFinal; }
    AudioMixer4& getFXOutL()     { return _fxChain.getOutputLeft(); }
    AudioMixer4& getFXOutR()     { return _fxChain.getOutputRight(); }

    // =========================================================================
    // BPM clock sync
    // =========================================================================
    void setBPMClock(BPMClockManager* clock);
    void updateBPMSync();   // Called from update() to refresh synced params

    void       setLFO1TimingMode(TimingMode mode);
    void       setLFO2TimingMode(TimingMode mode);
    TimingMode getLFO1TimingMode() const;
    TimingMode getLFO2TimingMode() const;

    void       setDelayTimingMode(TimingMode mode);
    TimingMode getDelayTimingMode() const;

private:
    // =========================================================================
    // 8-voice audio architecture
    //
    //   Voices 0-3 → _voiceMixerA  \
    //                                → _voiceMixerFinal → FX chain
    //   Voices 4-7 → _voiceMixerB  /
    //
    // Each voice contributes 1/8 of full scale.
    // CPU @ 44.1 kHz: ~30-40% for voices, leaves headroom for FX.
    // RAM: 8 × VoiceBlock (~8 KB each) = ~64 KB.
    // =========================================================================

    VoiceBlock  _voices[MAX_VOICES];
    bool        _activeNotes[MAX_VOICES];
    byte        _noteToVoice[128];          // note# → voice index lookup
    uint32_t    _noteTimestamps[MAX_VOICES]; // for LRU voice stealing
    uint32_t    _clock = 0;                  // monotonic event counter

    // -------------------------------------------------------------------------
    // Global modulation sources
    // -------------------------------------------------------------------------
    LFOBlock _lfo1;
    LFOBlock _lfo2;

    // Amp envelope × LFO multiply chain
    float                _ampModFixedLevel = 1.0f;
    AudioSynthWaveformDc _ampModFixedDc;
    AudioSynthWaveformDc _ampModLimitFixedDc;
    AudioEffectMultiply  _ampMultiply;
    AudioMixer4          _ampModMixer;       // Fixed DC + LFO1 + LFO2
    AudioMixer4          _ampModLimiterMixer;

    // -------------------------------------------------------------------------
    // Voice mixing — three-stage architecture
    // -------------------------------------------------------------------------
    AudioMixer4 _voiceMixerA;      // Voices 0-3
    AudioMixer4 _voiceMixerB;      // Voices 4-7
    AudioMixer4 _voiceMixerFinal;  // A + B → FX chain

    // -------------------------------------------------------------------------
    // FX chain
    // -------------------------------------------------------------------------
    FXChainBlock _fxChain;

    // -------------------------------------------------------------------------
    // Audio patch cables (heap-allocated, persistent)
    // -------------------------------------------------------------------------
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
    AudioConnection* _fxPatchInL;    // Amp multiply → JPFX left input
    AudioConnection* _fxPatchInR;    // Amp multiply → JPFX right input
    AudioConnection* _fxPatchDryL;   // Amp multiply → dry mixer left
    AudioConnection* _fxPatchDryR;   // Amp multiply → dry mixer right

    AudioConnection* _patchMixerAToFinal;  // Sub-mixer A → final
    AudioConnection* _patchMixerBToFinal;  // Sub-mixer B → final

    // =========================================================================
    // Cached synthesis parameters (typed, for UI getters)
    // =========================================================================

    // Oscillator
    int   _osc1Wave = 0,  _osc2Wave = 0;
    float _osc1PitchSemi = 0.0f, _osc2PitchSemi = 0.0f;
    float _osc1DetuneSemi = 0.0f, _osc2DetuneSemi = 0.0f;
    float _osc1FineCents = 0.0f,  _osc2FineCents = 0.0f;
    float _osc1Mix = 1.0f,  _osc2Mix = 1.0f;
    float _subMix = 0.0f,   _noiseMix = 0.0f;
    float _ring1Mix = 0.0f, _ring2Mix = 0.0f;
    float _supersawDetune[2] = {0.0f, 0.0f};
    float _supersawMix[2]    = {0.0f, 0.0f};
    float _osc1FreqDc = 0.0f,  _osc2FreqDc = 0.0f;
    float _osc1ShapeDc = 0.0f, _osc2ShapeDc = 0.0f;
    float _osc1FeedbackAmount = 0.0f, _osc2FeedbackAmount = 0.0f;
    float _osc1FeedbackMix    = 0.0f, _osc2FeedbackMix    = 0.0f;

    // LFO mirrors
    float _lfo1Frequency = 0.0f, _lfo2Frequency = 0.0f;
    float _lfo1Amount    = 0.0f, _lfo2Amount    = 0.0f;
    int   _lfo1Type = 0,         _lfo2Type = 0;
    LFODestination _lfo1Dest = (LFODestination)0;
    LFODestination _lfo2Dest = (LFODestination)0;

    // Filter
    float   _filterCutoffHz   = 20000.0f;
    float   _filterResonance  = 0.0f;
    float   _filterEnvAmount  = 0.0f;
    float   _filterKeyTrack   = 0.0f;
    float   _filterOctaves    = 0.0f;
    float   _filterMultimode  = 0.0f;
    bool    _filterUseTwoPole   = false;
    bool    _filterXpander4Pole = false;
    uint8_t _filterXpanderMode  = 0;
    bool    _filterBpBlend2Pole = false;
    bool    _filterPush2Pole    = false;
    float   _filterResModDepth    = 0.0f;
    float   _filterResonaceModDepth = 0.0f;  // note: intentional spelling kept for ABI

    // Glide
    bool  _glideEnabled = false;
    float _glideTimeMs  = 0.0f;
    float _lastNoteFreq = 0.0f;

    // Arbitrary waveform selection
    ArbBank  _osc1ArbBank  = ArbBank::BwBlended;
    ArbBank  _osc2ArbBank  = ArbBank::BwBlended;
    uint16_t _osc1ArbIndex = 0;
    uint16_t _osc2ArbIndex = 0;

    // JPFX cached parameters
    float  _fxBassGain       = 0.0f;
    float  _fxTrebleGain     = 0.0f;
    int8_t _fxModEffect      = -1;
    float  _fxModMix         = 0.5f;
    float  _fxModRate        = 0.0f;
    float  _fxModFeedback    = -1.0f;
    int8_t _fxDelayEffect    = -1;
    float  _fxDelayMix       = 0.5f;
    float  _fxDelayFeedback  = -1.0f;
    float  _fxDelayTime      = 0.0f;
    float  _fxDryMix         = 1.0f;
    float  _fxReverbRoomSize = 0.5f;
    float  _fxReverbHiDamp   = 0.5f;
    float  _fxReverbLoDamp   = 0.5f;
    float  _fxJPFXMixL       = 0.0f;
    float  _fxJPFXMixR       = 0.0f;
    float  _fxReverbMixL     = 0.0f;
    float  _fxReverbMixR     = 0.0f;

    // =========================================================================
    // Raw CC state cache — populated by handleControlChange()
    //   getCC(cc) reads from here; avoids needing per-parameter typed getters
    //   in the UI layer. Zero-initialized; only valid after first CC receive.
    // =========================================================================
    uint8_t _ccState[128] = {};

    // =========================================================================
    // BPM / timing
    // =========================================================================
    BPMClockManager* _bpmClock = nullptr;  // Pointer to global clock (not owned)

    // =========================================================================
    // UI notifier callback
    // =========================================================================
    NotifyFn _notify = nullptr;
};