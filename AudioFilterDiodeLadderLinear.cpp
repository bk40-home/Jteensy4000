#include <Arduino.h>
#include "Audio.h"
#include "AudioFilterDiodeLadderLinear.h"

/* --------------------------------------------------------------------------
 * Linear diode ladder, ZDF/TPT, with correct per-stage gains:
 *  - Stage 1 uses 2*g
 *  - Stages 2..4 use g
 *  Gauss–Seidel 2 passes per sample, then commit TPT states.
 * -------------------------------------------------------------------------- */

// void AudioFilterDiodeLadderLinear::update(void)
// {
//   audio_block_t *in  = receiveReadOnly(0);
//   if (!in) return;
//   audio_block_t *out = allocate();
//   if (!out) { release(in); return; }

//   const float fs   = _fs;
//   const float aCut = cutoffAlpha();

//   // local copies for speed
//   float s1=_s1, s2=_s2, s3=_s3, s4=_s4;
//   float y1=_y1, y2=_y2, y3=_y3, y4=_y4;

//   for (int i=0; i<AUDIO_BLOCK_SAMPLES; ++i) {
//     // input to float [-1,1]
//     const float x = in->data[i] * (1.0f/32768.0f);

//     // smooth cutoff and compute g
//     _fc += aCut * (_fcTarget - _fc);
//     float fc = _fc;
//     if (fc < 5.0f)       fc = 5.0f;
//     if (fc > 0.33f*fs) fc = 0.33f*fs;   // keep g saner near Nyquist


//     const float g  = tanf(PI * fc / fs);
//     const float g1 = 2.0f * g;

//     const float gg1 = g1 / (1.0f + g1);
//     const float gg  = g  / (1.0f + g);

//     // mild normalization so osc point doesn’t shift down at high g
//     const float knorm = 1.0f / (1.0f + 0.25f*g + 0.04f*g*g);
//     const float kEff  = _k * knorm;

//     // feedback input using last y4
//     const float x_fb = x - kEff * y4;


//     // 2× Gauss–Seidel across coupled stages (linear)
//     const float omega = 0.6f;

//     for (int it=0; it<4; ++it) {      // 4 iterations (was 2)
//       float u1 = x_fb + y2;
//       float v1 = (u1 - s1) * gg1;  float y1n = v1 + s1;
//       y1 = (1.0f - omega)*y1 + omega*y1n;

//       float u2 = 0.5f*(y1 + y3);
//       float v2 = (u2 - s2) * gg;   float y2n = v2 + s2;
//       y2 = (1.0f - omega)*y2 + omega*y2n;

//       float u3 = 0.5f*(y2 + y4);
//       float v3 = (u3 - s3) * gg;   float y3n = v3 + s3;
//       y3 = (1.0f - omega)*y3 + omega*y3n;

//       float u4 = 0.5f*y3;
//       float v4 = (u4 - s4) * gg;   float y4n = v4 + s4;
//       y4 = (1.0f - omega)*y4 + omega*y4n;
//     }


//     // Commit ZDF states once with final u,y
//     {
//       float u1 = x_fb + y2;            float v1 = (u1 - s1) * gg1; float y1f = v1 + s1; s1 = y1f + v1; y1 = y1f;
//       float u2 = 0.5f*(y1 + y3);       float v2 = (u2 - s2) * gg;  float y2f = v2 + s2; s2 = y2f + v2; y2 = y2f;
//       float u3 = 0.5f*(y2 + y4);       float v3 = (u3 - s3) * gg;  float y3f = v3 + s3; s3 = y3f + v3; y3 = y3f;
//       float u4 = 0.5f*y3;              float v4 = (u4 - s4) * gg;  float y4f = v4 + s4; s4 = y4f + v4; y4 = y4f;
//     }

//     // Output LP = y4 (unity DC when k=0)
//     float o = y4;
//     if (o >  1.0f) o =  1.0f;
//     if (o < -1.0f) o = -1.0f;
//     out->data[i] = (int16_t)(o * 32767.0f);
//   }

//   // write back states
//   _s1=s1; _s2=s2; _s3=s3; _s4=s4;
//   _y1=y1; _y2=y2; _y3=y3; _y4=y4;

//   transmit(out);
//   release(out);
//   release(in);
// }

void AudioFilterDiodeLadderLinear::update(void)
{
  audio_block_t *in  = receiveReadOnly(0);
  if (!in) return;
  audio_block_t *out = allocate();
  if (!out) { release(in); return; }

  const float fs   = _fs;
  const float aCut = cutoffAlpha();

  // local copies for speed
  float s1=_s1, s2=_s2, s3=_s3, s4=_s4;
  float y1=_y1, y2=_y2, y3=_y3, y4=_y4;

  for (int i=0; i<AUDIO_BLOCK_SAMPLES; ++i) {
    // input to float [-1,1]
    const float x = in->data[i] * (1.0f/32768.0f);

    // smooth cutoff and compute g
    _fc += aCut * (_fcTarget - _fc);
    float fc = _fc;
    if (fc < 5.0f)       fc = 5.0f;
    if (fc > 0.33f*fs) fc = 0.33f*fs;   // keep g saner near Nyquist


    const float g  = tanf(PI * fc / fs);
    const float g1 = 2.0f * g;

    const float gg1 = g1 / (1.0f + g1);
    const float gg  = g  / (1.0f + g);

    // mild normalization so osc point doesn’t shift down at high g
    const float knorm = 1.0f / (1.0f + 0.25f*g + 0.04f*g*g);
    const float kEff  = _k * knorm;

    // --- DC-blocked feedback + soft-limited k (prevents runaway without killing tone)

    // 2a) DC tracker: simple 1-pole lowpass on y4 (previous-sample output)
    // cutoff ≈ 5 Hz  -> alpha ≈ 1 - exp(-2π*5/fs)
    const float dcAlpha = 1.0f - expf(-2.0f * PI * 5.0f / fs);
    _dc += dcAlpha * (y4 - _dc);

    // AC-coupled ladder output to feed back (remove slow DC offsets)
    const float y4_ac = y4 - _dc;

    // // 2b) Envelope follower on |y4_ac| (fastish attack, slowish release)
    // const float envAttack  = 1.0f - expf(-2.0f * PI * 300.0f / fs); // ~1 ms
    // const float envRelease = 1.0f - expf(-2.0f * PI * 10.0f  / fs); // ~16 ms
    // float targetEnv = fabsf(y4_ac);
    // _env += (targetEnv > _env ? envAttack : envRelease) * (targetEnv - _env);

    // // 2c) Resonance soft limiter: reduce k only when energy spikes
    // // kSafe = kEff / (1 + beta * env^2)
    // // beta sets how quickly k is tamed when it screams; 6..12 works well.
    // const float beta  = 8.0f;
    // const float kSafe = kEff / (1.0f + beta * _env * _env);

    // // Finally, use AC output and safe k in feedback:
    // const float x_fb = x - kSafe * y4_ac;

// --- Envelope follower on |y4_ac| (unchanged) ---
const float envAttack  = 1.0f - expf(-2.0f * PI * 300.0f / fs); // ~1 ms
const float envRelease = 1.0f - expf(-2.0f * PI * 10.0f  / fs); // ~16 ms
float targetEnv = fabsf(y4_ac);
_env += (targetEnv > _env ? envAttack : envRelease) * (targetEnv - _env);

// Soft gate for clamp engagement with hysteresis
const float E_on  = 0.34f;   // engage here
const float E_off = 0.28f;   // disengage here
float gateTarget  = (_env > E_on) ? 1.0f : (_env < E_off ? 0.0f : _clampState);
// Smooth the gate so it doesn’t chatter
const float gAlpha = 0.2f;
_clampState += gAlpha * (gateTarget - _clampState);

// Over only when gate is partially/fully open
float over = (_env - E_on);
if (over < 0.0f) over = 0.0f;
over *= _clampState;          // no clamp when gate ~0

const float beta  = 4.0f;
const float kSafe = kEff / (1.0f + beta * over * over);
const float x_fb  = x - kSafe * y4_ac;



    // 2× Gauss–Seidel across coupled stages (linear)
    const float omega = 0.63f;

    for (int it=0; it<2; ++it) {      
      float u1 = x_fb + y2;
      float v1 = (u1 - s1) * gg1;  float y1n = v1 + s1;
      y1 = (1.0f - omega)*y1 + omega*y1n;

      float u2 = 0.5f*(y1 + y3);
      float v2 = (u2 - s2) * gg;   float y2n = v2 + s2;
      y2 = (1.0f - omega)*y2 + omega*y2n;

      float u3 = 0.5f*(y2 + y4);
      float v3 = (u3 - s3) * gg;   float y3n = v3 + s3;
      y3 = (1.0f - omega)*y3 + omega*y3n;

      float u4 = 0.5f*y3;
      float v4 = (u4 - s4) * gg;   float y4n = v4 + s4;
      y4 = (1.0f - omega)*y4 + omega*y4n;
    }


    // Commit ZDF states once with final u,y
    {
      float u1 = x_fb + y2;            float v1 = (u1 - s1) * gg1; float y1f = v1 + s1; s1 = y1f + v1; y1 = y1f;
      float u2 = 0.5f*(y1 + y3);       float v2 = (u2 - s2) * gg;  float y2f = v2 + s2; s2 = y2f + v2; y2 = y2f;
      float u3 = 0.5f*(y2 + y4);       float v3 = (u3 - s3) * gg;  float y3f = v3 + s3; s3 = y3f + v3; y3 = y3f;
      float u4 = 0.5f*y3;              float v4 = (u4 - s4) * gg;  float y4f = v4 + s4; s4 = y4f + v4; y4 = y4f;
    }

    // Output LP = y4 (unity DC when k=0)
    float o = y4;
    if (o >  1.0f) o =  1.0f;
    if (o < -1.0f) o = -1.0f;
    out->data[i] = (int16_t)(o * 32767.0f);
  }

  // write back states
  _s1=s1; _s2=s2; _s3=s3; _s4=s4;
  _y1=y1; _y2=y2; _y3=y3; _y4=y4;

  transmit(out);
  release(out);
  release(in);
}





