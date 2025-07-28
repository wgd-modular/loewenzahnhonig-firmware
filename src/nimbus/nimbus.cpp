#include "../../lib/loewy.h"
#include "granular_processor.h"

using namespace loewy;

GranularProcessorClouds processor;
Loewy hardware;

// Pre-allocate memory blocks for granular processor
uint8_t block_mem[118784];
uint8_t block_ccm[65536 - 128];

Parameters* parameters;

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    hardware.ProcessControls();

    // Manual Controls
    float pitch_knob = hardware.GetPot1();  // P1: Pitch (±2 octaves)
    parameters->pitch = powf(9.798f * (pitch_knob - .5f), 2.f);
    parameters->pitch *= pitch_knob < .5f ? -1.f : 1.f;
    
    parameters->size = hardware.GetPot2();      // P2: Size
    parameters->reverb = hardware.GetPot3();    // P3: Reverb
    parameters->dry_wet = hardware.GetPot4();   // P4: Dry/Wet
    
    // CV Inputs
    parameters->texture = hardware.GetCV1();   // CV1: Texture
    parameters->density = hardware.GetCV2();   // CV2: Density
    
    // Fixed position for granular processing
    parameters->position = 0.5f;

    // Fixed parameters
    parameters->stereo_spread = 0.25f;
    parameters->feedback = 0.15f;
    parameters->freeze = false;
    parameters->trigger = false;
    parameters->gate = false;

    FloatFrame input[size];
    FloatFrame output[size];

    // Audio I/O
    for(size_t i = 0; i < size; i++)
    {
        input[i].l = in[0][i];
        input[i].r = in[1][i];
        output[i].l = output[i].r = 0.f;
    }

    processor.Process(input, output, size);

    for(size_t i = 0; i < size; i++)
    {
        out[0][i] = output[i].l;
        out[1][i] = output[i].r;
    }
}

int main(void)
{
    Loewy::Config config;
    config.audio_block_size = 32;  // Required for Clouds algorithm
    hardware.Init(config);

    InitResources(hardware.GetSampleRate());

    processor.Init(hardware.GetSampleRate(),
                   block_mem,
                   sizeof(block_mem),
                   block_ccm,
                   sizeof(block_ccm));

    parameters = processor.mutable_parameters();

    processor.set_playback_mode(PLAYBACK_MODE_GRANULAR);
    processor.set_quality(0);

    hardware.StartAudio(AudioCallback);

    while(1)
    {
        processor.Prepare();
    }
}