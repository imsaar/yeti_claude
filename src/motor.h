#pragma once

#include <Arduino.h>
#include "config.h"

// One step in a vibration pattern: ms=0 is the end marker
struct VibeNote {
    uint16_t ms;  // duration; 0 = end marker
    bool     on;  // true = motor on, false = rest
};

// ─────────────────────────────────────────────────────────────────────────────
// MotorManager
//
//  Non-blocking vibration motor driver (GPIO, active-HIGH).
//  Call begin() once in setup(), update() every loop().
//  Call play(pattern) to start a pattern; stop() to abort.
// ─────────────────────────────────────────────────────────────────────────────
class MotorManager {
public:
    void begin();
    void play(VibePattern pattern);
    void stop();
    void update();  // must be called every loop()

private:
    bool            _active      = false;
    const VibeNote* _seq         = nullptr;
    uint8_t         _noteIdx     = 0;
    uint32_t        _noteStartMs = 0;

    static const VibeNote* getSequence(VibePattern p);
    void startNote(bool on);
};
