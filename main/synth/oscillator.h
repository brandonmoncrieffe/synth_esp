#pragma once

typedef struct {
    float sample_rate;
    float frequency_hz;
    float phase;
    float phase_increment;
} oscillator_t;

void oscillator_init(oscillator_t *osc, float sample_rate, float frequency_hz);
void oscillator_set_frequency(oscillator_t *osc, float frequency_hz);
float oscillator_render_sine(oscillator_t *osc);
