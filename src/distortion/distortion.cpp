#include "../../lib/loewy.h"
#include "daisy_seed.h"
#include "distortion_engine.h"

using namespace daisy;
using namespace loewy;

/*
 * Stereo distortion firmware for Loewenzahnhonig-Modul from WGD (Boss MT-2 inspired, 4x oversampled).
 *
 * Pot 1: Distortion
 * Pot 2: Low (shelf cut/boost, center = flat)
 * Pot 3: High (shelf cut/boost, center = flat)
 * Pot 4: Output Level
 * CV 1: Distortion modulation
 * CV 2: High modulation
 */

DaisySeed hw;
DistortionEngine distortion;

const float cvAmount = 0.5f;

float clamp(float n, float lower, float upper) {
  return n <= lower ? lower : n >= upper ? upper : n;
}

void ReadControls() {
  float distortionPot = hw.adc.GetFloat(Loewy::Pot::POT_1);
  float lowColor = hw.adc.GetFloat(Loewy::Pot::POT_2);
  float highColorPot = hw.adc.GetFloat(Loewy::Pot::POT_3);
  float level = hw.adc.GetFloat(Loewy::Pot::POT_4);

  float cv1 = 1.0f - hw.adc.GetFloat(Loewy::CV::CV_1);
  float cv2 = 1.0f - hw.adc.GetFloat(Loewy::CV::CV_2);

  float distortionAmount =
      clamp(distortionPot + cv1 * cvAmount, 0.0f, 1.0f);
  float highColor = clamp(highColorPot + cv2 * cvAmount, 0.0f, 1.0f);

  distortion.SetParameters(distortionAmount, lowColor, highColor, level);
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  distortion.ProcessBlock(in[0], in[1], out[0], out[1], size);
}

int main(void) {
  hw.Configure();
  hw.Init();
  hw.SetAudioBlockSize(4);

  float sampleRate = hw.AudioSampleRate();
  distortion.Init(sampleRate);

  AdcChannelConfig adcConfig[6];
  adcConfig[0].InitSingle(hw.GetPin(15));
  adcConfig[1].InitSingle(hw.GetPin(16));
  adcConfig[2].InitSingle(hw.GetPin(17));
  adcConfig[3].InitSingle(hw.GetPin(18));
  adcConfig[4].InitSingle(hw.GetPin(19));
  adcConfig[5].InitSingle(hw.GetPin(20));
  hw.adc.Init(adcConfig, 6);
  hw.adc.Start();

  ReadControls();
  hw.StartAudio(AudioCallback);

  while (1) {
    ReadControls();
    System::Delay(1);
  }
}
