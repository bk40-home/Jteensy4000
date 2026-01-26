#include <Audio.h>
#include <Wire.h>
#include <usb_midi.h>
#include <USBHost_t36.h>
#include "SynthEngine.h"
#include "HardwareInterface.h"
#include "UIManager.h"
#include "UIPageLayout.h"   // UI-only page->CC layout and labels
//#include "CCMap.h"
#include "Presets.h"
#include "AudioScopeTap.h" 

// ---------------------- Audio endpoints ----------------------
AudioOutputI2S2 i2sOut;                 // I2S audio out (to SGTL5000 / PCM path)
AudioInputUSB usbIn;                 // USB audio in
AudioOutputUSB usbOut;                 // USB audio out
AudioControlSGTL5000 audioShield;      // Audio shield control
AudioScopeTap scopeTap;                    // ➋ create the tap (always-on)

// Patch-cord pointers (kept alive for the program lifetime)
AudioConnection* patchMixerI2SL = nullptr;
AudioConnection* patchMixerI2SR = nullptr;
AudioConnection* patchUSBINMixerI2SL = nullptr;
AudioConnection* patchUSBINMixerI2SR = nullptr;
AudioConnection* patchOutL    = nullptr;
AudioConnection* patchOutR    = nullptr;

AudioConnection* patchAmpUSBL = nullptr;
AudioConnection* patchAmpUSBR = nullptr;
AudioConnection* patchOutUSBL = nullptr;
AudioConnection* patchOutUSBR = nullptr;

AudioConnection* patchOutScope  = nullptr;   // scope feed

// ---------------------- Engine & UI --------------------------
SynthEngine synth;
HardwareInterface hw;
UIManager ui;

// Split to 2 independent paths 
AudioMixer4  mixerI2SL;
AudioMixer4  mixerI2SR;
AudioAmplifier  ampUSBL;
AudioAmplifier  ampUSBR;


// ---------------------- USB Host MIDI ------------------------
USBHost myusb;
USBHub hub1(myusb);       // if a hub is used
MIDIDevice midiHost(myusb);

unsigned long lastDisplayUpdate = 0;
const unsigned long displayRefreshMs = 100;

static void onParam(uint8_t cc, uint8_t v) {
  Serial.printf("[NOTIFY] CC %u = %u\n", cc, v);
  // Optional: keep screen values fresh even if change came via MIDI/menu
  // ui.syncFromEngine(synth);  // uncomment if you want immediate reflection
}

// --- Debug: dump current page mapping and values -----------------------------
static void debugDumpPage(UIManager& ui, HardwareInterface& hw, SynthEngine& synth) {
  const int page = ui.getCurrentPage();
  Serial.printf("\n[PAGE] %d\n", page);

  for (int i = 0; i < 4; ++i) {
    const byte cc      = UIPage::ccMap[page][i];                 // UI routing
    const char* label  = UIPage::ccNames[page][i];               // OLED label
    const int  raw     = hw.readPot(i);                          // 0..1023
    const int  ccVal   = (raw >> 3);                             // 0..127
    const char* ccDesc = CC::name(cc);                           // friendly name or nullptr

    // Ask UIManager to compute the engine-reflected CC (inverse mapping)
    const int engineCC = (cc != 255) ? ui.ccToDisplayValue(cc, synth) : -1;

    if (cc == 255) {
      Serial.printf("  knob %d: label='%-10s'  [UNMAPPED]\n", i, label ? label : "");
      continue;
    }

    Serial.printf("  knob %d: label='%-10s'  map→ CC %3u  (%s)"
                  "  potRaw=%4d  potCC=%3d  engineCC=%3d\n",
                  i,
                  label ? label : "",
                  cc,
                  ccDesc ? ccDesc : "unnamed",
                  raw, ccVal, engineCC);
  }
  Serial.println();
}

//   // Jteensy4000.ino  (in setup() after synth/ui begin)
// static void onParam(uint8_t cc, uint8_t v) {
//   Serial.printf("[NOTIFY] CC %u = %u\n", cc, v);
//   // Optional: keep screen values fresh even if change came via MIDI/menu
//   // ui.syncFromEngine(synth);  // uncomment if you want immediate reflection
// }
// ---------------------- MIDI handlers ------------------------
void handleNoteOn(byte channel, byte note, byte velocity) {
  float normvelocity = velocity / 127.0f;
  Serial.printf("handleNoteOn : %d vel %0.2f\n", note, normvelocity);
  synth.noteOn(note, velocity);
}

void handleNoteOff(byte channel, byte note, byte velocity) {
  Serial.printf("handleNoteOff: %d\n", note);
  synth.noteOff(note);
}

void handleControlChange(byte channel, byte control, byte value) {
  Serial.printf("MIDI CC: ch=%d ctrl=%d val=%d\n", channel, control, value);
  synth.handleControlChange(channel, control, value);
}

// Example: type a number 0..8 in Serial Monitor and press Enter to load template
void handleSerialPresets(SynthEngine& synth, UIManager& ui) {
  if (!Serial.available()) return;
  int idx = Serial.parseInt();
  if (idx >= 0 && idx <= 8) {
    Serial.printf("Loading template %d: %s\n", idx, Presets::templateName(idx));
    Presets::loadTemplateByIndex(synth, (uint8_t)idx);
    ui.syncFromEngine(synth);
  } else {
    Serial.println("Enter a preset index 0..8");
  }
}

void setup() {
  Serial.begin(115200);                 // debug prints
  AudioMemory(100);                     // audio buffers before starting graph

  // Start USB host
  myusb.begin();

  // Audio shield config
  audioShield.enable();
  audioShield.volume(0.3f);
  audioShield.lineOutLevel(0);
  audioShield.lineInLevel(0);

  // Attach MIDI host handlers
  midiHost.setHandleNoteOn(handleNoteOn);
  midiHost.setHandleNoteOff(handleNoteOff);
  midiHost.setHandleControlChange(handleControlChange);

  // USB device MIDI handlers
  usbMIDI.setHandleNoteOn(handleNoteOn);
  usbMIDI.setHandleNoteOff(handleNoteOff);
  usbMIDI.setHandleControlChange(handleControlChange);

  Serial.println("JTeensy 4000 Synth Ready");

  // ---------------------- AUDIO PATCHING ----------------------
  // IMPORTANT:
  // - For getters that return AudioStream& (e.g., getFXOutL(), ui.scopeIn()), call them with ().
  // - For actual objects (ampI2SL/ampI2SR/i2sOut/usbOut), pass the object (NO parentheses).

  // FX L/R -> I2S amp L/R
  patchMixerI2SL = new AudioConnection(synth.getFXOutL(), 0, mixerI2SL, 0);
  patchMixerI2SR = new AudioConnection(synth.getFXOutR(), 0, mixerI2SR, 0);

  // FX L/R -> USB amp L/R
  patchAmpUSBL = new AudioConnection(synth.getFXOutL(), 0, ampUSBL, 0);
  patchAmpUSBR = new AudioConnection(synth.getFXOutR(), 0, ampUSBR, 0);

  // USB Audio
  patchUSBINMixerI2SL = new AudioConnection(usbIn, 0, mixerI2SL, 1);
  patchUSBINMixerI2SR = new AudioConnection(usbIn, 1, mixerI2SR, 1);

  // I2S amp L/R -> I2S out L/R
  patchOutL    = new AudioConnection(mixerI2SL, 0, i2sOut, 0);
  patchOutR    = new AudioConnection(mixerI2SR, 0, i2sOut, 1);

  // USB amp L/R -> USB out L/R
  patchOutUSBL = new AudioConnection(ampUSBL, 0, usbOut, 0);
  patchOutUSBR = new AudioConnection(ampUSBR, 0, usbOut, 1);

  // Scope tap (FX L -> UI scope input)
  // ui.scopeIn() must return an AudioStream& (e.g., an AudioAnalyze or custom scope sink)
  patchOutScope  = new AudioConnection(synth.getFXOutL(), 0, scopeTap, 0);

  // ---------------------- INIT UI/HW --------------------------
  hw.begin();
  ui.begin();

synth.setNotifier(onParam);


  // Split-path gains (will later map to a master volume)
  mixerI2SL.gain(0, 0.6f);
  mixerI2SR.gain(0, 0.6f);
  mixerI2SL.gain(1, 0.4f);
  mixerI2SR.gain(1, 0.4f);
  ampUSBL.gain(0.4f);
  ampUSBR.gain(0.4f);

  int page = ui.getCurrentPage();
  for (int i = 0; i < 4; ++i) {
    ui.setParameterLabel(i, UIPage::ccNames[page][i]);
  }
  ui.syncFromEngine(synth);
}

void loop() {
  // USB device MIDI
  usbMIDI.read();

  // Engine + hardware
  synth.update();
  hw.update();

  // USB host stack and MIDI host
  myusb.Task();
  midiHost.read();

  // Process USB device MIDI manually (if needed by your flow)
  while (usbMIDI.read()) {
    byte type  = usbMIDI.getType();
    byte data1 = usbMIDI.getData1();
    byte data2 = usbMIDI.getData2();

    if (type == usbMIDI.NoteOn && data2 > 0) {
      handleNoteOn(usbMIDI.getChannel(), data1, data2);
    } else if (type == usbMIDI.NoteOff || (type == usbMIDI.NoteOn && data2 == 0)) {
      handleNoteOff(usbMIDI.getChannel(), data1, data2);
    } else if (type == usbMIDI.ControlChange) {
      handleControlChange(usbMIDI.getChannel(), data1, data2);
    }
  }

  // UI input polling (encoder + 4 pots)
  ui.pollInputs(hw, synth);

  // OLED refresh
  if (millis() - lastDisplayUpdate > displayRefreshMs) {
    ui.updateDisplay(synth);
    lastDisplayUpdate = millis();
  }

  // Serial-driven preset loader
  handleSerialPresets(synth, ui);
}
