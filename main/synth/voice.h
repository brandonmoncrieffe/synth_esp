#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "synth/envelope.h"
#include "synth/oscillator.h"

typedef struct {
    bool active;
    uint8_t midi_note;
    float velocity;
    uint32_t age;
    oscillator_t oscillator;
    envelope_t amp_envelope;
} synth_voice_t;

void synth_voice_init(synth_voice_t *voice, float sample_rate);
void synth_voice_note_on(synth_voice_t *voice, uint8_t midi_note, uint8_t velocity, uint32_t age);
void synth_voice_note_off(synth_voice_t *voice);
float synth_voice_render(synth_voice_t *voice);
