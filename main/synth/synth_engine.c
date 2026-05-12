#include "synth_engine.h"

#include "audio/audio_config.h"

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

void synth_engine_init(synth_engine_t *engine, float sample_rate)
{
    oscillator_init(&engine->test_osc, sample_rate, SYNTH_TEST_FREQ_HZ);
    engine->output_level = SYNTH_TEST_LEVEL;
}

void synth_engine_render_i32(synth_engine_t *engine, int32_t *stereo_frames, size_t frame_count)
{
    for (size_t i = 0; i < frame_count; ++i) {
        float mono = oscillator_render_sine(&engine->test_osc) * engine->output_level;
        int32_t sample = synth_float_to_i2s_sample(mono);

        stereo_frames[i * SYNTH_CHANNEL_COUNT] = sample;
        stereo_frames[i * SYNTH_CHANNEL_COUNT + 1] = sample;
    }
}
