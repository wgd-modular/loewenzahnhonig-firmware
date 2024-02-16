#include "../../lib/loewy.h"
#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
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

DaisySeed hw;
float sample_rate;

// The output levels are calculated as: offset + cvLevel * cv
float levelL, levelR;

// Function to set value n to within the lower and upper limits
float clamp(float n, float lower, float upper) {
  return n <= lower ? lower : n >= upper ? upper : n;
}

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
  // Init hardware
  hw.Configure();
  hw.Init();
  hw.SetAudioBlockSize(4);
  sample_rate = hw.AudioSampleRate();

  // Configure, init and start listening on the ADC pins for each pot and CV
  // input
  AdcChannelConfig adcConfig[6];
  adcConfig[0].InitSingle(hw.GetPin(15));
  adcConfig[1].InitSingle(hw.GetPin(16));
  adcConfig[2].InitSingle(hw.GetPin(17));
  adcConfig[3].InitSingle(hw.GetPin(18));
  adcConfig[4].InitSingle(hw.GetPin(19));
  adcConfig[5].InitSingle(hw.GetPin(20));
  hw.adc.Init(adcConfig, 6);
  hw.adc.Start();

  // Start audio callback thread
  hw.StartAudio(AudioCallback);

  // Main loop to set parameters based on pot and CV controls
  while (1) {
    // Read CV values from CV inputs (0.0 - 1.0)
    float cvL = 1 - hw.adc.GetFloat(Loewy::CV::CV_1);
    float cvR = 1 - hw.adc.GetFloat(Loewy::CV::CV_2);

    float offsetL = hw.adc.GetFloat(Loewy::Pot::POT_1);
    float offsetR = hw.adc.GetFloat(Loewy::Pot::POT_2);
    float cvLevelL = hw.adc.GetFloat(Loewy::Pot::POT_3);
    float cvLevelR = hw.adc.GetFloat(Loewy::Pot::POT_4);

    levelL = clamp(offsetL + cvL * cvLevelL, 0.0f, 1.0f);
    levelR = clamp(offsetR + cvR * cvLevelR, 0.0f, 1.0f);
  }
}
