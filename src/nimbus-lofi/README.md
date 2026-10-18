# Nimbus Lo-fi

## Author

Ben van der Burgh

## Description

The [Nimbus](../nimbus/README.md) granular firmware with Clouds' lowest audio
quality setting, 8-bit mu-law mono at half the sample rate. The controls are the
same as in the default variant; what changes is the buffer and the sound:

- The recording buffer grows from about 0.7 s to about 5 s, so you can freeze
  whole phrases and scrub through them with Pot 1, and Pot 1 travels back
  further in time when not frozen.
- Up to 57 grains can overlap instead of 32.
- The audio is recorded in mono (the two inputs are summed) at 24 kHz with
  mu-law companding: bandwidth stops at 12 kHz and quiet passages get the
  grit that Clouds' 8-bit modes are known for. The output is still stereo,
  because the grains are panned randomly.

Controls, as in the default variant:

- Pot 1: Position (+ CV 1)
- Pot 2: Size
- Pot 3: Pitch
- Pot 4: Density, with texture and reverb rising over its right half
- CV 1: Position
- CV 2: Freeze gate; each rising edge plays one grain

See the [Nimbus README](../nimbus/README.md) for the full description of the
controls, the fixed parameters and the design decisions.

## Building

Run `pixi run build nimbus-lofi` from the repository root. It compiles `../nimbus/nimbus.cpp` with
`NIMBUS_VARIANT_LOFI` defined; the binary ends up in `build/nimbus-lofi.bin`.
