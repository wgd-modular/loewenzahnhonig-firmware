#include "daisy_seed.h"
#include "daisysp.h"
#include <chrono>
#include <random>
#include <math.h>

using namespace daisy;
using namespace daisysp;
using namespace std::chrono;

DaisySeed hw;

#define CV_CHANGE_TOLERANCE .01f

#define MAX_DELAY static_cast<size_t>(48000 * 16.0f)
static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS dell;
static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delr;
static Chorus  chorus;
static Tremolo bcL;
static Tremolo bcR;
static Oscillator osc;

// Init vars
float sample_rate, dryL, dryR, delayOutL, delayOutR, delayTimeSecsL, delayTimeSecsR, delayFeedback;
float currentDelayL, currentDelayR, cv1, cv2Temp;
float cv2 = 0.0;
float pot1Value, pot2Value, pot3Value, pot4Value, dryWetMixL, dryWetMixR, dryAmplitude, wetAmplitude;
float chorusL, chorusR, oscValue;
bool newClkVal = false;
bool oldClkVal = false;
float divisor = 1;
float delayTimeSecs = 0.4f; 
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
bool smoodgeActive = true;
float gainLevel = 0.5;
int rndTremFreq = 0.54;
bool cv2ControlsFeedback = true;

//	Function to generate a random int between high and low
int genRandInt( int high, int low ) {
  	std::srand( ( unsigned int )std::time( nullptr ) );
	return low + std::rand() % ( high - low );
}

// Function to set value n to within the lower and upper limits
float clamp(float n, float lower, float upper) {
	return n <= lower ? lower : n >= upper ? upper : n;
}

// Function returns true if value >= threshold
bool isInputHigh(float threshold, float value) {
   if (value >= threshold) {return true;} 
   else {return false;}
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
	for (size_t i = 0; i < size; i++) {
	
		dryL = in[0][i];
		dryR = in[1][i];

		// Add Chorus to Dry signal
		chorus.Process((dryL + dryR) * gainLevel);
		chorusL = chorus.GetLeft();
		chorusR = chorus.GetRight();

		// Set the delay time (fonepole/Filter one-pole smooths out the changes)
		fonepole(currentDelayL, delayTimeSecsL, .0001f);
		
		// Stereo effect: Reduce the delaytime of the Right channel as Pot4 increases
		fonepole(currentDelayR, (delayTimeSecsR * (1-(pot4Value/14))), .0001f);

		// Delayline processing with feedback
		delr.SetDelay(currentDelayL); 
		dell.SetDelay(currentDelayR); 
		delayOutL = dell.Read();
    	delayOutR = delr.Read();
    	dell.Write((delayFeedback * delayOutL) + (dryL) + (chorusL * pot4Value));
    	delr.Write((delayFeedback * delayOutR) + (dryR) + (chorusR * pot4Value));

		//	Add Tremolo to delayline output
		delayOutL = bcL.Process(delayOutL);
		delayOutR = bcR.Process(delayOutR);

		// Create finel dry/wet mix and send to the output with soft limitting
		dryWetMixL = (dryL * dryAmplitude) + (delayOutL * wetAmplitude);
		dryWetMixR = (dryR * dryAmplitude) + (delayOutR * wetAmplitude);
		out[0][i] = SoftLimit(dryWetMixL);
		out[1][i] = SoftLimit(dryWetMixR);

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
	
	bcL.Init(sample_rate);
	bcR.Init(sample_rate);
	bcL.SetWaveform(Oscillator::WAVE_SIN);
	bcR.SetWaveform(Oscillator::WAVE_SIN);
	
	osc.Init(sample_rate);
	osc.SetWaveform(Oscillator::WAVE_SIN);
	osc.SetAmp(0.0f);
	osc.SetFreq(0.05f);

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

		// Set some pot and CV values
		cv1 = 1- hw.adc.GetFloat(4);

		// Set cv2 value only if it has changed by > CV_CHANGE_TOLERANCE
		cv2Temp = 1- hw.adc.GetFloat(5);
		if (fabsf((cv2Temp - cv2)) > CV_CHANGE_TOLERANCE) { cv2 = cv2Temp; }

		pot1Value = hw.adc.GetFloat(0);
		pot2Value = hw.adc.GetFloat(1);
		pot3Value = hw.adc.GetFloat(2);

		if (cv2ControlsFeedback) {
			// Option: Use Pot1 and cv2 for feedback
			delayFeedback = maxFeedback * clamp(cv2 + pot1Value, 0.0f, 1.0f);
			pot4Value = hw.adc.GetFloat(3);
		} else {
			// Option: Use pot1 for feedback and cv2 + pot4 for smoodge
			delayFeedback = maxFeedback * pot1Value;
			pot4Value = clamp(cv2 + hw.adc.GetFloat(3), 0.0f, 1.0f);

		}

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
					// Add the time diff in Ms to an array/circular buffer (this is used to smooth out the bumps a little)
					durationArr[idx] = durationMs;

					// If we have >= 3 clock timings in our buffer, calculate the average time between the last three
					//   then update the delay time
					if (numClksRx >= 3) {
						for(int i = 0; i < 3; ++i) {sum += durationArr[i];}
						delayTimeSecs = float((sum / 3) / 1000.0f);
						sum = 0.0;
					}

					// Generate a new Tremelo frequency every 4th clock
					if (numClksRx % 4 == 0) {
						rndTremFreq = genRandInt(1,10) / 10.0f;
						bcL.SetFreq(rndTremFreq - 0.1f);
						bcR.SetFreq(rndTremFreq);
					}
				}

				previousTimestamp = System::GetNow();
				
			} else {
				// falling edge of the incoming gate/trigger
				hw.SetLed(false);
				// Switch CV2 mode if the gate was high for >= 10 seconds
				//if (durationMs >= 10000) { cv2ControlsFeedback = !cv2ControlsFeedback; }
			}

		}

		// Set the oldClkVal to the current one for comparison on the next loop iteration
   		oldClkVal = newClkVal;
		
		//	Set the target delay time based on the divisor
		delayTimeSecsL = (delayTimeSecs * 48000) / divisor;
		delayTimeSecsR = (delayTimeSecs * 48000) / divisor;

		// Add wow/flutter effect using the SINE wave oscillator
		// potVal*PotVal provides a more exponential increase in value as the pot value increases
		oscValue = osc.Process();
		osc.SetAmp(9.0f * pot4Value);
		delayTimeSecsL = delayTimeSecsL + ((20* oscValue) * (pot4Value*pot4Value));
		delayTimeSecsR = delayTimeSecsR + ((20* oscValue) * (pot4Value*pot4Value));

		//	Increase gain with pot4 (0.5 is unity)
		gainLevel = 0.5f + (pot4Value * 0.4f);

		// Set Chorus Params
		chorus.SetLfoFreq(pot4Value * 1.f);
		chorus.SetLfoDepth(0.2);
		chorus.SetFeedback((pot4Value * pot4Value) * 0.5);

		// Tremelo params
		bcL.SetDepth(pot4Value * 0.61f);
		bcR.SetDepth(pot4Value * 0.6f);

		// Set the wet and dry mix
		wetAmplitude = pot2Value;
		dryAmplitude = 1 - wetAmplitude;
	}
}
