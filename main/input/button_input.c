#include "button_input.h"

#include <stdbool.h>

#include "audio/audio_config.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MCP23017_IODIRA 0x00
#define MCP23017_IODIRB 0x01
#define MCP23017_GPPUA 0x0C
#define MCP23017_GPPUB 0x0D
#define MCP23017_GPIOA 0x12
#define MCP23017_GPIOB 0x13
#define MCP23017_INPUT_MASK ((uint16_t)((1U << SYNTH_CHROMATIC_NOTE_COUNT) - 1U))
#define BUTTON_INPUT_I2C_TIMEOUT_MS 20

static const char *TAG = "buttons";

static esp_err_t button_input_write_register(button_input_t *input, uint8_t reg, uint8_t value)
{
    uint8_t write_buffer[2] = {reg, value};
    return i2c_master_transmit(input->device_handle, write_buffer, sizeof(write_buffer), BUTTON_INPUT_I2C_TIMEOUT_MS);
}

static esp_err_t button_input_read_register(button_input_t *input, uint8_t reg, uint8_t *value)
{
    return i2c_master_transmit_receive(input->device_handle, &reg, 1, value, 1, BUTTON_INPUT_I2C_TIMEOUT_MS);
}

static esp_err_t button_input_probe_bus(button_input_t *input)
{
    bool found_device = false;
    bool found_configured_device = false;

    for (uint16_t address = 0x08; address <= 0x77; ++address) {
        esp_err_t err = i2c_master_probe(input->bus_handle, address, BUTTON_INPUT_I2C_TIMEOUT_MS);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "I2C device responded at 0x%02x", address);
            found_device = true;
            if (address == BUTTON_INPUT_MCP23017_ADDRESS) {
                found_configured_device = true;
            }
        }
    }

    if (!found_device) {
        ESP_LOGW(TAG, "No I2C address responded from 0x08 to 0x77");
        ESP_LOGW(TAG, "Check MCP RST is high, SDA/SCL pins match config, and I2C pullups are present");
        return ESP_ERR_NOT_FOUND;
    }

    if (!found_configured_device) {
        ESP_LOGW(TAG,
                 "Configured MCP23017 address 0x%02x did not respond; check A0/A1/A2 address pins",
                 BUTTON_INPUT_MCP23017_ADDRESS);
        return ESP_ERR_NOT_FOUND;
    }

    return ESP_OK;
}

static esp_err_t button_input_read_pressed_mask(button_input_t *input, uint16_t *pressed_mask)
{
    uint16_t raw_mask = 0;
    ESP_RETURN_ON_ERROR(button_input_read_debug_masks(input, &raw_mask, pressed_mask), TAG, "read debug masks");

    return ESP_OK;
}

esp_err_t button_input_read_debug_masks(button_input_t *input, uint16_t *raw_mask, uint16_t *pressed_mask)
{
    if (input == NULL || raw_mask == NULL || pressed_mask == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!input->available) {
        return ESP_ERR_NOT_FOUND;
    }

    uint8_t gpio_a = 0;
    uint8_t gpio_b = 0;

    ESP_RETURN_ON_ERROR(button_input_read_register(input, MCP23017_GPIOA, &gpio_a), TAG, "read GPIOA");
    ESP_RETURN_ON_ERROR(button_input_read_register(input, MCP23017_GPIOB, &gpio_b), TAG, "read GPIOB");

    *raw_mask = ((uint16_t)gpio_b << 8) | gpio_a;
    *pressed_mask = (uint16_t)(~(*raw_mask)) & MCP23017_INPUT_MASK;

    return ESP_OK;
}

esp_err_t button_input_init(button_input_t *input)
{
    if (input == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *input = (button_input_t){0};

    i2c_master_bus_config_t bus_config = {
        .i2c_port = BUTTON_INPUT_I2C_PORT,
        .sda_io_num = BUTTON_INPUT_I2C_SDA_GPIO,
        .scl_io_num = BUTTON_INPUT_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = {
            .enable_internal_pullup = true,
        },
    };
    ESP_LOGI(TAG,
             "Initializing MCP23017 at 0x%02x on I2C port=%d SDA=%d SCL=%d speed=%u",
             BUTTON_INPUT_MCP23017_ADDRESS,
             BUTTON_INPUT_I2C_PORT,
             BUTTON_INPUT_I2C_SDA_GPIO,
             BUTTON_INPUT_I2C_SCL_GPIO,
             (unsigned)BUTTON_INPUT_I2C_SCL_SPEED_HZ);
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &input->bus_handle), TAG, "create I2C bus");
    while (true) {
        ESP_LOGI(TAG,
                 "I2C idle levels before probe: SDA=%d SCL=%d",
                 gpio_get_level(BUTTON_INPUT_I2C_SDA_GPIO),
                 gpio_get_level(BUTTON_INPUT_I2C_SCL_GPIO));
        esp_err_t probe_err = button_input_probe_bus(input);
        if (probe_err == ESP_OK) {
            break;
        }

        if (!BUTTON_INPUT_I2C_RETRY_SCAN_ON_FAIL) {
            ESP_RETURN_ON_ERROR(probe_err, TAG, "probe MCP23017");
        }

        ESP_LOGW(TAG, "Retrying I2C scan in 1000 ms");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BUTTON_INPUT_MCP23017_ADDRESS,
        .scl_speed_hz = BUTTON_INPUT_I2C_SCL_SPEED_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(input->bus_handle, &device_config, &input->device_handle), TAG, "add MCP23017");

    ESP_RETURN_ON_ERROR(button_input_write_register(input, MCP23017_IODIRA, 0xFF), TAG, "configure IODIRA");
    ESP_RETURN_ON_ERROR(button_input_write_register(input, MCP23017_IODIRB, 0xFF), TAG, "configure IODIRB");
    ESP_RETURN_ON_ERROR(button_input_write_register(input, MCP23017_GPPUA, 0xFF), TAG, "enable GPPUA");
    ESP_RETURN_ON_ERROR(button_input_write_register(input, MCP23017_GPPUB, 0xFF), TAG, "enable GPPUB");

    button_input_reset(input);
    input->available = true;
    return ESP_OK;
}

void button_input_reset(button_input_t *input)
{
    if (input == NULL) {
        return;
    }

    input->stable_pressed_mask = 0;
    for (size_t i = 0; i < SYNTH_CHROMATIC_NOTE_COUNT; ++i) {
        input->debounce_counts[i] = 0;
    }
}

esp_err_t button_input_poll(button_input_t *input, midi_event_t *events, size_t max_events, size_t *event_count)
{
    if (input == NULL || events == NULL || event_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *event_count = 0;
    if (!input->available) {
        return ESP_ERR_NOT_FOUND;
    }

    uint16_t raw_pressed_mask = 0;
    ESP_RETURN_ON_ERROR(button_input_read_pressed_mask(input, &raw_pressed_mask), TAG, "read button state");

    for (size_t i = 0; i < SYNTH_CHROMATIC_NOTE_COUNT && *event_count < max_events; ++i) {
        uint16_t bit = (uint16_t)(1U << i);
        bool raw_pressed = (raw_pressed_mask & bit) != 0U;
        bool stable_pressed = (input->stable_pressed_mask & bit) != 0U;

        if (raw_pressed == stable_pressed) {
            input->debounce_counts[i] = 0;
            continue;
        }

        if (input->debounce_counts[i] < BUTTON_INPUT_DEBOUNCE_BLOCKS) {
            ++input->debounce_counts[i];
        }

        if (input->debounce_counts[i] < BUTTON_INPUT_DEBOUNCE_BLOCKS) {
            continue;
        }

        input->debounce_counts[i] = 0;
        if (raw_pressed) {
            input->stable_pressed_mask |= bit;
        } else {
            input->stable_pressed_mask &= (uint16_t)~bit;
        }

        midi_event_t *event = &events[*event_count];
        event->type = raw_pressed ? MIDI_EVENT_NOTE_ON : MIDI_EVENT_NOTE_OFF;
        event->channel = 0;
        event->data1 = (uint8_t)(SYNTH_ACTIVE_OCTAVE_BASE_NOTE + i);
        event->data2 = raw_pressed ? BUTTON_INPUT_VELOCITY : 0;
        ++(*event_count);
    }

    return ESP_OK;
}
