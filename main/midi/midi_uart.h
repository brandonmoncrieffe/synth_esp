#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t midi_uart_init(void);
esp_err_t midi_uart_flush_input(void);
bool midi_uart_read_byte(uint8_t *byte_out);
