# Smoodge Delay

Stereo Clockable Delay for the wgd modular löwenzahnhonig Daisy Seed Eurorack module.

This effect works best if InL is normalled to InR in hardware.

## Author

Nik Ansell (gamecat69)

### Version History
0.1		Nik Ansell		Initial plaything

## Description

### Controls:

| Control | Function        |
|---------|-----------------|
| P1      | Delay Feedback (Repeats)      |
| P2      | Dry/Wet      |
| P3      | Delay Time (See Usage below)      |
| P4      | Smoodge (See Usage below)      |
| CV1     | Input Clock      |
| CV2     | Smoodge CV      |

### Usage

Start by plugging in a stereo sound source (or mono into the L input if you have normalled your R-input to the L-input), then connect the left and right outputs to a mixer.

The Delay Feedback and Dry/Wet controls act as you would expect:
- Delay Feedback: At min will produce one repeat/echo. At max it will feedback infinitely - you can even remove your sound source once it is looping if you like!
- Dry/Wet; At min the output is 100% dry. At max the output is 100% wet.

#### Delay Time

The Delay Time control will change the delay time as a division or multiplication of the delay time set by the input clock.
Note, if you turn on the module without giving it a clock the default delay time is 1 second.
If you imagine the knob goes from 0 to 10, below you will find the divisions/multiplications the corespond to each number on the dial.

- 0: /8
- 1: /4
- 2: /3 (half-note triplets)
- 3: /2
- 4: /1.5 (whole note triplets)
- 5: /1 or x1 whichever feels better :)
- 6: x2
- 7: x3
- 8: x4
- 9: x5
- 10: x8

If for example you send in clocks which are 1 second apart, setting the Delay Time knob in the centre would produce echo's every second, at min it would produce an echo every 1/8th of a second and at max it would produce an echo every 8 seconds.
**Note:** The maximum delay time is capped automatically at 10 seconds.

#### Smoodge

This control produces a stereo effect. With the Smoodge knob at min there is no effect.
From 0 - 50% the control will add a nice stereo spread to the sound that makes everything sound quite spacious and lucious.
Above 50% you will notices the spread becomes less of a spatial effect and more of a 'special' effect - the delay times between the left and right channels will be more obvious and you will also notice a slow chorus effect that adds a warm fuzziness to the sound.

## Flashing your Daisy Seed

To load this firmware onto your Daisy seed, use the Daisy Web Programmer from electrosmith, just browse to the build/wgd-delay.bin file in this repo as part of the process.