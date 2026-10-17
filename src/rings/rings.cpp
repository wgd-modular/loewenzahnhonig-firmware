#include "../../lib/loewy.h"
#include "../../lib/utils.h"
#include "rings/dsp/part.h"
#include "rings/dsp/string_synth_part.h"
#include "rings/dsp/strummer.h"
#include "stmlib/dsp/dsp.h"

using namespace daisy;
using namespace loewy;
using namespace rings;

/*
 * Rings for the Löwenzahnhonig: the resonator of Mutable Instruments Rings
 * by Emilie Gillet (MIT licence), running on the module's Daisy Seed.
 *
 * Pot 1: frequency (coarse, 5 octaves; semitone steps when CV 1 is in use)
 * Pot 2: structure (also selects the chord in the sympathetic-string models)
 * Pot 3: brightness
 * Pot 4: damping
 * CV 1:  V/oct, added to Pot 1
 * CV 2:  strum trigger (rising edge above half scale)
 * In L:  exciter (audio in; the right input is ignored, as on Rings)
 * Out L: odd partials / Out R: even partials (Rings' ODD and EVEN outputs)
 *
 * The resonator model and polyphony are fixed at build time (no button on
 * the module): see the variant folders and rings.mk.
 */

#ifndef RINGS_MODEL
#define RINGS_MODEL 0  // RESONATOR_MODEL_MODAL
#endif

#ifndef RINGS_POLYPHONY
#define RINGS_POLYPHONY 2
#endif

// Voltage that the CV inputs read as full scale. NOT measured: tune this by
// ear (see README.md) if octaves on CV 1 come out too wide or too narrow.
#ifndef RINGS_CV_FULL_SCALE_VOLTS
#define RINGS_CV_FULL_SCALE_VOLTS 5.0f
#endif

// Fixed excitation position (Rings' POSITION knob), 0..1.
#ifndef RINGS_POSITION
#define RINGS_POSITION 0.5f
#endif

Loewy hardware;
Part part;
StringSynthPart string_synth;
Strummer strummer;

// The reverb of the string-and-reverb model and of the string synth.
uint16_t reverb_buffer[32768];

const float kNoiseGateThreshold = 0.00003f;
// Input level above which the audio input counts as patched (about -50 dBFS).
const float kExciterPresentLevel = 1e-5f;
// How long the inputs keep counting as patched after their last activity.
const uint32_t kPatchedHoldBlocks = static_cast<uint32_t>(2.0f * kSampleRate / kMaxBlockSize);

float in_level = 0.0f;
uint32_t exciter_hold = 0;  // blocks left in which the audio input counts as patched
uint32_t trigger_hold = 0;  // same for the strum trigger on CV 2
uint32_t note_hold = 0;     // same for V/oct on CV 1
bool trigger_high = false;
int32_t transpose_q = 0;
int32_t chord_q = 0;

float in_buf[kMaxBlockSize];
float out_buf[kMaxBlockSize];
float aux_buf[kMaxBlockSize];

static void ReadControls(Patch* patch, PerformanceState* ps) {
  float freq_pot = hardware.GetPot1();
  float structure = hardware.GetPot2();
  float brightness = hardware.GetPot3();
  float damping = hardware.GetPot4();
  float cv1 = hardware.GetCV1();
  float cv2 = hardware.GetCV2();

  patch->structure = clamp(structure, 0.0f, 0.9995f);
  patch->brightness = brightness;
  patch->damping = damping;
  patch->position = RINGS_POSITION;

  // Strum trigger on CV 2, with hysteresis.
  bool rising = false;
  if (!trigger_high && cv2 > 0.5f) {
    trigger_high = true;
    rising = true;
  } else if (trigger_high && cv2 < 0.4f) {
    trigger_high = false;
  }
  if (trigger_high) trigger_hold = kPatchedHoldBlocks;
  else if (trigger_hold) --trigger_hold;

  // V/oct on CV 1. An unpatched input reads 0 V; anything above about a
  // tenth of a volt counts as a patched V/oct source.
  const float semitones_per_unit = RINGS_CV_FULL_SCALE_VOLTS * 12.0f;
  if (cv1 * RINGS_CV_FULL_SCALE_VOLTS > 0.1f) note_hold = kPatchedHoldBlocks;
  else if (note_hold) --note_hold;

  // Coarse frequency: 60 semitones, quantized with hysteresis like Rings.
  float transpose = 60.0f * freq_pot;
  float hysteresis = transpose - transpose_q > 0.0f ? -0.3f : +0.3f;
  transpose_q = static_cast<int32_t>(transpose + hysteresis + 0.5f);

  ps->internal_note = note_hold == 0;
  ps->internal_exciter = exciter_hold == 0;
  ps->internal_strum = trigger_hold == 0;
  ps->strum = rising;
  ps->fm = 0.0f;
  if (ps->internal_note) {
    ps->note = 0.0f;
    ps->tonic = 12.0f + transpose;  // free tuning without V/oct
  } else {
    ps->note = cv1 * semitones_per_unit;
    ps->tonic = 12.0f + transpose_q;
  }

  // Chord selection for the sympathetic-string models, from Structure.
  float chord = structure * static_cast<float>(kNumChords - 1);
  hysteresis = chord - chord_q > 0.0f ? -0.1f : +0.1f;
  chord_q = static_cast<int32_t>(chord + hysteresis + 0.5f);
  if (chord_q < 0) chord_q = 0;
  if (chord_q > kNumChords - 1) chord_q = kNumChords - 1;
  ps->chord = chord_q;
}

static void AudioCallback(AudioHandle::InputBuffer in,
                          AudioHandle::OutputBuffer out, size_t size) {
  hardware.ProcessControls();

  // libDaisy is set to Rings' block size; process in chunks to be safe.
  for (size_t offset = 0; offset < size; offset += kMaxBlockSize) {
    size_t n = size - offset < kMaxBlockSize ? size - offset : kMaxBlockSize;

    // Noise gate and exciter detection on the left input, as on Rings.
    float block_peak = 0.0f;
    for (size_t i = 0; i < n; ++i) {
      float s = in[0][offset + i];
      float error = s * s - in_level;
      in_level += error * (error > 0.0f ? 0.1f : 0.0001f);
      float gain = in_level <= kNoiseGateThreshold
                       ? (1.0f / kNoiseGateThreshold) * in_level
                       : 1.0f;
      in_buf[i] = gain * s;
      if (in_level > block_peak) block_peak = in_level;
    }
    if (block_peak > kExciterPresentLevel) exciter_hold = kPatchedHoldBlocks;
    else if (exciter_hold) --exciter_hold;

    Patch patch;
    PerformanceState ps;
    ReadControls(&patch, &ps);

#ifdef RINGS_STRING_SYNTH
    strummer.Process(NULL, n, &ps);
    string_synth.Process(ps, patch, in_buf, out_buf, aux_buf, n);
#else
    strummer.Process(in_buf, n, &ps);
    part.Process(ps, patch, in_buf, out_buf, aux_buf, n);
#endif

    for (size_t i = 0; i < n; ++i) {
      out[0][offset + i] = out_buf[i];
      out[1][offset + i] = aux_buf[i];
    }
  }
}

int main(void) {
  Loewy::Config config;
  config.audio_block_size = kMaxBlockSize;
  config.boost = true;
  hardware.Init(config);

  strummer.Init(0.01f, kSampleRate / kMaxBlockSize);
  part.Init(reverb_buffer);
  string_synth.Init(reverb_buffer);
  part.set_polyphony(RINGS_POLYPHONY);
  part.set_model(static_cast<ResonatorModel>(RINGS_MODEL));
  string_synth.set_polyphony(RINGS_POLYPHONY);
#ifndef RINGS_SYNTH_FX
#define RINGS_SYNTH_FX FX_ENSEMBLE
#endif
  string_synth.set_fx(RINGS_SYNTH_FX);

  hardware.StartAudio(AudioCallback);
  while (1) {
  }
}
