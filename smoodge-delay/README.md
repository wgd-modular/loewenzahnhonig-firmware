# Smoodge Delay

Stereo Clockable Delay for the wgd modular löwenzahnhonig Daisy Seed Eurorack module.

This effect works best if InL is normalled to InR in hardware.

## Author

Nik Ansell (gamecat69)

### Version History
0.1		Nik Ansell		Initial plaything
0.2     Nik Ansell      Updated Smoodge Effect
0.3     Nik Ansell      Added tape effects to smoodge parameter

## Description

### Controls:

| Control | Function        |
|---------|-----------------|
| P1      | Delay Feedback (Repeats)      |
| P2      | Dry/Wet      |
| P3      | Delay Time (See Usage below)      |
| P4      | Smoodge (See Usage below)      |
| CV1     | Input Clock      |
| CV2     | Delay Feedback (Repeats) Summed with P1     |

### Usage

Start by plugging in a stereo sound source (or mono into the L input if you have normalled your R-input to the L-input), then connect the left and right outputs to a mixer.

You might find it easier to dial in the sound you want using the following process:

1. Set the feedback to min
2. Set the Dry/Wet to 50%
3. Set the Smoodget to min
4. Set the Delay Time to 50%
5. Start your sound source
6. Adjust the Delay Time to something that you like
7. Gradually increase the feedback until it sounds good to you
8. Gradually increase the Smoodge
9. Remember these settings, then go wild with the controls and see what happens!

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
- 5: /1 or x1 whichever sounds better to you
- 6: x2
- 7: x3
- 8: x4
- 9: x5
- 10: x8

If for example you send in clocks which are 1 second apart, setting the Delay Time knob in the centre would produce echo's every second, at min it would produce an echo every 1/8th of a second and at max it would produce an echo every 8 seconds.
**Note:** The maximum delay time is capped automatically at 16 seconds.

#### Smoodge

This control produces a spatial stereo effect from 0-50% and a tape saturation, wow and flutter effect with more spatial separation from 50-100%.
With the Smoodge knob at min there is no effect.

## Flashing your Daisy Seed

To load this firmware onto your Daisy seed, use the Daisy Web Programmer from electrosmith, just browse to the build/wgd-delay.bin file in this repo as part of the process.