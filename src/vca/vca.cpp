#include "../../lib/loewy.h"
#include "../../lib/utils.h"
#include "daisysp.h"

using namespace daisysp;
using namespace loewy;

/*
 * Author: Ben van der Burgh
 *
 * This firmware lets the module act as a simple dual VCA.
 *
 * Pot 1: left offset
 * Pot 2: right offset
 * Pot 3: left CV attenuation
 * Pot 4: right CV attenuation
 */

Loewy hardware;

// The output levels are calculated as: offset + cvLevel * cv
float levelL, levelR;


void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  for (size_t i = 0; i < size; i++) {
    // Read in audio Samples from left and right channels
    float inL = in[0][i];
    float inR = in[1][i];

    // Attenuate input according to the set offset and attenuated CV inputs.
    out[0][i] = inL * levelL;
    out[1][i] = inR * levelR;
  }
}

int main(void) {
  Loewy::Config config;
  config.audio_block_size = 4;
  hardware.Init(config);

  hardware.StartAudio(AudioCallback);

  // Main loop to set parameters based on pot and CV controls
  while (1) {
    hardware.ProcessControls();

    // Read CV values from CV inputs (0.0 - 1.0)
    float cvL = hardware.GetCV1();
    float cvR = hardware.GetCV2();

    float offsetL = hardware.GetPot1();
    float offsetR = hardware.GetPot2();
    float cvLevelL = hardware.GetPot3();
    float cvLevelR = hardware.GetPot4();

    levelL = clamp(offsetL + cvL * cvLevelL, 0.0f, 1.0f);
    levelR = clamp(offsetR + cvR * cvLevelR, 0.0f, 1.0f);
  }
}
