#include "Patch.h"
using namespace JT4000Map;

// Helper to avoid duplicates when scanning ccMap
static bool containsCC(const uint8_t* arr, int n, uint8_t cc) {
  for (int i = 0; i < n; ++i) if (arr[i] == cc) return true;
  return false;
}

int Patch::buildUsedCCList(uint8_t* outList, int maxCount) const {
  int count = 0;
  for (int p = 0; p < NUM_PAGES; ++p) {
    for (int row = 0; row < 4; ++row) {
      uint8_t cc = ccMap[p][row];
      if (!containsCC(outList, count, cc) && count < maxCount) {
        outList[count++] = cc;
      }
    }
  }
  return count;
}

void Patch::captureFrom(SynthEngine& synth) {
  clear();
  uint8_t used[128]; 
  int n = buildUsedCCList(used, 128);

  for (int i = 0; i < n; ++i) {
    uint8_t cc = used[i];
    // Map engine -> CC using your existing curves and getters
    // Keep this minimal; expand as you need. (Examples below)
    uint8_t cv = 0;

    switch (cc) {
      // ---- Filter main ----
      case 23: cv = cutoff_hz_to_cc(synth.getFilterCutoff()); break;      // Hz -> CC
      case 24: cv = resonance_to_cc(synth.getFilterResonance()); break;    // 0..1 -> CC

      // ---- Amp Env ----
      case 25: cv = time_ms_to_cc(synth.getAmpAttack()); break;
      case 26: cv = time_ms_to_cc(synth.getAmpDecay()); break;
      case 27: cv = norm_to_cc_lin(synth.getAmpSustain()); break;
      case 28: cv = time_ms_to_cc(synth.getAmpRelease()); break;

      // ---- Filter Env ----
      case 29: cv = time_ms_to_cc(synth.getFilterEnvAttack()); break;
      case 30: cv = time_ms_to_cc(synth.getFilterEnvDecay()); break;
      case 31: cv = norm_to_cc_lin(synth.getFilterEnvSustain()); break;
      case 32: cv = time_ms_to_cc(synth.getFilterEnvRelease()); break;

      // ---- LFO ----
      case 54: cv = lfo_hz_to_cc(synth.getLFO1Frequency()); break;
      case 55: cv = norm_to_cc_lin(synth.getLFO1Amount()); break;
      case 51: cv = lfo_hz_to_cc(synth.getLFO2Frequency()); break;
      case 52: cv = norm_to_cc_lin(synth.getLFO2Amount()); break;

      // ---- OSC basics (examples; adjust to your getters) ----
      case 21: cv = (uint8_t)synth.getOsc1Waveform(); break;
      case 22: cv = (uint8_t)synth.getOsc2Waveform(); break;

      // Mix taps (0..1 -> CC)
      case 47: case 60: cv = norm_to_cc_lin(synth.getOscMix1()); break;
      case 61:          cv = norm_to_cc_lin(synth.getOscMix2()); break;
      case 58:          cv = norm_to_cc_lin(synth.getSubMix());  break;
      case 59:          cv = norm_to_cc_lin(synth.getNoiseMix()); break;

      // Glide / Globals
      case 81: cv = synth.getGlideEnabled() ? 127 : 0; break;
      case 82: { float t = synth.getGlideTimeMs(); cv = (uint8_t)constrain(lroundf((t / 500.0f) * 127.0f), 0, 127); break; }

      // Add more as you need, staying 1:1 with your CC map
      default:
        // If no direct mapping defined, fall back to last-sent cache, or 0
        // (Optionally, expose a SynthEngine::getLastCCValue(cc))
        cv = value[cc]; // keep previous if any
        break;
    }

    setCC(cc, cv);
  }
}

void Patch::applyTo(SynthEngine& synth, uint8_t midiChannel, bool batch) const {
  if (batch) AudioNoInterrupts();              // optional: apply atomically to reduce zipper
  for (int cc = 0; cc < 128; ++cc) {
    if (!has[cc]) continue;
    synth.handleControlChange(midiChannel, (uint8_t)cc, value[cc]);
  }
  if (batch) AudioInterrupts();
}

// Very light JSON writer (no external dependency)
String Patch::toJson() const {
  String js = "{";
  js += "\"name\":\""; js += name; js += "\",";
  js += "\"v\":"; js += version; js += ",";
  js += "\"cc\":{";
  bool first = true;
  for (int cc = 0; cc < 128; ++cc) {
    if (!has[cc]) continue;
    if (!first) js += ",";
    first = false;
    js += "\""; js += cc; js += "\":"; js += value[cc];
  }
  js += "}}";
  return js;
}

// Very light parser: expects {"cc":{"23":64,"24":80,...}}
bool Patch::fromJson(const String& js) {
  clear();
  // Extract name (optional)
  int p = js.indexOf("\"name\"");
  if (p >= 0) {
    p = js.indexOf("\"", p + 6); int q = js.indexOf("\"", p + 1);
    if (p >= 0 && q > p) {
      int len = min((int)sizeof(name)-1, q - (p + 1));
      for (int i = 0; i < len; ++i) name[i] = js[p + 1 + i];
      name[len] = '\0';
    }
  }
  // Find "cc" map
  int c0 = js.indexOf("\"cc\"");
  if (c0 < 0) return true; // no cc block is fine
  int lb = js.indexOf("{", c0);
  int rb = js.indexOf("}", lb);
  if (lb < 0 || rb < 0) return false;

  int i = lb + 1;
  while (i < rb) {
    // key as "nn"
    int k1 = js.indexOf("\"", i); if (k1 < 0 || k1 > rb) break;
    int k2 = js.indexOf("\"", k1 + 1); if (k2 < 0 || k2 > rb) break;
    String key = js.substring(k1 + 1, k2);
    int colon = js.indexOf(":", k2); if (colon < 0 || colon > rb) break;
    int comma = js.indexOf(",", colon);
    if (comma < 0 || comma > rb) comma = rb;

    String val = js.substring(colon + 1, comma);
    uint8_t cc = (uint8_t) key.toInt();
    uint8_t v  = (uint8_t) val.toInt();
    setCC(cc, v);

    i = comma + 1;
  }
  return true;
}
