#include "midi_parser.h"

static uint8_t midi_parser_expected_data_bytes(uint8_t status)
{
    switch (status & 0xF0U) {
    case 0x80U:
    case 0x90U:
    case 0xA0U:
    case 0xB0U:
    case 0xE0U:
        return 2;
    case 0xC0U:
    case 0xD0U:
        return 1;
    default:
        return 0;
    }
}

void midi_parser_init(midi_parser_t *parser)
{
    parser->running_status = 0;
    parser->data[0] = 0;
    parser->data[1] = 0;
    parser->data_count = 0;
}

bool midi_parser_push_byte(midi_parser_t *parser, uint8_t byte, midi_event_t *event)
{
    event->type = MIDI_EVENT_NONE;
    event->channel = 0;
    event->data1 = 0;
    event->data2 = 0;

    if (byte >= 0xF8U) {
        return false;
    }

    if ((byte & 0x80U) != 0U) {
        parser->data_count = 0;

        if (byte >= 0xF0U) {
            parser->running_status = 0;
            return false;
        }

        parser->running_status = byte;
        return false;
    }

    if (parser->running_status == 0U) {
        return false;
    }

    uint8_t expected_data_bytes = midi_parser_expected_data_bytes(parser->running_status);
    if (expected_data_bytes == 0U) {
        parser->data_count = 0;
        return false;
    }

    parser->data[parser->data_count++] = byte;
    if (parser->data_count < expected_data_bytes) {
        return false;
    }

    parser->data_count = 0;

    event->channel = parser->running_status & 0x0FU;
    event->data1 = parser->data[0];
    event->data2 = (expected_data_bytes > 1U) ? parser->data[1] : 0;

    switch (parser->running_status & 0xF0U) {
    case 0x80U:
        event->type = MIDI_EVENT_NOTE_OFF;
        return true;
    case 0x90U:
        event->type = (event->data2 == 0U) ? MIDI_EVENT_NOTE_OFF : MIDI_EVENT_NOTE_ON;
        return true;
    case 0xB0U:
        event->type = MIDI_EVENT_CONTROL_CHANGE;
        return true;
    case 0xE0U:
        event->type = MIDI_EVENT_PITCH_BEND;
        return true;
    default:
        event->type = MIDI_EVENT_NONE;
        return false;
    }
}
