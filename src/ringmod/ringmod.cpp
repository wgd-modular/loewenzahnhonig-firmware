#include "../../lib/loewy.h"
#include "../../lib/utils.h"
#include "daisysp.h"

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

static Loewy hardware;
static Oscillator oscTri;
static Oscillator oscSin;
static Oscillator oscSq;

// The output signal is calculated as: modulator * carrier
float frequency, level, shape, blend;

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
  Loewy::Config config;
  config.audio_block_size = 4;
  hardware.Init(config);

  // Initialize oscillators
  float sample_rate = hardware.GetSampleRate();
  oscTri.Init(sample_rate);
  oscTri.SetWaveform(oscTri.WAVE_TRI);
  oscSin.Init(sample_rate);
  oscSin.SetWaveform(oscSin.WAVE_SIN);
  oscSq.Init(sample_rate);
  oscSq.SetWaveform(oscSq.WAVE_SQUARE);

  hardware.StartAudio(AudioCallback);

  // Main loop to set parameters based on pot and CV controls
  while (1) {
    hardware.ProcessControls();

    shape = hardware.GetPot1();
    blend = hardware.GetPot2();
    float freqOffset = hardware.GetPot3();
    float levelOffset = hardware.GetPot4();

    float carrierFrequencyCV = hardware.GetCV1();
    float carrierLevelCV = hardware.GetCV2();

    // Logarithmic frequency control
    float freqControl = clamp(freqOffset + carrierFrequencyCV, 0.0f, 1.0f);
    frequency = exp(CARRIER_FREQ_MIN +
                    (freqControl * (CARRIER_FREQ_MAX - CARRIER_FREQ_MIN)));

    level = clamp(levelOffset + carrierLevelCV, 0.0f, 1.0f);
  }
}
