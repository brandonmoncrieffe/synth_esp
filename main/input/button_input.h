#pragma once

#include <stddef.h>
#include <stdint.h>

#include "audio/audio_config.h"
#include "driver/i2c_types.h"
#include "esp_err.h"
#include "midi/midi.h"

typedef struct {
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t device_handle;
    uint16_t stable_pressed_mask;
    uint8_t debounce_counts[SYNTH_CHROMATIC_NOTE_COUNT];
} button_input_t;

esp_err_t button_input_init(button_input_t *input);
void button_input_reset(button_input_t *input);
esp_err_t button_input_poll(button_input_t *input, midi_event_t *events, size_t max_events, size_t *event_count);
