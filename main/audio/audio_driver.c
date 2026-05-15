#include "audio_driver.h"

#include <stddef.h>
#include <stdint.h>

#include "audio_config.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "input/button_input.h"
#include "midi/midi_parser.h"
#include "midi/midi_uart.h"
#include "synth/synth_engine.h"

static const char *TAG = "audio";

static i2s_chan_handle_t s_tx_chan;
static synth_engine_t s_engine;
static TaskHandle_t s_audio_task;
static midi_parser_t s_midi_parser;
static button_input_t s_button_input;
static adc_oneshot_unit_handle_t s_volume_adc_handle;
static adc_channel_t s_volume_adc_channel;
static float s_smoothed_volume = SYNTH_DEFAULT_MASTER_VOLUME;
static uint32_t s_button_debug_log_counter;
static bool s_button_input_available;
static bool s_button_unavailable_logged;

typedef enum {
    AUDIO_INPUT_MODE_MIDI = 0,
    AUDIO_INPUT_MODE_BUTTONS,
} audio_input_mode_t;

static audio_input_mode_t s_input_mode = AUDIO_INPUT_MODE_MIDI;

static void audio_handle_midi_event(const midi_event_t *event)
{
    switch (event->type) {
    case MIDI_EVENT_NOTE_ON:
        synth_engine_note_on(&s_engine, event->data1, event->data2);
        break;
    case MIDI_EVENT_NOTE_OFF:
        synth_engine_note_off(&s_engine, event->data1);
        break;
    case MIDI_EVENT_NONE:
    case MIDI_EVENT_CONTROL_CHANGE:
    case MIDI_EVENT_PITCH_BEND:
    default:
        break;
    }
}

static void audio_poll_midi(void)
{
    uint8_t byte = 0;
    midi_event_t event;

    while (midi_uart_read_byte(&byte)) {
        if (midi_parser_push_byte(&s_midi_parser, byte, &event)) {
            audio_handle_midi_event(&event);
        }
    }
}

static void audio_poll_buttons(void)
{
    if (!s_button_input_available) {
        if (BUTTON_INPUT_DEBUG_LOGS && !s_button_unavailable_logged) {
            ESP_LOGW(TAG, "Button input unavailable; skipping button polling");
            s_button_unavailable_logged = true;
        }
        return;
    }

    midi_event_t events[SYNTH_CHROMATIC_NOTE_COUNT];
    size_t event_count = 0;

    if (BUTTON_INPUT_DEBUG_LOGS && s_button_debug_log_counter++ >= 187U) {
        uint16_t raw_mask = 0;
        uint16_t pressed_mask = 0;
        esp_err_t debug_err = button_input_read_debug_masks(&s_button_input, &raw_mask, &pressed_mask);
        if (debug_err == ESP_OK) {
            ESP_LOGI(TAG, "Button raw=0x%04x pressed=0x%03x", raw_mask, pressed_mask);
        } else {
            ESP_LOGW(TAG, "Button debug read failed: %s", esp_err_to_name(debug_err));
        }
        s_button_debug_log_counter = 0;
    }

    esp_err_t err = button_input_poll(&s_button_input, events, SYNTH_CHROMATIC_NOTE_COUNT, &event_count);
    if (err != ESP_OK) {
        if (BUTTON_INPUT_DEBUG_LOGS && !s_button_unavailable_logged) {
            ESP_LOGW(TAG, "Button input read failed: %s", esp_err_to_name(err));
            s_button_unavailable_logged = true;
        }
        return;
    }

    for (size_t i = 0; i < event_count; ++i) {
        if (BUTTON_INPUT_DEBUG_LOGS) {
            ESP_LOGI(TAG,
                     "Button %s note=%u",
                     events[i].type == MIDI_EVENT_NOTE_ON ? "on" : "off",
                     events[i].data1);
        }
        audio_handle_midi_event(&events[i]);
    }
}

static audio_input_mode_t audio_read_input_mode_switch(void)
{
    int level = gpio_get_level(INPUT_MODE_SWITCH_GPIO);
    return (level == INPUT_MODE_SWITCH_BUTTON_LEVEL) ? AUDIO_INPUT_MODE_BUTTONS : AUDIO_INPUT_MODE_MIDI;
}

static void audio_set_input_mode(audio_input_mode_t next_mode)
{
    if (next_mode == s_input_mode) {
        return;
    }

    synth_engine_all_notes_off(&s_engine);
    midi_parser_init(&s_midi_parser);
    if (s_button_input_available) {
        button_input_reset(&s_button_input);
    }
    esp_err_t err = midi_uart_flush_input();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "MIDI UART flush failed: %s", esp_err_to_name(err));
    }

    s_input_mode = next_mode;
    ESP_LOGI(TAG, "Input mode changed: %s", s_input_mode == AUDIO_INPUT_MODE_BUTTONS ? "buttons" : "midi");
}

static esp_err_t audio_input_mode_switch_init(void)
{
    gpio_config_t io_config = {
        .pin_bit_mask = 1ULL << INPUT_MODE_SWITCH_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_RETURN_ON_ERROR(gpio_config(&io_config), TAG, "configure input mode switch");
    s_input_mode = audio_read_input_mode_switch();
    return ESP_OK;
}

static void audio_poll_volume_control(void)
{
    if (s_volume_adc_handle == NULL) {
        return;
    }

    int raw_reading = 0;
    esp_err_t err = adc_oneshot_read(s_volume_adc_handle, s_volume_adc_channel, &raw_reading);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Volume ADC read failed: %s", esp_err_to_name(err));
        return;
    }

    float target_volume = (float)raw_reading / VOLUME_POT_ADC_MAX_READING;
    if (target_volume < 0.0f) {
        target_volume = 0.0f;
    } else if (target_volume > 1.0f) {
        target_volume = 1.0f;
    }

    s_smoothed_volume += (target_volume - s_smoothed_volume) * VOLUME_POT_SMOOTHING_ALPHA;
    synth_engine_set_master_volume(&s_engine, s_smoothed_volume);
}

static esp_err_t audio_volume_control_init(void)
{
    adc_unit_t adc_unit = ADC_UNIT_1;
    adc_channel_t adc_channel = ADC_CHANNEL_0;

    ESP_RETURN_ON_ERROR(adc_oneshot_io_to_channel(VOLUME_POT_ADC_GPIO, &adc_unit, &adc_channel), TAG, "resolve ADC channel from GPIO");

    adc_oneshot_unit_init_cfg_t adc_unit_config = {
        .unit_id = adc_unit,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&adc_unit_config, &s_volume_adc_handle), TAG, "create ADC oneshot unit");

    adc_oneshot_chan_cfg_t channel_config = {
        .atten = VOLUME_POT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(s_volume_adc_handle, adc_channel, &channel_config), TAG, "configure ADC channel");

    s_volume_adc_channel = adc_channel;
    return ESP_OK;
}

static void audio_task(void *arg)
{
    (void)arg;

    static int32_t frames[SYNTH_BLOCK_FRAMES * SYNTH_CHANNEL_COUNT];

    while (true) {
        audio_set_input_mode(audio_read_input_mode_switch());
        if (s_input_mode == AUDIO_INPUT_MODE_BUTTONS) {
            audio_poll_buttons();
        } else {
            audio_poll_midi();
        }
        audio_poll_volume_control();
        synth_engine_render_i32(&s_engine, frames, SYNTH_BLOCK_FRAMES);

        size_t bytes_written = 0;
        const size_t bytes_to_write = sizeof(frames);
        esp_err_t err = i2s_channel_write(s_tx_chan, frames, bytes_to_write, &bytes_written, portMAX_DELAY);
        if (err != ESP_OK || bytes_written != bytes_to_write) {
            ESP_LOGW(TAG, "I2S write issue: err=%s bytes=%u/%u",
                     esp_err_to_name(err),
                     (unsigned)bytes_written,
                     (unsigned)bytes_to_write);
        }
    }
}

esp_err_t audio_driver_start(void)
{
    if (s_audio_task != NULL) {
        return ESP_OK;
    }

    synth_engine_init(&s_engine, (float)SYNTH_SAMPLE_RATE);
    midi_parser_init(&s_midi_parser);
    synth_engine_set_master_volume(&s_engine, SYNTH_DEFAULT_MASTER_VOLUME);

    ESP_RETURN_ON_ERROR(midi_uart_init(), TAG, "init MIDI UART");
    esp_err_t button_err = button_input_init(&s_button_input);
    if (button_err == ESP_OK) {
        s_button_input_available = true;
    } else {
        s_button_input_available = false;
        ESP_LOGW(TAG, "Button input unavailable: %s; continuing without buttons", esp_err_to_name(button_err));
    }
    ESP_RETURN_ON_ERROR(audio_input_mode_switch_init(), TAG, "init input mode switch");
    if (s_input_mode == AUDIO_INPUT_MODE_BUTTONS && !s_button_input_available) {
        ESP_LOGW(TAG, "Button mode selected but MCP23017 is unavailable; no button notes will play");
    }
    ESP_LOGI(TAG, "Input mode switch GPIO=%d level=%d mode=%s",
             INPUT_MODE_SWITCH_GPIO,
             gpio_get_level(INPUT_MODE_SWITCH_GPIO),
             s_input_mode == AUDIO_INPUT_MODE_BUTTONS ? "buttons" : "midi");
    if (VOLUME_POT_ENABLED) {
        ESP_RETURN_ON_ERROR(audio_volume_control_init(), TAG, "init volume ADC");
    } else {
        ESP_LOGI(TAG, "Volume pot disabled; using default master volume");
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 6;
    chan_cfg.dma_frame_num = SYNTH_BLOCK_FRAMES;

    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &s_tx_chan, NULL), TAG, "create I2S TX channel");

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SYNTH_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = PCM5102_I2S_BCLK_GPIO,
            .ws = PCM5102_I2S_WS_GPIO,
            .dout = PCM5102_I2S_DOUT_GPIO,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT;

    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_tx_chan, &std_cfg), TAG, "init I2S standard mode");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_tx_chan), TAG, "enable I2S TX channel");

    BaseType_t created = xTaskCreatePinnedToCore(
        audio_task,
        "audio_task",
        4096,
        NULL,
        5,
        &s_audio_task,
        1);
    ESP_RETURN_ON_FALSE(created == pdPASS, ESP_ERR_NO_MEM, TAG, "create audio task");

    ESP_LOGI(TAG,
             "PCM5102 I2S started: %u Hz, %u-bit signal in %u-bit slots, BCLK=%d WS=%d DOUT=%d, MIDI RX=%d, volume ADC GPIO=%d, input mode=%s",
             (unsigned)SYNTH_SAMPLE_RATE,
             (unsigned)SYNTH_PCM_BITS,
             (unsigned)SYNTH_I2S_SLOT_BITS,
             PCM5102_I2S_BCLK_GPIO,
             PCM5102_I2S_WS_GPIO,
             PCM5102_I2S_DOUT_GPIO,
             MIDI_UART_RX_GPIO,
             VOLUME_POT_ADC_GPIO,
             s_input_mode == AUDIO_INPUT_MODE_BUTTONS ? "buttons" : "midi");

    return ESP_OK;
}
