#include "buzzer.h"

// ─── Sound pattern definitions ────────────────────────────────────────────────
// {freq_Hz, duration_ms}, terminated by {0, 0}
// freq=0 in a non-terminal entry = rest (silence for that duration)

// C5=523 D5=587 E5=659 G5=784 A5=880 C6=1047
// G4=392 Eb4=311 Bb4=466 D5=587 Eb5=622  (Star Wars notes)

// Star Wars main theme — two phrases, ~108 BPM
// Rhythm key: q=quarter(480+30ms), dq=dotted-quarter(720+30ms),
//             e=eighth(220+20ms), h=half(900+80ms phrase-end)
static const BuzzNote SEQ_STARWARS[] = {
    // -- Phrase 1: G G G Eb(dq) Bb(e) G Eb(dq) Bb(e) G(h) --
    {392,480},{0,30}, {392,480},{0,30}, {392,480},{0,30},
    {311,720},{0,30}, {466,220},{0,20},
    {392,480},{0,30}, {311,720},{0,30}, {466,220},{0,20},
    {392,900},{0,80},
    // -- Phrase 2 (higher): D5 D5 D5 Eb5(dq) Bb4(e) G4 Eb4(dq) Bb4(e) G4(h) --
    {587,480},{0,30}, {587,480},{0,30}, {587,480},{0,30},
    {622,720},{0,30}, {466,220},{0,20},
    {392,480},{0,30}, {311,720},{0,30}, {466,220},{0,20},
    {392,900},{0,0},  // end
};

static const BuzzNote SEQ_BOOT[]       = {{523,80},{659,80},{784,80},{1047,160},{0,0}};
static const BuzzNote SEQ_TAP[]        = {{1200,35},{0,0}};
static const BuzzNote SEQ_DOUBLE[]     = {{1200,35},{0,35},{1400,35},{0,0}};
static const BuzzNote SEQ_LONG[]       = {{523,90},{0,20},{659,90},{0,20},{784,90},{0,20},{1047,220},{0,0}};
static const BuzzNote SEQ_HAPPY[]      = {{523,70},{659,70},{784,70},{1047,160},{0,0}};
static const BuzzNote SEQ_SAD[]        = {{523,110},{440,110},{392,110},{349,220},{0,0}};
static const BuzzNote SEQ_ALERT[]      = {{880,90},{0,55},{880,90},{0,55},{880,160},{0,0}};

// ─────────────────────────────────────────────────────────────────────────────

#define BUZZ_CH  0   // LEDC channel reserved for buzzer
#define BUZZ_RES 10  // 10-bit resolution

void BuzzerManager::begin() {
    ledcSetup(BUZZ_CH, 1000, BUZZ_RES);
    ledcAttachPin(BUZZER_PIN, BUZZ_CH);
    ledcWrite(BUZZ_CH, 0);
}

const BuzzNote* BuzzerManager::getSequence(BuzzPattern p) {
    switch (p) {
        case BUZZ_BOOT:       return SEQ_BOOT;
        case BUZZ_TAP:        return SEQ_TAP;
        case BUZZ_DOUBLE_TAP: return SEQ_DOUBLE;
        case BUZZ_LONG_PRESS: return SEQ_LONG;
        case BUZZ_HAPPY:      return SEQ_HAPPY;
        case BUZZ_SAD:        return SEQ_SAD;
        case BUZZ_ALERT:      return SEQ_ALERT;
        case BUZZ_STARWARS:   return SEQ_STARWARS;
        default:              return nullptr;
    }
}

void BuzzerManager::startNote(uint16_t freq) {
    ledcWriteTone(BUZZ_CH, freq);
}

void BuzzerManager::play(BuzzPattern pattern) {
    const BuzzNote* seq = getSequence(pattern);
    if (!seq) return;
    stop();
    _seq         = seq;
    _noteIdx     = 0;
    _active      = true;
    _noteStartMs = millis();
    startNote(_seq[0].freq);
}

void BuzzerManager::stop() {
    ledcWriteTone(BUZZ_CH, 0);
    _active  = false;
    _seq     = nullptr;
    _noteIdx = 0;
}

void BuzzerManager::update() {
    if (!_active || !_seq) return;
    const BuzzNote& note = _seq[_noteIdx];
    if (note.ms == 0) { stop(); return; }  // end marker
    if (millis() - _noteStartMs >= note.ms) {
        _noteIdx++;
        const BuzzNote& next = _seq[_noteIdx];
        if (next.ms == 0) { stop(); return; }  // end marker
        startNote(next.freq);
        _noteStartMs = millis();
    }
}
