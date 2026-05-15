#include "voice.h"

#include <math.h>

#include "audio/audio_config.h"

static float synth_voice_midi_note_to_frequency(uint8_t midi_note)
{
    return 440.0f * powf(2.0f, ((float)midi_note - 69.0f) / 12.0f);
}

void synth_voice_init(synth_voice_t *voice, float sample_rate)
{
    voice->active = false;
    voice->pending_note_on = false;
    voice->midi_note = 0;
    voice->pending_midi_note = 0;
    voice->velocity = 0.0f;
    voice->pending_velocity = 0.0f;
    voice->steal_fade_gain = 1.0f;
    if (SYNTH_VOICE_STEAL_FADE_TIME_SEC <= 0.0f || sample_rate <= 0.0f) {
        voice->steal_fade_increment = 1.0f;
    } else {
        voice->steal_fade_increment = 1.0f / (SYNTH_VOICE_STEAL_FADE_TIME_SEC * sample_rate);
    }
    voice->age = 0;
    voice->pending_age = 0;
    envelope_init(&voice->amp_envelope, sample_rate);
    oscillator_init(&voice->oscillator, sample_rate, 440.0f);
}

void synth_voice_note_on(synth_voice_t *voice, uint8_t midi_note, uint8_t velocity, uint32_t age)
{
    voice->active = true;
    voice->pending_note_on = false;
    voice->midi_note = midi_note;
    voice->velocity = (float)velocity / 127.0f;
    voice->steal_fade_gain = 1.0f;
    voice->age = age;

    oscillator_set_frequency(&voice->oscillator, synth_voice_midi_note_to_frequency(midi_note));
    oscillator_reset_phase(&voice->oscillator);
    envelope_note_on(&voice->amp_envelope);
}

void synth_voice_queue_note_on(synth_voice_t *voice, uint8_t midi_note, uint8_t velocity, uint32_t age)
{
    if (!voice->active) {
        synth_voice_note_on(voice, midi_note, velocity, age);
        return;
    }

    voice->pending_note_on = true;
    voice->pending_midi_note = midi_note;
    voice->pending_velocity = (float)velocity / 127.0f;
    voice->pending_age = age;
    voice->steal_fade_gain = 1.0f;
}

void synth_voice_note_off(synth_voice_t *voice)
{
    if (!voice->active) {
        voice->pending_note_on = false;
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
    float steal_fade_gain = voice->steal_fade_gain;
    bool start_pending_note = false;

    if (voice->pending_note_on) {
        steal_fade_gain -= voice->steal_fade_increment;
        if (steal_fade_gain <= 0.0f) {
            steal_fade_gain = 0.0f;
            start_pending_note = true;
        }
        voice->steal_fade_gain = steal_fade_gain;
    }

    if (!envelope_is_active(&voice->amp_envelope)) {
        voice->active = false;
        if (voice->pending_note_on) {
            start_pending_note = true;
        } else {
            return 0.0f;
        }
    }

    float sample = oscillator_render_sine(&voice->oscillator) * amplitude * voice->velocity * steal_fade_gain;

    if (start_pending_note) {
        uint8_t midi_note = voice->pending_midi_note;
        uint8_t velocity = (uint8_t)(voice->pending_velocity * 127.0f);
        uint32_t age = voice->pending_age;
        synth_voice_note_on(voice, midi_note, velocity, age);
    }

    return sample;
}
