/**
 * TouchInput.h
 *
 * Capacitive/resistive touch input for ILI9341 + FT6206.
 *
 * Gesture detection: tap, hold, swipe (up/down/left/right).
 * Hit-test helpers for parameter rows and buttons.
 *
 * IMPORTANT — implementations live in TouchInput.cpp.
 * Do NOT put function bodies in this header; doing so causes
 * "multiple definition" linker errors when more than one .cpp
 * file includes this header.
 */

#pragma once
#include <Arduino.h>
#include <Wire.h>

// Select touch controller — uncomment exactly one:
#define USE_CAPACITIVE_TOUCH   // FT6206 (I2C)
// #define USE_RESISTIVE_TOUCH // XPT2046 (SPI)

#ifdef USE_CAPACITIVE_TOUCH
  #include "Adafruit_FT6206.h"
#endif
#ifdef USE_RESISTIVE_TOUCH
  #include <XPT2046_Touchscreen.h>
#endif

class TouchInput {
public:
    // -------------------------------------------------------------------------
    // Touch point
    // -------------------------------------------------------------------------
    struct Point {
        int16_t x;
        int16_t y;
        bool    valid;

        Point()                        : x(0),  y(0),  valid(false) {}
        Point(int16_t _x, int16_t _y) : x(_x), y(_y), valid(true)  {}
    };

    // -------------------------------------------------------------------------
    // Gesture types
    // -------------------------------------------------------------------------
    enum Gesture {
        GESTURE_NONE,
        GESTURE_TAP,           // Short touch, minimal movement
        GESTURE_HOLD,          // Touch held > HOLD_MIN_DURATION ms
        GESTURE_SWIPE_UP,
        GESTURE_SWIPE_DOWN,
        GESTURE_SWIPE_LEFT,
        GESTURE_SWIPE_RIGHT
    };

    // -------------------------------------------------------------------------
    // Timing / distance thresholds
    // -------------------------------------------------------------------------
    static constexpr uint32_t TAP_MAX_DURATION   = 300;   // ms
    static constexpr uint32_t HOLD_MIN_DURATION  = 500;   // ms
    static constexpr int16_t  SWIPE_MIN_DISTANCE = 50;    // pixels (signed, matches dx/dy type)
    static constexpr uint32_t SWIPE_MAX_DURATION = 500;   // ms

    // -------------------------------------------------------------------------
    // API
    // -------------------------------------------------------------------------
    TouchInput();

    /** Initialise touch controller. Returns true on success. */
    bool begin();

    /** Poll touch hardware — call at 100+ Hz in main loop. */
    void update();

    /** True while the screen is being touched. */
    bool isTouched() const { return _isTouched; }

    /** Current touch position (valid flag false when not touched). */
    Point   getTouchPoint() const;

    /**
     * Return and consume the last detected gesture.
     * Returns GESTURE_NONE if no gesture since last call.
     */
    Gesture getGesture();

    /**
     * Returns true if the current touch point is inside the given rect.
     * (x, y) = top-left corner, (w, h) = dimensions.
     */
    bool hitTest(int16_t x, int16_t y, int16_t w, int16_t h) const;

private:
    // -------------------------------------------------------------------------
    // Hardware driver
    // -------------------------------------------------------------------------
#ifdef USE_CAPACITIVE_TOUCH
    Adafruit_FT6206 _touchController;
#endif
#ifdef USE_RESISTIVE_TOUCH
    XPT2046_Touchscreen _touchController;
    static constexpr uint8_t TOUCH_CS_PIN  = 8;
    static constexpr uint8_t TOUCH_IRQ_PIN = 2;
#endif

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------
    bool    _isTouched;
    Point   _currentPoint;
    Point   _lastPoint;

    Gesture  _detectedGesture;
    Point    _gestureStart;
    uint32_t _touchStartTime;
    uint32_t _touchEndTime;

    // -------------------------------------------------------------------------
    // Private helpers — defined in TouchInput.cpp
    // -------------------------------------------------------------------------

    /** Classify a completed touch into a Gesture. */
    void  detectGesture();

    /**
     * Map raw hardware coordinates to screen coordinates.
     * Adjust calibration values in TouchInput.cpp if touch feels misaligned.
     */
    Point mapCoordinates(int16_t rawX, int16_t rawY);
};
