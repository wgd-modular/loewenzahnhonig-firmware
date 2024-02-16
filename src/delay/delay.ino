#include "DaisyDuino.h"

DaisyHardware hw;

size_t num_channels;
float sample_rate;

DelayLine<float, 24000> delayL, delayR;

static float dryLevel, wetLevel, cv1, cv2, delayTime, feedback, send;

float CtrlVal(uint8_t pin) {
  analogReadResolution(16);
  return (analogRead(pin)) / 65535.f;
}

void callback(float **in, float **out, size_t size) {
  float dryL, dryR, wetL, wetR;

  for (size_t i = 0; i < size; i++) {
    dryL = in[0][i];
    dryR  = in[1][i];
   
    wetL = delayL.Read();
    wetR = delayR.Read();

    delayL.Write((wetL * feedback) + dryL);
    delayR.Write((wetR * feedback) + dryR);

    out[0][i] = (dryL * dryLevel) + wetL * wetLevel;
    out[1][i] = (dryR * dryLevel) + wetR * wetLevel;
  }
} 

void setup() {
  hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
  num_channels = hw.num_channels;
  sample_rate = DAISY.get_samplerate();

  delayL.Init();
  delayR.Init();

  delayL.SetDelay(12000.0f);
  delayR.SetDelay(12000.0f);

  wetLevel = 0.1f;
  DAISY.begin(callback);
}

void loop() {
  cv1 = 1 - CtrlVal(A4);
  cv2 = 1 - CtrlVal(A5);
  dryLevel = CtrlVal(A0);
  wetLevel = CtrlVal(A1);
  feedback = constrain(cv1 + CtrlVal(A2), 0.f, 1.f);
  delayL.SetDelay(constrain(cv2 + CtrlVal(A3), 0.f, 1.f) * 24000);
  delayR.SetDelay(constrain(cv2 + CtrlVal(A3), 0.f, 1.f) * 24000);
}
