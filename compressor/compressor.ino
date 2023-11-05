#include "DaisyDuino.h"

DaisyHardware hw;

size_t num_channels;
float sample_rate;

// Compressor parameters
float threshold = 0.5f;     // Threshold for compression (adjust as needed)
float ratio = 4.0f;         // Compression ratio (4:1 in this example)
float attackTime = 10.0f;   // Attack time in milliseconds
float releaseTime = 100.0f; // Release time in milliseconds

// Compressor state variables
float gain = 1.0f;          // Current gain
float envelope = 0.0f;      // Envelope follower value
float alphaAttack;          // Attack coefficient
float alphaRelease;         // Release coefficient

void callback(float **in, float **out, size_t size) {
  for (size_t i = 0; i < size; i++) {
    // Calculate the envelope follower value (rms)
    float input = 0.5f * (in[0][i] + in[1][i]); // Average of left and right channels
    envelope = (1.0f - alphaRelease) * fabs(input) + alphaRelease * envelope;

    // Calculate the gain reduction based on the envelope and threshold
    float reduction = 1.0f;
    if (envelope > threshold) {
      reduction = 1.0f - ((envelope - threshold) / (ratio * (envelope - threshold) + threshold));
    }

    // Apply the gain reduction
    gain = 1.0f / reduction;

    // Apply gain to the output
    out[0][i] = in[0][i] * gain;
    out[1][i] = in[1][i] * gain;
  }
}

float CtrlVal(uint8_t pin) {
  analogReadResolution(16);
  return (analogRead(pin)) / 65535.f;
}

void setup() {
  hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
  num_channels = hw.num_channels;
  sample_rate = DAISY.get_samplerate();

  alphaAttack = exp(-1.0f / (attackTime * 0.001f * sample_rate));
  alphaRelease = exp(-1.0f / (releaseTime * 0.001f * sample_rate));
  DAISY.begin(callback);
}

void loop() {
  // Read analog inputs for parameter adjustment
  threshold = 0.2f + CtrlVal(A0) * 0.8f;         // Adjust threshold from 0.2 to 1.0
  ratio = 2.0f + CtrlVal(A1) * 6.0f;             // Adjust ratio from 2.0 to 8.0
  attackTime = 5.0f + CtrlVal(A2) * 100.0f;      // Adjust attack time from 5ms to 105ms
  releaseTime = 50.0f + CtrlVal(A3) * 200.0f;    // Adjust release time from 50ms to 250ms

  // Recalculate coefficients based on updated parameters
  alphaAttack = exp(-1.0f / (attackTime * 0.001f * sample_rate));
  alphaRelease = exp(-1.0f / (releaseTime * 0.001f * sample_rate));
}
