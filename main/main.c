#include "audio/audio_config.h"
#include "audio/audio_driver.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting synth scaffold");
    ESP_LOGI(TAG,
             "Audio target: %u Hz, %u-bit PCM in %u-bit I2S slots",
             (unsigned)SYNTH_SAMPLE_RATE,
             (unsigned)SYNTH_PCM_BITS,
             (unsigned)SYNTH_I2S_SLOT_BITS);

    ESP_ERROR_CHECK(audio_driver_start());
}
