#include "voice.h"

void synth_voice_init(synth_voice_t *voice, float sample_rate)
{
    voice->active = false;
    voice->midi_note = 0;
    voice->velocity = 0.0f;
    envelope_init(&voice->amp_envelope);

    for (int i = 0; i < SYNTH_OSCILLATORS_PER_VOICE; ++i) {
        oscillator_init(&voice->oscillators[i], sample_rate, 440.0f);
    }
}
