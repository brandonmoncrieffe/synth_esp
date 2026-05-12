#include "envelope.h"

void envelope_init(envelope_t *env)
{
    env->stage = ENVELOPE_STAGE_IDLE;
    env->value = 0.0f;
}

float envelope_render(envelope_t *env)
{
    return env->value;
}
