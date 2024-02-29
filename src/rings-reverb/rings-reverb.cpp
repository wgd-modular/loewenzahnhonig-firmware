#include "daisy_seed.h"
#include "rings/dsp/fx/reverb.h"

using namespace rings;
using namespace daisy;

Reverb reverb;
uint16_t reverb_buffer[65536];

DaisySeed hw;
float sample_rate;

// Function to set value n to within the lower and upper limits
float clamp(float n, float lower, float upper) {
    return n <= lower ? lower : n >= upper ? upper : n;
}

static void AudioCallback(AudioHandle::InputBuffer in,
                          AudioHandle::OutputBuffer out,
                          size_t size) {
    float ins_left[size];
    float ins_right[size];

    for (size_t i = 0; i < size; i++) {
        ins_left[i] = in[0][i];
        ins_right[i] = in[1][i];

        reverb.Process(&ins_left[i], &ins_right[i], 1);

        out[0][i] = ins_left[i];
        out[1][i] = ins_right[i];
    }
}

int main(void) {
    hw.Configure();
    hw.Init();
    sample_rate = hw.AudioSampleRate();
    //hw.SetAudioBlockSize(4);

    AdcChannelConfig adcConfig[6];
    adcConfig[0].InitSingle(hw.GetPin(15));
    adcConfig[1].InitSingle(hw.GetPin(16));
    adcConfig[2].InitSingle(hw.GetPin(17));
    adcConfig[3].InitSingle(hw.GetPin(18));
    adcConfig[4].InitSingle(hw.GetPin(19));
    adcConfig[5].InitSingle(hw.GetPin(20));
    hw.adc.Init(adcConfig, 6);
    hw.adc.Start();

    reverb.Init(reverb_buffer);

    hw.StartAudio(AudioCallback);

    while (1) {
        float diffusion_cv = 1 - hw.adc.GetFloat(4);
        float time_cv = 1 - hw.adc.GetFloat(5);

        float wet = hw.adc.GetFloat(0);
        float lp = hw.adc.GetFloat(1);
        float diffusion = hw.adc.GetFloat(2);
        float time = hw.adc.GetFloat(3);

        reverb.set_amount(wet);
        reverb.set_lp(lp);
        reverb.set_diffusion(clamp(diffusion + diffusion_cv, 0, 1));
        reverb.set_time(clamp(time + time_cv, 0, 1));
        reverb.set_input_gain(0.5);
    }
}
