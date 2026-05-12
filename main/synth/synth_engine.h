#pragma once

#include <stddef.h>
#include <stdint.h>

#include "synth/oscillator.h"

typedef struct {
    oscillator_t test_osc;
    float output_level;
} synth_engine_t;

void synth_engine_init(synth_engine_t *engine, float sample_rate);
void synth_engine_render_i32(synth_engine_t *engine, int32_t *stereo_frames, size_t frame_count);
