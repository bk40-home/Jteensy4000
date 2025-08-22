
/* Audio Library for Teensy, Ladder Filter
 * Copyright (c) 2021, Richard van Hoesel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

//-----------------------------------------------------------
// AudioFilterLadder2 for Teensy Audio Library
// Based on Huovilainen's full model, presented at DAFX 2004
// Richard van Hoesel, March. 11, 2021
// v.1.02 includes linear interpolation option
// v.1.01 Adds a "resonanceLoudness" parameter (1.0 - 10.0)
//        to control resonance loudness 
// v.1.0 Uses 2x oversampling. Host resonance range is 0-1.0. 
//
// please retain this header if you use this code.
//-----------------------------------------------------------

#include <Arduino.h>
#include "filter_ladder2.h"
#include "arm_math.h"
#include <math.h>
#include <stdint.h>

#define MAX_RESONANCE ((float)1.015)
#define MAX_FREQUENCY ((float)(AUDIO_SAMPLE_RATE_EXACT * 0.425f))
#define fI_NUM_SAMPLES  AUDIO_BLOCK_SAMPLES * INTERPOLATION

//=== 2 xOS, 32taps (88.2kHz)
static float AudioFilterLadder2 ::finterpolation_coeffs[AudioFilterLadder2::finterpolation_taps] = {    
-129.6800658538650740E-6, 0.001562843504973615, 0.004914813078250063, 0.007186231102125209, 0.002728024844356039,-0.007973931496234599,-0.013255882861166815,-841.1646993462468340E-6,
 0.022309967341058432, 0.027240071522569135,-0.007073857882406733,-0.056523975383091875,-0.055929860712812876, 0.042278471323346570, 0.207056768528768836, 0.335633514003638445,
 0.335633514003638445, 0.207056768528768836, 0.042278471323346570,-0.055929860712812876,-0.056523975383091875,-0.007073857882406733, 0.027240071522569135, 0.022309967341058432,
-841.1646993462468340E-6,-0.013255882861166815,-0.007973931496234599, 0.002728024844356039, 0.007186231102125209, 0.004914813078250063, 0.001562843504973615,-129.6800658538650740E-6
};

void AudioFilterLadder2::initpoly()
{
  if (arm_fir_interpolate_init_f32(&interpolation, INTERPOLATION, finterpolation_taps,
     finterpolation_coeffs, finterpolation_state, AUDIO_BLOCK_SAMPLES)) {
    polyCapable = false;
    return;
  }
  if (arm_fir_decimate_init_f32(&decimation, finterpolation_taps, INTERPOLATION,
     finterpolation_coeffs, fdecimation_state, fI_NUM_SAMPLES)) {
    polyCapable = false;
    return;
  }
  // TODO: should we fill interpolation_state & decimation_state with zeros?
  polyCapable = true;
  polyOn = true;
}

void AudioFilterLadder2::interpolationMethod(AudioFilterLadderInterpolation imethod)
{
  if (imethod == LADDER_FILTER_INTERPOLATION_FIR_POLY && polyCapable == true) {
    // TODO: if polyOn == false, clear interpolation_state & decimation_state ??
    polyOn = true;
  } else {
    polyOn = false;
  }
}

void AudioFilterLadder2::resonanceLoudness(float v)  // input 1-10
{
  if(v<1) v=1;
  if(v>10) v=10;
  VTx2 =  v;   
  inv2VT = 1.0/VTx2;
}

static inline float fast_exp2f(float x)
{
  float i;
  float f = modff(x, &i);
  f *= 0.693147f / 256.0f;
  f += 1.0f;
  f *= f;
  f *= f;
  f *= f;
  f *= f;
  f *= f;
  f *= f;
  f *= f;
  f *= f;
  f = ldexpf(f, i);
  return f;
}

void AudioFilterLadder2::inputDrive(float drive) 
{
  if(drive<1)
      overdrive = 1.0f;
  else 
  if(drive>3)
     overdrive =  3.0f;  
  else
    overdrive = drive;   
}

void AudioFilterLadder2::portamento(float pc)
{
  FcPorta = pc; 
}

void AudioFilterLadder2::compute_coeffs(float freq, float res) // tuned for 2x os
{
    float fc, freq2, freq3;          
    if (res > MAX_RESONANCE) res = MAX_RESONANCE ;
    if (res < 0.0f) res = 0.0f;    if (freq < 5) freq = 5;
    if (freq > MAX_FREQUENCY) freq = MAX_FREQUENCY;       
    fc = freq *  FC_SCALER;                             
    freq2 = freq*freq;
    freq3 = freq2 * freq;      
    float qa = 0.998f + 3.5e-5f * freq - 8e-10f * freq2 - 4e-14f * freq3; 
    K = 4.0f * res * qa;       
    float fa = 1.0036f - 2e-5f *freq + 1e-9f*freq2   - 1.75e-14f * freq3;  
    Gx2Vt = float((1.0f - exp(-fc * fa)) * VTx2);                                      
}

void AudioFilterLadder2::frequency(float c)
{
    if (c < 1) c = 1;
    if (c > MAX_FREQUENCY) c = MAX_FREQUENCY;  
    targetFbase = c; 
    //Fbase = c;   
}

void AudioFilterLadder2::resonance(float res)
{  
    res *= MAX_RESONANCE;                                 // normalize so host specifies 0-1 rather than 0-1.015
    if (res > MAX_RESONANCE) res = MAX_RESONANCE ;
    if (res < 0.0f) res = 0.0f;
    Qbase = res;
}

void AudioFilterLadder2::octaveControl(float octaves)
{
  if (octaves > 10.0f) {             // increased range compared to previous filters to span full audio range with 0-1 envelope
    octaves = 10.0f;
  } else if (octaves < 0.0f) {
    octaves = 0.0f;
  }
  octaveScale = octaves / 32768.0f;
}

static inline float fast_tanh(float x)
{
  if(x>3) return 1;
  if(x<-3) return -1;
  float x2 = x * x;
  return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

bool AudioFilterLadder2::resonating()
{
  for (int i=0; i < 4; i++) {
    if (fabsf(z0[i]) > 0.0001f) return true;
    if (fabsf(z1[i]) > 0.0001f) return true;
  }
  return false;
}

void AudioFilterLadder2::update(void)
{
  audio_block_t *blocka, *blockb, *blockc;
  float ftot, qtot;
  bool FCmodActive = true;
  bool QmodActive = true;

      blocka = receiveWritable(0);
      blockb = receiveReadOnly(1);
      blockc = receiveReadOnly(2);
      if (!blocka) {
        if (resonating()) {
          // When no data arrives but the filter is still
          // resonating, we must continue computing the filter
          // with zero input to sustain the resonance
          blocka = allocate();
        }
        if (!blocka) {
          if (blockb) release(blockb);
          if (blockc) release(blockc);
          return;
        }
        for (int i=0; i < AUDIO_BLOCK_SAMPLES; i++) {
          blocka->data[i] = 0;
        }
      }
      if (!blockb) {
        FCmodActive = false;
        ftot = Fbase;
      }
      if (!blockc) {
        QmodActive = false;
        qtot = Qbase;
      }
      float blockOut[AUDIO_BLOCK_SAMPLES]; 
      
      if (polyOn == true) {
        float blockIn[AUDIO_BLOCK_SAMPLES];   
        float blockOS[fI_NUM_SAMPLES], blockOutOS[fI_NUM_SAMPLES];                
        for (int i=0; i < AUDIO_BLOCK_SAMPLES; i++)
          blockIn[i] = blocka->data[i]  * overdrive *  float(INTERPOLATION /32768.0f);
        arm_fir_interpolate_f32(&interpolation, blockIn, blockOS, AUDIO_BLOCK_SAMPLES);             
        for (int i=0; i < AUDIO_BLOCK_SAMPLES; i++) {                   
          Fbase = FcPorta * Fbase + (1-FcPorta) * targetFbase; 
          ftot = Fbase;            
          if (FCmodActive) {
            float FCmod = blockb->data[i] * octaveScale;
            ftot = Fbase * fast_exp2f(FCmod);                    
          }
          if (QmodActive) {
            float Qmod = blockc->data[i] * (1.0f/32768.0f);
            qtot = Qbase + Qmod;
          }            
          compute_coeffs(ftot,qtot) ;                          
          for(int os = 0; os < INTERPOLATION; os++)  
          {      
            float input = blockOS[i * INTERPOLATION + os];                            
            filter_y1 = filter_y1 + Gx2Vt * (fast_tanh((input - K * filter_out) * inv2VT) - save_tan1);
            save_tan1=  fast_tanh(filter_y1 * inv2VT);                  
            filter_y2 = filter_y2 + Gx2Vt * (save_tan1 - save_tan2);
            save_tan2 = fast_tanh(filter_y2 * inv2VT);                  
            filter_y3 = filter_y3 + Gx2Vt * (save_tan2 - save_tan3);  
            save_tan3 = fast_tanh(filter_y3 * inv2VT);                   
            filter_y4 = filter_y4 + Gx2Vt * (save_tan3 - fast_tanh(filter_y4 * inv2VT));                             
            filter_out = (filter_y4 + filter_y5) * 0.5f;
            filter_y5 = filter_y4;   
            blockOutOS[i*INTERPOLATION + os] = filter_out;
          }      
        }
        arm_fir_decimate_f32(&decimation, blockOutOS, blockOut, fI_NUM_SAMPLES);  
      }     
      else {         
        for (int i=0; i < AUDIO_BLOCK_SAMPLES; i++) {                  
          Fbase = FcPorta * Fbase + (1-FcPorta) * targetFbase;   
          if (FCmodActive) {
            float FCmod = blockb->data[i] * octaveScale;
            ftot = Fbase * fast_exp2f(FCmod);                    
          }
          if (QmodActive) {
            float Qmod = blockc->data[i] * (1.0f/32768.0f);
            qtot = Qbase + Qmod;
          }            
          compute_coeffs(ftot,qtot) ;                        
          float total = 0.0f;
          float interp = 0.0f;
          float input = blocka->data[i] * overdrive * (1.0f/32768.0f);
          for (int os = 0; os < INTERPOLATION; os++) {
            float inputOS = (interp * oldinput + (1.0f - interp) * input); 
            filter_y1 = filter_y1 + Gx2Vt * (fast_tanh((inputOS - K * filter_out) * inv2VT) - save_tan1);
            save_tan1=  fast_tanh(filter_y1 * inv2VT);                  
            filter_y2 = filter_y2 + Gx2Vt * (save_tan1 - save_tan2);
            save_tan2 = fast_tanh(filter_y2 * inv2VT);                  
            filter_y3 = filter_y3 + Gx2Vt * (save_tan2 - save_tan3);  
            save_tan3 = fast_tanh(filter_y3 * inv2VT);                   
            filter_y4 = filter_y4 + Gx2Vt * (save_tan3 - fast_tanh(filter_y4 * inv2VT));                             
            filter_out = (filter_y4 + filter_y5) * 0.5f;
            filter_y5 = filter_y4;   
            total += filter_out * (1.0f / (float)INTERPOLATION);
            interp += (1.0f / (float)INTERPOLATION);            
          } 
          blockOut[i] = total;  
          oldinput = input;
        }      
      }                 
      float absblk;
      for (int i=0; i < AUDIO_BLOCK_SAMPLES; i++) {  
        absblk = 1.25f * blockOut[i];
        if(absblk<0) absblk=-absblk;        
        if(absblk > 1)
           if(absblk > peak)
              peak = absblk;
      }     
      for (int i=0; i < AUDIO_BLOCK_SAMPLES; i++) {         
        if(peak>1){ 
          pgain = 0.99f * pgain + 0.01f/peak;    // fast attack    
          peak *=0.99995f;
        }   
        blocka->data[i] = blockOut[i]  * pgain * float(0.85f * float(32767.0));  
      } 
      transmit(blocka);
      release(blocka);
      if (blockb) release(blockb);
      if (blockc) release(blockc);
}