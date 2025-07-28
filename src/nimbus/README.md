# Nimbus for Loewenzahnhonig

A port of Nimbus (Mutable Instruments Clouds) granular processor firmware for the Loewenzahnhonig Eurorack module.

## Overview

This firmware brings the powerful granular synthesis capabilities of Nimbus (based on Mutable Instruments Clouds) to the Loewenzahnhonig hardware platform. The Loewenzahnhonig module features a Daisy Seed microcontroller with 4 potentiometers and 2 CV inputs, providing a simplified but effective interface for granular audio processing.

## Controls

| Control | Function | Description |
|---------|----------|-------------|
| P1      | Position | Playback position in the audio buffer |
| P2      | Size     | Grain size/length |
| P3      | Pitch    | Pitch transpose (±2 octaves) |
| P4      | Density  | Grain generation rate |
| CV1     | Texture  | Grain envelope shape |
| CV2     | Dry/Wet  | Effect blend |

## Fixed Parameters

To simplify the interface for the 6-control hardware, some parameters are fixed:

- **Mode**: Granular processing only (no alternate modes)
- **Quality**: 16-bit stereo
- **Stereo Spread**: Center (0.5)
- **Feedback**: Disabled (0.0)
- **Reverb**: Moderate amount (0.3)
- **Freeze**: Disabled (could be added as CV threshold)

## Features

- Full Nimbus granular synthesis engine
- Real-time granular processing with up to 8-second buffer
- Stereo input/output
- Low-latency audio processing
- Optimized for Daisy Seed performance

## Technical Details

- **Sample Rate**: 48kHz
- **Audio Block Size**: 32 samples (optimized for Clouds engine)
- **Memory**: Uses SDRAM for audio buffers
- **Processing**: ARM Cortex-M7 @ 400MHz

## Installation

1. Flash the `nimbus.bin` file to your Daisy Seed using the [Daisy Web Flasher](https://electro-smith.github.io/Programmer/)
2. Insert the Daisy Seed into your Loewenzahnhonig module
3. Connect audio sources and start processing!

## Building from Source

Prerequisites:
- ARM GCC toolchain
- libDaisy and DaisySP libraries (included as submodules)

```bash
# Build libraries first
./build_libs.sh

# Build firmware
cd src/nimbus
make
```

The resulting binary will be in `build/nimbus.bin`.

## Credits

- Original Clouds firmware: Émilie Gillet (Mutable Instruments)
- Nimbus port: Ben Sergentanis (Electro-Smith)
- Loewenzahnhonig port: Created using Claude Code

## License

This port maintains the original Nimbus licensing terms.