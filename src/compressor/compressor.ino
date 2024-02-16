#include "DaisyDuino.h"

DaisyHardware hw;

size_t num_channels;
float sample_rate;

Compressor DSY_SDRAM_BSS comp; // Use the DaisyDuino Compressor

static float attack, rel, threshold, ratio;

float CtrlVal(uint8_t pin) {
  analogReadResolution(16);
  return (analogRead(pin)) / 65535.f;
}

void callback(float **in, float **out, size_t size) {
  float dryL, dryR, compL, compR;

  for (size_t i = 0; i < size; i++) {
    dryL = in[0][i];
    dryR = in[1][i];
    
    compL = comp.Process(dryL); // Apply compression to the left channel
    compR = comp.Process(dryR); // Apply compression to the right channel

    out[0][i] = compL;
    out[1][i] = compR;
  }
}

void setup() {
  hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
  num_channels = hw.num_channels;
  sample_rate = DAISY.get_samplerate();

  comp.Init(sample_rate); // Initialize the compressor
  comp.SetThreshold(-40.0f); // Set a reasonable threshold in dB (adjust as needed)
  comp.SetRatio(4.0f); // Set compression ratio (e.g., 4:1)
  comp.AutoMakeup(false);
  
  DAISY.begin(callback);
}

void loop() {
  threshold = -35.0f + CtrlVal(A2) * 35.0f; // Adjust threshold from -35 to 0 dB
  ratio = 1.0f + CtrlVal(A3) * 39.9f; // Adjust ratio from 1.0 to 40.0
  attack = CtrlVal(A0) * 9.9f; // Control attack
  rel = CtrlVal(A1)* 9.9f; // Control release

  comp.SetThreshold(threshold); // Update the threshold
  comp.SetRatio(ratio); // Update the ratio
  comp.SetAttack(attack);
  comp.SetRelease(rel);
}
