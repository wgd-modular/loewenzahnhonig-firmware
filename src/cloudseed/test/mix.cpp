#include <cmath>
#include <cstdio>

#include "hid/ctrl.h"
#include "mix.h"

using cloudseed_firmware::CrossfadeCos;
using cloudseed_firmware::CrossfadeSin;
using cloudseed_firmware::MixPosition;

int main() {
  int failures = 0;
  auto check = [&](bool condition, const char* description) {
    if (!condition) {
      std::fprintf(stderr, "FAIL: %s\n", description);
      ++failures;
    }
  };

  // Exercise the actual dependency with the firmware's callback rate and
  // pot slew. Repeated endpoints cover settling from both directions.
  uint16_t adc = 0;
  daisy::AnalogControl pot;
  pot.Init(&adc, 1000.f, false, false, 0.02f);
  const uint16_t endpoints[] = {0, 65535, 0, 65535};
  for (uint16_t endpoint : endpoints) {
    adc = endpoint;
    for (int i = 0; i < 1000; ++i) pot.Process();
    const float mix = MixPosition(pot.Value());
    const bool wet = endpoint != 0;
    check(CrossfadeCos(mix) == (wet ? 0.f : 1.f), "exact dry endpoint gain");
    check(CrossfadeSin(mix) == (wet ? 1.f : 0.f), "exact wet endpoint gain");
    std::printf("adc=%u smoothed=%.9g mix=%g dry=%g wet=%g\n", adc,
                pot.Value(), mix, CrossfadeCos(mix), CrossfadeSin(mix));
  }

  // Check the transfer over the full ADC range, including the end margins.
  // This detects discontinuous gain choices and changes to the mix law.
  float previous_mix = -1.f, previous_dry = 1.f, previous_wet = 0.f;
  for (int code = 0; code <= 65535; ++code) {
    const float mix = MixPosition(static_cast<float>(code) / 65536.f);
    const float dry = CrossfadeCos(mix), wet = CrossfadeSin(mix);
    check(mix >= 0.f && mix <= 1.f && mix >= previous_mix,
          "bounded monotonic mix");
    check(dry <= previous_dry && wet >= previous_wet, "monotonic gains");
    check(std::abs(dry * dry + wet * wet - 1.f) <= 2e-7f, "equal power");
    if (code) {
      check(previous_dry - dry < 2.5e-5f && wet - previous_wet < 2.5e-5f,
            "continuous gains across endpoint margins");
    }
    previous_mix = mix;
    previous_dry = dry;
    previous_wet = wet;
  }
  check(MixPosition(0.5f) == 0.5f, "unchanged centre position");
  check(std::abs(CrossfadeCos(0.5f) - std::sqrt(0.5f)) < 1e-7f &&
            std::abs(CrossfadeSin(0.5f) - std::sqrt(0.5f)) < 1e-7f,
        "centre gains are -3 dB");
  std::printf("Mix regression: %d failures\n", failures);
  return failures ? 1 : 0;
}
