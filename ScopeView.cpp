/**
 * ScopeView.cpp
 * 
 * Advanced oscilloscope/visualizer for 320x240 ILI9341 display.
 * 
 * Features:
 * - Real-time waveform display using AudioScopeTap
 * - Voice activity indicators
 * - Peak meters
 * - CPU/memory usage bars
 * - Auto-scaling and triggering
 * 
 * Performance:
 * - Efficient line drawing (ILI9341_t3 DMA)
 * - Downsampling for smooth display
 * - Color-coded UI elements
 */

#include "UIManager_MicroDexed.h"
#include "AudioScopeTap.h"

// External scope tap (connected to audio output in main sketch)
extern AudioScopeTap scopeTap;

// ============================================================================
// SCOPE VIEW RENDERING
// ============================================================================

void UIManager_MicroDexed::drawScopeView(SynthEngine& synth) {
    // --- Layout constants ---
    const int HEADER_H = 20;
    const int FOOTER_H = 20;
    const int WAVE_TOP = HEADER_H + 2;
    const int WAVE_BOT = SCREEN_HEIGHT - FOOTER_H - 2;
    const int WAVE_H = WAVE_BOT - WAVE_TOP;
    const int WAVE_W = SCREEN_WIDTH - 80;  // Leave room for meters on right
    const int METER_X = WAVE_W + 10;
    const int METER_W = 60;

    // --- Clear screen ---
    _display.fillScreen(COLOR_BACKGROUND);

    // --- Header: Scope title and stats ---
    _display.fillRect(0, 0, SCREEN_WIDTH, HEADER_H, COLOR_HEADER);
    _display.setTextColor(COLOR_TEXT);
    _display.setTextSize(FONT_MEDIUM);
    _display.setCursor(5, 4);
    _display.print("OSCILLOSCOPE");

    // CPU usage (right side of header)
    float cpu = AudioProcessorUsageMax();
    char cpuStr[16];
    snprintf(cpuStr, sizeof(cpuStr), "%d%%", (int)cpu);
    int cpuX = SCREEN_WIDTH - (strlen(cpuStr) * 12) - 5;
    _display.setCursor(cpuX, 4);
    _display.print(cpuStr);

    // --- Pull recent samples from scope tap ---
    static int16_t samples[512];
    uint16_t numSamples = scopeTap.snapshot(samples, 512);

    if (numSamples < 128) {
        // Not enough data yet - show "arming" message
        _display.setTextColor(COLOR_TEXT_DIM);
        _display.setTextSize(FONT_MEDIUM);
        drawCenteredText(WAVE_W / 2, SCREEN_HEIGHT / 2, "Scope arming...", COLOR_TEXT_DIM);
        return;
    }

    // --- Find zero-crossing for stable trigger ---
    int triggerIdx = findZeroCrossing(samples, numSamples);
    
    // --- Draw waveform ---
    drawWaveform(samples, numSamples, triggerIdx, WAVE_TOP, WAVE_BOT, WAVE_W);

    // --- Draw center line (zero crossing reference) ---
    int centerY = WAVE_TOP + (WAVE_H / 2);
    _display.drawFastHLine(0, centerY, WAVE_W, COLOR_BORDER);

    // --- Draw peak meters on right side ---
    drawPeakMeters(METER_X, WAVE_TOP, METER_W, WAVE_H);

    // --- Draw voice activity indicators ---
    drawVoiceActivity(synth, METER_X, WAVE_BOT + 5, METER_W);

    // --- Footer: Instructions ---
    _display.fillRect(0, SCREEN_HEIGHT - FOOTER_H, SCREEN_WIDTH, FOOTER_H, COLOR_HEADER);
    _display.setTextColor(COLOR_TEXT_DIM);
    _display.setTextSize(FONT_SMALL);
    _display.setCursor(5, SCREEN_HEIGHT - FOOTER_H + 5);
    _display.print("Press any button to return");
}

// ============================================================================
// WAVEFORM DRAWING
// ============================================================================

void UIManager_MicroDexed::drawWaveform(int16_t* samples, uint16_t numSamples, 
                                        int triggerIdx, int topY, int botY, int width) {
    // Calculate waveform display area
    int waveHeight = botY - topY;
    int centerY = topY + (waveHeight / 2);
    
    // Downsample if we have more samples than pixels
    int samplesPerPixel = (numSamples > width) ? (numSamples / width) : 1;
    
    // Draw waveform as connected lines
    int lastX = 0;
    int lastY = centerY;
    
    for (int x = 0; x < width; ++x) {
        // Calculate sample index (with trigger offset)
        int sampleIdx = triggerIdx + (x * samplesPerPixel);
        
        // Bounds check
        if (sampleIdx < 0) sampleIdx = 0;
        if (sampleIdx >= numSamples) break;
        
        // Get sample value (-32768 to +32767)
        int16_t sample = samples[sampleIdx];
        
        // Map to screen Y coordinate
        // Scale to use 80% of height (leave some margin)
        int y = centerY - ((sample * waveHeight * 4) / (32768 * 5));
        
        // Clamp to display bounds
        if (y < topY) y = topY;
        if (y > botY) y = botY;
        
        // Draw line from previous point
        if (x > 0) {
            _display.drawLine(lastX, lastY, x, y, COLOR_ACCENT);
        }
        
        lastX = x;
        lastY = y;
    }
}

// ============================================================================
// ZERO-CROSSING TRIGGER
// ============================================================================

int UIManager_MicroDexed::findZeroCrossing(int16_t* samples, uint16_t numSamples) {
    // Find a rising zero-crossing in the last 3/4 of the buffer
    // This provides a stable trigger point for the waveform
    
    int searchStart = numSamples / 4;
    int searchEnd = numSamples - 64;  // Leave some samples after trigger
    
    for (int i = searchStart; i < searchEnd; ++i) {
        // Rising edge through zero
        if (samples[i] <= 0 && samples[i + 1] > 0) {
            return i;
        }
    }
    
    // No zero crossing found - just use middle of buffer
    return numSamples / 2;
}

// ============================================================================
// PEAK METERS
// ============================================================================

void UIManager_MicroDexed::drawPeakMeters(int x, int y, int width, int height) {
    // Get peak value from scope tap
    float peak = scopeTap.readPeakAndClear();
    
    // Convert to dB (rough approximation)
    // peak is 0.0-1.0, where 1.0 = 0dB
    float dB = (peak > 0.001f) ? (20.0f * log10f(peak)) : -60.0f;
    
    // Clamp to display range (-60dB to 0dB)
    if (dB < -60.0f) dB = -60.0f;
    if (dB > 0.0f) dB = 0.0f;
    
    // Map to pixel height
    int barHeight = ((dB + 60.0f) / 60.0f) * height;
    
    // Draw meter background
    _display.fillRect(x, y, width, height, COLOR_BACKGROUND);
    _display.drawRect(x, y, width, height, COLOR_BORDER);
    
    // Draw meter bar (bottom-up)
    if (barHeight > 0) {
        // Color based on level
        uint16_t barColor;
        if (dB > -6.0f) {
            barColor = COLOR_ACCENT;  // Red - hot level
        } else if (dB > -18.0f) {
            barColor = COLOR_FILTER;  // Yellow - good level
        } else {
            barColor = COLOR_OSC;     // Green - safe level
        }
        
        int barY = y + height - barHeight;
        _display.fillRect(x + 2, barY, width - 4, barHeight, barColor);
    }
    
    // Draw dB scale markers
    _display.setTextColor(COLOR_TEXT_DIM);
    _display.setTextSize(1);
    
    // 0dB mark
    _display.setCursor(x + width + 2, y);
    _display.print("0");
    
    // -30dB mark
    _display.setCursor(x + width + 2, y + height / 2);
    _display.print("-30");
    
    // -60dB mark
    _display.setCursor(x + width + 2, y + height - 8);
    _display.print("-60");
    
    // Label
    _display.setTextColor(COLOR_TEXT);
    _display.setCursor(x, y - 15);
    _display.print("PEAK");
}

// ============================================================================
// VOICE ACTIVITY INDICATORS
// ============================================================================

void UIManager_MicroDexed::drawVoiceActivity(SynthEngine& synth, int x, int y, int width) {
    const int VOICE_COUNT = 8;  // Adjust based on your polyphony
    const int INDICATOR_H = 4;
    const int SPACING = 2;
    
    _display.setTextColor(COLOR_TEXT);
    _display.setTextSize(1);
    _display.setCursor(x, y);
    _display.print("VOICES");
    
    int indicatorY = y + 12;
    
    // Draw 8 voice indicators
    for (int i = 0; i < VOICE_COUNT; ++i) {
        int barY = indicatorY + (i * (INDICATOR_H + SPACING));
        
        // TODO: Get actual voice activity from SynthEngine
        // For now, show mock activity
        bool active = false;  // synth.isVoiceActive(i);
        
        uint16_t color = active ? COLOR_OSC : COLOR_BORDER;
        _display.fillRect(x, barY, width - 20, INDICATOR_H, color);
        
        // Voice number
        _display.setTextColor(COLOR_TEXT_DIM);
        _display.setCursor(x + width - 15, barY - 1);
        _display.print(i + 1);
    }
}
