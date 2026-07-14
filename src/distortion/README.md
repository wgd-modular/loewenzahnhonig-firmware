# Distortion V0.6

## Current State

Boss MT-2 (Metal Zone) inspired stereo distortion for the Loewenzahnhonig-module by WGD Modular,
with 4x oversampling around the clipping stages. Perfectly fits your Acid-Distortion-Needs with an 303'ish synth.

- DaisySeed and stereo audio initialized
- ADC set up for four pots and two CV inputs
- Two-stage distortion (soft -> hard) with pre-emphasis before and tone
  shaping after
- 4x oversampling of the nonlinearities via polyphase allpass halfband
  filters
- Separate filter states for left and right channel
- Fixed (non-adjustable) noise gate that mutes the amplified idle noise
  floor at high gain settings
- Block-based, sample-rate-aware parameter smoothing
- Safe output protection

> The firmware is MT-2 inspired by, but not an exact digital reproduction of
> the original pedal.

## Architecture

- `distortion.cpp`: hardware initialization, ADC, control updates and
  audio callback
- `distortion_engine.cpp/.h`: parameter state, smoothing, oversampler and
  the complete stereo DSP signal path
- The audio callback delegates one stereo block directly to the DSP engine.
- All filters run without dynamic memory allocation.

## Controls

- Pot 1: Distortion
- Pot 2: Low (shelving filter at 100 Hz, center = flat)
- Pot 3: High (shelving filter at 3.2 kHz, center = flat)
- Pot 4: Output Level
- CV 1: Distortion modulation
- CV 2: High modulation

Following the hardware convention, CV1 and CV2 are read inverted, scaled by
`0.5` and added to the corresponding pot value. The result is clamped to
`0.0` to `1.0`.

## Signal Path

DC blocker -> tight highpass (75 Hz) -> pre-emphasis (mid boost 950 Hz,
high shelf 2.2 kHz) -> distortion gain -> [4x upsampling -> soft clip
(tanh) -> hard clip (cubic curve with hard limit) -> 4x downsampling] ->
fixed mid scoop (700 Hz, -8 dB) -> fizz lowpass (7.5 kHz) -> low shelf ->
high shelf -> output level -> noise gate -> output clamp

The noise gate sidechain reads the DC-blocked *input*, so the gate decision
is unaffected by gain and tone settings; the gate gain is applied to the
*output*, like a gate placed after a distortion pedal.

Processing stays fully stereo. There is no mono summing.

## Sound Goal

- classic MT-2 sound: scooped mids, dense compression, sawing presence
- tight bass through the pre-distortion tight highpass, recoverable via
  the Low control after distortion
- controlled highs without digital fizz (fizz lowpass)
- significantly reduced aliasing through 4x oversampling

## Technical Details

- DC blocker: sample-rate-aware one-pole highpass at 5 Hz
- Tight highpass: one-pole highpass at 75 Hz before distortion
- Pre-emphasis: peaking EQ +8 dB at 950 Hz (Q 0.70) and high shelf +4 dB
  at 2.2 kHz -- shapes the MT-2 aggression before clipping
- Distortion gain: `3.0 * powf(120.0, distortion)`, approx. 3x to 360x
  (+9.5 dB to +51 dB)
- Oversampling: 4x via two cascaded 2x polyphase allpass halfband stages
  (2 allpass sections per path, ~70 dB stopband, numerically verified:
  audible aliasing 15-20 dB lower than without oversampling, passband
  ripple of the chain < 0.01 dB)
- First clipping stage: symmetric soft clip using `tanhf()`
- Second clipping stage: 1.6x gain, cubic saturation `1.5x - 0.5x^3` with
  hard limit at `+/-1.0`
- Mid scoop: fixed peaking EQ at 700 Hz, Q 0.80, -8 dB (MT-2 signature)
- Fizz lowpass: one-pole lowpass at 7.5 kHz after distortion
- Low: RBJ low shelf at 100 Hz, -12 dB to +12 dB, center flat
- High: RBJ high shelf at 3.2 kHz, -12 dB to +8 dB, center flat
- Output level: quadratic curve `level * level * 1.25`
- Noise gate (fixed): opens at -54 dBFS input level, closes below -60 dBFS
  (hysteresis); envelope attack 0.5 ms, release 25 ms; gate opens in ~2 ms,
  holds 50 ms and fades out over ~80 ms per time constant (silence roughly
  0.7 s after the signal stops); per channel, no controls
- Smoothing: block-based with approx. 20 ms time constant, coefficient
  derived from sample rate and block size
- Filter coefficients: RBJ biquads; fixed filters are computed once in
  `Init()`, variable filters only after relevant parameter changes
- Output protection: hard limit to `-1.0` to `+1.0`; non-finite values are
  discarded

## Not Yet Included (Maybe to be continued)

- exact analog MT-2 circuit emulation (incl. parametric mid control)
- presets
- switching between multiple models
- auto gain

Final sound tuning has to happen on the target hardware with synths, bass,
drums, drones and noise.
