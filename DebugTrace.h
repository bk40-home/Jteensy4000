// DebugTrace.h
// ---------------------------------------------------------------------------
// Tiny, zero-dependency serial tracing. Enable by defining JT_DEBUG_TRACE 1.
// ---------------------------------------------------------------------------
#pragma once
#include <Arduino.h>

#ifndef JT_DEBUG_TRACE
#define JT_DEBUG_TRACE 1   // set to 0 to completely strip logs at compile time
#endif

#if JT_DEBUG_TRACE
  #define JT_LOGF(fmt, ...)    do { Serial.printf(fmt, ##__VA_ARGS__); } while (0)
  #define JT_LOGNL()           do { Serial.println(); } while (0)
  // Log only when a float actually changes by a small epsilon
  #define JT_SETF_WITH_LOG(var, newv, label)                                      \
    do {                                                                          \
      float __old = (var); float __nv = (newv);                                   \
      if (fabsf(__old - __nv) > 1e-6f) {                                          \
        (var) = __nv;                                                             \
        Serial.printf("[ENG] %s: %.6f -> %.6f\n", (label), __old, __nv);          \
      }                                                                           \
    } while (0)
#else
  #define JT_LOGF(...)        do{}while(0)
  #define JT_LOGNL()          do{}while(0)
  #define JT_SETF_WITH_LOG(v,n,l) do { (v) = (n); } while (0)
#endif
