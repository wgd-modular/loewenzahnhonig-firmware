#pragma once

#include "cloudseed/presets.h"

namespace cloudseed_firmware {

// One program of the firmware: the preset it is built on and the late delay
// lines per channel it runs with.
//
// This is deliberately not the library's cloudseed_daisy::Program, which
// lives in engine.h and so needs the STM32 HAL: keeping the table in a
// header the host can compile lets test/freeze.sh measure exactly the
// programs the module runs. The audio callback reads this table; main()
// copies it into the array the engine wants.
struct Program {
  const cloudseed::presets::Preset* preset;
  int lines;
};

// The programs in the order of the zones of Pot 1: the plugin's nine factory
// programs as spaces, then washes, then echoes, and the successor's plate
// appended, so the nine keep the numbers the logs and the hardware baselines
// have used. Each with the late delay lines per channel it runs with: the
// programs' own counts (see the library's TECHNICAL.md, "Measured
// performance"). The build's CLOUDSEED_MAX_LINES caps them, and the engine's
// overload recovery still reduces a program that exceeds the budget.
//
// The line count also sets how much wet level a program loses when the
// freeze gate closes, which is why test/freeze.sh measures the step against
// this table: see README.md, "CV 2: Freeze".
constexpr Program kPrograms[] = {
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

}  // namespace cloudseed_firmware
