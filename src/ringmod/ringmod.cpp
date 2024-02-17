#include "../../lib/loewy.h"
#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;
using namespace loewy;

/*
 * Author: Ben van der Burgh
 *
 * This firmware lets the module act as a ring modulator.
 *
 * In Left: Modulator
 * In Right: Carrier (internal oscillator if disconnected)
 *
 * Pot 1: Carrier shape
 * Pot 2: Blend
 * Pot 3: Carrier frequency offset
 * Pot 4: Carrier level offset
 *
 * CV 1: Carrier frequency CV
 * CV 2: Carrier level CV
 *
 * Out Left: ring modulated signal
 * Out Right: carrier
 */

static const float CARRIER_FREQ_MIN = log(1.f);
static const float CARRIER_FREQ_MAX = log(16000.f);

static DaisySeed hw;
static Oscillator oscTri;
static Oscillator oscSin;
static Oscillator oscSq;
float sample_rate;

// The output signal is calculated as: modulator * carrier
float frequency, level, shape, blend;

// Function to set value n to within the lower and upper limits
float clamp(float n, float lower, float upper) {
  return n <= lower ? lower : n >= upper ? upper : n;
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  for (size_t i = 0; i < size; i++) {
    float modulator = in[0][i];

    oscTri.SetFreq(frequency);
    oscTri.SetAmp(level);
    oscSin.SetFreq(frequency);
    oscSin.SetAmp(level);
    oscSq.SetFreq(frequency);
    oscSq.SetAmp(level);

    float sin_wave = oscSin.Process();
    float tri_wave = oscTri.Process();
    float square_wave = oscSq.Process();

    float carrier = (1.0f - shape) * sin_wave + shape * tri_wave;
    carrier = (1.0f - shape) * carrier + shape * square_wave;

    // Get the current wet (ring-modulated) signal
    float modulated = modulator * carrier;

    // Mix the dry and wet signals based on the blend parameter
    float output = (1.0f - blend) * modulator + blend * modulated;

    // Attenuate input according to the set offset and attenuated CV inputs.
    out[0][i] = output;
    out[1][i] = carrier;
  }
}

int main(void) {
  // Init hardware
  hw.Configure();
  hw.Init();
  hw.SetAudioBlockSize(4);
  sample_rate = hw.AudioSampleRate();

  // Initialize oscillators
  oscTri.Init(sample_rate);
  oscTri.SetWaveform(oscTri.WAVE_TRI);
  oscSin.Init(sample_rate);
  oscSin.SetWaveform(oscSin.WAVE_SIN);
  oscSq.Init(sample_rate);
  oscSq.SetWaveform(oscSq.WAVE_SQUARE);

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
    float carrierFrequencyCV = 1 - hw.adc.GetFloat(Loewy::CV::CV_1);
    float carrierLevelCV = 1 - hw.adc.GetFloat(Loewy::CV::CV_2);

    shape = hw.adc.GetFloat(Loewy::Pot::POT_1);
    blend = hw.adc.GetFloat(Loewy::Pot::POT_2);

    float freqOffset = hw.adc.GetFloat(Loewy::Pot::POT_3);
    float levelOffset = hw.adc.GetFloat(Loewy::Pot::POT_4);

    // Logarithmic frequency control
    float freqControl = clamp(freqOffset + carrierFrequencyCV, 0, 1);
    frequency = exp(CARRIER_FREQ_MIN +
                    (freqControl * (CARRIER_FREQ_MAX - CARRIER_FREQ_MIN)));

    level = clamp(levelOffset + carrierLevelCV, 0, 1);
  }
}
