# `synth_esp`

ESP32-S3 synth scaffold with:

- six-voice sine-wave polyphony
- one-octave chromatic note mapping over MIDI or buttons
- UART MIDI input for an external MIDI keyboard/controller
- MCP23017-based 12-button chromatic input
- potentiometer-based master volume control

## Current note behavior

- Accepted notes: `C4 C#4 D4 D#4 E4 F4 F#4 G4 G#4 A4 A#4 B4`
- MIDI/button notes outside `60..71` are ignored
- Up to six notes play at once
- A seventh accepted note steals the oldest active voice
- MIDI and button input are mutually exclusive, selected by the configured input-mode switch

## Hardware notes

- MIDI input expects a proper MIDI-to-UART input stage; do not wire a 5-pin DIN MIDI connector directly to the ESP32-S3 UART pin.
- The default MIDI RX pin is configured in `main/audio/audio_config.h`.
- The 12 note buttons connect to an MCP23017 over I2C, with normally open buttons wired from MCP23017 inputs to `GND`.
- Current I2C defaults are `SDA=GPIO11` and `SCL=GPIO12`.
- The mode switch is pulled up internally; open/high selects button mode, and pulling the configured switch pin low selects MIDI mode.
- The volume potentiometer is optional for test builds; when `VOLUME_POT_ENABLED` is `0`, the synth uses the default master volume.
