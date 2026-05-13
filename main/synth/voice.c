#include "voice.h"

#include <math.h>

static float synth_voice_midi_note_to_frequency(uint8_t midi_note)
{
    return 440.0f * powf(2.0f, ((float)midi_note - 69.0f) / 12.0f);
}

void synth_voice_init(synth_voice_t *voice, float sample_rate)
{
    voice->active = false;
    voice->midi_note = 0;
    voice->velocity = 0.0f;
    voice->age = 0;
    envelope_init(&voice->amp_envelope, sample_rate);
    oscillator_init(&voice->oscillator, sample_rate, 440.0f);
}

void synth_voice_note_on(synth_voice_t *voice, uint8_t midi_note, uint8_t velocity, uint32_t age)
{
    voice->active = true;
    voice->midi_note = midi_note;
    voice->velocity = (float)velocity / 127.0f;
    voice->age = age;

    oscillator_set_frequency(&voice->oscillator, synth_voice_midi_note_to_frequency(midi_note));
    envelope_note_on(&voice->amp_envelope);
}

void synth_voice_note_off(synth_voice_t *voice)
{
    if (!voice->active) {
        return;
    }

    envelope_note_off(&voice->amp_envelope);
}

float synth_voice_render(synth_voice_t *voice)
{
    if (!voice->active) {
        return 0.0f;
    }

    float amplitude = envelope_render(&voice->amp_envelope);
    if (!envelope_is_active(&voice->amp_envelope)) {
        voice->active = false;
        return 0.0f;
    }

    return oscillator_render_sine(&voice->oscillator) * amplitude * voice->velocity;
}
