/**
 * TouchInput.h
 * 
 * Capacitive touch input support for ILI9341 with FT6206 controller.
 * 
 * Features:
 * - Touch point detection with coordinate mapping
 * - Tap, hold, and swipe gesture recognition
 * - Parameter hot-zones for direct access
 * - Touch-based preset browsing
 * 
 * Hardware:
 * - FT6206 capacitive touch controller (I2C)
 * - 320x240 touch overlay on ILI9341
 * 
 * Alternative: XPT2046 resistive touch (SPI-based)
 */

#pragma once
#include <Arduino.h>
#include <Wire.h>

// Conditional compilation for different touch controllers
// Uncomment one of these based on your hardware:
#define USE_CAPACITIVE_TOUCH   // FT6206 capacitive (I2C)
// #define USE_RESISTIVE_TOUCH    // XPT2046 resistive (SPI)

#ifdef USE_CAPACITIVE_TOUCH
  #include "Adafruit_FT6206.h"
#endif

#ifdef USE_RESISTIVE_TOUCH
  #include <XPT2046_Touchscreen.h>
#endif

class TouchInput {
public:
    // --- Touch point structure ---
    struct Point {
        int16_t x;
        int16_t y;
        bool valid;
        
        Point() : x(0), y(0), valid(false) {}
        Point(int16_t _x, int16_t _y) : x(_x), y(_y), valid(true) {}
    };

    // --- Gesture types ---
    enum Gesture {
        GESTURE_NONE,
        GESTURE_TAP,        // Quick touch and release
        GESTURE_HOLD,       // Touch and hold for >500ms
        GESTURE_SWIPE_UP,
        GESTURE_SWIPE_DOWN,
        GESTURE_SWIPE_LEFT,
        GESTURE_SWIPE_RIGHT
    };

    TouchInput();
    
    /**
     * Initialize touch controller.
     * Returns true if successful.
     */
    bool begin();
    
    /**
     * Update touch state.
     * Call frequently in main loop (100+ Hz).
     */
    void update();
    
    /**
     * Check if screen is currently being touched.
     */
    bool isTouched() const { return _isTouched; }
    
    /**
     * Get current touch point (if touched).
     * Returns Point with valid=true if touched, valid=false otherwise.
     */
    Point getTouchPoint() const;
    
    /**
     * Get detected gesture (consumes the gesture).
     */
    Gesture getGesture();
    
    /**
     * Check if a point is within a rectangular region.
     * Useful for button/parameter hit testing.
     */
    bool hitTest(int16_t x, int16_t y, int16_t w, int16_t h) const;

private:
    // --- Touch controller instance ---
#ifdef USE_CAPACITIVE_TOUCH
    Adafruit_FT6206 _touchController;
#endif

#ifdef USE_RESISTIVE_TOUCH
    XPT2046_Touchscreen _touchController;
    static constexpr uint8_t TOUCH_CS_PIN = 8;   // Touch chip select
    static constexpr uint8_t TOUCH_IRQ_PIN = 2;  // Touch interrupt
#endif

    // --- Touch state ---
    bool _isTouched;
    Point _currentPoint;
    Point _lastPoint;
    
    // --- Gesture detection ---
    Gesture _detectedGesture;
    Point _gestureStart;
    uint32_t _touchStartTime;
    uint32_t _touchEndTime;
    
    // --- Gesture timing thresholds ---
    static constexpr uint32_t TAP_MAX_DURATION = 300;   // ms
    static constexpr uint32_t HOLD_MIN_DURATION = 500;  // ms
    static constexpr uint32_t SWIPE_MIN_DISTANCE = 50;  // pixels
    static constexpr uint32_t SWIPE_MAX_DURATION = 500; // ms

    /**
     * Detect gesture from touch start/end points and timing.
     */
    void detectGesture();
    
    /**
     * Map raw touch coordinates to screen coordinates.
     * Some displays need rotation/flipping correction.
     */
    Point mapCoordinates(int16_t rawX, int16_t rawY);
};

// ============================================================================
// IMPLEMENTATION
// ============================================================================

TouchInput::TouchInput()
    : _isTouched(false)
    , _detectedGesture(GESTURE_NONE)
    , _touchStartTime(0)
    , _touchEndTime(0)
{
#ifdef USE_RESISTIVE_TOUCH
    // Resistive touch needs pin configuration in constructor
    // (Capacitive FT6206 uses I2C, no pins needed)
#endif
}

bool TouchInput::begin() {
#ifdef USE_CAPACITIVE_TOUCH
    // Initialize FT6206 capacitive touch controller
    if (!_touchController.begin(40)) {  // I2C address 0x38, max 40 touches
        Serial.println("FT6206 touch controller not found");
        return false;
    }
    Serial.println("FT6206 capacitive touch initialized");
    return true;
#endif

#ifdef USE_RESISTIVE_TOUCH
    // Initialize XPT2046 resistive touch controller
    _touchController.begin();
    _touchController.setRotation(1);  // Match display rotation
    Serial.println("XPT2046 resistive touch initialized");
    return true;
#endif

    return false;
}

void TouchInput::update() {
#ifdef USE_CAPACITIVE_TOUCH
    // Check if screen is touched
    if (_touchController.touched()) {
        TS_Point p = _touchController.getPoint();
        
        // Map to screen coordinates (may need calibration)
        _currentPoint = mapCoordinates(p.x, p.y);
        
        // Touch started
        if (!_isTouched) {
            _isTouched = true;
            _gestureStart = _currentPoint;
            _touchStartTime = millis();
            _detectedGesture = GESTURE_NONE;
        }
        
        _lastPoint = _currentPoint;
    } else {
        // Touch ended
        if (_isTouched) {
            _isTouched = false;
            _touchEndTime = millis();
            detectGesture();
        }
    }
#endif

#ifdef USE_RESISTIVE_TOUCH
    // Resistive touch update
    if (_touchController.touched()) {
        TS_Point p = _touchController.getPoint();
        
        // Map to screen coordinates
        _currentPoint = mapCoordinates(p.x, p.y);
        
        if (!_isTouched) {
            _isTouched = true;
            _gestureStart = _currentPoint;
            _touchStartTime = millis();
            _detectedGesture = GESTURE_NONE;
        }
        
        _lastPoint = _currentPoint;
    } else {
        if (_isTouched) {
            _isTouched = false;
            _touchEndTime = millis();
            detectGesture();
        }
    }
#endif
}

TouchInput::Point TouchInput::getTouchPoint() const {
    return _currentPoint;
}

TouchInput::Gesture TouchInput::getGesture() {
    Gesture g = _detectedGesture;
    _detectedGesture = GESTURE_NONE;  // Consume gesture
    return g;
}

bool TouchInput::hitTest(int16_t x, int16_t y, int16_t w, int16_t h) const {
    if (!_isTouched) return false;
    
    return (_currentPoint.x >= x && _currentPoint.x < x + w &&
            _currentPoint.y >= y && _currentPoint.y < y + h);
}

void TouchInput::detectGesture() {
    uint32_t duration = _touchEndTime - _touchStartTime;
    
    // Calculate touch movement
    int16_t dx = _lastPoint.x - _gestureStart.x;
    int16_t dy = _lastPoint.y - _gestureStart.y;
    int16_t distance = sqrt(dx * dx + dy * dy);
    
    // Detect gesture type
    if (distance < 10 && duration < TAP_MAX_DURATION) {
        // Quick tap with minimal movement
        _detectedGesture = GESTURE_TAP;
    }
    else if (distance < 10 && duration >= HOLD_MIN_DURATION) {
        // Hold in place
        _detectedGesture = GESTURE_HOLD;
    }
    else if (distance >= SWIPE_MIN_DISTANCE && duration < SWIPE_MAX_DURATION) {
        // Swipe - determine direction
        if (abs(dx) > abs(dy)) {
            // Horizontal swipe
            _detectedGesture = (dx > 0) ? GESTURE_SWIPE_RIGHT : GESTURE_SWIPE_LEFT;
        } else {
            // Vertical swipe
            _detectedGesture = (dy > 0) ? GESTURE_SWIPE_DOWN : GESTURE_SWIPE_UP;
        }
    }
    else {
        // No clear gesture
        _detectedGesture = GESTURE_NONE;
    }
}

TouchInput::Point TouchInput::mapCoordinates(int16_t rawX, int16_t rawY) {
    // Map raw touch coordinates to screen coordinates
    // This may need calibration based on your specific display/touch combo
    
#ifdef USE_CAPACITIVE_TOUCH
    // FT6206 typical mapping for 320x240 display in landscape mode
    // Adjust these values based on your hardware calibration
    int16_t x = map(rawX, 0, 240, 0, 320);
    int16_t y = map(rawY, 0, 320, 0, 240);
    
    // May need to flip axes depending on display orientation
    // Uncomment if touch is mirrored:
    // x = 320 - x;
    // y = 240 - y;
    
    return Point(x, y);
#endif

#ifdef USE_RESISTIVE_TOUCH
    // XPT2046 typical mapping
    // The library usually handles rotation, but you may need fine-tuning
    int16_t x = map(rawX, 300, 3800, 0, 320);  // Calibration values
    int16_t y = map(rawY, 300, 3800, 0, 240);
    
    return Point(x, y);
#endif

    return Point(0, 0);
}
