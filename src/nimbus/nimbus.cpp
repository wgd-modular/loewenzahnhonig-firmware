#include <math.h>

#include "../../lib/loewy.h"
#include "../../lib/utils.h"
#include "daisysp.h"
#include "granular_processor.h"

using namespace daisy;
using namespace loewy;

/*
 * Author: Ben van der Burgh
 *
 * Nimbus (Electro-Smith's port of Mutable Instruments Clouds) for the
 * Löwenzahnhonig. This file is shared by the Nimbus variants: every variant
 * folder compiles it with a NIMBUS_VARIANT_* define that selects one of the
 * configurations below (playback mode, audio quality, what the pots control
 * and the values of the parameters without a control). See README.md.
 *
 * Common to all variants:
 * - The output is always fully wet and soft-clipped.
 * - CV 1 is added to the playback position.
 * - CV 2 is a freeze/trigger gate: the buffer is frozen while the gate is
 *   high, and every rising edge sends Clouds' trigger (a single grain in
 *   granular mode, a clock in the other modes).
 */

namespace {

// What a pot controls.
enum class Role {
  kPosition,  // where in the buffer playback happens (CV 1 is added to it)
  kSize,      // grain size, window size or loop length, depending on the mode
  kPitch,     // -2..+2 octaves with a unison dead zone around the centre
  kDensity,   // Clouds' density; in granular mode the right half also raises
              // texture (0.5 -> 1.0) and reverb (0.2 -> 0.6)
  kTexture,   // grain envelope (granular) or low-pass/high-pass filter
  kFeedback,  // amount of output fed back into the buffer
};

struct Variant {
  PlaybackMode playback_mode;
  int32_t quality;  // 0: 16-bit stereo, 1: 16-bit mono, 2: 8-bit stereo,
                    // 3: 8-bit mono (mu-law, half the sample rate)
  Role pot[Loewy::kNumPots];
  // Values of the parameters that no pot controls in this variant. Position
  // without a pot is 0 (the most recent audio) plus CV 1.
  float density;
  float texture;
  float feedback;
  float reverb;
  float stereo_spread;
};

#if defined(NIMBUS_VARIANT_STRETCH)
// Clouds' pitch-shifter/time-stretcher: overlapping windows instead of grains.
// Pot 4 is the low-pass/high-pass filter (open at the centre).
constexpr Variant kVariant = {
    PLAYBACK_MODE_STRETCH,
    0,
    {Role::kPosition, Role::kSize, Role::kPitch, Role::kTexture},
    /*density=*/0.f,
    /*texture=*/0.5f,
    /*feedback=*/0.f,
    /*reverb=*/0.2f,
    /*stereo_spread=*/0.5f};
#elif defined(NIMBUS_VARIANT_LOOPING_DELAY)
// Clouds' looping delay: Pot 1 is the delay time (loop start when frozen),
// Pot 2 the feedback, Pot 3 the pitch of the repeats, Pot 4 the size of the
// pitch-shifting windows (loop length when frozen).
constexpr Variant kVariant = {
    PLAYBACK_MODE_LOOPING_DELAY,
    0,
    {Role::kPosition, Role::kFeedback, Role::kPitch, Role::kSize},
    /*density=*/0.f,
    /*texture=*/0.5f,
    /*feedback=*/0.f,
    /*reverb=*/0.2f,
    /*stereo_spread=*/0.5f};
#elif defined(NIMBUS_VARIANT_LOFI)
// The default mapping with 8-bit mu-law mono audio: about 5 s of buffer.
constexpr Variant kVariant = {
    PLAYBACK_MODE_GRANULAR,
    3,
    {Role::kPosition, Role::kSize, Role::kPitch, Role::kDensity},
    /*density=*/0.f,
    /*texture=*/0.5f,
    /*feedback=*/0.15f,
    /*reverb=*/0.2f,
    /*stereo_spread=*/0.5f};
#else
// Default: Clouds' granular mode in 16-bit stereo.
constexpr Variant kVariant = {
    PLAYBACK_MODE_GRANULAR,
    0,
    {Role::kPosition, Role::kSize, Role::kPitch, Role::kDensity},
    /*density=*/0.f,
    /*texture=*/0.5f,
    /*feedback=*/0.15f,
    /*reverb=*/0.2f,
    /*stereo_spread=*/0.5f};
#endif

// Pitch pot: quadratic curve for fine control near unison, with a dead zone so
// that a roughly centred pot is exactly unison.
constexpr float kPitchDeadZone = 0.04f;
constexpr float kPitchRangeSemitones = 24.f;

// Freeze gate thresholds in volts on CV 2, with hysteresis. They are low enough
// for gates of 3.3 V and more and well above the 0 V of a gate that is low.
constexpr float kFreezeOnVolts = 2.0f;
constexpr float kFreezeOffVolts = 1.0f;

float PitchFromPot(float pot) {
  float offset = pot - 0.5f;
  float magnitude = fabsf(offset) - kPitchDeadZone;
  if (magnitude <= 0.f) return 0.f;
  float x = magnitude / (0.5f - kPitchDeadZone);
  float semitones = kPitchRangeSemitones * x * x;
  return offset < 0.f ? -semitones : semitones;
}

}  // namespace

Loewy hardware;
GranularProcessorClouds processor;

// Sample memory and FX workspace of the granular processor, sized like the
// original Clouds firmware. Both arrays live in the Seed's internal SRAM.
uint8_t block_mem[118784];
uint8_t block_ccm[65536 - 128];

Parameters* parameters;
bool frozen = false;

// Reads the pots and CV 1 into the parameters according to the variant.
void ReadControls() {
  parameters->position = 0.f;
  parameters->density = kVariant.density;
  parameters->texture = kVariant.texture;
  parameters->feedback = kVariant.feedback;
  parameters->reverb = kVariant.reverb;

  const float pots[Loewy::kNumPots] = {hardware.GetPot1(), hardware.GetPot2(),
                                       hardware.GetPot3(), hardware.GetPot4()};
  for (size_t i = 0; i < Loewy::kNumPots; i++) {
    const float value = pots[i];
    switch (kVariant.pot[i]) {
      case Role::kPosition:
        parameters->position = value;
        break;
      case Role::kSize:
        parameters->size = value;
        break;
      case Role::kPitch:
        parameters->pitch = PitchFromPot(value);
        break;
      case Role::kDensity: {
        parameters->density = value;
        if (kVariant.playback_mode == PLAYBACK_MODE_GRANULAR) {
          // The clockwise half also morphs the grain envelopes from triangle
          // to Hann (Clouds engages its diffuser above texture 0.75) and
          // raises the reverb amount.
          const float wash = clamp((value - 0.5f) * 2.f, 0.f, 1.f);
          parameters->texture = 0.5f + 0.5f * wash;
          parameters->reverb = 0.2f + 0.4f * wash;
        }
        break;
      }
      case Role::kTexture:
        parameters->texture = value;
        break;
      case Role::kFeedback:
        parameters->feedback = value;
        break;
    }
  }

  parameters->position =
      clamp(parameters->position + hardware.GetCV1(), 0.f, 1.f);
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  hardware.ProcessControls();
  ReadControls();

  // CV 2 freezes the buffer while high and sends a trigger per rising edge.
  // The trigger flag is consumed by the processor within this block.
  const float gate = hardware.GetCVVolts(Loewy::CV::CV_2);
  const bool was_frozen = frozen;
  if (gate > kFreezeOnVolts) {
    frozen = true;
  } else if (gate < kFreezeOffVolts) {
    frozen = false;
  }
  parameters->freeze = frozen;
  parameters->trigger = frozen && !was_frozen;
  hardware.SetLed(frozen);

  FloatFrame input[size];
  FloatFrame output[size];

  for (size_t i = 0; i < size; i++) {
    input[i].l = in[0][i];
    input[i].r = in[1][i];
    output[i].l = output[i].r = 0.f;
  }

  processor.Process(input, output, size);

  // The wet signal can exceed full scale (dense overlapping grains, the 1.2
  // post-gain, reverb and feedback) and libDaisy hard-clamps the output. The
  // original Clouds firmware soft-clips its output with this same curve
  // (SoftConvert), which the Daisy port dropped.
  for (size_t i = 0; i < size; i++) {
    out[0][i] = daisysp::SoftClip(output[i].l);
    out[1][i] = daisysp::SoftClip(output[i].r);
  }
}

int main(void) {
  Loewy::Config config;
  config.audio_block_size = 32;  // Clouds does not work with larger blocks
  hardware.Init(config);

  float sample_rate = hardware.GetSampleRate();
  InitResources(sample_rate);
  processor.Init(sample_rate, block_mem, sizeof(block_mem), block_ccm,
                 sizeof(block_ccm));
  processor.set_playback_mode(kVariant.playback_mode);
  processor.set_quality(kVariant.quality);

  parameters = processor.mutable_parameters();
  parameters->dry_wet = 1.f;
  parameters->stereo_spread = kVariant.stereo_spread;
  parameters->freeze = false;
  parameters->trigger = false;
  parameters->gate = false;

  hardware.StartAudio(AudioCallback);

  // Prepare() does the slow, non-real-time work of the processor and has to
  // run continuously in the main loop.
  while (1) {
    processor.Prepare();
  }
}
