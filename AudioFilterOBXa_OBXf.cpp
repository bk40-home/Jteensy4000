#include "AudioFilterOBXa.h"

// Keep math constants local
static constexpr float OBXA_PI = 3.14159265358979323846f;
static constexpr int   OBXA_NUM_XPANDER_MODES = 15;

// 1-pole TPT helper
static inline float obxa_tpt_process(float &state, float input, float g)
{
    float v = (input - state) * g;
    float y = v + state;
    state = y + v;
    return y;
}

static inline bool obxa_is_huge(float x)
{
    return fabsf(x) > OBXA_HUGE_THRESHOLD;
}

// -----------------------------------------------------------------------------
// Core implementation (kept out of header to reduce include/ODR issues)
// -----------------------------------------------------------------------------
struct AudioFilterOBXa::Core
{
    // Pole mix table for Xpander modes (4-pole only)
    static constexpr float poleMixFactors[OBXA_NUM_XPANDER_MODES][5] =
    {
        {0,  0,  0,  0,  1}, // 0: LP4
        {0,  0,  0,  1,  0}, // 1: LP3
        {0,  0,  1,  0,  0}, // 2: LP2
        {0,  1,  0,  0,  0}, // 3: LP1
        {1, -3,  3, -1,  0}, // 4: HP3
        {1, -2,  1,  0,  0}, // 5: HP2
        {1, -1,  0,  0,  0}, // 6: HP1
        {0,  0,  2, -4,  2}, // 7: BP4
        {0, -2,  2,  0,  0}, // 8: BP2
        {1, -2,  2,  0,  0}, // 9: N2
        {1, -3,  6, -4,  0}, // 10: PH3
        {0, -1,  2, -1,  0}, // 11: HP2+LP1
        {0, -1,  3, -3,  1}, // 12: HP3+LP1
        {0, -1,  2, -2,  0}, // 13: N2+LP1
        {0, -1,  3, -6,  4}, // 14: PH3+LP1
    };

    struct State
    {
        float pole1{0.f}, pole2{0.f}, pole3{0.f}, pole4{0.f};
        float res2Pole{1.f};
        float res4Pole{0.f};
        float resCorrection{1.f};
        float resCorrectionInv{1.f};
        float multimodeXfade{0.f};
        int   multimodePole{0};
    } st;

    float fs{AUDIO_SAMPLE_RATE_EXACT};
    float fsInv{1.f / AUDIO_SAMPLE_RATE_EXACT};

    // Parameters mirrored from wrapper
    bool bpBlend2Pole{false};
    bool push2Pole{false};
    bool xpander4Pole{false};
    uint8_t xpanderMode{0};
    float multimode01{0.f};

#if OBXA_DEBUG
    struct CoreDebugEvent
    {
        uint32_t ms = 0;
        float fs = 0.0f;
        float cutoffHz = 0.0f;
        float resonance01 = 0.0f;
        float x = 0.0f;

        float g = 0.0f;
        float lpc = 0.0f;
        float res4Pole = 0.0f;
        float S = 0.0f;
        float G = 0.0f;
        float denom = 0.0f;

        float y0 = 0.0f, y1 = 0.0f, y2 = 0.0f, y3 = 0.0f, y4 = 0.0f;
        float out = 0.0f;

        float pole1 = 0.0f, pole2 = 0.0f, pole3 = 0.0f, pole4 = 0.0f;

        uint8_t reason = 0;
        uint8_t stage  = 0;
    };

    // Pre-event snapshot (cheap)
    struct PreEvent
    {
        uint32_t ms = 0;
        float x = 0.0f;
        float cutoffHz = 0.0f;
        float resonance01 = 0.0f;
        float g = 0.0f;
        float lpc = 0.0f;
        float p1 = 0.0f, p2 = 0.0f, p3 = 0.0f, p4 = 0.0f;
    };

    static constexpr int PRE_CAP = 64;
    PreEvent pre[PRE_CAP]{};
    uint8_t  preHead = 0;
    uint8_t  preDecim = 0;       // count down
    uint8_t  preDecimN = 2;      // snapshot every N samples

    // Fault latch in core: we only forward one event per "fault burst"
    bool faultLatched = false;
    uint16_t faultClearCountdown = 0; // needs N good samples to clear

    // Hook (wrapper thunk)
    void (*hook)(void *ctx, const void *evt) = nullptr;
    void *hookCtx = nullptr;

    float dbgRes01 = 0.0f;

    void setHook(void (*h)(void*, const void*), void *ctx) { hook = h; hookCtx = ctx; }

    void pushPre(float x, float cutoffHz, float g, float lpc)
    {
        if (preDecim++ < preDecimN) return;
        preDecim = 0;

        PreEvent &p = pre[preHead];
        p.ms = millis();
        p.x = x;
        p.cutoffHz = cutoffHz;
        p.resonance01 = dbgRes01;
        p.g = g;
        p.lpc = lpc;
        p.p1 = st.pole1; p.p2 = st.pole2; p.p3 = st.pole3; p.p4 = st.pole4;

        preHead = (uint8_t)((preHead + 1) % PRE_CAP);
    }

    bool shouldEmit(bool faultNow)
    {
        // rising-edge latch with "good sample" hysteresis to avoid spam
        if (faultNow)
        {
            faultClearCountdown = 64; // require 64 good samples to clear
            if (!faultLatched)
            {
                faultLatched = true;
                return true;
            }
            return false;
        }
        else
        {
            if (faultClearCountdown > 0) faultClearCountdown--;
            if (faultClearCountdown == 0) faultLatched = false;
            return false;
        }
    }

    void emit(uint8_t stage, uint8_t reason,
              float cutoffHzRequested, float x,
              float g, float lpc,
              float y0, float y1, float y2, float y3, float y4, float out)
    {
        if (!hook) return;

        float ml = 1.f / (1.f + g);
        float S =
            (lpc * (lpc * (lpc * st.pole1 + st.pole2) + st.pole3) + st.pole4) * ml;
        float G = lpc * lpc * lpc * lpc;
        float denom = 1.f + st.res4Pole * G;

        CoreDebugEvent e;
        e.ms = millis();
        e.fs = fs;
        e.cutoffHz = cutoffHzRequested;
        e.resonance01 = dbgRes01;
        e.x = x;

        e.g = g;
        e.lpc = lpc;
        e.res4Pole = st.res4Pole;
        e.S = S;
        e.G = G;
        e.denom = denom;

        e.y0 = y0; e.y1 = y1; e.y2 = y2; e.y3 = y3; e.y4 = y4;
        e.out = out;

        e.pole1 = st.pole1; e.pole2 = st.pole2; e.pole3 = st.pole3; e.pole4 = st.pole4;

        e.reason = reason;
        e.stage = stage;

        hook(hookCtx, &e);
    }
#endif // OBXA_DEBUG

    void reset()
    {
        st.pole1 = st.pole2 = st.pole3 = st.pole4 = 0.f;
#if OBXA_DEBUG
        // also clear pre buffer index (optional)
        // preHead = 0;
#endif
    }

    void setSampleRate(float sr)
    {
        fs = sr;
        fsInv = 1.f / fs;
        float rcRate = sqrtf(44000.0f / fs);
        st.resCorrection = (970.f / 44000.f) * rcRate;
        st.resCorrectionInv = 1.f / st.resCorrection;
    }

    void setResonance(float r01)
    {
        st.res2Pole = 1.f - r01;
        st.res4Pole = 3.5f * r01;
#if OBXA_DEBUG
        dbgRes01 = r01;
#endif
    }

    void setMultimode(float m01)
    {
        multimode01 = m01;
        st.multimodePole = (int)(multimode01 * 3.0f);
        st.multimodeXfade = (multimode01 * 3.0f) - st.multimodePole;
    }

    inline float diodePairResistanceApprox(float x)
    {
        return (((((0.0103592f) * x + 0.00920833f) * x + 0.185f) * x + 0.05f) * x + 1.f);
    }

    inline float resolveFeedback2Pole(float sample, float g)
    {
        float push = -1.f - (push2Pole ? 0.035f : 0.0f);
        float tCfb = diodePairResistanceApprox(st.pole1 * 0.0876f) + push;

        float y = (sample
                   - 2.f * (st.pole1 * (st.res2Pole + tCfb))
                   - g * st.pole1
                   - st.pole2)
                  /
                  (1.f + g * (2.f * (st.res2Pole + tCfb) + g));

        return y;
    }

    float process2Pole(float x, float cutoffHz)
    {
        float g = tanf(cutoffHz * fsInv * OBXA_PI);
        float v = resolveFeedback2Pole(x, g);

        float y1 = v * g + st.pole1;
        st.pole1 = v * g + y1;

        float y2 = y1 * g + st.pole2;
        st.pole2 = y1 * g + y2;

        float out;
        if (bpBlend2Pole)
        {
            if (multimode01 < 0.5f) out = 2.f * ((0.5f - multimode01) * y2 + (multimode01 * y1));
            else                    out = 2.f * ((1.f - multimode01) * y1 + (multimode01 - 0.5f) * v);
        }
        else
        {
            out = (1.f - multimode01) * y2 + multimode01 * v;
        }
        return out;
    }

    inline float resolveFeedback4Pole(float sample, float g, float lpc)
    {
        float ml = 1.f / (1.f + g);
        float S =
            (lpc * (lpc * (lpc * st.pole1 + st.pole2) + st.pole3) + st.pole4) * ml;
        float G = lpc * lpc * lpc * lpc;

        float y = (sample - st.res4Pole * S) / (1.f + st.res4Pole * G);
        return y;
    }

    float process4Pole(float x, float cutoffHz)
    {
        float cutoffHzRequested = cutoffHz;

        // Prewarp
        float g = tanf(cutoffHz * fsInv * OBXA_PI);
        float lpc = g / (1.f + g);

#if OBXA_DEBUG
        pushPre(x, cutoffHzRequested, g, lpc);
#endif

        float y0 = resolveFeedback4Pole(x, g, lpc);

        // Inline first pole with nonlinearity
        double v = (y0 - st.pole1) * lpc;
        double res = v + st.pole1;
        st.pole1 = (float)(res + v);

        st.pole1 = atanf(st.pole1 * st.resCorrection) * st.resCorrectionInv;

        float y1 = (float)res;
        float y2 = obxa_tpt_process(st.pole2, y1, g);
        float y3 = obxa_tpt_process(st.pole3, y2, g);
        float y4 = obxa_tpt_process(st.pole4, y3, g);

        float out = 0.f;

        if (xpander4Pole)
        {
            const float *m = poleMixFactors[xpanderMode];
            out = y0 * m[0] + y1 * m[1] + y2 * m[2] + y3 * m[3] + y4 * m[4];
        }
        else
        {
            switch (st.multimodePole)
            {
            case 0: out = (1.f - st.multimodeXfade) * y4 + st.multimodeXfade * y3; break;
            case 1: out = (1.f - st.multimodeXfade) * y3 + st.multimodeXfade * y2; break;
            case 2: out = (1.f - st.multimodeXfade) * y2 + st.multimodeXfade * y1; break;
            case 3: out = y1; break;
            default: out = 0.f; break;
            }
        }

        // Detect anomalies
        bool nonfinite =
            (!isfinite(y0) || !isfinite(y1) || !isfinite(y2) || !isfinite(y3) || !isfinite(y4) || !isfinite(out) ||
             !isfinite(st.pole1) || !isfinite(st.pole2) || !isfinite(st.pole3) || !isfinite(st.pole4) ||
             !isfinite(g) || !isfinite(lpc));

        bool huge =
            (obxa_is_huge(st.pole1) || obxa_is_huge(st.pole2) ||
             obxa_is_huge(st.pole3) || obxa_is_huge(st.pole4) ||
             obxa_is_huge(out) || obxa_is_huge(y0));

        float G = lpc * lpc * lpc * lpc;
        float denom = 1.f + st.res4Pole * G;
        bool denomSmall = (fabsf(denom) < 1.0e-6f);

#if OBXA_DEBUG
        bool faultNow = nonfinite || huge || denomSmall;
        if (shouldEmit(faultNow))
        {
            uint8_t reason = nonfinite ? 1 : (huge ? 2 : 3);
            emit(20, reason, cutoffHzRequested, x, g, lpc, y0, y1, y2, y3, y4, out);
        }
#endif

        // Resonance-dependent volume compensation
        return out * (1.f + st.res4Pole * 0.45f);
    }
};

constexpr float AudioFilterOBXa::Core::poleMixFactors[OBXA_NUM_XPANDER_MODES][5];

// -----------------------------------------------------------------------------
// Wrapper implementation
// -----------------------------------------------------------------------------
AudioFilterOBXa::AudioFilterOBXa()
    : AudioStream(3, _inQ)
{
    _core = new Core();
    _core->setSampleRate(AUDIO_SAMPLE_RATE_EXACT);
    _core->setResonance(_res01Target);
    _core->setMultimode(_multimode01);

#if OBXA_DEBUG
    _core->setHook(&AudioFilterOBXa::coreHookThunk, this);
#endif
}

void AudioFilterOBXa::frequency(float hz)
{
    // allow nearly to Nyquist, but keep stable margin
    float maxHz = 0.24f * AUDIO_SAMPLE_RATE_EXACT;
    if (hz < 5.0f) hz = 5.0f;
    if (hz > maxHz) hz = maxHz;
    _cutoffHzTarget = hz;
}

void AudioFilterOBXa::resonance(float r01)
{
    if (r01 < 0.f) r01 = 0.f;
    if (r01 > 1.f) r01 = 1.f;
    _res01Target = r01;
    _core->setResonance(r01);
}

void AudioFilterOBXa::multimode(float m01)
{
    if (m01 < 0.f) m01 = 0.f;
    if (m01 > 1.f) m01 = 1.f;
    _multimode01 = m01;
    _core->setMultimode(m01);
}

void AudioFilterOBXa::setTwoPole(bool enabled)
{
    _useTwoPole = enabled;
}

void AudioFilterOBXa::setXpander4Pole(bool enabled)
{
    _xpander4Pole = enabled;
    _core->xpander4Pole = enabled;
}

void AudioFilterOBXa::setXpanderMode(uint8_t mode)
{
    if (mode >= OBXA_NUM_XPANDER_MODES) mode = OBXA_NUM_XPANDER_MODES - 1;
    _xpanderMode = mode;
    _core->xpanderMode = mode;
}

void AudioFilterOBXa::setBPBlend2Pole(bool enabled)
{
    _bpBlend2Pole = enabled;
    _core->bpBlend2Pole = enabled;
}

void AudioFilterOBXa::setPush2Pole(bool enabled)
{
    _push2Pole = enabled;
    _core->push2Pole = enabled;
}

void AudioFilterOBXa::setCutoffModOctaves(float oct)
{
    if (oct < 0.f) oct = 0.f;
    if (oct > 8.f) oct = 8.f;
    _cutoffModOct = oct;
}

void AudioFilterOBXa::setResonanceModDepth(float depth01)
{
    if (depth01 < 0.f) depth01 = 0.f;
    if (depth01 > 1.f) depth01 = 1.f;
    _resModDepth = depth01;
}

void AudioFilterOBXa::setKeyTrack(float amount01)
{
    if (amount01 < 0.f) amount01 = 0.f;
    if (amount01 > 1.f) amount01 = 1.f;
    _keyTrack = amount01;
}

void AudioFilterOBXa::setEnvModOctaves(float oct)
{
    if (oct < 0.f) oct = 0.f;
    if (oct > 8.f) oct = 8.f;
    _envModOct = oct;
}

void AudioFilterOBXa::setMidiNote(float note)
{
    if (note < 0.f) note = 0.f;
    if (note > 127.f) note = 127.f;
    _midiNote = note;
}

void AudioFilterOBXa::setEnvValue(float env01)
{
    if (env01 < 0.f) env01 = 0.f;
    if (env01 > 1.f) env01 = 1.f;
    _envValue = env01;
}

#if OBXA_DEBUG
void AudioFilterOBXa::dbgPush(const DebugEvent &e)
{
    uint8_t next = (uint8_t)((_dbgWrite + 1) % DBG_RING_SIZE);
    if (next == _dbgRead)
    {
        _dbgRead = (uint8_t)((_dbgRead + 1) % DBG_RING_SIZE);
    }
    _dbgRing[_dbgWrite] = e;
    _dbgWrite = next;
}

void AudioFilterOBXa::coreHookThunk(void *ctx, const void *evt)
{
    auto *self = (AudioFilterOBXa *)ctx;
    if (self) self->onCoreEvent(evt);
}

void AudioFilterOBXa::onCoreEvent(const void *evt)
{
    const Core::CoreDebugEvent &ce = *(const Core::CoreDebugEvent *)evt;

    // Rising-edge latch at wrapper level too (belt & braces)
    if (_faultLatched) return;
    _faultLatched = true;

    DebugEvent e;
    e.ms = ce.ms;
    e.twoPole = _useTwoPole;

    e.fs = ce.fs;
    e.cutoffHz = ce.cutoffHz;
    e.resonance01 = ce.resonance01;

    e.x = ce.x;
    e.out = ce.out;

    e.g = ce.g;
    e.lpc = ce.lpc;
    e.res4Pole = ce.res4Pole;
    e.denom = ce.denom;
    e.S = ce.S;
    e.G = ce.G;

    e.y0 = ce.y0; e.y1 = ce.y1; e.y2 = ce.y2; e.y3 = ce.y3; e.y4 = ce.y4;

    e.pole1 = ce.pole1; e.pole2 = ce.pole2; e.pole3 = ce.pole3; e.pole4 = ce.pole4;

    e.reason = ce.reason;
    e.stage  = ce.stage;

    dbgPush(e);
}

void AudioFilterOBXa::debugClearFault()
{
    _faultLatched = false;
    if (_core) _core->faultLatched = false;
}

void AudioFilterOBXa::debugFlush(Stream &s)
{
    // Print any fault events that were captured (usually 1)
    while (_dbgRead != _dbgWrite)
    {
        DebugEvent e = _dbgRing[_dbgRead];
        _dbgRead = (uint8_t)((_dbgRead + 1) % DBG_RING_SIZE);

        s.printf("\n[OBXA] t=%lu ms  mode=%s  stage=%u  reason=%u\n",
                 (unsigned long)e.ms, e.twoPole ? "2P" : "4P", e.stage, e.reason);

        s.printf("  fs=%.1f cutoff=%.2f res=%.3f x=%.6f out=%.6f\n",
                 e.fs, e.cutoffHz, e.resonance01, e.x, e.out);

        s.printf("  g=%.6f lpc=%.6f res4=%.6f denom=%.10f S=%.6f G=%.6f\n",
                 e.g, e.lpc, e.res4Pole, e.denom, e.S, e.G);

        s.printf("  poles: p1=%.6f p2=%.6f p3=%.6f p4=%.6f\n",
                 e.pole1, e.pole2, e.pole3, e.pole4);

        s.printf("  y0=%.6f y1=%.6f y2=%.6f y3=%.6f y4=%.6f\n",
                 e.y0, e.y1, e.y2, e.y3, e.y4);

        // Dump pre-events (cheap snapshot ring inside core)
        if (_core)
        {
            s.printf("  --- pre-events (oldest->newest), decim=%u, count=%u ---\n",
                     (unsigned)_core->preDecimN, (unsigned)Core::PRE_CAP);

            for (int i = 0; i < Core::PRE_CAP; ++i)
            {
                const auto &p = _core->pre[(_core->preHead + i) % Core::PRE_CAP];
                s.printf("  pre[%02d] t=%lu x=%.6f cutoff=%.2f res=%.3f g=%.6f lpc=%.6f "
                         "p1=%.6f p2=%.6f p3=%.6f p4=%.6f\n",
                         i, (unsigned long)p.ms, p.x, p.cutoffHz, p.resonance01,
                         p.g, p.lpc, p.p1, p.p2, p.p3, p.p4);
            }
        }
    }

    // Allow new fault to be logged after we've flushed (optional)
    _faultLatched = false;
}
#endif // OBXA_DEBUG

void AudioFilterOBXa::update(void)
{
    audio_block_t *in0 = receiveReadOnly(0);
    if (!in0) return;

    audio_block_t *in1 = receiveReadOnly(1); // cutoff mod bus
    audio_block_t *in2 = receiveReadOnly(2); // resonance mod bus

    audio_block_t *out = allocate();
    if (!out)
    {
        release(in0);
        if (in1) release(in1);
        if (in2) release(in2);
        return;
    }

    // If we recently had a reset, optionally mute a couple blocks to avoid thumps
    if (_cooldownBlocks > 0) _cooldownBlocks--;

    // Precompute keytrack factor (control-rate)
    // note=60 => 1.0; note+12 => x2; note-12 => x0.5
    float keyOct = (_midiNote - 60.0f) / 12.0f;
    float keyMul = powf(2.0f, _keyTrack * keyOct);

    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i)
    {
        float x = (float)in0->data[i] * (1.0f / 32768.0f);

        // Audio-rate mods
        float cutMod = in1 ? ((float)in1->data[i] * (1.0f / 32768.0f)) : 0.0f;  // -1..+1
        float resMod = in2 ? ((float)in2->data[i] * (1.0f / 32768.0f)) : 0.0f;  // -1..+1

        // Convert cutoff mods to a multiplier in octaves:
        float modOct = (cutMod * _cutoffModOct) + (_envValue * _envModOct);
        float modMul = powf(2.0f, modOct);

        float cutoffHz = _cutoffHzTarget * keyMul * modMul;

        // Keep stable
        float maxHz = 0.24f * AUDIO_SAMPLE_RATE_EXACT;
        if (cutoffHz < 5.0f) cutoffHz = 5.0f;
        if (cutoffHz > maxHz) cutoffHz = maxHz;

        // Resonance (0..1) plus optional audio-rate modulation depth
        float r01 = _res01Target + (resMod * _resModDepth);
        if (r01 < 0.0f) r01 = 0.0f;
        if (r01 > 1.0f) r01 = 1.0f;
        _core->setResonance(r01);

        float y = 0.0f;

        if (_cooldownBlocks > 0)
        {
            y = 0.0f;
        }
        else if (_useTwoPole)
        {
            _core->bpBlend2Pole = _bpBlend2Pole;
            _core->push2Pole = _push2Pole;
            y = _core->process2Pole(x, cutoffHz);
        }
        else
        {
            _core->xpander4Pole = _xpander4Pole;
            _core->xpanderMode = _xpanderMode;
            y = _core->process4Pole(x, cutoffHz);
        }

#if OBXA_STATE_GUARD
        // Recovery: if output goes non-finite or runaway, reset and cool down.
        if (!isfinite(y) || obxa_is_huge(y) ||
            obxa_is_huge(_core->st.pole1) || obxa_is_huge(_core->st.pole2) ||
            obxa_is_huge(_core->st.pole3) || obxa_is_huge(_core->st.pole4))
        {
            _core->reset();
            _cooldownBlocks = 2; // mute 2 blocks after reset
            y = 0.0f;
#if OBXA_DEBUG
            // allow a new fault to be captured after recovery
            _faultLatched = false;
#endif
        }
#endif

        if (y > 1.0f) y = 1.0f;
        if (y < -1.0f) y = -1.0f;

        out->data[i] = (int16_t)(y * 32767.0f);
    }

    transmit(out);

    release(out);
    release(in0);
    if (in1) release(in1);
    if (in2) release(in2);
}
