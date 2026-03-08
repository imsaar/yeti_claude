#pragma once

#include <Arduino.h>
#include "config.h"

// ─────────────────────────────────────────────────────────────────────────────
// TouchHandler
//
//  Reads the TTP223 capacitive sensor and classifies gestures:
//  • Single tap    – brief press < 1 s
//  • Double tap    – two taps within DOUBLE_TAP_WINDOW_MS
//  • Long press    – held ≥ LONG_PRESS_MS
//
//  Call poll() every loop(). It returns a TouchEvent only once per gesture.
// ─────────────────────────────────────────────────────────────────────────────
class TouchHandler {
public:
    TouchHandler() = default;

    void       begin();
    TouchEvent poll();   // call every loop() — returns TOUCH_NONE most of the time

private:
    // Internal state
    bool     _wasPressed     = false;
    uint32_t _pressStart     = 0;
    uint8_t  _tapCount       = 0;
    bool     _mediumFired    = false;   // prevent repeated medium-press events
    bool     _longFired      = false;   // prevent repeated long-press events
    bool     _pendingSingle  = false;   // single tap is "pending" until double-tap window closes
    uint32_t _pendingStart   = 0;

    // Non-blocking debounce state
    bool     _lastPhys       = false;
    uint32_t _lastPhysChange = 0;

    bool rawPressed();
};
