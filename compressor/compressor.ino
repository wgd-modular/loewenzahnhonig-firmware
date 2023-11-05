#include "DaisyDuino.h"

DaisyHardware hw;

size_t num_channels;
float sample_rate;

Compressor DSY_SDRAM_BSS comp; // Use the DaisyDuino Compressor

static float dryLevel, wetLevel, threshold, ratio;

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

    out[0][i] = (dryL * dryLevel) + compL * wetLevel;
    out[1][i] = (dryR * dryLevel) + compR * wetLevel;
  }
}

void setup() {
  hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
  num_channels = hw.num_channels;
  sample_rate = DAISY.get_samplerate();

  comp.Init(sample_rate); // Initialize the compressor
  comp.SetThreshold(-30.0f); // Set a reasonable threshold in dB (adjust as needed)
  comp.SetRatio(4.0f); // Set compression ratio (e.g., 4:1)
  
  wetLevel = 0.1f;
  DAISY.begin(callback);
}

void loop() {
  threshold = -20.0f + CtrlVal(A2) * 30.0f; // Adjust threshold from -20 to 10 dB
  ratio = 1.0f + CtrlVal(A3) * 19.0f; // Adjust ratio from 1.0 to 20.0
  dryLevel = CtrlVal(A0); // Control dry level
  wetLevel = CtrlVal(A1); // Control wet level

  comp.SetThreshold(threshold); // Update the threshold
  comp.SetRatio(ratio); // Update the ratio
}
