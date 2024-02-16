#include "DaisyDuino.h"

DaisyHardware hw;

size_t num_channels;
float sample_rate;

ReverbSc DSY_SDRAM_BSS verb;
PitchShifter ps;
PitchShifter ps2;

static float dryLevel, wetLevel, cv1, cv2, ps_level, send;

float CtrlVal(uint8_t pin) {
  analogReadResolution(16);
  return (analogRead(pin)) / 65535.f;
}

void callback(float **in, float **out, size_t size) {
  float dryL, dryR, verbL, verbR, shiftedL, shiftedR, shiftedL2, shiftedR2;

  for (size_t i = 0; i < size; i++) {
    dryL = in[0][i];
    dryR = in[1][i];
   
    shiftedL = ps.Process(dryL);
    shiftedR = ps.Process(dryR);
    shiftedL2 = ps2.Process(dryL);
    shiftedR2 = ps2.Process(dryR);

    verb.Process(dryL + shiftedL * ps_level + shiftedL2 * ps_level * 0.5f, dryR + shiftedR * ps_level + shiftedR2 * ps_level * 0.5f, &verbL, &verbR);

    out[0][i] = (dryL * dryLevel) + verbL * wetLevel;
    out[1][i] = (dryR * dryLevel) + verbR * wetLevel;
  }
} 

void setup() {
  hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
  num_channels = hw.num_channels;
  sample_rate = DAISY.get_samplerate();

  verb.Init(sample_rate);
  verb.SetFeedback(0.95f);
  verb.SetLpFreq(18000.0f);

  ps.Init(sample_rate);
  ps.SetTransposition(12.0f);

  ps2.Init(sample_rate);
  ps2.SetTransposition(24.0f);

  wetLevel = 0.1f;
  DAISY.begin(callback);
}

void loop() {
  cv1 = 1 - CtrlVal(A4);
  cv2 = 1 - CtrlVal(A5);
  dryLevel = CtrlVal(A0);
  wetLevel = CtrlVal(A1);
  ps_level = constrain(cv2 + CtrlVal(A3), 0.f, 1.f);
  verb.SetFeedback(0.8f + constrain(cv1 + CtrlVal(A2), 0.f, 1.f) * .199f);
}
