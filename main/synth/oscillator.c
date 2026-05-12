#include "oscillator.h"

#include <math.h>

#define OSC_TWO_PI 6.28318530717958647692f

static float oscillator_phase_increment(float frequency_hz, float sample_rate)
{
    return frequency_hz / sample_rate;
}

void oscillator_init(oscillator_t *osc, float sample_rate, float frequency_hz)
{
    osc->sample_rate = sample_rate;
    osc->phase = 0.0f;
    oscillator_set_frequency(osc, frequency_hz);
}

void oscillator_set_frequency(oscillator_t *osc, float frequency_hz)
{
    osc->frequency_hz = frequency_hz;
    osc->phase_increment = oscillator_phase_increment(frequency_hz, osc->sample_rate);
}

float oscillator_render_sine(oscillator_t *osc)
{
    float sample = sinf(osc->phase * OSC_TWO_PI);

    osc->phase += osc->phase_increment;
    if (osc->phase >= 1.0f) {
        osc->phase -= 1.0f;
    }

    return sample;
}
