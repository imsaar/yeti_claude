#include "motor.h"

// ─── Vibration pattern definitions ────────────────────────────────────────────
// {ms, on}, terminated by {0, false}

// Matches buzzer SEQ_PURR: 25 ms on / 12 ms off × 24 cycles ≈ 888 ms
static const VibeNote SEQ_PURR[] = {
    {25,true},{12,false},{25,true},{12,false},{25,true},{12,false},{25,true},{12,false},
    {25,true},{12,false},{25,true},{12,false},{25,true},{12,false},{25,true},{12,false},
    {25,true},{12,false},{25,true},{12,false},{25,true},{12,false},{25,true},{12,false},
    {25,true},{12,false},{25,true},{12,false},{25,true},{12,false},{25,true},{12,false},
    {25,true},{12,false},{25,true},{12,false},{25,true},{12,false},{25,true},{12,false},
    {25,true},{12,false},{25,true},{12,false},{25,true},{12,false},{25,true},{0,false},
};

static const VibeNote SEQ_BOOT[]        = {{250,true},{0,false}};
static const VibeNote SEQ_TAP[]         = {{40, true},{0,false}};
static const VibeNote SEQ_DOUBLE[]      = {{40, true},{60,false},{40,true},{0,false}};
static const VibeNote SEQ_LONG_PRESS[]  = {{400,true},{0,false}};
static const VibeNote SEQ_ALERT[]       = {{80, true},{80,false},{80,true},{80,false},{80,true},{0,false}};

// Mirrors the Star Wars buzzer sequence: on during each note, off during each rest
static const VibeNote SEQ_STARWARS[] = {
    // Phrase 1: G G G Eb(dq) Bb(e) G Eb(dq) Bb(e) G(h)
    {480,true},{30,false}, {480,true},{30,false}, {480,true},{30,false},
    {720,true},{30,false}, {220,true},{20,false},
    {480,true},{30,false}, {720,true},{30,false}, {220,true},{20,false},
    {900,true},{80,false},
    // Phrase 2: D5 D5 D5 Eb5(dq) Bb4(e) G4 Eb4(dq) Bb4(e) G4(h)
    {480,true},{30,false}, {480,true},{30,false}, {480,true},{30,false},
    {720,true},{30,false}, {220,true},{20,false},
    {480,true},{30,false}, {720,true},{30,false}, {220,true},{20,false},
    {900,true},{0,false},  // end
};

// ─────────────────────────────────────────────────────────────────────────────

void MotorManager::begin() {
    pinMode(MOTOR_PIN, OUTPUT);
    digitalWrite(MOTOR_PIN, LOW);
}

const VibeNote* MotorManager::getSequence(VibePattern p) {
    switch (p) {
        case VIBE_BOOT:        return SEQ_BOOT;
        case VIBE_TAP:         return SEQ_TAP;
        case VIBE_DOUBLE_TAP:  return SEQ_DOUBLE;
        case VIBE_LONG_PRESS:  return SEQ_LONG_PRESS;
        case VIBE_ALERT:       return SEQ_ALERT;
        case VIBE_STARWARS:    return SEQ_STARWARS;
        case VIBE_PURR:        return SEQ_PURR;
        default:               return nullptr;
    }
}

void MotorManager::startNote(bool on) {
    digitalWrite(MOTOR_PIN, on ? HIGH : LOW);
}

void MotorManager::play(VibePattern pattern) {
    const VibeNote* seq = getSequence(pattern);
    if (!seq) return;
    stop();
    _seq         = seq;
    _noteIdx     = 0;
    _active      = true;
    _noteStartMs = millis();
    startNote(_seq[0].on);
}

void MotorManager::stop() {
    digitalWrite(MOTOR_PIN, LOW);
    _active  = false;
    _seq     = nullptr;
    _noteIdx = 0;
}

void MotorManager::update() {
    if (!_active || !_seq) return;
    const VibeNote& note = _seq[_noteIdx];
    if (note.ms == 0) { stop(); return; }
    if (millis() - _noteStartMs >= note.ms) {
        _noteIdx++;
        const VibeNote& next = _seq[_noteIdx];
        if (next.ms == 0) { stop(); return; }
        startNote(next.on);
        _noteStartMs = millis();
    }
}
