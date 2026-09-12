# Cloud Seed

## Author

Ben van der Burgh

Cloud Seed is an open-source algorithmic reverb by Valdemar Erlingsson, built
"for emulating huge, endless spaces and modulated echoes" and released as a
VST plugin under the MIT license ([ValdemarOrn/CloudSeed](https://github.com/ValdemarOrn/CloudSeed)).
This firmware runs it on the Löwenzahnhonig through the
[cloudseed-daisy](https://github.com/benjaminvdb/cloudseed-daisy) library
(`lib/cloudseed-daisy`), a port of the plugin's C++ kernel to the Daisy Seed
with an engine that places and stages the reverb's delay memory and recovers
from CPU overload. `cloudseed.cpp` maps the module's pots and CV inputs onto
that engine and mixes the wet signal. The library ships the plugin's nine
factory programs and the one built-in program of its successor, Ghost Note
Audio's Cloud Seed 2 (a plate, from its MIT-licensed
[core](https://github.com/GhostNoteAudio/CloudSeedCore)). The port learned
from the earlier Daisy ports of Cloud Seed
([erwincoumans/DaisyCloudSeed](https://github.com/erwincoumans/DaisyCloudSeed)
for the Daisy Patch, [GuitarML/DaisyCloudSeed](https://github.com/GuitarML/DaisyCloudSeed)
for the Terrarium pedal and [baylessj/daisy-reverb](https://github.com/baylessj/daisy-reverb)),
but shares no code with them.

This file describes the module and how to use and build it. The library's
[README](../../lib/cloudseed-daisy/README.md) describes the engine and its
options, and its [TECHNICAL.md](../../lib/cloudseed-daisy/TECHNICAL.md) is
the engineering reference: the architecture, the optimization history with
its measurements (taken on this module), the verification, every defect found
and corrected, and the lessons worth keeping.

## What it does

Cloud Seed is built like the classic studio reverbs of the 1980s. The input
passes through a pre-delay into an early-reflections block (a multitap delay
with up to 50 taps followed by up to 8 modulated allpass filters in series),
and from there into the late reverberation: parallel, modulated delay lines
with feedback, each with a low shelf, a high shelf and a low-pass filter plus
its own allpass diffuser in the feedback path. Everything "random" in it, the
tap positions and gains, the allpass lengths and the delay-line lengths, is
derived from SHA-256 hashes of a few seed numbers, so the delay and tap layout
is reproducible on the little-endian platforms supported by the port. The
plugin's documentation has a
[block diagram and a description of every parameter](https://github.com/ValdemarOrn/CloudSeed/blob/13a625e00ec47db98b158a17e309d2cdaebb57ed/Documentation/readme.md).

On the Löwenzahnhonig the reverb runs in stereo. Pot 1 selects one of ten
programs, each a complete set of the plugin's 46 parameters: its nine
factory programs, from a small room to endless washes and chorused echoes,
and its successor's plate. The other pots take over the three parameters
that every reverb puts on its panel (mix, decay and tone), CV 1 plays the
decay and CV 2 freezes the reverb. Everything else stays as in the program.
Each program runs all the delay lines the plugin gives it; the engine
measures its CPU load and reduces a program that exceeds the budget (see
[CPU load](#cpu-load)).

## Quick start

1. Patch a stereo signal into the audio inputs and take the audio outputs to
   your mixer. A mono source goes into both inputs (or into In L only if your
   module normals In L to In R).
2. Turn Pot 1 to about 9 o'clock ("Medium Space"), Pot 2 (mix) to the
   centre, Pot 3 (decay) to 10 o'clock and Pot 4 (tone) to 2 o'clock. You
   hear the source in a medium-sized space with a tail of about a second.
3. Turn Pot 3 up: the tail grows to 10 s at 3 o'clock and to a minute fully
   clockwise, where the reverb becomes an endless wash that keeps building up
   while the source plays.
4. Turn Pot 4 down to darken the tail; every repetition through the delay
   lines loses more highs.
5. Turn Pot 1 through its zones: the reverb goes quiet for a moment and
   comes back as another program. Past the centre come the washes, at the
   end the echoes.
6. Patch a gate into CV 2 while something plays: the reverb freezes and
   holds its tail as long as the gate is high, while new notes pass dry on
   top. Patch an envelope into CV 1 instead to open up the decay for a note
   or a beat.

## Controls

| Control | Function |
|---------|----------|
| Pot 1   | Program: one of the ten programs |
| Pot 2   | Mix: dry/wet balance |
| Pot 3   | Decay: decay time of the late reverb |
| Pot 4   | Tone: low-pass filter in the feedback path of the delay lines |
| CV 1    | Decay, added to Pot 3 |
| CV 2    | Freeze gate |
| In L/R  | Audio input |
| Out L/R | Audio output |
| LED     | Lit while frozen; flashes when the reverb gets close to using all of the CPU time (see [CPU load](#cpu-load)) |

### Pot 1: Program

The pot's travel is split into ten zones of equal width, in the order
below: the plugin's spaces first, then its washes, then its echoes, then
the successor's plate (appended, so the nine keep the numbers the logs
and the hardware baselines use). The program determines
everything the other pots do not: the early reflections (tap count, length
and decay), the allpass diffusers, the modulation of the delay lines and the
diffusers, the shelving and input filters, the length of the delay lines and
how many of them run in parallel. The firmware gives every program its
own number of delay lines per channel (`kPrograms` in `cloudseed.cpp`;
the measured loads are in [Performance](#performance)). If the measured
CPU load is too high, that program reloads with fewer lines automatically
(see [CPU load](#cpu-load)).

| Zone (pot travel) | Program | Character |
|-------------------|---------|-----------|
| 1 (0 to 10%)   | Small Room | 3 lines with short delays, 21 early taps, no low-pass in the loop by default: a tight, bright room. |
| 2 (10 to 20%)  | Medium Space | The plugin's all-round space: 26 early taps, 7 early and 5 late allpass stages, 3 lines, mild modulation, a low shelf in the feedback. |
| 3 (20 to 30%)  | Noise in the Hallway | 50 configured taps bypassed by zero tap gain, 3 early allpass stages, 8 lines with a single late allpass stage and no modulation: a static, metallic hall. |
| 4 (30 to 40%)  | Hyperplane | A dense program: 9 lines with all 8 allpass stages each, strongly modulated lines, and all filters in use, including the input filters. One of the most CPU-hungry programs. |
| 5 (40 to 50%)  | Rubi-Ka Fields | A lush pad reverb: no early reflections in the output, 4 lines with 6 heavily modulated allpass stages each, and the two inputs partly mixed into each other. |
| 6 (50 to 60%)  | Through the Looking Glass | 50 early taps, 8 early and 8 late allpass stages, 12 lines with long delays, the widest stereo image: the endless wash, and the most CPU-hungry program. |
| 7 (60 to 70%)  | The 90s Are Back | A single early tap, 5 early allpass stages, 9 lines without late diffusion and with deep modulation: the chorused digital reverb of its decade. |
| 8 (70 to 80%)  | Dull Echoes | A 70 ms pre-delay, a low-pass on the input, 12 lines without late diffusion, taken after the delay: dark, modulated echoes. |
| 9 (80 to 90%)  | Chorus Delay | A 70 ms pre-delay, early taps spread over half a second, 12 lines with few allpass stages, deep modulation and no interpolation in the late diffusers (the plugin's "airy" mode): chorused echoes. |
| 10 (90 to 100%) | Dark Plate | The successor's one built-in program, converted (the library's `presets.h` says how): no early reflections, 12 lines of 96 ms with 4 interpolated allpass stages each, a 4.8 s decay, a low cut on the input and a high shelf in the feedback: a dark plate. Its random delay pattern is a variant of the successor's, whose random generator differs. |

The pot has to travel a quarter of a zone past a boundary before the
program changes, so a pot resting near a boundary stays where it is. A
change fades the reverb out over 10 ms, clears its memory, loads the program
and fades the reverb back in: the old tail is cut, the dry signal is never
interrupted. Pot 1 is read at start-up, so the module comes up with the
program it points at.

### Pot 2: Mix

An equal-power crossfade between the dry input and the reverb: fully
counter-clockwise only the input is heard, fully clockwise only the reverb,
and both are 3 dB down at the centre. The balance between the early
reflections and the late reverb inside the wet signal comes from the program.

The reverb's wet signal is soft-clipped before it is mixed in. With long decay
times the reverb builds up well beyond full scale when the source keeps
playing, and a soft clip sounds a lot better there than the hard clip of the
Seed's output. The dry signal is not clipped by the firmware. The combined
dry/wet signal can still exceed full scale at intermediate mix settings; the
codec output conversion then clips it. Allow some input headroom.

### Pot 3: Decay (with CV 1)

The time in which the late reverb decays by 60 dB, from 50 ms fully
counter-clockwise to 60 s fully clockwise, with the plugin's curve: about
0.3 s at 9 o'clock, 1.9 s at the centre and 10 s at 3 o'clock. The last part
of the travel is where the endless washes live.

The decay sets the feedback gain of every delay line so that all lines, whose
lengths differ, have the same nominal decay time. Feedback gains ramp across
an audio block when the decay changes, reducing steps from pot or CV movement.

CV 1 is added to the pot (0 V adds nothing, full scale adds the whole range),
with the sum limited to the longest decay. With the pot low, a gate or
envelope into CV 1 gives a note its own long tail while the reverb keeps
taking new input; this is the "infinite" of reverb pedals, where new notes
add to the sustained sound.

### Pot 4: Tone

The cutoff of the first-order low-pass filter that sits in the feedback path
of every delay line, from 400 Hz fully counter-clockwise to 20 kHz fully
clockwise, with the plugin's curve: about 1.7 kHz at 9 o'clock, 4.3 kHz at
the centre and 9.5 kHz at 3 o'clock. Because the filter is in the feedback
loop, the tail gets darker with every repetition, like air and walls absorb
high frequencies. Fully clockwise it still has a mild rolloff near Nyquist. The
programs' low and high shelf filters in the same loop stay as they are.

### CV 2: Freeze

While the gate is high (above about half of the CV range, with hysteresis),
the reverb is frozen the way a reverb pedal's "freeze" works: nothing new
enters the reverb, the delay lines recirculate what they hold with unity
feedback and bypass their damping filters. New notes pass through the dry
path at the level set by the mix pot. At fully wet, the dry notes are inaudible.
What was still on its way through the pre-delay and the early reflections when the
gate went high joins the held tail. When the gate goes low, the decay and
tone pots take over again and the tail dies away at the set decay time. The
LED is lit while frozen.

The hold is not perfectly lossless in the modulated programs: interpolation
attenuates highs, and time-varying delay reads can change energy. The test
signal in "Medium Space" lost about a decibel per second during the measured
hold. "Noise in the Hallway" has no modulation or interpolation loss.

## Why these controls

With one pot taken by the program selection, the three remaining pots and
two CV inputs follow what reverbs with a program selector put on their
panels:

- Reverb pedals with a type selector and three knobs all have the same three:
  decay/time, tone and mix (Boss RV-6: E.LEVEL, TONE, TIME; Electro-Harmonix
  Oceans 11: FX LVL, TIME, TONE; TC Electronic Hall of Fame 2: DECAY, TONE,
  LEVEL). Pedals with more knobs add pre-delay and modulation after those.
- The smallest Eurorack reverbs choose the same three: the 2hp Verb has TIME,
  DAMP and MIX; the Erica Synths Pico DSP's reverb has time and tone plus a
  dry/wet knob.
- When a Eurorack reverb has few CV inputs, decay/time gets one first (Pico
  DSP: only its first parameter, the reverb time, has CV), mix second (the
  2hp Verb's only CV input is MIX). This firmware puts the decay on CV 1.
- A freeze or hold gate is on most Eurorack reverbs and reverb-like
  processors (Noise Engineering Desmodus Versio, Intellijel Sealegs, Qu-Bit
  Aurora, Mutable Instruments Clouds and Beads), and in this repository the
  Nimbus firmware freezes on CV 2 as well, so CV 2 freezes here too. The
  freeze follows Strymon's definition (BigSky manual): the frozen reverb
  holds while new notes play on top without being added; the "infinite"
  alternative, where new notes are added, is what CV 1 into the decay does.

## Differences from the plugin

- Single precision in the signal path halves delay-buffer storage compared
  with the plugin's double precision. Parameter scaling, seeds, delay lengths
  and coefficient calculations use double precision.
- Every program runs its own number of delay lines (`kPrograms` in
  `cloudseed.cpp`, capped by the library's `CLOUDSEED_MAX_LINES` and
  reducible by the overload guard); the plugin's random values are generated
  for 12 lines, so a program that runs fewer uses the plugin's first ones.
- The delay-modulation LFOs start from fixed pseudo-random phases instead of
  `rand()`, and the sine table is interpolated.
- The dry signal is mixed in by the firmware (the program's dry level is
  ignored), the low-pass filter in the feedback path is always enabled
  because the tone pot drives it, and the wet signal is soft-clipped.
- Freeze is new; the plugin has no such function.
- Negative one-pole tails are allowed to decay symmetrically, and changing a
  diffuser seed also refreshes modulation depth and rate. These are
  deliberate bug fixes, made in the library.
- Pots are smoothed, mix and feedback changes ramp across a block, and the
  audio callback enables flush-to-zero for subnormal floating-point values.

The library's fidelity suite compares its four-line and twelve-line builds
with a corrected copy of the legacy C++ reference: with modulation off,
differences are 82 to 147 dB below the reference signal; with modulation on,
the measured maximum 100 ms envelope deviation is 3.22 dB (different LFO
phases prevent sample equality). See the library's TECHNICAL.md, "Fidelity
contract".

## CPU load

Cloud Seed is heavy for the Seed: every allpass stage, tap and delay line
costs memory accesses per sample. The engine measures each block with
libDaisy's `CpuLoadMeter`. When a block uses more than 90% of the available
time, subsequent callbacks take the dry path immediately. The main loop then
clears and reloads that program with one fewer late delay line per channel,
and the wet signal fades back in. Reductions are remembered separately for
each program until power-off; lighter programs keep their original line
counts. This changes the density of an overloaded program and cuts its
existing tail during recovery.

The LED flashes for one second after a budget breach. If even one late line
exceeds the budget, that program stays dry-only with the LED flashing until
another program is selected. The dry level still follows the mix pot, so
fully wet means silence during recovery or bypass. Pot 1 initiates switching
inside the callback, so the reverb is released even if heavy audio
interrupts have prevented the main loop from running.

The meter excludes libDaisy's surrounding audio-format conversion and some
interrupt overhead; the 10% reserve is a policy margin, not a measured bound.
An initial overrun can still glitch before recovery takes effect. `make
CLOUDSEED_MAX_LINES=4` caps every program's line count if desired (1 to 12;
a changed option rebuilds).

## Performance

The reverb's delay memory is placed in the Seed's internal SRAM per program
and staged through the MDMA in the tightly coupled memories, so that the
kernels run from zero-wait-state memory; the Seed runs at 480 MHz where its
silicon allows it, with the SDRAM refreshed and timed as its datasheet
requires and the internal SRAM mapped without write allocation. The
library's README and TECHNICAL.md describe the mechanisms and their measured
effects.

Measured on this module with the profiling build (`captures/` in the
library), the programs load the callback as follows, unfrozen, with no
overload:

| Program | Lines per channel (plugin) | Mean load | Peak block |
|---------|---------------------------:|---------:|-----------:|
| Small Room | 3 (3) | 20.1% | 21.6% |
| Medium Space | 3 (3) | 23.7% | 25.0% |
| Noise in the Hallway | 8 (8) | 20.1% | 21.2% |
| Hyperplane | 9 (9) | 56.2% | 59.3% |
| Rubi-Ka Fields | 4 (4) | 27.7% | 29.1% |
| Through the Looking Glass | 12 (12) | 80.9% | 84.1% |
| The 90s Are Back | 9 (9) | 22.7% | 24.3% |
| Dull Echoes | 12 (12) | 27.3% | 28.8% |
| Chorus Delay | 12 (12) | 44.0% | 46.0% |
| Dark Plate | 12 | not yet measured | |

The capture measures a build before the latest changes (the library's
TECHNICAL.md says which); the current build is host-verified only and has
not run on the module. The adaptive overload guard stays enabled.

### Measuring the load

```sh
make CLOUDSEED_PROFILE=1 flash
```

builds a variant that logs over the Seed's USB serial port (a CDC device,
any terminal program) and flashes it (`flash` builds first; libDaisy's
`program-dfu` alone flashes whatever the build directory holds). Every
build ends by printing `image=`, the CRC-32 of its `.bin` (also
`make image-crc`); the firmware prints the same value for the image it
runs from, so a log names its build. At start-up it prints the library's
build options, the clock, the silicon revision and that CRC (`build ...`),
then this firmware's board options (`board ...`). Whenever a program is
loaded it prints what that program makes the reverb do, and once per second
a report with the load, its breakdown into the callback's sections, the
controls and the staging's transport. The library's README describes the
lines; the `controls=` values are, in order, the four pots, CV 1 as used and
raw, CV 2 as used and raw, the decay the callback derived and the mix, each
0 to 1000. `mixed=1` marks an interval whose program, line count or freeze
state changed; use `mixed=0` for steady-state comparisons.

Measure with the module in the rack. On USB power alone the CV input
stage is unpowered, its op-amps' outputs rest at 0 V, and the firmware
reads that as full scale on both CV inputs: the reverb is frozen and the
decay at its maximum, which the `controls` and `freeze` fields show.

The board options below belong to this firmware's Makefile; the library's
options (`CLOUDSEED_MAX_LINES`, `CLOUDSEED_STAGING`, `CLOUDSEED_PROFILE`,
`CLOUDSEED_DTCM_STAGING_KB`, ...) are documented in the library's README and
take effect on the same command line.

| Option | Default | Effect |
|--------|---------|--------|
| `CLOUDSEED_BOOST` | 1 | 480 MHz on silicon revision V or X |
| `CLOUDSEED_SDRAM_WRITE_ALLOCATE` | 0 | Cache SDRAM writes (write-back, write-allocate) |
| `CLOUDSEED_SRAM_WRITE_ALLOCATE` | 0 | Cache internal SRAM writes (the Cortex-M7 default) instead of write-back without write allocation |
| `CLOUDSEED_SDRAM_FAST_TIMING` | 0 | The SDRAM's datasheet row/column delays, with a memory test at boot that falls back to the conservative ones (experiment) |

With ARM GCC 16.2, the default build takes 113,420 bytes of the 131,072-byte
flash; the profiling build takes 127,768 bytes, leaving 3,304 bytes. The
default uses 106,240 bytes of DTCM before the stack (the engine, the reverb,
the sine table and 24 KB of staging memory), 65,504 bytes of ITCM, 489,280
bytes of AXI SRAM, 294,272 bytes of D2 SRAM and 15,974,400 bytes of SDRAM.
The stack has the remaining 24,832 bytes of the DTCM; the profiling build
reports how much of it was never used.

## Building

Fetch the submodules (`git submodule update --init --recursive`; the
library is `lib/cloudseed-daisy`), build the libraries once with
`./build_libs.sh` from the repository root, then run `make` in this folder;
the binary ends up in `build/cloudseed.bin`. The options above can be given
on the make command line; a changed option rebuilds the firmware.

The host tests of the reverb, the staging, the engine and the fidelity
against the plugin's reference live in the library (`lib/cloudseed-daisy/test`,
see its README).

## License

The reverb kernel is a port of Cloud Seed, copyright (c) 2018 Valdemar
Erlingsson, MIT License; see the library's LICENSE for its notices and those
of the works it contains.
