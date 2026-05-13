#pragma once

#include <stddef.h>
#include <stdint.h>

#include "audio/audio_config.h"
#include "synth/voice.h"

typedef struct {
    synth_voice_t voices[SYNTH_MAX_VOICES];
    float master_volume;
    uint32_t next_voice_age;
} synth_engine_t;

void synth_engine_init(synth_engine_t *engine, float sample_rate);
void synth_engine_note_on(synth_engine_t *engine, uint8_t midi_note, uint8_t velocity);
void synth_engine_note_off(synth_engine_t *engine, uint8_t midi_note);
void synth_engine_all_notes_off(synth_engine_t *engine);
void synth_engine_set_master_volume(synth_engine_t *engine, float master_volume);
void synth_engine_render_i32(synth_engine_t *engine, int32_t *stereo_frames, size_t frame_count);
