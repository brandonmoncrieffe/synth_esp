#include "midi_uart.h"

#include "audio/audio_config.h"
#include "driver/uart.h"
#include "esp_check.h"

esp_err_t midi_uart_init(void)
{
    if (!uart_is_driver_installed(MIDI_UART_PORT_NUM)) {
        uart_config_t uart_config = {
            .baud_rate = MIDI_UART_BAUD_RATE,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .rx_flow_ctrl_thresh = 0,
            .source_clk = UART_SCLK_DEFAULT,
        };

        ESP_RETURN_ON_ERROR(uart_driver_install(MIDI_UART_PORT_NUM, MIDI_UART_BUFFER_SIZE, 0, 0, NULL, 0), "midi", "install UART driver");
        ESP_RETURN_ON_ERROR(uart_param_config(MIDI_UART_PORT_NUM, &uart_config), "midi", "configure UART");
        ESP_RETURN_ON_ERROR(uart_set_pin(MIDI_UART_PORT_NUM, MIDI_UART_TX_GPIO, MIDI_UART_RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE), "midi", "set UART pins");
    }

    ESP_RETURN_ON_ERROR(midi_uart_flush_input(), "midi", "flush UART RX");
    return ESP_OK;
}

esp_err_t midi_uart_flush_input(void)
{
    return uart_flush_input(MIDI_UART_PORT_NUM);
}

bool midi_uart_read_byte(uint8_t *byte_out)
{
    if (byte_out == NULL) {
        return false;
    }

    int bytes_read = uart_read_bytes(MIDI_UART_PORT_NUM, byte_out, 1, 0);
    return bytes_read == 1;
}
