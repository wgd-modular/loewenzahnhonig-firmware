# Nimbus Looping Delay

## Author

Ben van der Burgh

## Description

A variant of the [Nimbus](../nimbus/README.md) firmware that runs Clouds'
looping delay mode: the buffer is played back continuously without any
granularisation, like a tape delay whose repeats can be pitch-shifted, and
which turns into a loop when frozen.

- Pot 1: Delay time (+ CV 1), the distance between the record and playback
  heads, up to about 0.7 s. Modulating it with CV 1 gives vinyl-scratch and
  tape-manipulation effects. When the buffer is frozen it sets the loop start.
- Pot 2: Feedback. How much of the output is recorded back into the buffer.
  The feedback path is soft-limited, so fully clockwise gives endless repeats
  without running away.
- Pot 3: Pitch. -2 to +2 octaves, exactly unison around the centre. The
  repeats are transposed on every pass, so a fifth or an octave with feedback
  spirals upwards or downwards. When the buffer is frozen, the loop plays at
  this pitch.
- Pot 4: Size. The size of the windows used for pitch shifting: fully
  counter-clockwise grainy, almost ring-modulated, fully clockwise smooth.
  When the buffer is frozen it sets the loop length instead.
- CV 1: Delay time, added to Pot 1.
- CV 2: Freeze gate. While the gate is high the buffer content loops (Pot 1
  loop start, Pot 4 loop length). Each rising edge also sends Clouds' trigger:
  while frozen, a clock here gives a clock-synchronised stutter loop; while not
  frozen, the period between two pulses sets the delay time, as long as it fits
  in the buffer. Use short pulses for that, since a long gate freezes.

Fixed parameters: diffusion off, filter open, reverb 0.2, stereo spread 0.5,
fully wet (mix the dry signal in outside the module), 16-bit stereo. The output
is soft-clipped like in the other variants.

## Building

Run `pixi run build nimbus-looping-delay` from the repository root. It compiles `../nimbus/nimbus.cpp` with
`NIMBUS_VARIANT_LOOPING_DELAY` defined; the binary ends up in
`build/nimbus-looping-delay.bin`.
