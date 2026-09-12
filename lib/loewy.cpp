#include "loewy.h"

// The Seed's system settings (the 480 MHz clock, the cache policies, the
// SDRAM's refresh and timings) come from the cloudseed-daisy library, which
// keeps them for any Daisy Seed firmware; see its seed_system.h.
#include "cloudseed-daisy/src/cloudseed_daisy/seed_system.h"

namespace loewy {

namespace {

constexpr size_t kNumAdcChannels = Loewy::kNumPots + Loewy::kNumCVs;

// Daisy Seed pins of the ADC channels, in channel order: P1-P4 on A0-A3
// (pins 15-18) followed by CV1-CV2 on A4-A5 (pins 19-20).
constexpr uint8_t kAdcPins[kNumAdcChannels] = {15, 16, 17, 18, 19, 20};

}  // namespace

void Loewy::Init() { Init(Config()); }

void Loewy::Init(const Config& config) {
  config_ = config;

  hw_.Init(config_.boost && cloudseed_daisy::SupportsBoost());
  if (config_.sdram_write_allocate)
    cloudseed_daisy::ConfigureSdramWriteAllocate();
  if (!config_.sram_write_allocate)
    cloudseed_daisy::ConfigureSramNoWriteAllocate();
  // libDaisy's SDRAM driver refreshes the Seed's part too slowly for its
  // datasheet and misses two of its minimum timings; program the part's own
  // values now that hw_.Init() has initialized the SDRAM.
  ConfigureSdramRefresh();
  SetSdramTiming(config_.sdram_datasheet_timing);
  hw_.SetAudioBlockSize(config_.audio_block_size);

  InitAdc();
  InitControls();
}

void Loewy::SetSdramTiming(bool datasheet) {
  cloudseed_daisy::SetSdramTiming(datasheet);
}

void Loewy::ConfigureSdramRefresh() {
  cloudseed_daisy::ConfigureSdramRefresh();
}

void Loewy::InitAdc() {
  daisy::AdcChannelConfig adc_config[kNumAdcChannels];
  for (size_t i = 0; i < kNumAdcChannels; i++) {
    adc_config[i].InitSingle(daisy::DaisySeed::GetPin(kAdcPins[i]));
  }
  hw_.adc.Init(adc_config, kNumAdcChannels);
  hw_.adc.Start();
}

void Loewy::InitControls() {
  // The controls are processed once per audio callback, so the smoothing
  // filters run at the callback rate, not at the audio sample rate.
  const float rate = hw_.AudioCallbackRate();

  for (size_t i = 0; i < kNumPots; i++) {
    pots_[i].Init(hw_.adc.GetPtr(i), rate, /*flip=*/false, /*invert=*/false,
                  config_.pot_slew_seconds);
  }
  for (size_t i = 0; i < kNumCVs; i++) {
    // `flip` maps the reading to 1 - x; `invert` would negate it instead.
    cvs_[i].Init(hw_.adc.GetPtr(kNumPots + i), rate, /*flip=*/config_.flip_cv,
                 /*invert=*/false, config_.cv_slew_seconds);
  }
}

void Loewy::ProcessControls() {
  for (auto& pot : pots_) pot.Process();
  for (auto& cv : cvs_) cv.Process();
}

}  // namespace loewy
