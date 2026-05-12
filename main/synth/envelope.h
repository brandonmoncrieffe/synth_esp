#pragma once

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
} envelope_t;

void envelope_init(envelope_t *env);
float envelope_render(envelope_t *env);
