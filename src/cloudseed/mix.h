#pragma once

#include "cloudseed/fdlibm_trig.h"

namespace cloudseed_firmware {

// libDaisy's AnalogControl divides a 16-bit ADC value by 65536, so its
// maximum is one code below 1. Its float slew filter can settle a little
// below that. Reserve two ADC codes at each end, and stretch the interior
// continuously, so the mix reaches fully dry/wet without changing centre.
inline float MixPosition(float pot) {
  constexpr float kEndpointMargin = 2.f / 65536.f;
  if (pot <= kEndpointMargin) return 0.f;
  if (pot >= 1.f - kEndpointMargin) return 1.f;
  return (pot - kEndpointMargin) / (1.f - 2.f * kEndpointMargin);
}

// Keep the angle and pi/2 in double. Use the library's trig so libm's
// sinf/cosf stay out of the image; return the endpoints explicitly so
// cos(pi/2)'s finite-precision residual cannot leak dry audio at fully wet.
constexpr double kHalfPi = 1.5707963267948966;

inline float CrossfadeCos(float mix) {
  if (mix <= 0.f) return 1.f;
  if (mix >= 1.f) return 0.f;
  return static_cast<float>(
      cloudseed::trig::Cos(static_cast<double>(mix) * kHalfPi));
}

inline float CrossfadeSin(float mix) {
  if (mix <= 0.f) return 0.f;
  if (mix >= 1.f) return 1.f;
  return static_cast<float>(
      cloudseed::trig::Sin(static_cast<double>(mix) * kHalfPi));
}

}  // namespace cloudseed_firmware
