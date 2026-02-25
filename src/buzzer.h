#pragma once

#include <Arduino.h>
#include "config.h"

// One note in a sound pattern: freq=0 means silence, ms=0 means end-of-sequence
struct BuzzNote {
    uint16_t freq;  // Hz
    uint16_t ms;    // duration; 0 = end marker
};

// ─────────────────────────────────────────────────────────────────────────────
// BuzzerManager
//
//  Non-blocking piezo buzzer driver. Uses Arduino tone()/noTone() (LEDC).
//  Call begin() once in setup(), update() every loop().
//  Call play(pattern) to start a sound; stop() to abort.
// ─────────────────────────────────────────────────────────────────────────────
class BuzzerManager {
public:
    void begin();
    void play(BuzzPattern pattern);
    void stop();
    void update();  // must be called every loop()

private:
    bool              _active      = false;
    const BuzzNote*   _seq         = nullptr;
    uint8_t           _noteIdx     = 0;
    uint32_t          _noteStartMs = 0;

    static const BuzzNote* getSequence(BuzzPattern p);
    void startNote(uint16_t freq);
};
