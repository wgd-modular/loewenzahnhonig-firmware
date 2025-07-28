#include "rings/dsp/fx/reverb.h"
#include "../../lib/loewy.h"
#include "../../lib/utils.h"

using namespace rings;
using namespace loewy;

Reverb reverb;
uint16_t reverb_buffer[65536];
Loewy hardware;

static void AudioCallback(AudioHandle::InputBuffer in,
                          AudioHandle::OutputBuffer out,
                          size_t size) {
    hardware.ProcessControls();

    float wet = hardware.GetPot1();
    float lp = hardware.GetPot2();
    float diffusion = hardware.GetPot3();
    float time = hardware.GetPot4();
    
    float diffusion_cv = hardware.GetCV1();
    float time_cv = hardware.GetCV2();

    float final_diffusion = clamp(diffusion + diffusion_cv * 0.5f, 0.0f, 1.0f);
    float final_time = clamp(time + time_cv * 0.5f, 0.0f, 1.0f);

    reverb.set_amount(wet);
    reverb.set_lp(lp);
    reverb.set_diffusion(final_diffusion);
    reverb.set_time(final_time);
    reverb.set_input_gain(0.5f);

    float ins_left[size];
    float ins_right[size];

    for (size_t i = 0; i < size; i++) {
        ins_left[i] = in[0][i];
        ins_right[i] = in[1][i];
    }

    reverb.Process(ins_left, ins_right, size);

    for (size_t i = 0; i < size; i++) {
        out[0][i] = ins_left[i];
        out[1][i] = ins_right[i];
    }
}

int main(void) {
    hardware.Init();
    reverb.Init(reverb_buffer);
    hardware.StartAudio(AudioCallback);
    while(1) {}
}
