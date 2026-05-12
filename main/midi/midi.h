#pragma once

#include <stdint.h>

typedef enum {
    MIDI_EVENT_NONE = 0,
    MIDI_EVENT_NOTE_ON,
    MIDI_EVENT_NOTE_OFF,
    MIDI_EVENT_CONTROL_CHANGE,
    MIDI_EVENT_PITCH_BEND,
} midi_event_type_t;

typedef struct {
    midi_event_type_t type;
    uint8_t channel;
    uint8_t data1;
    uint8_t data2;
} midi_event_t;
