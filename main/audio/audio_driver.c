#include "audio_driver.h"

#include <stddef.h>
#include <stdint.h>

#include "audio_config.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "synth/synth_engine.h"

static const char *TAG = "audio";

static i2s_chan_handle_t s_tx_chan;
static synth_engine_t s_engine;
static TaskHandle_t s_audio_task;

static void audio_task(void *arg)
{
    (void)arg;

    static int32_t frames[SYNTH_BLOCK_FRAMES * SYNTH_CHANNEL_COUNT];

    while (true) {
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
             "PCM5102 I2S started: %u Hz, %u-bit signal in %u-bit slots, BCLK=%d WS=%d DOUT=%d",
             (unsigned)SYNTH_SAMPLE_RATE,
             (unsigned)SYNTH_PCM_BITS,
             (unsigned)SYNTH_I2S_SLOT_BITS,
             PCM5102_I2S_BCLK_GPIO,
             PCM5102_I2S_WS_GPIO,
             PCM5102_I2S_DOUT_GPIO);

    return ESP_OK;
}
