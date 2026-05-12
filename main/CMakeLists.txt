idf_component_register(
    SRCS
        "main.c"
        "audio/audio_driver.c"
        "synth/synth_engine.c"
        "synth/oscillator.c"
        "synth/voice.c"
        "synth/envelope.c"
        "synth/modulation.c"
        "midi/midi_parser.c"
        "midi/midi_uart.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_driver_i2s
)
