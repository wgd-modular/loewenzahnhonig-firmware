#ifndef DISTORTION_ENGINE_H
#define DISTORTION_ENGINE_H

#include <stddef.h>

class DistortionEngine {
 public:
  void Init(float sampleRate);
  void SetParameters(float distortion, float lowColor, float highColor,
                     float level);
  void ProcessBlock(const float* inputLeft, const float* inputRight,
                    float* outputLeft, float* outputRight, size_t size);

 private:
  static const int kNumChannels = 2;
  static const int kOversampleFactor = 4;
  static const int kHalfbandPaths = 2;
  static const int kHalfbandSections = 2;

  struct Biquad {
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
    float z1[kNumChannels];
    float z2[kNumChannels];
  };

  struct Halfband {
    float inputState[kNumChannels][kHalfbandPaths][kHalfbandSections];
    float outputState[kNumChannels][kHalfbandPaths][kHalfbandSections];
  };

  float Clamp(float value, float lower, float upper);
  void UpdateSmoothingCoefficient(size_t blockSize);
  void SmoothParameters();
  void UpdateDerivedParameters();

  void ResetBiquad(Biquad* filter);
  void ConfigurePeaking(Biquad* filter, float frequency, float q,
                        float gainDb);
  void ConfigureLowShelf(Biquad* filter, float frequency, float q,
                         float gainDb);
  void ConfigureHighShelf(Biquad* filter, float frequency, float q,
                          float gainDb);
  float ProcessBiquad(float sample, int channel, Biquad* filter);
  float MapShelfGain(float value, float cutDb, float boostDb);

  void ResetHalfband(Halfband* filter);
  float ProcessHalfbandAllpass(float sample, float coefficient,
                               float* inputState, float* outputState);
  float ProcessHalfbandPath(float sample, int channel, int path,
                            Halfband* filter);
  void Upsample(float sample, int channel, Halfband* filter, float* outputs);
  float Downsample(const float* samples, int channel, Halfband* filter);

  float ApplyOnePoleHighpass(float sample, int channel, float coefficient,
                             float* inputState, float* outputState);
  float UpdateNoiseGate(float sample, int channel);
  float ApplyDcBlock(float sample, int channel);
  float ApplyTightHighpass(float sample, int channel);
  float ApplyPreEmphasis(float sample, int channel);
  float ApplyFirstClip(float sample);
  float ApplySecondClip(float sample);
  float ApplyClipStages(float sample, int channel);
  float ApplyMidScoop(float sample, int channel);
  float ApplyFizzLowpass(float sample, int channel);
  float ApplyLowShelf(float sample, int channel);
  float ApplyHighShelf(float sample, int channel);
  float OutputClamp(float sample);
  float ProcessSample(float sample, int channel);

  float sampleRate_;
  size_t smoothingBlockSize_;
  float smoothingCoefficient_;

  float distortionTarget_;
  float distortionCurrent_;
  float lowTarget_;
  float lowCurrent_;
  float highTarget_;
  float highCurrent_;
  float levelTarget_;
  float levelCurrent_;

  float distortionGain_;
  float distortionGainValue_;
  float lowValue_;
  float highValue_;
  float levelGain_;

  float dcBlockerCoefficient_;
  float tightHighpassCoefficient_;
  float fizzLowpassCoefficient_;
  float gateEnvelopeAttackCoefficient_;
  float gateEnvelopeReleaseCoefficient_;
  float gateOpenCoefficient_;
  float gateCloseCoefficient_;
  int gateHoldSamples_;
  float gateEnvelope_[kNumChannels];
  float gateGain_[kNumChannels];
  int gateHoldCounter_[kNumChannels];
  bool gateOpen_[kNumChannels];
  float dcInputState_[kNumChannels];
  float dcOutputState_[kNumChannels];
  float tightInputState_[kNumChannels];
  float tightOutputState_[kNumChannels];
  float fizzState_[kNumChannels];

  Biquad preMidBoostFilter_;
  Biquad preHighShelfFilter_;
  Biquad midScoopFilter_;
  Biquad lowShelfFilter_;
  Biquad highShelfFilter_;

  Halfband upsampleStage1_;
  Halfband upsampleStage2_;
  Halfband downsampleStage1_;
  Halfband downsampleStage2_;
};

#endif
