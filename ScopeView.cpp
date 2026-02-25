// ScopeView.cpp
// =============================================================================
// Oscilloscope / level-meter view for the ILI9341 320x240 display.
// Implements the scope helper methods declared in UIManager_MicroDexed.h.
//
// Screen layout when scope mode is active:
//
//   +--------------------------------------------------+  <- y=0
//   | OSCILLOSCOPE                        CPU: xx%     |  SCOPE_HEADER_H = 20 px
//   +--------------------------------------+-----------+  <- y=20
//   |                                      |   PEAK    |
//   |   waveform area                      |    dB     |  waveform: ~200px tall
//   |   (WAVEFORM_WIDTH pixels wide)       |   bars    |  meters: METER_COL_W wide
//   |  - - - - - - - - (zero line) - - -  |           |
//   |                                      +-----+-----+
//   |                                      | VOICES 1-8|  voice activity bars
//   +--------------------------------------+-----------+  <- y=220
//   | Press any button to return                       |  SCOPE_FOOTER_H = 20 px
//   +--------------------------------------------------+
//
// Naming: uses COLOUR_*, SCREEN_WIDTH/HEIGHT, FONT_SMALL/MEDIUM from the header.
//
// AudioScopeTap interface:
//   scopeTap.snapshot(buffer, maxSamples)  -> returns number of valid samples
//   scopeTap.readPeakAndClear()            -> returns 0.0-1.0 peak amplitude
//
// SynthEngine requirement — add this to SynthEngine.h public section:
//   inline bool isVoiceActive(uint8_t voiceIndex) const {
//       return (voiceIndex < MAX_VOICES) && (_activeNotes[voiceIndex] != 0);
//   }
// =============================================================================

#include "UIManager_MicroDexed.h"
#include "AudioScopeTap.h"
#include <math.h>   // log10f

// External AudioScopeTap instance declared and wired to audio output in the main sketch.
extern AudioScopeTap scopeTap;

// =============================================================================
// Scope-local layout constants
// These are local to this file — the header does not need them.
// =============================================================================
static constexpr int SCOPE_HEADER_H  = 20;   // header bar height (px)
static constexpr int SCOPE_FOOTER_H  = 20;   // footer bar height (px)
static constexpr int METER_COL_W     = 62;   // right-side meter column total width (px)
static constexpr int METER_GAP       = 8;    // gap between waveform and meter column (px)

// Derived — waveform occupies the left portion of the display
// WAVEFORM_WIDTH = SCREEN_WIDTH - METER_COL_W - METER_GAP  (250 - 62 - 8 = 180? calculated inside)


// =============================================================================
// drawScopeView() — top-level scope repaint; called every frame from updateDisplay()
// =============================================================================

void UIManager_MicroDexed::drawScopeView(SynthEngine& synth) {

    // ---- Derived layout for this frame ----
    const int WAVEFORM_WIDTH  = SCREEN_WIDTH - METER_COL_W - METER_GAP; // ~250px
    const int WAVEFORM_TOP    = SCOPE_HEADER_H + 2;
    const int WAVEFORM_BOTTOM = SCREEN_HEIGHT - SCOPE_FOOTER_H - 2;
    const int WAVEFORM_HEIGHT = WAVEFORM_BOTTOM - WAVEFORM_TOP;
    const int METER_COL_X     = WAVEFORM_WIDTH + METER_GAP;  // X start of right column

    // Split the right column vertically: meters above, voice activity below
    // Voice activity needs 8 bars * (5px + 2px gap) + 12px label = 70px
    const int VOICE_AREA_H    = 8 * 7 + 14;
    const int METER_BAR_H     = WAVEFORM_HEIGHT - VOICE_AREA_H - 4; // remaining height for meter

    // ---- Full screen clear each frame (scope view always fully redraws) ----
    _display.fillScreen(COLOUR_BACKGROUND);

    // ---- Header bar ----
    _display.fillRect(0, 0, SCREEN_WIDTH, SCOPE_HEADER_H, COLOUR_HEADER_BG);
    _display.setTextColor(COLOUR_TEXT);
    _display.setTextSize(FONT_MEDIUM);
    _display.setCursor(5, 3);
    _display.print("OSCILLOSCOPE");

    // CPU % — right side of header
    char cpuBuffer[12];
    snprintf(cpuBuffer, sizeof(cpuBuffer), "CPU:%d%%", (int)AudioProcessorUsageMax());
    drawTextRight(SCREEN_WIDTH - 4, 3, cpuBuffer, COLOUR_TEXT_DIM, FONT_SMALL);

    // ---- Pull audio samples from the scope tap ----
    // Static buffer avoids allocating 1 KB on the stack every frame.
    static int16_t sampleBuffer[512];
    const uint16_t numSamples = scopeTap.snapshot(sampleBuffer, 512);

    if (numSamples < 128) {
        // Tap not yet filled — show arming message centred in waveform area
        drawTextCentred(WAVEFORM_WIDTH / 2, SCREEN_HEIGHT / 2,
                        "Scope arming...", COLOUR_TEXT_DIM, FONT_MEDIUM);
    } else {
        // ---- Find a stable rising zero-crossing for the trigger ----
        const int triggerIndex = findZeroCrossing(sampleBuffer, numSamples);

        // ---- Draw waveform ----
        drawWaveform(sampleBuffer, numSamples,
                     triggerIndex, WAVEFORM_TOP, WAVEFORM_BOTTOM, WAVEFORM_WIDTH);

        // ---- Zero-reference line (dim horizontal line at vertical centre) ----
        const int centreY = WAVEFORM_TOP + WAVEFORM_HEIGHT / 2;
        _display.drawFastHLine(0, centreY, WAVEFORM_WIDTH, COLOUR_BORDER);
    }

    // ---- Peak level meter — right column, upper portion ----
    drawPeakMeters(METER_COL_X, WAVEFORM_TOP, METER_COL_W - 2, METER_BAR_H);

    // ---- Voice activity indicators — right column, lower portion ----
    drawVoiceActivity(synth,
                      METER_COL_X,
                      WAVEFORM_TOP + METER_BAR_H + 4,
                      METER_COL_W - 2);

    // ---- Footer bar ----
    _display.fillRect(0, SCREEN_HEIGHT - SCOPE_FOOTER_H,
                      SCREEN_WIDTH, SCOPE_FOOTER_H, COLOUR_HEADER_BG);
    _display.drawFastHLine(0, SCREEN_HEIGHT - SCOPE_FOOTER_H,
                           SCREEN_WIDTH, COLOUR_BORDER);
    _display.setCursor(5, SCREEN_HEIGHT - SCOPE_FOOTER_H + 5);
    _display.setTextColor(COLOUR_TEXT_DIM);
    _display.setTextSize(FONT_SMALL);
    _display.print("Press any button to return");
}


// =============================================================================
// drawWaveform()
// =============================================================================
// Renders the audio waveform as connected line segments within the given bounds.
//
// Downsampling:
//   When numSamples > width pixels we average groups of samples per pixel
//   (box-filter / mean) to prevent aliasing and the jumping/strobing that
//   single-sample picks cause on complex waveforms.  At Teensy 4.1 speeds
//   this is fast enough at 30 FPS.
//
// Y scaling:
//   80% of the available pixel height is used so clipping is visible as
//   hard lines against the top/bottom bounds rather than hidden.

void UIManager_MicroDexed::drawWaveform(int16_t* samples,
                                        uint16_t numSamples,
                                        int triggerIndex,
                                        int topY,
                                        int bottomY,
                                        int width) {
    const int waveformHeight = bottomY - topY;
    const int centreY        = topY + waveformHeight / 2;

    // Samples per pixel — how many source samples map to one display column
    const int samplesPerPixel = (numSamples > (uint16_t)width)
                                ? (numSamples / width) : 1;

    int previousX = 0;
    int previousY = centreY;

    for (int pixelX = 0; pixelX < width; ++pixelX) {
        const int baseIndex = triggerIndex + pixelX * samplesPerPixel;
        if (baseIndex >= (int)numSamples) break;   // ran off end of buffer

        // Box-filter average across samplesPerPixel to reduce aliasing
        int32_t accumulator = 0;
        int     sampleCount = 0;
        for (int s = 0; s < samplesPerPixel; ++s) {
            const int index = baseIndex + s;
            if (index < (int)numSamples) {
                accumulator += samples[index];
                ++sampleCount;
            }
        }
        const int16_t averageSample = (sampleCount > 0)
                                      ? (int16_t)(accumulator / sampleCount) : 0;

        // Map sample (±32767) to screen Y — 80% of available height
        // Integer arithmetic: multiply first to preserve precision, then divide
        const int rawY = centreY
                         - (int)((int32_t)averageSample * waveformHeight * 4
                                 / (32767 * 5));

        // Clamp to waveform bounds
        const int clampedY = (rawY < topY) ? topY : (rawY > bottomY) ? bottomY : rawY;

        // Connect to previous pixel with a line
        if (pixelX > 0) {
            _display.drawLine(previousX, previousY, pixelX, clampedY, COLOUR_ACCENT);
        }

        previousX = pixelX;
        previousY = clampedY;
    }
}


// =============================================================================
// findZeroCrossing()
// =============================================================================
// Searches the sample buffer for a rising-edge zero crossing to use as the
// oscilloscope trigger point.
//
// Search window: starts at 1/4 of the buffer (to allow trigger offset room)
// and ends 64 samples before the end (to ensure pixels to draw after trigger).
//
// Returns the crossing index, or numSamples/2 as a fallback so the waveform
// still draws even on pure DC or very low amplitude signals.

int UIManager_MicroDexed::findZeroCrossing(int16_t* samples, uint16_t numSamples) {
    if (numSamples < 4) return 0;

    const int searchStart = numSamples / 4;
    const int searchEnd   = (int)numSamples - 64;

    if (searchEnd <= searchStart) return numSamples / 2;   // buffer too short

    for (int i = searchStart; i < searchEnd; ++i) {
        // Rising edge: sample was at or below zero, next sample is above zero
        if (samples[i] <= 0 && samples[i + 1] > 0) {
            return i;
        }
    }

    // No crossing found — use midpoint so the waveform still renders
    return numSamples / 2;
}


// =============================================================================
// drawPeakMeters()
// =============================================================================
// Draws a vertical bar-graph level meter with three colour zones and dB labels.
//
// Colour zones (matching standard audio convention):
//   Green  (COLOUR_OSC)    : below -18 dB — plenty of headroom
//   Yellow (COLOUR_FILTER) : -18 to -6 dB — good working level
//   Red    (COLOUR_ACCENT) : above  -6 dB — approaching clipping
//
// dB labels are drawn to the right of the bar at 0 / -12 / -30 / -60 dB.

void UIManager_MicroDexed::drawPeakMeters(int x, int y, int width, int height) {

    // Read current peak from scope tap and immediately reset the detector
    const float peakLinear = scopeTap.readPeakAndClear();

    // Convert linear amplitude (0-1) to dB; clamp to -60..0 dB display range
    const float peakDB       = (peakLinear > 0.001f) ? 20.0f * log10f(peakLinear) : -60.0f;
    const float peakDBclamped = (peakDB < -60.0f) ? -60.0f : (peakDB > 0.0f) ? 0.0f : peakDB;

    // Map 0 dB -> full bar height, -60 dB -> zero bar height
    const int barFilledHeight = (int)(((peakDBclamped + 60.0f) / 60.0f) * height);

    // Meter background and outline border
    _display.fillRect(x, y, width, height, COLOUR_BACKGROUND);
    _display.drawRect(x, y, width, height, COLOUR_BORDER);

    if (barFilledHeight > 0) {
        // Calculate zone boundaries in pixels (measured from the BOTTOM of the bar)
        // -6 dB boundary: (60-6)/60 = 54/60 of full height
        // -18 dB boundary: (60-18)/60 = 42/60 of full height
        const int pixelsTo6dB  = (height * 54) / 60;
        const int pixelsTo18dB = (height * 42) / 60;

        const int barTopY = y + height - barFilledHeight;  // top pixel of the filled bar

        // Green segment — bottom of bar up to -18 dB boundary
        const int greenHeight = min(barFilledHeight, pixelsTo18dB);
        if (greenHeight > 0) {
            _display.fillRect(x + 2,
                              y + height - greenHeight,
                              width - 4, greenHeight,
                              COLOUR_OSC);
        }

        // Yellow segment — -18 dB boundary up to -6 dB boundary
        const int yellowTop    = min(barFilledHeight, pixelsTo6dB);
        const int yellowHeight = yellowTop - min(barFilledHeight, pixelsTo18dB);
        if (yellowHeight > 0) {
            _display.fillRect(x + 2,
                              y + height - yellowTop,
                              width - 4, yellowHeight,
                              COLOUR_FILTER);
        }

        // Red segment — above -6 dB boundary
        const int redHeight = barFilledHeight - min(barFilledHeight, pixelsTo6dB);
        if (redHeight > 0) {
            _display.fillRect(x + 2,
                              barTopY,
                              width - 4, redHeight,
                              COLOUR_ACCENT);
        }
    }

    // dB scale labels — positioned to the right of the bar
    _display.setTextColor(COLOUR_TEXT_DIM);
    _display.setTextSize(FONT_SMALL);

    _display.setCursor(x + width + 2, y);                           // 0 dB — top
    _display.print("0");

    _display.setCursor(x + width + 2, y + (height * 12) / 60);     // -12 dB
    _display.print("-12");

    _display.setCursor(x + width + 2, y + height / 2);             // -30 dB
    _display.print("-30");

    _display.setCursor(x + width + 2, y + height - 8);             // -60 dB — bottom
    _display.print("-60");

    // Column label above the meter
    _display.setTextColor(COLOUR_TEXT);
    _display.setCursor(x + 2, y - 11);
    _display.print("PEAK");
}


// =============================================================================
// drawVoiceActivity()
// =============================================================================
// Draws one horizontal bar per voice.  Active voices are lit green; idle
// voices show only a dim border.  Also prints the voice number alongside.
//
// Requires SynthEngine to expose isVoiceActive().
// Add the following to the PUBLIC section of SynthEngine.h:
//
//   inline bool isVoiceActive(uint8_t voiceIndex) const {
//       return (voiceIndex < MAX_VOICES) && (_activeNotes[voiceIndex] != 0);
//   }

void UIManager_MicroDexed::drawVoiceActivity(SynthEngine& synth,
                                              int x, int y, int width) {
    static constexpr int TOTAL_VOICES    = 8;   // must match MAX_VOICES in SynthEngine
    static constexpr int BAR_HEIGHT      = 5;   // pixel height of each voice bar
    static constexpr int BAR_GAP         = 2;   // pixel gap between consecutive bars
    static constexpr int LABEL_WIDTH_PX  = 12;  // width reserved for "1".."8" labels

    // "VOICES" section label
    _display.setTextColor(COLOUR_TEXT);
    _display.setTextSize(FONT_SMALL);
    _display.setCursor(x, y);
    _display.print("VOICES");

    const int barAreaWidth = width - LABEL_WIDTH_PX - 4; // bar width excluding label
    const int firstBarY    = y + 11;                     // top of bar row 0

    for (int voiceIndex = 0; voiceIndex < TOTAL_VOICES; ++voiceIndex) {
        const int barY     = firstBarY + voiceIndex * (BAR_HEIGHT + BAR_GAP);
        const bool isActive = synth.isVoiceActive((uint8_t)voiceIndex);

        // Bar: green when note is playing, dim border-only when idle
        _display.fillRect(x, barY, barAreaWidth, BAR_HEIGHT,
                          isActive ? COLOUR_OSC : COLOUR_BORDER);

        // Voice number label to the right of the bar
        _display.setTextColor(COLOUR_TEXT_DIM);
        char labelBuffer[3];
        snprintf(labelBuffer, sizeof(labelBuffer), "%d", voiceIndex + 1);
        _display.setCursor(x + barAreaWidth + 3, barY - 1);
        _display.print(labelBuffer);
    }
}
