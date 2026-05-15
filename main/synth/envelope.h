#pragma once

#include <stdbool.h>

typedef enum {
    ENVELOPE_STAGE_IDLE = 0,
    ENVELOPE_STAGE_ATTACK,
    ENVELOPE_STAGE_DECAY,
    ENVELOPE_STAGE_SUSTAIN,
    ENVELOPE_STAGE_RELEASE,
} envelope_stage_t;

typedef struct {
    envelope_stage_t stage;
    float value;
    float attack_increment;
    float release_increment;
} envelope_t;

void envelope_init(envelope_t *env, float sample_rate);
void envelope_note_on(envelope_t *env);
void envelope_note_off(envelope_t *env);
void envelope_reset(envelope_t *env);
float envelope_render(envelope_t *env);
bool envelope_is_active(const envelope_t *env);
