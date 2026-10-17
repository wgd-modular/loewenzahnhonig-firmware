# Rings

## Author

Port to the Löwenzahnhonig: Hans. DSP: Mutable Instruments Rings by Emilie Gillet (MIT licence), used unchanged from `lib/eurorack`.

## Description

The resonator of Mutable Instruments Rings: modal bodies, sympathetic strings and inharmonic strings, excited by the audio input or by its own internal plucker. Up to two notes ring on together (each strum moves to the next voice while the previous one decays).

The module has no button, so the resonator model is chosen by flashing a variant:

| Firmware | Model |
| --- | --- |
| `rings` | Modal resonator (Rings' default) |
| `rings-sympathetic` | Sympathetic strings |
| `rings-string` | Inharmonic string |
| `rings-fm` | FM voice (bonus model) |
| `rings-sympathetic-quantized` | Quantized sympathetic strings (bonus model) |
| `rings-string-reverb` | String and reverb (bonus model) |
| `rings-string-synth` | "Disastrous Peace" string synth with ensemble (Rings' easter egg) |

## Controls

- Pot 1: frequency. Five octaves; snaps to semitones while CV 1 carries a V/oct signal, free otherwise.
- Pot 2: structure. In the sympathetic-string models it also picks one of Rings' 11 chords.
- Pot 3: brightness
- Pot 4: damping
- CV 1: V/oct, added to Pot 1. Bipolar, −5 V to +5 V.
- CV 2: strum. A rising edge above 1 V plays a new note (it resets below 0.5 V).
- In L: exciter. The right input is not used, as on Rings.
- Out L / Out R: Rings' ODD and EVEN outputs (odd and even partials, or the two voices); together they give a wide stereo image.

Position (Rings' fifth knob) is fixed at 0.5.

## How it decides what is patched

Rings senses which jacks are patched. The Löwenzahnhonig cannot, so the firmware watches for activity instead and keeps each input "patched" for two seconds after its last activity:

- Audio on In L above about −50 dBFS: the input is the exciter. Otherwise the internal plucker excites the resonator.
- CV 2 going high: strums come from CV 2. Otherwise Rings' own logic strums: on a new note on CV 1, or on onsets in the audio input.
- CV 1 more than 50 mV away from 0 V: the pitch follows it in semitone steps.

With nothing patched at all the module stays silent, as Rings does. Patch a trigger into CV 2 to make it play.

## CV range

The CV inputs read −5 V to +5 V (`RINGS_CV_MIN_VOLTS` / `RINGS_CV_MAX_VOLTS` in `rings.cpp`). V/oct tracking follows from that range; if a particular module tracks slightly off, adjust those two values.

## Build-time options

Set in a variant's `Makefile` through `RINGS_DEFS`, or at the top of `rings.cpp`:

- `RINGS_MODEL`: 0–5, see the table above
- `RINGS_POLYPHONY`: 1, 2 or 4 (default 2). More voices means fewer partials per voice, as on Rings.
- `RINGS_STRING_SYNTH`: build the string synth instead of the resonator
- `RINGS_SYNTH_FX`: effect of the string synth (`FX_FORMANT`, `FX_CHORUS`, `FX_REVERB`, `FX_FORMANT_2`, `FX_ENSEMBLE`, `FX_REVERB_2`)
- `RINGS_CV_MIN_VOLTS`, `RINGS_CV_MAX_VOLTS`, `RINGS_POSITION`: see above

## Notes

- The firmware uses 95 % of the Seed's 128 KB internal flash (Rings' lookup tables take most of it). Adding a lot more code will need the QSPI boot mode.
- The Mutable Instruments sources are `.cc` files; libDaisy's Makefile only compiles `.cpp`, so `mi/` contains one-line wrappers that include them.
