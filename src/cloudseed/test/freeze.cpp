// The wet level step each program takes when the freeze gate closes. The
// firmware does not correct it (README.md, "CV 2: Freeze"), so this pins it:
// the step follows from where a program taps its lines, how many it runs and
// how much late diffusion it has, and a change to any of those moves it by
// six decibels and more. Runs the library's reverb the way the firmware
// drives it (DryOut 0, CutoffEnabled 1, the firmware's own line counts - it
// shares the firmware's program table, so a line count cannot change under
// it), fills it with a signal, closes the freeze gate and compares the wet
// level before and after. Deterministic: the noise source is seeded.
#include <cmath>
#include <cstdio>
#include <vector>

#include "cloudseed/fast_sin.h"
#include "cloudseed/presets.h"
#include "cloudseed/reverb_controller.h"
#include "programs.h"

using cloudseed::Parameter;
using cloudseed::ReverbController;
using cloudseed_firmware::kNumPrograms;
using cloudseed_firmware::kPrograms;
using cloudseed_firmware::Program;

namespace {

constexpr int kRate = 48000;
constexpr int kBlock = cloudseed::kMaxBlockSize;
// A held sound before the gate, long enough for the reverb to fill at the
// decay settings below.
constexpr double kSettle = 6.0;
constexpr double kWindow = 0.75;
// The step each program is expected to take, in dB, positive for quieter
// when held: the mean over the sources and settings below, measured on this
// code. The two programs that tap their lines before the delay with little
// or no late diffusion lose several decibels; the rest barely move, and
// several come back louder, since unity feedback without the damping filters
// keeps energy the running loop was shedding. The band is wide enough for
// the spread across the conditions and narrow enough to catch a change in
// the mechanism.
const double kExpectedStep[] = {1.5,  -1.6, 0.0,  0.8,  -0.9,
                                -1.7, 7.5,  -1.5, -1.8, 5.9};
constexpr double kTolerance = 3.0;

ReverbController reverb;
std::vector<float> pool;

// A small deterministic noise source (xorshift), and a three-note saw chord.
uint32_t rng_state = 0x1234567u;
float lp_l, lp_r, phase[3];

float Noise() {
  rng_state ^= rng_state << 13;
  rng_state ^= rng_state >> 17;
  rng_state ^= rng_state << 5;
  return (static_cast<float>(rng_state >> 8) * (1.f / 8388608.f) - 1.f) * 0.2f;
}

struct Source {
  const char* name;
  bool chord;
  float cutoff;  // one-pole low-pass on the source, 0 for none
};
const Source kSources[] = {
    {"saw chord, low-passed at 2 kHz", true, 2000.f},
    {"noise, low-passed at 1 kHz", false, 1000.f},
};
constexpr int kNumSources = sizeof(kSources) / sizeof(kSources[0]);
const float kChordHz[3] = {110.f, 164.81f, 220.f};

void ResetSource() {
  rng_state = 0x1234567u;
  lp_l = lp_r = 0.f;
  phase[0] = 0.1f;
  phase[1] = 0.4f;
  phase[2] = 0.7f;
}

void Fill(const Source& source, float* left, float* right) {
  for (int i = 0; i < kBlock; i++) {
    float l, r;
    if (source.chord) {
      float saw = 0.f;
      for (int k = 0; k < 3; k++) {
        phase[k] += kChordHz[k] / kRate;
        if (phase[k] >= 1.f) phase[k] -= 1.f;
        saw += 2.f * phase[k] - 1.f;
      }
      l = r = saw * 0.115f;
    } else {
      l = Noise();
      r = Noise();
    }
    if (source.cutoff > 0.f) {
      const float k =
          1.f - std::exp(-2.f * 3.14159265f * source.cutoff / kRate);
      lp_l += k * (l - lp_l);
      lp_r += k * (r - lp_r);
      const float gain = std::sqrt(2.f / k - 1.f);  // back to the same RMS
      l = lp_l * gain;
      r = lp_r * gain;
    }
    left[i] = l;
    right[i] = r;
  }
}

double Render(const Source& source, double seconds) {
  float in_l[kBlock], in_r[kBlock], out_l[kBlock], out_r[kBlock];
  const long blocks = static_cast<long>(seconds * kRate / kBlock);
  double sum = 0.0;
  long count = 0;
  for (long b = 0; b < blocks; b++) {
    Fill(source, in_l, in_r);
    reverb.Process(in_l, in_r, out_l, out_r, kBlock);
    for (int i = 0; i < kBlock; i++) {
      sum += static_cast<double>(out_l[i]) * out_l[i] +
             static_cast<double>(out_r[i]) * out_r[i];
      count += 2;
    }
  }
  return count ? std::sqrt(sum / count) : 0.0;
}

// The wet level lost when the gate closes, in positive dB.
double FreezeLoss(const Program& program, const Source& source, double decay,
                  double tone) {
  ResetSource();
  reverb.LoadPreset(program.preset->values, false);
  reverb.SetParameter(Parameter::DryOut, 0.0);         // the firmware's hook
  reverb.SetParameter(Parameter::CutoffEnabled, 1.0);  // the firmware's hook
  if (reverb.line_count() > program.lines)
    reverb.SetParameter(
        Parameter::LineCount,
        double(program.lines - 1) / (cloudseed::kPluginLineCount - 1));
  reverb.PlaceBuffers();
  reverb.ClearBuffers();
  reverb.SetFrozen(false);
  reverb.SetParameter(Parameter::LineDecay, decay);
  reverb.SetParameter(Parameter::PostCutoffFrequency, tone);

  Render(source, kSettle);
  const double running = Render(source, kWindow);
  reverb.SetFrozen(true);
  const double frozen = Render(source, kWindow);
  return -20.0 * std::log10((frozen + 1e-30) / (running + 1e-30));
}

struct Setting {
  double decay, tone;
};
const Setting kSettings[] = {{0.20, 0.25}, {0.45, 0.45}};
constexpr int kNumSettings = sizeof(kSettings) / sizeof(kSettings[0]);

}  // namespace

int main() {
  int failures = 0;
  auto check = [&](bool condition, const char* description) {
    if (!condition) {
      std::fprintf(stderr, "FAIL: %s\n", description);
      ++failures;
    }
  };

  cloudseed::FastSin::Init();
  pool.resize(ReverbController::RequiredPoolFloats(kRate));
  cloudseed::MemoryPool memory;
  memory.Init(pool.data(), pool.size());
  check(reverb.Init(kRate, memory), "the reverb initializes");

  std::printf("%-26s %8s %8s %8s   %s\n", "program", "step", "expected",
              "delta", "source and setting");
  for (int p = 0; p < kNumPrograms; p++) {
    const double expected = kExpectedStep[p];
    for (int s = 0; s < kNumSources; s++) {
      for (int t = 0; t < kNumSettings; t++) {
        const double loss = FreezeLoss(kPrograms[p], kSources[s],
                                       kSettings[t].decay, kSettings[t].tone);
        const double delta = loss - expected;
        std::printf("%-26s %7.1f %8.1f %8.1f   %s, decay %.2f tone %.2f\n",
                    kPrograms[p].preset->name, loss, expected, delta,
                    kSources[s].name, kSettings[t].decay, kSettings[t].tone);
        if (std::fabs(delta) > kTolerance) {
          std::fflush(stdout);
          std::fprintf(stderr,
                       "FAIL: %s steps %.1f dB, expected about %.1f dB "
                       "(%s, decay %.2f tone %.2f)\n",
                       kPrograms[p].preset->name, loss, expected,
                       kSources[s].name, kSettings[t].decay, kSettings[t].tone);
          ++failures;
        }
      }
    }
  }

  if (failures) {
    std::fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  std::printf("freeze level steps: OK\n");
  return 0;
}
