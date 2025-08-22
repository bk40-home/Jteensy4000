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

// https://forum.pjrc.com/threads/60488?p=269609&viewfull=1#post269609

#ifndef filter_ladder2_h_
#define filter_ladder2_h_

#include "Arduino.h"
#include "AudioStream.h"
#include "arm_math.h"
#include <Audio.h>
#define MOOG_PI ((float)3.14159265358979323846264338327950288)  

class AudioFilterLadder2: public AudioStream
{
public:
  AudioFilterLadder2() : AudioStream(3, inputQueueArray) { initpoly(); };
  void frequency(float FC);
  void resonance(float reson);
  void octaveControl(float octaves);
  void inputDrive(float drive);
  void portamento(float pcf);   
  void compute_coeffs(float freq, float res);  
  void initpoly(); 
  void interpolationMethod(AudioFilterLadderInterpolation im);
  void resonanceLoudness(float v);  
  virtual void update(void);
  
private:
  static const int INTERPOLATION = 2;
  static const int finterpolation_taps = 32;
  static const float FC_SCALER = 2.0f * MOOG_PI / (INTERPOLATION * AUDIO_SAMPLE_RATE_EXACT);
  float finterpolation_state[(AUDIO_BLOCK_SAMPLES-1) + finterpolation_taps / INTERPOLATION];
  arm_fir_interpolate_instance_f32 interpolation;
  float fdecimation_state[(AUDIO_BLOCK_SAMPLES*INTERPOLATION-1) + finterpolation_taps];
  arm_fir_decimate_instance_f32 decimation;   
  static float finterpolation_coeffs[finterpolation_taps];
  
  float Gx2Vt, K, filter_res, filter_y1, filter_y2, filter_y3, filter_y4, filter_y5, filter_out; 
  float VTx2 = 5; 
  float inv2VT = 1.0/VTx2;
  float Fbase=1000, Qbase=0.5;
  float overdrive=1;
  float ftot=Fbase, qtot=Qbase;
  bool  resonating();
  bool  polyCapable = false;
  bool  polyOn = false;    // FIR is default after initpoly()
  float octaveScale = 1.0f/32768.0f;
  float z0[4] = {0.0, 0.0, 0.0, 0.0};
  float z1[4] = {0.0, 0.0, 0.0, 0.0};
  float pbg=0;
  float save_tan1=0;
  float save_tan2=0;
  float save_tan4=0;
  float save_tan3=0;
  float peak=1.0;
  float pgain=1.0;
  float targetFbase = 1000;
  float FcPorta = 0;   
  float oldinput=0;  
  audio_block_t *inputQueueArray[3];
};

#endif