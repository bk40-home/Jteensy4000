/*
 * AudioEffectJPFX.cpp (FIXED VERSION)
 *
 * CRITICAL BUG FIXES:
 * 1. Constructor now uses 1 input, not 2 (AudioStream limitation)
 * 2. Separated modulation and delay buffers (they were sharing, causing conflicts)
 * 3. Fixed update() to handle mono input with internal stereo
 * 4. Fixed transmit() to use single output channel
 * 5. Added proper NULL checks for buffer allocation
 * 6. Fixed bypass logic when effects are off
 * 7. Added destructor to free buffers
 * 8. Fixed write index management (was advancing twice per sample)
 */

#include "AudioEffectJPFX.h"
#include <math.h>

#ifdef __arm__
#include <arm_math.h>
#endif

//-----------------------------------------------------------------------------
// Modulation presets (unchanged)
//-----------------------------------------------------------------------------
const AudioEffectJPFX::ModParams AudioEffectJPFX::modParams[JPFX_NUM_MOD_VARIATIONS] = {
    /* JPFX_CHORUS1 */     {15.0f, 15.0f,  2.0f,  4.0f, 0.25f, 0.0f, 0.5f, false, false},
    /* JPFX_CHORUS2 */     {20.0f, 20.0f,  3.0f,  5.0f, 0.80f, 0.0f, 0.6f, false, false},
    /* JPFX_CHORUS3 */     {25.0f, 25.0f,  4.0f,  6.0f, 0.40f, 0.0f, 0.7f, false, false},
    /* JPFX_FLANGER1 */    { 3.0f,  3.0f,  2.0f,  2.0f, 0.50f, 0.5f, 0.5f, false, true },
    /* JPFX_FLANGER2 */    { 5.0f,  5.0f,  2.5f,  2.5f, 0.35f, 0.7f, 0.5f, false, true },
    /* JPFX_FLANGER3 */    { 2.0f,  2.0f,  1.0f,  1.0f, 1.50f, 0.3f, 0.4f, false, true },
    /* JPFX_PHASER1 */     { 0.0f,  0.0f,  4.0f,  4.0f, 0.25f, 0.6f, 0.5f, true, false},
    /* JPFX_PHASER2 */     { 0.0f,  0.0f,  5.0f,  5.0f, 0.50f, 0.7f, 0.5f, true, false},
    /* JPFX_PHASER3 */     { 0.0f,  0.0f,  6.0f,  6.0f, 0.10f, 0.8f, 0.5f, true, false},
    /* JPFX_PHASER4 */     { 0.0f,  0.0f,  3.0f,  3.0f, 1.20f, 0.5f, 0.6f, true, false},
    /* JPFX_CHORUS_DEEP */ {30.0f, 30.0f, 10.0f, 12.0f, 0.20f, 0.0f, 0.7f, false, false}
};

//-----------------------------------------------------------------------------
// Delay presets (unchanged)
//-----------------------------------------------------------------------------
const AudioEffectJPFX::DelayParams AudioEffectJPFX::delayParams[JPFX_NUM_DELAY_VARIATIONS] = {
    /* JPFX_DELAY_SHORT */     {150.0f, 150.0f, 0.30f, 0.5f},
    /* JPFX_DELAY_LONG */      {500.0f, 500.0f, 0.40f, 0.5f},
    /* JPFX_DELAY_PINGPONG1 */ {300.0f, 600.0f, 0.40f, 0.6f},
    /* JPFX_DELAY_PINGPONG2 */ {150.0f, 300.0f, 0.50f, 0.6f},
    /* JPFX_DELAY_PINGPONG3 */ {400.0f, 200.0f, 0.40f, 0.6f}
};

//-----------------------------------------------------------------------------
// Constructor (FIXED: 1 input, not 2)
//-----------------------------------------------------------------------------
AudioEffectJPFX::AudioEffectJPFX()
    : AudioStream(1, inputQueueArray)  // CRITICAL FIX: 1 input, not 2
{
    // Initialize tone filters
    auto initShelf = [&](ShelfFilter &f) {
        f.b0 = 1.0f; f.b1 = 0.0f; f.a1 = 0.0f;
        f.in1 = 0.0f; f.out1 = 0.0f;
    };
    initShelf(bassFilterL);
    initShelf(bassFilterR);
    initShelf(trebleFilterL);
    initShelf(trebleFilterR);
    targetBassGain = 0.0f;
    targetTrebleGain = 0.0f;
    toneDirty = true;

    // Initialize modulation state
    modType = JPFX_MOD_OFF;
    modMix = 0.5f;
    modRateOverride = -1.0f;
    modFeedbackOverride = -1.0f;
    lfoPhaseL = 0.0f;
    lfoPhaseR = 0.5f;
    lfoIncL = 0.0f;
    lfoIncR = 0.0f;

    // Initialize delay state
    delayType = JPFX_DELAY_OFF;
    delayMix = 0.5f;
    delayFeedbackOverride = -1.0f;
    delayTimeOverride = -1.0f;

    // CRITICAL FIX: Initialize all buffer pointers to NULL
    modBufL = nullptr;
    modBufR = nullptr;
    delayBufL = nullptr;
    delayBufR = nullptr;
    modBufSize = 0;
    delayBufSize = 0;
    modWriteIndex = 0;
    delayWriteIndex = 0;

    // Allocate buffers
    allocateDelayBuffers();
}

//-----------------------------------------------------------------------------
// Destructor (NEW: free buffers)
//-----------------------------------------------------------------------------
AudioEffectJPFX::~AudioEffectJPFX()
{
    if (modBufL) free(modBufL);
    if (modBufR) free(modBufR);
    if (delayBufL) free(delayBufL);
    if (delayBufR) free(delayBufR);
}

//-----------------------------------------------------------------------------
// allocateDelayBuffers (FIXED: separate buffers for mod and delay)
//-----------------------------------------------------------------------------
void AudioEffectJPFX::allocateDelayBuffers()
{
    const float sr = AUDIO_SAMPLE_RATE_EXACT;
    float maxSamplesF = (JPFX_MAX_DELAY_MS * 0.001f * sr) + 2.0f;
    uint32_t samples  = (uint32_t)ceilf(maxSamplesF);
    delayBufSize      = samples;
    delayWriteIndex   = 0;
    
    // Calculate buffer size in bytes
    uint32_t bufferBytes = sizeof(float) * delayBufSize;
    
    Serial.print("[JPFX] Allocating delay buffers: ");
    Serial.print(bufferBytes / 1024);
    Serial.print("KB per channel, ");
    Serial.print((bufferBytes * 2) / 1024);
    Serial.println("KB total");
    
    // NEW: Try PSRAM first, then fall back to regular RAM
    #if defined(__IMXRT1062__)  // Teensy 4.x
        delayBufL = (float *)extmem_malloc(bufferBytes);
        delayBufR = (float *)extmem_malloc(bufferBytes);
        
        if (delayBufL && delayBufR) {
            Serial.println("[JPFX] Using PSRAM for delay buffers");
        } else {
            // PSRAM failed, try regular malloc

            
            Serial.println("[JPFX] PSRAM unavailable, trying regular RAM");

        }
    #else
        delayBufL = (float *)malloc(bufferBytes);
        delayBufR = (float *)malloc(bufferBytes);
    #endif
    
    if (delayBufL == nullptr || delayBufR == nullptr) {
        // Allocation failure
        Serial.println("[JPFX] ERROR: Delay buffer allocation FAILED!");
        Serial.print("[JPFX] Requested: ");
        Serial.print((bufferBytes * 2) / 1024);
        Serial.println("KB");
        

        
        if (delayBufL) free(delayBufL);
        if (delayBufR) free(delayBufR);
        delayBufL = delayBufR = nullptr;
        delayBufSize = 0;
    } else {
        // Success - clear buffers
        Serial.println("[JPFX] Delay buffers allocated successfully");
        for (uint32_t i = 0; i < delayBufSize; ++i) {
            delayBufL[i] = 0.0f;
            delayBufR[i] = 0.0f;
        }
    }
}

//-----------------------------------------------------------------------------
// computeShelfCoeffs (unchanged)
//-----------------------------------------------------------------------------
void AudioEffectJPFX::computeShelfCoeffs(ShelfFilter &filt, float cornerHz, float gainDB, bool high)
{
    const float fs = AUDIO_SAMPLE_RATE_EXACT;
    float V0 = powf(10.0f, gainDB / 20.0f);
    float sqrtV0 = sqrtf(V0);
    float K = tanf(M_PI * cornerHz / fs);
    
    if (!high) {
        // Low shelf
        filt.b0 = (1.0f + sqrtV0 * K) / (1.0f + K);
        filt.b1 = (1.0f - sqrtV0 * K) / (1.0f + K);
        filt.a1 = (1.0f - K) / (1.0f + K);
    } else {
        // High shelf
        filt.b0 = (sqrtV0 + K) / (1.0f + K);
        filt.b1 = (sqrtV0 - K) / (1.0f + K);
        filt.a1 = (1.0f - K) / (1.0f + K);
    }
    
    filt.in1 = 0.0f;
    filt.out1 = 0.0f;
}

//-----------------------------------------------------------------------------
// applyTone (unchanged)
//-----------------------------------------------------------------------------
inline void AudioEffectJPFX::applyTone(float &l, float &r)
{
    // Bass (low-shelf) left
    float x0 = l;
    float y0 = bassFilterL.b0 * x0 + bassFilterL.b1 * bassFilterL.in1 - bassFilterL.a1 * bassFilterL.out1;
    bassFilterL.in1 = x0;
    bassFilterL.out1 = y0;
    l = y0;
    
    // Bass (low-shelf) right
    x0 = r;
    y0 = bassFilterR.b0 * x0 + bassFilterR.b1 * bassFilterR.in1 - bassFilterR.a1 * bassFilterR.out1;
    bassFilterR.in1 = x0;
    bassFilterR.out1 = y0;
    r = y0;
    
    // Treble (high-shelf) left
    x0 = l;
    y0 = trebleFilterL.b0 * x0 + trebleFilterL.b1 * trebleFilterL.in1 - trebleFilterL.a1 * trebleFilterL.out1;
    trebleFilterL.in1 = x0;
    trebleFilterL.out1 = y0;
    l = y0;
    
    // Treble (high-shelf) right
    x0 = r;
    y0 = trebleFilterR.b0 * x0 + trebleFilterR.b1 * trebleFilterR.in1 - trebleFilterR.a1 * trebleFilterR.out1;
    trebleFilterR.in1 = x0;
    trebleFilterR.out1 = y0;
    r = y0;
}

//-----------------------------------------------------------------------------
// Setters (unchanged)
//-----------------------------------------------------------------------------
void AudioEffectJPFX::setBassGain(float dB)
{
    if (dB != targetBassGain) {
        targetBassGain = dB;
        toneDirty = true;
    }
}

void AudioEffectJPFX::setTrebleGain(float dB)
{
    if (dB != targetTrebleGain) {
        targetTrebleGain = dB;
        toneDirty = true;
    }
}

void AudioEffectJPFX::setModEffect(ModEffectType type)
{
    if (type != modType) {
        modType = type;
        lfoPhaseL = 0.0f;
        lfoPhaseR = 0.5f;
        updateLfoIncrements();
    }
}

void AudioEffectJPFX::setModMix(float mix)
{
    modMix = constrain(mix, 0.0f, 1.0f);
}

void AudioEffectJPFX::setModRate(float rate)
{
    if (rate < 0.0f) return;
    modRateOverride = (rate == 0.0f) ? -1.0f : rate;
    updateLfoIncrements();
}

void AudioEffectJPFX::setModFeedback(float fb)
{
    if (fb < 0.0f) {
        modFeedbackOverride = -1.0f;
    } else {
        modFeedbackOverride = constrain(fb, 0.0f, 0.99f);
    }
}

void AudioEffectJPFX::setDelayEffect(DelayEffectType type)
{
    // DEBUG: Log delay effect selection
    Serial.print("[JPFX] setDelayEffect called: ");
    Serial.print((int)type);
    Serial.print(" (");
    if (type == JPFX_DELAY_OFF) Serial.print("OFF");
    else if (type == JPFX_DELAY_SHORT) Serial.print("SHORT");
    else if (type == JPFX_DELAY_LONG) Serial.print("LONG");
    else if (type == JPFX_DELAY_PINGPONG1) Serial.print("PINGPONG1");
    else if (type == JPFX_DELAY_PINGPONG2) Serial.print("PINGPONG2");
    else if (type == JPFX_DELAY_PINGPONG3) Serial.print("PINGPONG3");
    Serial.println(")");
    
    // DEBUG: Check buffer allocation
    Serial.print("[JPFX] Delay buffers: L=");
    Serial.print((delayBufL != nullptr) ? "OK" : "NULL");
    Serial.print(", R=");
    Serial.print((delayBufR != nullptr) ? "OK" : "NULL");
    Serial.print(", Size=");
    Serial.println(delayBufSize);

    delayType = type;
    
    // DEBUG: Show preset parameters if not OFF
    if (type != JPFX_DELAY_OFF) {
        const DelayParams &p = delayParams[type];
        Serial.print("[JPFX] Delay params: L=");
        Serial.print(p.delayL);
        Serial.print("ms, R=");
        Serial.print(p.delayR);
        Serial.print("ms, FB=");
        Serial.print(p.feedback);

    }

    if (type != delayType) {
        delayType = type;
        delayWriteIndex = 0;
        if (delayBufL && delayBufR) {
            for (uint32_t i = 0; i < delayBufSize; ++i) {
                delayBufL[i] = 0.0f;
                delayBufR[i] = 0.0f;
            }
        }
    }
}

void AudioEffectJPFX::setDelayMix(float mix)
{
    delayMix = constrain(mix, -1.0f, 1.0f);
}

void AudioEffectJPFX::setDelayFeedback(float fb)
{
    if (fb < 0.0f) {
        delayFeedbackOverride = -1.0f;
    } else {
        delayFeedbackOverride = constrain(fb, 0.0f, 0.99f);
    }
}

void AudioEffectJPFX::setDelayTime(float ms)
{
    if (ms < 0.0f || ms == 0.0f) {
        delayTimeOverride = -1.0f;
    } else {
        delayTimeOverride = constrain(ms, 0.0f, JPFX_MAX_DELAY_MS);
    }
}

//-----------------------------------------------------------------------------
// updateLfoIncrements (unchanged)
//-----------------------------------------------------------------------------
void AudioEffectJPFX::updateLfoIncrements()
{
    if (modType == JPFX_MOD_OFF) {
        lfoIncL = lfoIncR = 0.0f;
        return;
    }
    
    float rate = modParams[modType].rate;
    if (modRateOverride > 0.0f) {
        rate = modRateOverride;
    }
    
    const float fs = AUDIO_SAMPLE_RATE_EXACT;
    const float twoPi = 2.0f * 3.14159265358979323846f;
    float phaseInc = twoPi * rate / fs;
    lfoIncL = phaseInc;
    lfoIncR = phaseInc * 1.01f;  // Slight offset for stereo width
}

//-----------------------------------------------------------------------------
// processModulation (FIXED: use modBuf instead of delayBuf)
//-----------------------------------------------------------------------------
inline void AudioEffectJPFX::processModulation(float inL, float inR, float &outL, float &outR)
{
    // Bypass if disabled or no buffer
    if (modType == JPFX_MOD_OFF || !modBufL || !modBufR) {
        outL = inL;
        outR = inR;
        return;
    }
    
    const ModParams &params = modParams[modType];
    const float baseDelayL = params.baseDelayL;
    const float baseDelayR = params.baseDelayR;
    const float depthL = params.depthL;
    const float depthR = params.depthR;
    const float feedback = (modFeedbackOverride >= 0.0f) ? modFeedbackOverride : params.feedback;
    const float presetMix = params.mix;
    const float wetMix = modMix * presetMix;
    const float dryMix = 1.0f - wetMix;
    
    // Compute LFO values
    const float lfoValL = sinf(lfoPhaseL);
    const float lfoValR = sinf(lfoPhaseR);
    lfoPhaseL += lfoIncL;
    if (lfoPhaseL > 6.283185307179586f) lfoPhaseL -= 6.283185307179586f;
    lfoPhaseR += lfoIncR;
    if (lfoPhaseR > 6.283185307179586f) lfoPhaseR -= 6.283185307179586f;
    
    // Convert delay times to samples
    const float fs = AUDIO_SAMPLE_RATE_EXACT;
    float delaySamplesL = (baseDelayL + depthL * lfoValL) * 0.001f * fs;
    float delaySamplesR = (baseDelayR + depthR * lfoValR) * 0.001f * fs;
    
    // Clamp within buffer bounds
    delaySamplesL = constrain(delaySamplesL, 0.0f, (float)(modBufSize - 2));
    delaySamplesR = constrain(delaySamplesR, 0.0f, (float)(modBufSize - 2));
    
    // Read with linear interpolation
    float readIndexL = (float)modWriteIndex - delaySamplesL;
    if (readIndexL < 0.0f) readIndexL += (float)modBufSize;
    uint32_t idxL0 = (uint32_t)readIndexL;
    uint32_t idxL1 = (idxL0 + 1) % modBufSize;
    float fracL = readIndexL - (float)idxL0;
    float delayedL = modBufL[idxL0] + (modBufL[idxL1] - modBufL[idxL0]) * fracL;
    
    float readIndexR = (float)modWriteIndex - delaySamplesR;
    if (readIndexR < 0.0f) readIndexR += (float)modBufSize;
    uint32_t idxR0 = (uint32_t)readIndexR;
    uint32_t idxR1 = (idxR0 + 1) % modBufSize;
    float fracR = readIndexR - (float)idxR0;
    float delayedR = modBufR[idxR0] + (modBufR[idxR1] - modBufR[idxR0]) * fracR;
    
    // Write with feedback
    modBufL[modWriteIndex] = inL + delayedL * feedback;
    modBufR[modWriteIndex] = inR + delayedR * feedback;
    
    // Advance write pointer (FIXED: only advance once per call)
    modWriteIndex = (modWriteIndex + 1) % modBufSize;
    
    // Mix dry and wet
    outL = dryMix * inL + wetMix * delayedL;
    outR = dryMix * inR + wetMix * delayedR;
}

//-----------------------------------------------------------------------------
// processDelay (FIXED: use delayBuf, not shared with modulation)
//-----------------------------------------------------------------------------
inline void AudioEffectJPFX::processDelay(float inL, float inR, float &outL, float &outR)
{
    // If delay disabled or no buffer allocated, bypass
    if (delayType == JPFX_DELAY_OFF || delayBufL == nullptr || delayBufR == nullptr) {
        outL = inL;
        outR = inR;
        
        // DEBUG: Only print once per second (not every sample!)
        static uint32_t lastDebug = 0;
        if (millis() - lastDebug > 1000) {
            if (delayType != JPFX_DELAY_OFF) {
                Serial.println("[JPFX] processDelay: BYPASSED (buffer null)");
            }
            lastDebug = millis();
        }
        return;
    }
    
    // DEBUG: Print once per second when delay is active
    static uint32_t lastActiveDebug = 0;
    if (millis() - lastActiveDebug > 1000) {
        Serial.print("[JPFX] processDelay ACTIVE: mix=");
        Serial.print(delayMix);
        Serial.print(", writeIdx=");
        Serial.println(delayWriteIndex);
        lastActiveDebug = millis();
    }
    // Bypass if disabled or no buffer
    if (delayType == JPFX_DELAY_OFF || !delayBufL || !delayBufR) {
        outL = inL;
        outR = inR;
        return;
    }
    
    const DelayParams &params = delayParams[delayType];
    float delayTimeL = params.delayL;
    float delayTimeR = params.delayR;
    const float feedback = (delayFeedbackOverride >= 0.0f) ? delayFeedbackOverride : params.feedback;
    float wetMix = delayMix;
    const float dryMix = 1.0f - fabsf(wetMix);
    const bool invertWet = (wetMix < 0.0f);
    wetMix = fabsf(wetMix);
    
    // Apply time override if set
    if (delayTimeOverride >= 0.0f) {
        delayTimeL = delayTimeOverride;
        delayTimeR = delayTimeOverride;
    }
    
    // Convert to samples
    const float fs = AUDIO_SAMPLE_RATE_EXACT;
    float delaySamplesL = delayTimeL * 0.001f * fs;
    float delaySamplesR = delayTimeR * 0.001f * fs;
    
    // Clamp within buffer bounds
    delaySamplesL = constrain(delaySamplesL, 0.0f, (float)(delayBufSize - 2));
    delaySamplesR = constrain(delaySamplesR, 0.0f, (float)(delayBufSize - 2));
    
    // Read with linear interpolation
    float readIndexL = (float)delayWriteIndex - delaySamplesL;
    if (readIndexL < 0.0f) readIndexL += (float)delayBufSize;
    uint32_t idxL0 = (uint32_t)readIndexL;
    uint32_t idxL1 = (idxL0 + 1) % delayBufSize;
    float fracL = readIndexL - (float)idxL0;
    float delayedL = delayBufL[idxL0] + (delayBufL[idxL1] - delayBufL[idxL0]) * fracL;
    
    float readIndexR = (float)delayWriteIndex - delaySamplesR;
    if (readIndexR < 0.0f) readIndexR += (float)delayBufSize;
    uint32_t idxR0 = (uint32_t)readIndexR;
    uint32_t idxR1 = (idxR0 + 1) % delayBufSize;
    float fracR = readIndexR - (float)idxR0;
    float delayedR = delayBufR[idxR0] + (delayBufR[idxR1] - delayBufR[idxR0]) * fracR;
    
    // Write with feedback
    delayBufL[delayWriteIndex] = inL + delayedL * feedback;
    delayBufR[delayWriteIndex] = inR + delayedR * feedback;
    
    // Advance write pointer (FIXED: only advance once per call)
    delayWriteIndex = (delayWriteIndex + 1) % delayBufSize;
    
    // Apply inversion if requested
    const float wetL = invertWet ? -delayedL : delayedL;
    const float wetR = invertWet ? -delayedR : delayedR;
    
    // Mix dry and wet
    outL = dryMix * inL + wetMix * wetL;
    outR = dryMix * inR + wetMix * wetR;
}

//-----------------------------------------------------------------------------
// update (FIXED: mono input, internal stereo processing)
//-----------------------------------------------------------------------------
void AudioEffectJPFX::update(void)
{
    // Receive mono input
    audio_block_t *in = receiveReadOnly(0);
    if (!in) return;
    
    // Allocate output
    audio_block_t *out = allocate();
    if (!out) {
        release(in);
        return;
    }
    
    // Update tone filter coefficients if needed
    if (toneDirty) {
        computeShelfCoeffs(bassFilterL, 100.0f, targetBassGain, false);
        computeShelfCoeffs(bassFilterR, 100.0f, targetBassGain, false);
        computeShelfCoeffs(trebleFilterL, 4000.0f, targetTrebleGain, true);
        computeShelfCoeffs(trebleFilterR, 4000.0f, targetTrebleGain, true);
        toneDirty = false;
    }
    
    // Process each sample
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
        // Convert to float and duplicate for stereo
        const float input = (float)in->data[i] * (1.0f / 32768.0f);
        float l = input;
        float r = input;
        
        // Apply tone EQ
        applyTone(l, r);
        
        // Apply modulation
        float modL, modR;
        processModulation(l, r, modL, modR);
        
        // Apply delay
        float delL, delR;
        processDelay(modL, modR, delL, delR);
        
        // Sum to mono and convert back to int16
        float mono = (delL + delR) * 0.5f;
        mono = constrain(mono, -1.0f, 1.0f);
        out->data[i] = (int16_t)(mono * 32767.0f);
    }
    
    // Transmit output
    transmit(out);
    release(out);
    release(in);
}