#include <math.h>
#include <stdint.h>

#include <atomic>

#include "../../lib/loewy.h"
#include "../../lib/utils.h"
#include "cloudseed/fdlibm_trig.h"
#include "cloudseed/presets.h"
#include "cloudseed/utils.h"
#include "cloudseed_daisy/engine.h"
#include "daisysp.h"

using daisy::AudioHandle;
using daisy::System;
using namespace loewy;
using cloudseed::Parameter;
using cloudseed_daisy::Engine;

/*
 * Author: Ben van der Burgh
 *
 * Cloud Seed, Valdemar Erlingsson's algorithmic reverb, for the
 * Löwenzahnhonig. The reverb, its memory management and its recovery live
 * in the cloudseed-daisy library (lib/cloudseed-daisy); this file maps the
 * module's controls onto its engine and mixes the wet signal. See README.md.
 *
 * The reverb runs in stereo with one of ten programs as its base sound: the
 * plugin's nine factory programs and its successor's plate:
 *
 *   Pot 1: program (ten zones)
 *   Pot 2: dry/wet mix
 *   Pot 3: decay time of the late reverb (CV 1 is added)
 *   Pot 4: tone (low-pass filter in the feedback path of the delay lines)
 *   CV 2:  freeze gate
 *   LED:   lit while frozen, flashing when the CPU is overloaded
 */

// Board options (see the Makefile and README.md, "Performance"); the
// library's own options are in lib/cloudseed-daisy/cloudseed.mk.
#ifndef CLOUDSEED_BOOST
#define CLOUDSEED_BOOST 1  // 480 MHz on silicon that supports it
#endif
#ifndef CLOUDSEED_SDRAM_WRITE_ALLOCATE
#define CLOUDSEED_SDRAM_WRITE_ALLOCATE 0  // cache SDRAM writes (write-allocate)
#endif
#ifndef CLOUDSEED_SRAM_WRITE_ALLOCATE
#define CLOUDSEED_SRAM_WRITE_ALLOCATE 0  // internal SRAM: cache writes
#endif
#ifndef CLOUDSEED_SDRAM_FAST_TIMING
#define CLOUDSEED_SDRAM_FAST_TIMING 0  // datasheet SDRAM timings, tested at boot
#endif

namespace {

// The programs in the order of the zones of Pot 1: the plugin's nine factory
// programs as spaces, then washes, then echoes, and the successor's plate
// appended, so the nine keep the numbers the logs and the hardware baselines
// have used. Each with the late delay lines per channel it runs with: the
// programs' own counts (see the library's TECHNICAL.md, "Measured
// performance"). The build's CLOUDSEED_MAX_LINES caps them, and the
// engine's overload recovery still reduces a program that exceeds the
// budget.
const cloudseed_daisy::Program kPrograms[] = {
    {&cloudseed::presets::kSmallRoom, 3},
    {&cloudseed::presets::kMediumSpace, 3},
    {&cloudseed::presets::kNoiseInTheHallway, 8},
    {&cloudseed::presets::kHyperplane, 9},
    {&cloudseed::presets::kRubiKaFields, 4},
    {&cloudseed::presets::kThroughTheLookingGlass, 12},
    {&cloudseed::presets::kThe90sAreBack, 9},
    {&cloudseed::presets::kDullEchoes, 12},
    {&cloudseed::presets::kChorusDelay, 12},
    {&cloudseed::presets::kDarkPlate, 12},
};
constexpr int kNumPrograms = sizeof(kPrograms) / sizeof(kPrograms[0]);

// Pot 1 is split into kNumPrograms zones of equal width. The pot has to
// travel this fraction of a zone past a boundary before the program
// changes, so that noise on the reading cannot flip it back and forth
// (stmlib's HysteresisQuantizer uses the same quarter of a step).
constexpr float kProgramHysteresis = 0.25f;

// A pot has to move by this much (of its 0..1 range) before its parameter
// is updated, which keeps the pots' noise from recomputing the delay lines.
constexpr float kPotThreshold = 0.001f;

// Freeze gate thresholds on the 0..1 CV reading, with hysteresis.
constexpr float kFreezeOn = 0.55f;
constexpr float kFreezeOff = 0.45f;

// daisysp::SoftClip without its two float compares: the same soft limit on
// the input clamped to -3..3, where the limit is exactly -1 and 1.
static inline float SoftClip(float x) {
  const int32_t bits = cloudseed::FloatBits(x);
  constexpr int32_t kThreeBits = 0x40400000;
  if ((bits & 0x7fffffff) > kThreeBits) x = bits < 0 ? -3.f : 3.f;
  return daisysp::SoftLimit(x);
}

constexpr float kHalfPi = 1.5707963f;

// The equal-power crossfade gains of a mix position, through the library's
// sin() and cos() (cloudseed/fdlibm_trig.h): libm's sinf() and cosf() then
// stay out of the image.
inline float CrossfadeCos(float mix) {
  return static_cast<float>(
      cloudseed::trig::Cos(static_cast<double>(mix) * kHalfPi));
}
inline float CrossfadeSin(float mix) {
  return static_cast<float>(
      cloudseed::trig::Sin(static_cast<double>(mix) * kHalfPi));
}

Loewy hardware;
// The engine and the callback's buffers in the DTCM: neither cached nor
// subject to wait states (see engine.h).
CLOUDSEED_DAISY_DTCM Engine engine;
CLOUDSEED_DAISY_DTCM float wet_l[cloudseed::kMaxBlockSize];
CLOUDSEED_DAISY_DTCM float wet_r[cloudseed::kMaxBlockSize];

// Audio callback publishes the LED state.
std::atomic<unsigned int> led_status{0};
constexpr unsigned int kFrozenLed = 1;
constexpr unsigned int kOverloadLed = 2;

int quantized_program = 0;     // audio callback after startup
bool frozen = false;           // audio callback
float current_dry_gain = 1.f;  // audio callback after startup
float current_wet_gain = 0.f;

// SDRAM timing in use: 0 conservative, 1 faster datasheet timings, 2 faster
// timings failed the memory test and conservative timings were restored.
__attribute__((unused)) int sdram_timing_state = 0;

// After every program load: the dry signal is mixed in by this firmware,
// and the tone pot always drives the low-pass filter in the feedback path.
void OnProgramLoaded(cloudseed::ReverbController& reverb, void*) {
  reverb.SetParameter(Parameter::DryOut, 0.0);
  reverb.SetParameter(Parameter::CutoffEnabled, 1.0);
}

// Zone of Pot 1, with hysteresis around the zone boundaries.
int QuantizeProgram(float pot) {
  const float value = pot * kNumPrograms - 0.5f;
  const float sign = value > static_cast<float>(quantized_program) ? -1.f : 1.f;
  int zone = static_cast<int>(floorf(value + sign * kProgramHysteresis + 0.5f));
  if (zone < 0) zone = 0;
  if (zone >= kNumPrograms) zone = kNumPrograms - 1;
  quantized_program = zone;
  return zone;
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  engine.BeginBlock();
  hardware.ProcessControls();
  engine.RequestProgram(QuantizeProgram(hardware.GetPot1()));

  const float mix = hardware.GetPot2();
  const float decay = clamp(hardware.GetPot3() + hardware.GetCV1(), 0.f, 1.f);
  const float tone = hardware.GetPot4();
  engine.SetParameter(Parameter::LineDecay, decay, kPotThreshold);
  engine.SetParameter(Parameter::PostCutoffFrequency, tone, kPotThreshold);

  // CV 2 freezes the reverb while high.
  const float gate = hardware.GetCV2();
  if (gate > kFreezeOn) {
    frozen = true;
  } else if (gate < kFreezeOff) {
    frozen = false;
  }
  engine.SetFrozen(frozen);

#if CLOUDSEED_PROFILE
  // The controls as read, for the report: every pot, and both CV inputs as
  // the ADC sees them (raw) and as the firmware uses them (flipped and
  // smoothed), to tell an unpatched jack from a patched one; then the
  // decay and the mix the callback derived.
  const float controls[] = {
      hardware.GetPot1(), hardware.GetPot2(),
      hardware.GetPot3(), hardware.GetPot4(),
      hardware.GetCV1(),  hardware.GetCVRaw(Loewy::CV::CV_1),
      hardware.GetCV2(),  hardware.GetCVRaw(Loewy::CV::CV_2),
      decay,              mix};
  engine.SetProfileControls(controls, sizeof(controls) / sizeof(controls[0]));
#endif

  // Equal-power crossfade between the dry input and the (wet only) reverb,
  // its gains ramped across the block.
  const float dry_gain = CrossfadeCos(mix);
  const float wet_gain = CrossfadeSin(mix);
  const float dry_step = size ? (dry_gain - current_dry_gain) / size : 0.f;
  const float wet_step = size ? (wet_gain - current_wet_gain) / size : 0.f;

  const bool wet = engine.Process(in[0], in[1], wet_l, wet_r, size);
  CLOUDSEED_PROFILE_SECTION(cloudseed::kProfileMix);
  if (!wet) {
    // Loading or recovering: dry signal only.
    for (size_t i = 0; i < size; i++) {
      current_dry_gain += dry_step;
      out[0][i] = current_dry_gain * in[0][i];
      out[1][i] = current_dry_gain * in[1][i];
    }
  } else {
    // Long decays let the reverb build up well beyond full scale with a
    // sustained input, and libDaisy hard-clips the output; soft-clip the
    // wet signal instead. The dry signal passes untouched. No float compare
    // in the loop: the clip compares the value's bits.
    for (size_t i = 0; i < size; i++) {
      current_dry_gain += dry_step;
      current_wet_gain += wet_step;
      out[0][i] = current_dry_gain * in[0][i] + current_wet_gain * SoftClip(wet_l[i]);
      out[1][i] = current_dry_gain * in[1][i] + current_wet_gain * SoftClip(wet_r[i]);
    }
  }
  current_dry_gain = dry_gain;
  current_wet_gain = wet_gain;
  CLOUDSEED_PROFILE_SECTION(cloudseed::kProfileOther);
  engine.EndBlock();

  led_status.store((engine.frozen() ? kFrozenLed : 0u) |
                       (engine.overloaded() ? kOverloadLed : 0u),
                   std::memory_order_relaxed);
}

#if CLOUDSEED_PROFILE
void Print(const char* text) { hardware.GetHardware().Print("%s", text); }
#endif

}  // namespace

int main(void) {
#if CLOUDSEED_PROFILE
  Engine::FillStack();
#endif
  Loewy::Config config;
  config.audio_block_size = cloudseed::kMaxBlockSize;
  // The default 2 ms slew gives AnalogControl a coefficient of 1 at 1 kHz,
  // which is no smoothing. Use 20 ms for the pots; keep the gate/CV fast.
  config.pot_slew_seconds = 0.02f;
  config.boost = CLOUDSEED_BOOST != 0;
  config.sdram_write_allocate = CLOUDSEED_SDRAM_WRITE_ALLOCATE != 0;
  config.sram_write_allocate = CLOUDSEED_SRAM_WRITE_ALLOCATE != 0;
  config.sdram_datasheet_timing = CLOUDSEED_SDRAM_FAST_TIMING != 0;
  hardware.Init(config);
#if CLOUDSEED_PROFILE
  hardware.GetHardware().StartLog(false);
#endif
#if CLOUDSEED_SDRAM_FAST_TIMING
  // The SDRAM runs with the datasheet timings (Loewy::Init); test it before
  // the delay memory goes there, and fall back to conservative timings on
  // failure.
  sdram_timing_state = 1;
  if (!Engine::TestDelayMemory()) {
    hardware.SetSdramTiming(false);
    sdram_timing_state = 2;
  }
#endif

  Engine::Config engine_config;
  engine_config.programs = kPrograms;
  engine_config.program_count = kNumPrograms;
  engine_config.sample_rate = hardware.GetSampleRate();
  engine_config.block_size = hardware.GetBlockSize();
  engine_config.on_program_loaded = OnProgramLoaded;
  if (!engine.Init(engine_config)) {
    // The delay memory is too small for this sample rate: blink forever.
    while (1) {
      hardware.SetLed(true);
      System::Delay(100);
      hardware.SetLed(false);
      System::Delay(100);
    }
  }

  // Let the smoothed pot readings settle, then load the program Pot 1 points
  // at before the audio starts.
  for (int i = 0; i < 100; i++) {
    hardware.ProcessControls();
    System::Delay(1);
  }
  engine.Start(QuantizeProgram(hardware.GetPot1()));
  current_dry_gain = CrossfadeCos(hardware.GetPot2());
  current_wet_gain = CrossfadeSin(hardware.GetPot2());

#if CLOUDSEED_PROFILE
  engine.PrintBuild(Print);
  hardware.GetHardware().Print(
      "board boost=%d sdram_wa=%d sram_wa=%d sdram_timing=%s\r\n",
      CLOUDSEED_BOOST, CLOUDSEED_SDRAM_WRITE_ALLOCATE,
      CLOUDSEED_SRAM_WRITE_ALLOCATE,
      sdram_timing_state == 1   ? "datasheet"
      : sdram_timing_state == 2 ? "datasheet-FAILED"
                                : "conservative");
#endif
  hardware.StartAudio(AudioCallback);

  uint32_t last_led_update = System::GetNow();
  bool led_flash = false;

  while (1) {
    engine.Service();

    // The LED is lit while frozen and flashes while the CPU is overloaded.
    const uint32_t now = System::GetNow();
    if (now - last_led_update >= 100) {
      last_led_update = now;
      led_flash = !led_flash;
    }
    const unsigned int status = led_status.load(std::memory_order_relaxed);
    hardware.SetLed(status & kOverloadLed ? led_flash : (status & kFrozenLed));

#if CLOUDSEED_PROFILE
    // The workload of every program once it is loaded, and one report per
    // second with the load, its breakdown and the control values.
    engine.PrintProgramIfChanged(Print);
    cloudseed_daisy::ProfileReport report;
    if (engine.TakeReport(&report)) engine.PrintReport(report, Print);
#endif

    System::Delay(2);
  }
}
