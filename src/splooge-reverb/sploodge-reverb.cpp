#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

/*
Signal flow:

InL + InR --> PitchShifter --> StereoChorus --> Reverb --> High Pass Filter --> Wet/DryMix --> OuL + OutR

*/

DaisySeed hw;

// Create DSP objects
static PitchShifter pitch_shifter;
static Chorus chorus;
static ReverbSc DSY_SDRAM_BSS verb;
#define MAX_DELAY static_cast<size_t>(48000 * 2.5f)
static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS dell;
static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delr;
static Svf        filtL;
static Svf        filtR;
static Balance    balL;
static Balance    balR;

// Init vars
float dryAmplitude, wetAmplitude, sample_rate;
float delayTimeSecs, delayOutL, delayOutR, delayFeedback;
float dryL, dryR, verbL, verbR, chorusL, chorusR, pitchShifter1, pitchShifter2, pitchShifterLR, pot4Value;
float reverbLpFreqControl, reverbLpFreq;
float dryWetMixL, dryWetMixR;

// Function to set value n to within the lower and upper limits
float clamp(float n, float lower, float upper) {
	return n <= lower ? lower : n >= upper ? upper : n;
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
	for (size_t i = 0; i < size; i++) {

		// Bypass all processing - leave in for simple troubleshooting during dev
		// out[0][i] = in[0][i];
		// out[1][i] = in[1][i];

		// Read in Audio Samples from left and right channels
		dryL = in[0][i];
		dryR = in[1][i];

		// Bounce the inputs down to mono for DSP functions
		float monoInput = (dryL * 0.7) + (dryR * 0.7);

		// Pitch-shift the input
		pitch_shifter.SetTransposition(24.0f);
		pitchShifter1 = pitch_shifter.Process(monoInput);
		pitch_shifter.SetTransposition(12.0f);
		pitchShifter2 = pitch_shifter.Process(monoInput);
		pitchShifterLR = (pitchShifter1 * (0.5 * (pot4Value * pot4Value))) + (pitchShifter2 * (0.2 * (pot4Value * pot4Value)));
		
		//	Push the pitch-shifted input through the chorus effect
		chorus.Process(pitchShifterLR);
        chorusL = chorus.GetLeft();
        chorusR = chorus.GetRight();

		// Run through a stereo delay line
		delr.SetDelay(delayTimeSecs);
		dell.SetDelay(delayTimeSecs);
		delayOutL = dell.Read();
    	delayOutR = delr.Read();
		delayFeedback = (pot4Value * 0.91);
    	dell.Write((delayFeedback * delayOutL) + (dryL + (chorusL * 0.5)));
    	delayOutL = (delayFeedback * delayOutL) + ((1.0f - delayFeedback) * (dryL + (chorusL * 0.5)));
    	delr.Write((delayFeedback * delayOutR) + (dryL + (chorusR * 0.5)));
    	delayOutR = (delayFeedback * delayOutR) + ((1.0f - delayFeedback) * (dryR + (chorusR * 0.5)));

		// Add reverb
		verb.Process( (dryL * 0.5) + (delayOutL * pot4Value), (dryL * 0.5) + (delayOutR * pot4Value), &verbL, &verbR);

		// Filter the final output for the Tone control
		filtL.Process(verbL * wetAmplitude);
		filtR.Process(verbR * wetAmplitude);

		//out[0][i] = (dryL * dryAmplitude) + filtL.High();
		//out[1][i] = (dryR * dryAmplitude) + filtR.High();

		dryWetMixL = (dryL * dryAmplitude) + filtL.High();
		dryWetMixR = (dryR * dryAmplitude) + filtR.High();

		//	Apply make up gain to the wet signal if needed
		balL.Process(dryWetMixL, dryL);
		balL.Process(dryWetMixR, dryR);

		out[0][i] = dryWetMixL;
		out[1][i] = dryWetMixR;

	}
}

int main(void)
{
    // Init hardware
	hw.Configure();
    hw.Init();
    hw.SetAudioBlockSize(4);
	sample_rate = hw.AudioSampleRate();

	// Init DSP
	pitch_shifter.Init(sample_rate);
    chorus.Init(sample_rate);
	verb.Init(sample_rate);
    dell.Init();
    delr.Init();
	filtL.Init(sample_rate);
	filtR.Init(sample_rate);
	balL.Init(sample_rate);
	balR.Init(sample_rate);

	// Set delay time
	delayTimeSecs = sample_rate * 0.15f;
    dell.SetDelay(delayTimeSecs);
    delr.SetDelay(delayTimeSecs-0.01f);

    // Configure, init and start listening on the ADC pins for each pot and CV input
    AdcChannelConfig adcConfig[6];
    adcConfig[0].InitSingle(hw.GetPin(15));
	adcConfig[1].InitSingle(hw.GetPin(16));
    adcConfig[2].InitSingle(hw.GetPin(17));
	adcConfig[3].InitSingle(hw.GetPin(18));
	adcConfig[4].InitSingle(hw.GetPin(19));
	adcConfig[5].InitSingle(hw.GetPin(20));
    hw.adc.Init(adcConfig, 6);
    hw.adc.Start();

	//	Start audio callback thread
	hw.StartAudio(AudioCallback);

	// Main loop to set parameters based on pot and CV controls
	while(1) {

		// Read CV values from CV inputs (0.0 - 1.0)
		float cv1 = 1- hw.adc.GetFloat(4);
		float cv2 = 1- hw.adc.GetFloat(5);

		// Set chorus params
		chorus.SetLfoFreq(.33f, .2f);
		chorus.SetDelay(.75f, .9f);
		chorus.SetLfoDepth(1.f, 1.f);
		chorus.SetFeedback(0.75f, 0.75f);

		//	Sum the CV2 with Pot4 and limit range to 0.0 - 1.0
		pot4Value = clamp(cv2 + hw.adc.GetFloat(3), 0.0f, 1.0f);

		// Set the wet and dry mix
		wetAmplitude = hw.adc.GetFloat(1);
		dryAmplitude = 1 - wetAmplitude;
 
		// Update reverb params based on controls
		verb.SetFeedback(0.1 + (0.8 * hw.adc.GetFloat(0)));
		reverbLpFreqControl = clamp(cv1 + hw.adc.GetFloat(2), 0.f, 1.f);
		const float reverbLpFreqMin = log(100.f);
		const float reverbLpFreqMax = log(28000.f);
		reverbLpFreq = exp(reverbLpFreqMin + (reverbLpFreqControl * (reverbLpFreqMax - reverbLpFreqMin)));
  		verb.SetLpFreq(reverbLpFreq);

		// Set Filter params
		filtL.SetFreq(500.0 * hw.adc.GetFloat(2));
    	filtL.SetRes(0.1);
    	filtL.SetDrive(0.6);
		filtR.SetFreq(500.0 * hw.adc.GetFloat(2));
    	filtR.SetRes(0.1);
    	filtR.SetDrive(0.6);

	}
}
