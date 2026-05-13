#include "envelope.h"

#define ENVELOPE_ATTACK_TIME_SEC 0.005f
#define ENVELOPE_RELEASE_TIME_SEC 0.050f

static float envelope_compute_increment(float time_seconds, float sample_rate)
{
    if (time_seconds <= 0.0f || sample_rate <= 0.0f) {
        return 1.0f;
    }

    return 1.0f / (time_seconds * sample_rate);
}

void envelope_init(envelope_t *env, float sample_rate)
{
    env->stage = ENVELOPE_STAGE_IDLE;
    env->value = 0.0f;
    env->attack_increment = envelope_compute_increment(ENVELOPE_ATTACK_TIME_SEC, sample_rate);
    env->release_increment = envelope_compute_increment(ENVELOPE_RELEASE_TIME_SEC, sample_rate);
}

void envelope_note_on(envelope_t *env)
{
    env->stage = ENVELOPE_STAGE_ATTACK;
}

void envelope_note_off(envelope_t *env)
{
    if (env->stage != ENVELOPE_STAGE_IDLE) {
        env->stage = ENVELOPE_STAGE_RELEASE;
    }
}

float envelope_render(envelope_t *env)
{
    switch (env->stage) {
    case ENVELOPE_STAGE_ATTACK:
        env->value += env->attack_increment;
        if (env->value >= 1.0f) {
            env->value = 1.0f;
            env->stage = ENVELOPE_STAGE_SUSTAIN;
        }
        break;
    case ENVELOPE_STAGE_RELEASE:
        env->value -= env->release_increment;
        if (env->value <= 0.0f) {
            env->value = 0.0f;
            env->stage = ENVELOPE_STAGE_IDLE;
        }
        break;
    case ENVELOPE_STAGE_SUSTAIN:
        env->value = 1.0f;
        break;
    case ENVELOPE_STAGE_IDLE:
    case ENVELOPE_STAGE_DECAY:
    default:
        break;
    }

    return env->value;
}

bool envelope_is_active(const envelope_t *env)
{
    return env->stage != ENVELOPE_STAGE_IDLE;
}
