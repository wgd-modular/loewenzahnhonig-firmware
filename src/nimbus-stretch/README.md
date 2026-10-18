# Nimbus Stretch

## Author

Ben van der Burgh

## Description

A variant of the [Nimbus](../nimbus/README.md) firmware that runs Clouds'
pitch-shifter/time-stretcher mode instead of the granular mode. The buffer is
played back through overlapping windows rather than grains, which gives clean
transposition and, on a frozen buffer, time-stretching: sweep through the
captured audio at any speed without changing its pitch.

- Pot 1: Position (+ CV 1). Where in the buffer playback happens. Fully
  counter-clockwise follows the input; clockwise travels back in time, up to
  about 0.7 s. On a frozen buffer this is the time-stretch control.
- Pot 2: Size. The size of the overlapping windows: fully counter-clockwise an
  extremely grainy "drilling" sound, fully clockwise smooth (with smeared
  transients).
- Pot 3: Pitch. -2 to +2 octaves, exactly unison around the centre.
- Pot 4: Tone. Clouds' TEXTURE filter: a low-pass filter closing towards the
  left, open at the centre, a high-pass filter opening towards the right.
- CV 1: Position, added to Pot 1.
- CV 2: Freeze gate. The buffer is frozen while the gate is high; each rising
  edge also sends Clouds' trigger.

Fixed parameters: diffusion off, feedback 0, reverb 0.2, stereo spread 0.5,
fully wet, 16-bit stereo (about 0.7 s of buffer). The output is soft-clipped
like in the other variants.

Starting points: Pot 1 fully counter-clockwise and Pot 4 centred give a plain
pitch shifter; freeze a phrase with CV 2 and turn Pot 1 slowly to stretch it.

## Building

Run `pixi run build nimbus-stretch` from the repository root. It compiles `../nimbus/nimbus.cpp` with
`NIMBUS_VARIANT_STRETCH` defined; the binary ends up in
`build/nimbus-stretch.bin`.
