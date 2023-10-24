#include "daisy_seed.h"
#include "daisysp.h"
#include <chrono>

using namespace daisy;
using namespace daisysp;
using namespace std::chrono;

DaisySeed hw;

#define MAX_DELAY static_cast<size_t>(48000 * 16.0f)
static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS dell;
static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delr;
static Chorus  chorus;

// Init vars
float sample_rate, dryL, dryR, delayOutL, delayOutR, delayTimeSecsL, delayTimeSecsR, delayFeedback;
float currentDelayL, currentDelayR;
float pot3Value, pot4Value, dryWetMixL, dryWetMixR, dryAmplitude, wetAmplitude;
float chorusL, chorusR;
bool newClkVal = false;
bool oldClkVal = false;
float divisor = 1;
float delayTimeSecs = 1.0f; 
float previousDivisor = divisor;
bool divisorChanged = false;
float maxFeedback = 1.0f;
int idx = 0;

int numClksRx = 0;
uint32_t previousTimestamp = 0;
uint32_t currentTimestamp = 0;
uint32_t durationMs = 0;
uint32_t durationArr[3] = {0,0,0};
float sum=0.0;

// Function to set value n to within the lower and upper limits
float clamp(float n, float lower, float upper) {
	return n <= lower ? lower : n >= upper ? upper : n;
}

bool isInputHigh(float threshold, float value) {
   if (value >= threshold) {return true;} 
   else {return false;}
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
	for (size_t i = 0; i < size; i++) {
	
		//out[0][i] = in[0][i];
		//out[1][i] = in[1][i];

		dryL = in[0][i];
		dryR = in[1][i];

		// Add Chorus to Dry signal
		chorus.Process((dryL + dryR) * 0.7);
		chorusL = chorus.GetLeft();
		chorusR = chorus.GetRight();

		// Set the delay time (fonepole smooths out the changes)
		fonepole(currentDelayL, delayTimeSecsL, .0001f);
		// Stereo effect: Reduce the delaytime of the Right channel as Pot4 increases
		fonepole(currentDelayR, (delayTimeSecsR * (1-pot4Value/7)), .0001f);
		delr.SetDelay(currentDelayL);
		dell.SetDelay(currentDelayR); 
		delayOutL = dell.Read();
    	delayOutR = delr.Read();
    	dell.Write((delayFeedback * delayOutL) + (dryL) + (chorusL * pot4Value));
    	delr.Write((delayFeedback * delayOutR) + (dryR) + (chorusR * pot4Value));

		// Add feedback
		dryWetMixL = (dryL * dryAmplitude) + (delayOutL * wetAmplitude);
		dryWetMixR = (dryR * dryAmplitude) + (delayOutR * wetAmplitude);

		// Send to the output
		out[0][i] = dryWetMixL;
		out[1][i] = dryWetMixR;

	}
}

int main(void)
{

    // Init hardware
	hw.Configure();
    hw.Init();
	hw.SetAudioBlockSize(4); // number of samples handled per callback
	hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
	sample_rate = hw.AudioSampleRate();

	// Init DSP
	dell.Init();
    delr.Init();
	chorus.Init(sample_rate);

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

	hw.StartAudio(AudioCallback);

	while(1) {

		float cv1 = 1- hw.adc.GetFloat(4);
		float cv2 = 1- hw.adc.GetFloat(5);

		//	Set the pot CV values
		//	CV2 is summed with with Pot3+4
		//	Range is limited to 0.0 - 1.0
		pot3Value = hw.adc.GetFloat(2);
		pot4Value = clamp(cv2 + hw.adc.GetFloat(3), 0.0f, 1.0f);

		delayFeedback = maxFeedback * hw.adc.GetFloat(0);

		//	Set the delay time based on Pot3
		if (pot3Value == 0.0f)      {divisor = 8;} 
		else if (pot3Value <= 0.1f) {divisor = 4;}
		else if (pot3Value <= 0.2f) {divisor = 3;}    // half note triplets
		else if (pot3Value <= 0.3f) {divisor = 2;} 
		else if (pot3Value <= 0.4f) {divisor = 1.5;}  // whole note triplets
		else if (pot3Value <= 0.5f) {divisor = 1;} 
		else if (pot3Value <= 0.6f) {divisor = 0.5;}  // x2
		else if (pot3Value <= 0.7f) {divisor = 0.33;} // x3
		else if (pot3Value <= 0.8f) {divisor = 0.25;} // x4
		else if (pot3Value <= 0.9f) {divisor = 0.2;}  // x5
		else {divisor = 0.125;} // x8		
		
		// Check for clock input on CV1 - use onboard LED as a visual indicator
		newClkVal = isInputHigh(0.5f, cv1);

		if (newClkVal != oldClkVal) {
			if (newClkVal == true) {
				// rising edge of the incoming gate/trigger
				numClksRx++;
				idx = numClksRx % 3;
				hw.SetLed(true);

				if (previousTimestamp != 0) {
					// We have a previous timestamp, get the time between that and now
					currentTimestamp = System::GetNow();
					durationMs = (currentTimestamp - previousTimestamp);
					// Add the time diff in Ms to an array (the array is used to smooth out the bumps a little)
					durationArr[idx] = durationMs;
					// If we have >= 3 clocks, calculate the average time between the last three
					//   then update the delay time
					if (numClksRx >= 3) {
						for(int i = 0; i < 3; ++i) {sum += durationArr[i];}
						delayTimeSecs = float((sum / 3) / 1000.0f);
						sum = 0.0;
					}
				}

				previousTimestamp = System::GetNow();
			} else {
				// falling edge of the incoming gate/trigger
				hw.SetLed(false);
			}

		}
		// Set the oldClkVal to the current one for comparison on the next loop iteration
   		oldClkVal = newClkVal;
		
		delayTimeSecsL = (delayTimeSecs * 48000) / divisor;
		delayTimeSecsR = (delayTimeSecs * 48000) / divisor;

		// Set Chorus Params
		chorus.SetLfoFreq(pot4Value * 1.f);
		chorus.SetLfoDepth(0.2);
		chorus.SetFeedback(pot4Value * 0.5);

		// Set the wet and dry mix
		wetAmplitude = hw.adc.GetFloat(1);
		dryAmplitude = 1 - wetAmplitude;
	}
}
