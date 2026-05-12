#include "midi_parser.h"

void midi_parser_init(midi_parser_t *parser)
{
    parser->running_status = 0;
    parser->data[0] = 0;
    parser->data[1] = 0;
    parser->data_count = 0;
}

bool midi_parser_push_byte(midi_parser_t *parser, uint8_t byte, midi_event_t *event)
{
    (void)byte;

    event->type = MIDI_EVENT_NONE;
    event->channel = 0;
    event->data1 = 0;
    event->data2 = 0;
    parser->data_count = 0;

    return false;
}
