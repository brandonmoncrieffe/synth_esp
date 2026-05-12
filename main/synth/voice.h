#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "synth/envelope.h"
#include "synth/oscillator.h"

#define SYNTH_OSCILLATORS_PER_VOICE 3

typedef struct {
    bool active;
    uint8_t midi_note;
    float velocity;
    oscillator_t oscillators[SYNTH_OSCILLATORS_PER_VOICE];
    envelope_t amp_envelope;
} synth_voice_t;

void synth_voice_init(synth_voice_t *voice, float sample_rate);
