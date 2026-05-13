#include "synth_engine.h"

static int32_t synth_float_to_i2s_sample(float sample)
{
    if (sample > 1.0f) {
        sample = 1.0f;
    } else if (sample < -1.0f) {
        sample = -1.0f;
    }

    int32_t pcm24 = (int32_t)(sample * (float)SYNTH_PCM_MAX_24);
    if (pcm24 > SYNTH_PCM_MAX_24) {
        pcm24 = SYNTH_PCM_MAX_24;
    } else if (pcm24 < SYNTH_PCM_MIN_24) {
        pcm24 = SYNTH_PCM_MIN_24;
    }

    return pcm24 << SYNTH_I2S_24BIT_LEFT_SHIFT;
}

static bool synth_engine_is_supported_note(uint8_t midi_note)
{
    return midi_note >= SYNTH_ACTIVE_OCTAVE_BASE_NOTE &&
           midi_note < (SYNTH_ACTIVE_OCTAVE_BASE_NOTE + SYNTH_CHROMATIC_NOTE_COUNT);
}

static synth_voice_t *synth_engine_find_voice_by_note(synth_engine_t *engine, uint8_t midi_note)
{
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        if (engine->voices[i].active && engine->voices[i].midi_note == midi_note) {
            return &engine->voices[i];
        }
    }

    return NULL;
}

static synth_voice_t *synth_engine_find_free_voice(synth_engine_t *engine)
{
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        if (!engine->voices[i].active) {
            return &engine->voices[i];
        }
    }

    return NULL;
}

static synth_voice_t *synth_engine_find_oldest_voice(synth_engine_t *engine)
{
    synth_voice_t *oldest_voice = &engine->voices[0];

    for (size_t i = 1; i < SYNTH_MAX_VOICES; ++i) {
        if (engine->voices[i].age < oldest_voice->age) {
            oldest_voice = &engine->voices[i];
        }
    }

    return oldest_voice;
}

void synth_engine_init(synth_engine_t *engine, float sample_rate)
{
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_voice_init(&engine->voices[i], sample_rate);
    }

    engine->master_volume = SYNTH_DEFAULT_MASTER_VOLUME;
    engine->next_voice_age = 1;
}

void synth_engine_note_on(synth_engine_t *engine, uint8_t midi_note, uint8_t velocity)
{
    if (velocity == 0U) {
        synth_engine_note_off(engine, midi_note);
        return;
    }

    if (!synth_engine_is_supported_note(midi_note)) {
        return;
    }

    synth_voice_t *voice = synth_engine_find_voice_by_note(engine, midi_note);
    if (voice == NULL) {
        voice = synth_engine_find_free_voice(engine);
    }
    if (voice == NULL) {
        voice = synth_engine_find_oldest_voice(engine);
    }

    synth_voice_note_on(voice, midi_note, velocity, engine->next_voice_age++);
}

void synth_engine_note_off(synth_engine_t *engine, uint8_t midi_note)
{
    synth_voice_t *voice = synth_engine_find_voice_by_note(engine, midi_note);
    if (voice != NULL) {
        synth_voice_note_off(voice);
    }
}

void synth_engine_all_notes_off(synth_engine_t *engine)
{
    for (size_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        synth_voice_note_off(&engine->voices[i]);
    }
}

void synth_engine_set_master_volume(synth_engine_t *engine, float master_volume)
{
    if (master_volume < 0.0f) {
        master_volume = 0.0f;
    } else if (master_volume > 1.0f) {
        master_volume = 1.0f;
    }

    engine->master_volume = master_volume;
}

void synth_engine_render_i32(synth_engine_t *engine, int32_t *stereo_frames, size_t frame_count)
{
    for (size_t i = 0; i < frame_count; ++i) {
        float mono = 0.0f;
        size_t active_voice_count = 0;

        for (size_t voice_index = 0; voice_index < SYNTH_MAX_VOICES; ++voice_index) {
            if (engine->voices[voice_index].active) {
                ++active_voice_count;
            }
            mono += synth_voice_render(&engine->voices[voice_index]);
        }

        if (active_voice_count > 0U) {
            mono *= (SYNTH_MIX_GAIN / (float)active_voice_count) * engine->master_volume;
        }

        int32_t sample = synth_float_to_i2s_sample(mono);

        stereo_frames[i * SYNTH_CHANNEL_COUNT] = sample;
        stereo_frames[i * SYNTH_CHANNEL_COUNT + 1] = sample;
    }
}
