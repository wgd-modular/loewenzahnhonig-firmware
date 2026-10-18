#include "../../lib/loewy.h"
#include "../../lib/utils.h"
#include "distortion_engine.h"

using namespace daisy;
using namespace loewy;

/*
 * Stereo distortion firmware for Loewenzahnhonig-Modul from WGD (Boss MT-2
 * inspired, 4x oversampled).
 *
 * Pot 1: Distortion
 * Pot 2: Low (shelf cut/boost, center = flat)
 * Pot 3: High (shelf cut/boost, center = flat)
 * Pot 4: Output Level
 * CV 1: Distortion modulation
 * CV 2: High modulation
 */

Loewy hardware;
DistortionEngine distortion;

const float cvAmount = 0.5f;

void ReadControls() {
  float distortionPot = hardware.GetPot1();
  float lowColor = hardware.GetPot2();
  float highColorPot = hardware.GetPot3();
  float level = hardware.GetPot4();

  float cv1 = hardware.GetCV1();
  float cv2 = hardware.GetCV2();

  float distortionAmount = clamp(distortionPot + cv1 * cvAmount, 0.0f, 1.0f);
  float highColor = clamp(highColorPot + cv2 * cvAmount, 0.0f, 1.0f);

  distortion.SetParameters(distortionAmount, lowColor, highColor, level);
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  hardware.ProcessControls();
  ReadControls();
  distortion.ProcessBlock(in[0], in[1], out[0], out[1], size);
}

int main(void) {
  Loewy::Config config;
  config.audio_block_size = 4;
  hardware.Init(config);

  distortion.Init(hardware.GetSampleRate());

  hardware.StartAudio(AudioCallback);

  while (1) {
  }
}
