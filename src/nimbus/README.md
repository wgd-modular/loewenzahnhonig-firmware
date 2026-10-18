# Nimbus

## Author

Ben van der Burgh

Nimbus is Ben Sergentanis' (Electro-Smith) port of Clouds by Émilie Gillet
(Mutable Instruments). The Clouds DSP code in this folder (`dsp/`, `resources.*`,
`shy_fft.h`, `stmtemp.h`, `buffer_allocator.h`, `parameter_interpolator.h`) is
taken unchanged from the [Daisy Patch version of Nimbus](https://github.com/electro-smith/DaisyExamples/tree/master/patch/Nimbus)
in DaisyExamples. Only `nimbus.cpp` is specific to the Löwenzahnhonig: it maps
the module's four pots and two CV inputs onto Clouds' parameters and fixes the
parameters that have no control of their own.

## What it does

Clouds continuously records the incoming audio into a short buffer and plays
back short, overlapping, enveloped fragments of it ("grains"), optionally
transposed, panned and reverberated. Nimbus on the Löwenzahnhonig runs Clouds'
granular mode in 16-bit stereo quality with a buffer of about 0.7 s, and the
output is always fully wet. Nothing needs to be patched into the CV inputs: with
them unpatched, the four pots give you a live granular effect. Patch a gate into
CV 2 and the module becomes Clouds' other half, a frozen buffer you play with
the pots.

## Quick start

1. Patch a stereo (or mono, left) signal into the audio inputs and take the
   audio outputs to your mixer. The dry signal is not passed through; mult the
   source if you want to mix it in.
2. Turn Pot 1 fully counter-clockwise, Pot 2 to about 10 o'clock, Pot 3 to the
   centre and Pot 4 a little left of the centre. You hear the input, slightly
   thickened. Play Pot 3: the grains are transposed up to two octaves in either
   direction.
3. Turn Pot 4 past the centre. The grains are now sown randomly and get
   smoother and more reverberant the further you go: the classic cloud.
4. Turn Pot 1 up while something plays: the grains are taken from further back
   in time.
5. Hold a gate high on CV 2 while a sound plays. The buffer freezes; sweep Pot 1
   through the captured audio and play Pot 2 and Pot 3.

## Controls

| Control | Function |
|---------|----------|
| Pot 1   | Position: where in the recording buffer the grains are taken from |
| Pot 2   | Size: grain length |
| Pot 3   | Pitch: transposition of the grains |
| Pot 4   | Density: rate and randomness of the grains, plus texture and reverb on the right half |
| CV 1    | Position, added to Pot 1 |
| CV 2    | Freeze gate; each rising edge also plays one grain |
| In L/R  | Audio input (recorded into the buffer while not frozen) |
| Out L/R | Audio output (fully wet, soft-clipped) |

### Pot 1: Position (with CV 1)

Selects from which part of the 0.7 s recording buffer the grains are taken.
Fully counter-clockwise, grains come from the most recent audio and the effect
follows the input with only a grain's worth of delay. Turning clockwise travels
back in time, up to about 0.7 s fully clockwise. While the buffer is frozen this
scrubs through the captured audio.

CV 1 is added to the pot (0 V adds nothing, +5 V adds the whole range and
-5 V takes it away), with the sum clamped to the buffer. A slow LFO or a random voltage smears the
grains over the recent past; a slow ramp or sequence scrubs a frozen buffer.

### Pot 2: Size

The length of each grain, from about 20 ms fully counter-clockwise to about
340 ms fully clockwise (half the buffer), with an exponential response. Short
grains sound buzzy or metallic and turn the position into a timbre control on
frozen audio; long grains preserve the character of the source.

### Pot 3: Pitch

Transposes the grains from two octaves down (fully counter-clockwise) to two
octaves up (fully clockwise). A small zone around the centre, about 4% of the
travel on either side, is exactly unison, so the pot does not need to be
centred precisely. Outside that zone the response is quadratic: fine steps near
the centre, coarse steps towards the ends, like the Daisy Patch version.

### Pot 4: Density

This is Clouds' density control, with the same shape as on the original
module:

- Left of the centre, grains are played at a regular rate. The further left,
  the more grains overlap (up to 32 at once).
- At the centre, in a small dead zone, no grains are generated at all. Because
  the output is fully wet, the module is silent here apart from the reverb tail
  and grains triggered through CV 2.
- Right of the centre, grains are sown at random times, again denser towards
  the end.

On the Löwenzahnhonig the right half of the pot additionally does what Clouds'
Texture knob and reverb setting would do:

- The grain envelopes morph from triangular to a smooth Hann window
  (texture 0.5 to 1.0), and in the last quarter of the travel Clouds' diffuser
  fades in and smears the transients.
- The reverb amount rises from 0.2 to 0.6.

So the left half is the clean side (regular grains, useful as a pitch shifter
or a thickener) and the right half goes from a sparse random scatter to a
thick, diffuse wash.

### CV 1: Position

Added to Pot 1, see above. The input is bipolar: -5 V to +5 V moves the
position back and forth across the buffer, and 0 V changes nothing.

### CV 2: Freeze / trigger

While the voltage is above about +2 V, the buffer stops recording and the
pots play the captured audio. Recording resumes when the voltage drops below
about +1 V (there is some hysteresis, so slow voltages do not chatter).
The Daisy Seed's on-board LED lights while the buffer is frozen. As on Clouds,
the feedback path is muted while frozen and the reverb rises a little.

Every rising edge on CV 2 also plays a single grain, which is what Clouds' TRIG
input does. With Pot 4 centred (no automatic grains) this turns the module into
a trigger-driven micro-sampler: each pulse plays one grain of the audio just
before the pulse, at the current position, size and pitch. Use triggers of at
least a millisecond, and keep them short if the buffer should keep recording
between them; a long gate is a freeze.

## Patch ideas

- **Pitch shifter.** Pot 1 fully counter-clockwise, Pot 2 around 10 o'clock,
  Pot 4 a little left of the centre. Play Pot 3, or turn it to an octave and
  mix with the dry signal for a harmoniser.
- **Cloud.** Pot 4 right of the centre, Pot 2 past noon, an LFO into CV 1 and
  Pot 3 a fifth or an octave up.
- **Drone.** Play a chord or a phrase into the module and hold a gate high on
  CV 2 at the moment you like. Sweep Pot 1 slowly, play Pot 2 and Pot 3, and
  push Pot 4 to the right for a wash. Send short pulses to CV 2 instead to keep
  capturing new fragments while a patch plays.
- **Micro-sampler.** Pot 4 centred, a clock into CV 2: one grain per pulse.
  Pot 1 chooses which moment of the last 0.7 s is played, Pot 2 how much of it,
  Pot 3 at which pitch. A random voltage into CV 1 picks a different moment
  every time.
- **Stutter.** A square LFO into CV 2 freezes and releases the buffer
  rhythmically; add a slow ramp into CV 1.

## Fixed parameters

| Parameter     | Value                                   |
|---------------|-----------------------------------------|
| Dry/wet       | 1.0 (fully wet)                          |
| Stereo spread | 0.5 (random panning of the grains)      |
| Feedback      | 0.15                                    |
| Texture       | 0.5, rising to 1.0 over the right half of Pot 4 |
| Reverb        | 0.2, rising to 0.6 over the right half of Pot 4 |
| Mode          | Granular                                |
| Quality       | 16-bit stereo                           |

All of these are constants at the top of `nimbus.cpp`, together with the pitch
dead zone and the freeze thresholds, if you want to tune them.

## Design decisions

The Löwenzahnhonig has four pots and two CV inputs and nothing else, while
Clouds has six knobs, four blend parameters, a freeze button and five CV inputs.
The mapping was chosen so that the module feels like Clouds and stays playful
with nothing patched into the CV inputs, and so that the CV inputs control what
is most rewarding to modulate.

- **The four pots are Clouds' four grain controls.** Position, Size, Pitch and
  Density are the knobs one plays on a Clouds; Texture and the blend page are
  usually set once. Giving them the pots keeps the essential hands-on
  interaction intact.
- **Position starts at "now".** An earlier version fixed Position at the middle
  of the buffer. At unison pitch every grain reads the same sample of the buffer
  regardless of when it started, so a fixed position turns the granular engine
  into a plain delay of that length (340 ms in that version). Starting at the
  most recent audio makes the effect follow the input, and the delay grows only
  when you ask for it.
- **CV 1 modulates Position.** This is the classic Clouds modulation: an LFO or
  random voltage on Position produces the evolving cloud, and on a frozen buffer
  it scans the captured sound. Pitch CV would have been the alternative, but it
  is less central to the cloud sound and a V/Oct response would need the input
  stage to be calibrated.
- **CV 2 freezes and triggers.** Freeze is Clouds' signature performance
  feature and the one thing a pot cannot do, so it needs a jack. Clouds' trigger
  input is folded into the same jack because a trigger is a short gate: the
  buffer is frozen only for the length of the pulse, which is barely
  noticeable, and the grain it launches is what Clouds' TRIG input would play.
  With Pot 4 centred this turns Clouds' silent centre into the micro-sampler
  mode instead of a dead spot.
- **Pot 4 doubles as texture and reverb.** No pot was left for Texture. Its
  effect is most valuable on the random side of Density, where smoother
  envelopes and the diffuser make the wash, so it follows the right half of
  Pot 4, together with a rising reverb amount. The left half keeps the plain
  triangular envelopes that make regular grains a clean pitch shifter.
- **The output is always fully wet.** A dry/wet pot would have displaced one of
  the grain controls. Fully wet is also what makes freeze behave like Clouds'
  frozen state: the live input disappears and only the captured cloud remains.
  The dry signal is easy to mix in outside the module.
- **The output is soft-clipped.** Overlapping grains at unison pitch add up
  coherently; 32 regular grains come out roughly 15 dB louder than the input,
  and the wet path carries a fixed 1.2 post-gain, reverb and feedback on top.
  The original Clouds firmware soft-clips its output for this reason, the Daisy
  port had dropped that step, and libDaisy otherwise hard-clamps the output at
  full scale. The same curve as in Clouds (`SoftClip`) is applied to the output.
- **Pitch has a dead zone.** The pots have no centre detent, so a pot that is
  visually centred would otherwise sit a few cents off unison.
- **Granular mode and 16-bit stereo by default; other engines and qualities
  as separate builds.** The module has no way to switch Clouds' modes or
  quality settings at runtime, so they are offered as variants (see below)
  instead of a menu. 16-bit stereo gives the best fidelity; it limits the
  buffer to about 0.7 s at 48 kHz, which is plenty for a live granulator and
  for freezing phrases. The lo-fi variant trades fidelity for a 5 s buffer.

Known limitations that follow from these choices: there is no pitch CV and no
dry/wet control; Pot 4 is silent at its centre unless CV 2 is triggered; dense
regular grains at unison are loud; and a gate on CV 2 has to rise above about
+2 V to freeze.

## Variants

Nimbus comes in several variants, each a separate binary with a different
configuration, so you can pick one without touching the code. All of them share
`nimbus.cpp`, the fully wet and soft-clipped output, CV 1 on the position and
the freeze/trigger gate on CV 2.

| Variant | Folder | Engine | What differs |
|---------|--------|--------|--------------|
| `nimbus` | `src/nimbus` | Granular, 16-bit stereo | The default described in this file |
| `nimbus-stretch` | `src/nimbus-stretch` | Pitch-shifter/time-stretcher | Pot 4 is a low-pass/high-pass tone filter; clean transposition, and time-stretching of a frozen buffer with Pot 1 |
| `nimbus-looping-delay` | `src/nimbus-looping-delay` | Looping delay | Pot 1 delay time, Pot 2 feedback, Pot 3 pitch of the repeats, Pot 4 window size (loop length when frozen); freeze loops the buffer, triggers set the delay time |
| `nimbus-lofi` | `src/nimbus-lofi` | Granular, 8-bit mu-law mono | Same controls, about 5 s of buffer, up to 57 grains, gritty 24 kHz audio |

Each folder has a README with the details, and the binaries are attached to
the releases next to the other firmwares.

### Making your own variant

1. Copy one of the `src/nimbus-*` folders to a new folder under `src/` and give
   it its own `TARGET` name and `NIMBUS_VARIANT` define in the Makefile.
2. Add a matching `#elif defined(NIMBUS_VARIANT_...)` block in
   `src/nimbus/nimbus.cpp` with a `Variant`: the playback mode, the audio
   quality (0 = 16-bit stereo, 1 = 16-bit mono, 2 = 8-bit stereo, 3 = 8-bit
   mono), the role of each pot (position, size, pitch, density, texture or
   feedback) and the values of the parameters that get no pot.
3. Run `make` in the new folder and describe the variant in its README.
   `build_firmwares.py` and CI pick up every folder with a Makefile.

## Technical notes

- The audio callback runs with blocks of 32 samples; Clouds does not work with
  larger blocks.
- The recording buffer and the FX workspace (about 180 kB) live in the Daisy
  Seed's internal SRAM. In 16-bit stereo quality the buffer holds 32704 samples
  per channel, about 0.68 s at 48 kHz.
- `processor.Prepare()` runs continuously in the main loop; it performs the
  slow, non-real-time work of the granular processor.
- The pots and CV inputs are read through the shared `Loewy` class in `lib/`,
  which smooths them at the audio callback rate. The freeze gate is evaluated
  once per block, so triggers shorter than a block (about 0.7 ms) can be missed.
- All variants compile the same `nimbus.cpp`. `nimbus.mk` holds the shared
  build rules; a variant folder's Makefile only sets the target name and the
  `NIMBUS_VARIANT_*` define that selects the configuration.

## Building

Build the libraries once with `./build_libs.sh` from the repository root, then:

```shell
cd src/nimbus
make
```

The binary ends up in `build/nimbus.bin`. The variants are built the same way
from their own folders (`src/nimbus-stretch` and so on).

## Credits

- Clouds: Émilie Gillet (Mutable Instruments), MIT licensed
- Nimbus (Daisy port of Clouds): Ben Sergentanis (Electro-Smith)
- Löwenzahnhonig port: created using Claude Code
