#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "midi/midi.h"

typedef struct {
    uint8_t running_status;
    uint8_t data[2];
    uint8_t data_count;
} midi_parser_t;

void midi_parser_init(midi_parser_t *parser);
bool midi_parser_push_byte(midi_parser_t *parser, uint8_t byte, midi_event_t *event);
