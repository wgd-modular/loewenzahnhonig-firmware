#include "distortion_engine.h"

#include <float.h>
#include <math.h>

namespace {

const float kPi = 3.14159265358979323846f;
const float kDefaultSampleRate = 48000.0f;
const float kSmoothingTimeSeconds = 0.02f;
const float kParameterUpdateThreshold = 0.0005f;

const float kDcBlockerFrequency = 5.0f;
const float kTightHighpassFrequency = 75.0f;
const float kFizzLowpassFrequency = 7500.0f;

const float kMinimumDistortionGain = 3.0f;
const float kDistortionGainRange = 120.0f;
const float kSecondClipGain = 1.6f;

const float kPreMidBoostFrequency = 950.0f;
const float kPreMidBoostQ = 0.70f;
const float kPreMidBoostGainDb = 8.0f;
const float kPreHighShelfFrequency = 2200.0f;
const float kPreHighShelfQ = 0.707f;
const float kPreHighShelfGainDb = 4.0f;

const float kMidScoopFrequency = 700.0f;
const float kMidScoopQ = 0.80f;
const float kMidScoopGainDb = -8.0f;

const float kLowShelfFrequency = 100.0f;
const float kLowShelfQ = 0.707f;
const float kLowShelfCutDb = 12.0f;
const float kLowShelfBoostDb = 12.0f;
const float kHighShelfFrequency = 3200.0f;
const float kHighShelfQ = 0.707f;
const float kHighShelfCutDb = 12.0f;
const float kHighShelfBoostDb = 8.0f;

const float kOutputMakeupGain = 1.25f;

// Fixed noise gate: sidechain reads the DC-blocked input, the gain is
// applied to the output. Thresholds sit far below any real eurorack
// signal but above the amplified codec noise floor.
const float kGateOpenThreshold = 0.002f;   // ~ -54 dBFS
const float kGateCloseThreshold = 0.001f;  // ~ -60 dBFS
const float kGateEnvelopeAttackSeconds = 0.0005f;
const float kGateEnvelopeReleaseSeconds = 0.025f;
const float kGateOpenSeconds = 0.002f;
const float kGateCloseSeconds = 0.08f;
const float kGateHoldSeconds = 0.05f;

// Polyphase allpass halfband pair (hiir-style, ~70 dB stopband above
// 0.6 * Nyquist, flat passband). [path][section]
const float kHalfbandCoefficients[2][2] = {
    {0.07986642623635751f, 0.5453536510711322f},
    {0.28382934487410993f, 0.8344118914807379f},
};

}  // namespace

void DistortionEngine::Init(float sampleRate) {
  sampleRate_ = sampleRate > 1.0f ? sampleRate : kDefaultSampleRate;
  smoothingBlockSize_ = 0;
  smoothingCoefficient_ = 1.0f;

  distortionTarget_ = 0.0f;
  distortionCurrent_ = 0.0f;
  lowTarget_ = 0.5f;
  lowCurrent_ = 0.5f;
  highTarget_ = 0.5f;
  highCurrent_ = 0.5f;
  levelTarget_ = 0.0f;
  levelCurrent_ = 0.0f;

  distortionGain_ = kMinimumDistortionGain;
  distortionGainValue_ = -1.0f;
  lowValue_ = -1.0f;
  highValue_ = -1.0f;
  levelGain_ = 0.0f;

  dcBlockerCoefficient_ =
      expf(-2.0f * kPi * kDcBlockerFrequency / sampleRate_);
  tightHighpassCoefficient_ =
      expf(-2.0f * kPi * kTightHighpassFrequency / sampleRate_);
  fizzLowpassCoefficient_ =
      1.0f - expf(-2.0f * kPi * kFizzLowpassFrequency / sampleRate_);

  gateEnvelopeAttackCoefficient_ =
      1.0f - expf(-1.0f / (kGateEnvelopeAttackSeconds * sampleRate_));
  gateEnvelopeReleaseCoefficient_ =
      1.0f - expf(-1.0f / (kGateEnvelopeReleaseSeconds * sampleRate_));
  gateOpenCoefficient_ = 1.0f - expf(-1.0f / (kGateOpenSeconds * sampleRate_));
  gateCloseCoefficient_ =
      1.0f - expf(-1.0f / (kGateCloseSeconds * sampleRate_));
  gateHoldSamples_ = static_cast<int>(kGateHoldSeconds * sampleRate_);

  for (int channel = 0; channel < kNumChannels; channel++) {
    dcInputState_[channel] = 0.0f;
    dcOutputState_[channel] = 0.0f;
    tightInputState_[channel] = 0.0f;
    tightOutputState_[channel] = 0.0f;
    fizzState_[channel] = 0.0f;
    gateEnvelope_[channel] = 0.0f;
    gateGain_[channel] = 0.0f;
    gateHoldCounter_[channel] = 0;
    gateOpen_[channel] = false;
  }

  ResetBiquad(&preMidBoostFilter_);
  ResetBiquad(&preHighShelfFilter_);
  ResetBiquad(&midScoopFilter_);
  ResetBiquad(&lowShelfFilter_);
  ResetBiquad(&highShelfFilter_);

  ResetHalfband(&upsampleStage1_);
  ResetHalfband(&upsampleStage2_);
  ResetHalfband(&downsampleStage1_);
  ResetHalfband(&downsampleStage2_);

  ConfigurePeaking(&preMidBoostFilter_, kPreMidBoostFrequency, kPreMidBoostQ,
                   kPreMidBoostGainDb);
  ConfigureHighShelf(&preHighShelfFilter_, kPreHighShelfFrequency,
                     kPreHighShelfQ, kPreHighShelfGainDb);
  ConfigurePeaking(&midScoopFilter_, kMidScoopFrequency, kMidScoopQ,
                   kMidScoopGainDb);

  UpdateDerivedParameters();
}

void DistortionEngine::SetParameters(float distortion, float lowColor,
                                     float highColor, float level) {
  distortionTarget_ = Clamp(distortion, 0.0f, 1.0f);
  lowTarget_ = Clamp(lowColor, 0.0f, 1.0f);
  highTarget_ = Clamp(highColor, 0.0f, 1.0f);
  levelTarget_ = Clamp(level, 0.0f, 1.0f);
}

void DistortionEngine::ProcessBlock(const float* inputLeft,
                                    const float* inputRight, float* outputLeft,
                                    float* outputRight, size_t size) {
  UpdateSmoothingCoefficient(size);
  SmoothParameters();
  UpdateDerivedParameters();

  for (size_t i = 0; i < size; i++) {
    outputLeft[i] = ProcessSample(inputLeft[i], 0);
    outputRight[i] = ProcessSample(inputRight[i], 1);
  }
}

float DistortionEngine::Clamp(float value, float lower, float upper) {
  return value <= lower ? lower : value >= upper ? upper : value;
}

void DistortionEngine::UpdateSmoothingCoefficient(size_t blockSize) {
  if (blockSize == smoothingBlockSize_) {
    return;
  }

  smoothingBlockSize_ = blockSize;
  smoothingCoefficient_ =
      1.0f - expf(-static_cast<float>(blockSize) /
                  (kSmoothingTimeSeconds * sampleRate_));
}

void DistortionEngine::SmoothParameters() {
  distortionCurrent_ +=
      smoothingCoefficient_ * (distortionTarget_ - distortionCurrent_);
  lowCurrent_ += smoothingCoefficient_ * (lowTarget_ - lowCurrent_);
  highCurrent_ += smoothingCoefficient_ * (highTarget_ - highCurrent_);
  levelCurrent_ += smoothingCoefficient_ * (levelTarget_ - levelCurrent_);
}

void DistortionEngine::UpdateDerivedParameters() {
  if (fabsf(distortionCurrent_ - distortionGainValue_) >=
      kParameterUpdateThreshold) {
    distortionGainValue_ = distortionCurrent_;
    distortionGain_ = kMinimumDistortionGain *
                      powf(kDistortionGainRange, distortionCurrent_);
  }

  if (fabsf(lowCurrent_ - lowValue_) >= kParameterUpdateThreshold) {
    lowValue_ = lowCurrent_;
    float gainDb = MapShelfGain(lowCurrent_, kLowShelfCutDb, kLowShelfBoostDb);
    ConfigureLowShelf(&lowShelfFilter_, kLowShelfFrequency, kLowShelfQ,
                      gainDb);
  }

  if (fabsf(highCurrent_ - highValue_) >= kParameterUpdateThreshold) {
    highValue_ = highCurrent_;
    float gainDb =
        MapShelfGain(highCurrent_, kHighShelfCutDb, kHighShelfBoostDb);
    ConfigureHighShelf(&highShelfFilter_, kHighShelfFrequency, kHighShelfQ,
                       gainDb);
  }

  levelGain_ = levelCurrent_ * levelCurrent_ * kOutputMakeupGain;
}

void DistortionEngine::ResetBiquad(Biquad* filter) {
  filter->b0 = 1.0f;
  filter->b1 = 0.0f;
  filter->b2 = 0.0f;
  filter->a1 = 0.0f;
  filter->a2 = 0.0f;

  for (int channel = 0; channel < kNumChannels; channel++) {
    filter->z1[channel] = 0.0f;
    filter->z2[channel] = 0.0f;
  }
}

void DistortionEngine::ConfigurePeaking(Biquad* filter, float frequency,
                                        float q, float gainDb) {
  float safeFrequency = Clamp(frequency, 10.0f, sampleRate_ * 0.45f);
  float safeQ = q < 0.1f ? 0.1f : q;
  float amplitude = powf(10.0f, gainDb / 40.0f);
  float omega = 2.0f * kPi * safeFrequency / sampleRate_;
  float alpha = sinf(omega) / (2.0f * safeQ);
  float cosine = cosf(omega);

  float b0 = 1.0f + alpha * amplitude;
  float b1 = -2.0f * cosine;
  float b2 = 1.0f - alpha * amplitude;
  float a0 = 1.0f + alpha / amplitude;
  float a1 = -2.0f * cosine;
  float a2 = 1.0f - alpha / amplitude;

  filter->b0 = b0 / a0;
  filter->b1 = b1 / a0;
  filter->b2 = b2 / a0;
  filter->a1 = a1 / a0;
  filter->a2 = a2 / a0;
}

void DistortionEngine::ConfigureLowShelf(Biquad* filter, float frequency,
                                         float q, float gainDb) {
  float safeFrequency = Clamp(frequency, 10.0f, sampleRate_ * 0.45f);
  float safeQ = q < 0.1f ? 0.1f : q;
  float amplitude = powf(10.0f, gainDb / 40.0f);
  float omega = 2.0f * kPi * safeFrequency / sampleRate_;
  float alpha = sinf(omega) / (2.0f * safeQ);
  float cosine = cosf(omega);
  float amplitudeRoot = 2.0f * sqrtf(amplitude) * alpha;

  float b0 = amplitude *
             ((amplitude + 1.0f) - (amplitude - 1.0f) * cosine + amplitudeRoot);
  float b1 = 2.0f * amplitude *
             ((amplitude - 1.0f) - (amplitude + 1.0f) * cosine);
  float b2 = amplitude *
             ((amplitude + 1.0f) - (amplitude - 1.0f) * cosine - amplitudeRoot);
  float a0 = (amplitude + 1.0f) + (amplitude - 1.0f) * cosine + amplitudeRoot;
  float a1 = -2.0f * ((amplitude - 1.0f) + (amplitude + 1.0f) * cosine);
  float a2 = (amplitude + 1.0f) + (amplitude - 1.0f) * cosine - amplitudeRoot;

  filter->b0 = b0 / a0;
  filter->b1 = b1 / a0;
  filter->b2 = b2 / a0;
  filter->a1 = a1 / a0;
  filter->a2 = a2 / a0;
}

void DistortionEngine::ConfigureHighShelf(Biquad* filter, float frequency,
                                          float q, float gainDb) {
  float safeFrequency = Clamp(frequency, 10.0f, sampleRate_ * 0.45f);
  float safeQ = q < 0.1f ? 0.1f : q;
  float amplitude = powf(10.0f, gainDb / 40.0f);
  float omega = 2.0f * kPi * safeFrequency / sampleRate_;
  float alpha = sinf(omega) / (2.0f * safeQ);
  float cosine = cosf(omega);
  float amplitudeRoot = 2.0f * sqrtf(amplitude) * alpha;

  float b0 = amplitude *
             ((amplitude + 1.0f) + (amplitude - 1.0f) * cosine + amplitudeRoot);
  float b1 = -2.0f * amplitude *
             ((amplitude - 1.0f) + (amplitude + 1.0f) * cosine);
  float b2 = amplitude *
             ((amplitude + 1.0f) + (amplitude - 1.0f) * cosine - amplitudeRoot);
  float a0 = (amplitude + 1.0f) - (amplitude - 1.0f) * cosine + amplitudeRoot;
  float a1 = 2.0f * ((amplitude - 1.0f) - (amplitude + 1.0f) * cosine);
  float a2 = (amplitude + 1.0f) - (amplitude - 1.0f) * cosine - amplitudeRoot;

  filter->b0 = b0 / a0;
  filter->b1 = b1 / a0;
  filter->b2 = b2 / a0;
  filter->a1 = a1 / a0;
  filter->a2 = a2 / a0;
}

float DistortionEngine::ProcessBiquad(float sample, int channel,
                                      Biquad* filter) {
  float output = filter->b0 * sample + filter->z1[channel];
  filter->z1[channel] =
      filter->b1 * sample - filter->a1 * output + filter->z2[channel];
  filter->z2[channel] = filter->b2 * sample - filter->a2 * output;
  return output;
}

float DistortionEngine::MapShelfGain(float value, float cutDb, float boostDb) {
  if (value < 0.5f) {
    return -cutDb * (1.0f - value * 2.0f);
  }

  return boostDb * (value * 2.0f - 1.0f);
}

void DistortionEngine::ResetHalfband(Halfband* filter) {
  for (int channel = 0; channel < kNumChannels; channel++) {
    for (int path = 0; path < kHalfbandPaths; path++) {
      for (int section = 0; section < kHalfbandSections; section++) {
        filter->inputState[channel][path][section] = 0.0f;
        filter->outputState[channel][path][section] = 0.0f;
      }
    }
  }
}

float DistortionEngine::ProcessHalfbandAllpass(float sample, float coefficient,
                                               float* inputState,
                                               float* outputState) {
  float output = *inputState + coefficient * (sample - *outputState);
  *inputState = sample;
  *outputState = output;
  return output;
}

float DistortionEngine::ProcessHalfbandPath(float sample, int channel,
                                            int path, Halfband* filter) {
  float output = sample;
  for (int section = 0; section < kHalfbandSections; section++) {
    output = ProcessHalfbandAllpass(
        output, kHalfbandCoefficients[path][section],
        &filter->inputState[channel][path][section],
        &filter->outputState[channel][path][section]);
  }
  return output;
}

void DistortionEngine::Upsample(float sample, int channel, Halfband* filter,
                                float* outputs) {
  outputs[0] = ProcessHalfbandPath(sample, channel, 0, filter);
  outputs[1] = ProcessHalfbandPath(sample, channel, 1, filter);
}

float DistortionEngine::Downsample(const float* samples, int channel,
                                   Halfband* filter) {
  float even = ProcessHalfbandPath(samples[1], channel, 0, filter);
  float odd = ProcessHalfbandPath(samples[0], channel, 1, filter);
  return 0.5f * (even + odd);
}

float DistortionEngine::ApplyOnePoleHighpass(float sample, int channel,
                                             float coefficient,
                                             float* inputState,
                                             float* outputState) {
  float feedforward = 0.5f * (1.0f + coefficient);
  float output = feedforward * (sample - inputState[channel]) +
                 coefficient * outputState[channel];
  inputState[channel] = sample;
  outputState[channel] = output;
  return output;
}

float DistortionEngine::UpdateNoiseGate(float sample, int channel) {
  float magnitude = sample >= 0.0f ? sample : -sample;
  float* envelope = &gateEnvelope_[channel];
  float coefficient = magnitude > *envelope ? gateEnvelopeAttackCoefficient_
                                            : gateEnvelopeReleaseCoefficient_;
  *envelope += coefficient * (magnitude - *envelope);

  if (*envelope >= kGateOpenThreshold) {
    gateOpen_[channel] = true;
    gateHoldCounter_[channel] = gateHoldSamples_;
  } else if (*envelope < kGateCloseThreshold) {
    if (gateHoldCounter_[channel] > 0) {
      gateHoldCounter_[channel]--;
    } else {
      gateOpen_[channel] = false;
    }
  }

  float target = gateOpen_[channel] ? 1.0f : 0.0f;
  float gainCoefficient = target > gateGain_[channel] ? gateOpenCoefficient_
                                                      : gateCloseCoefficient_;
  gateGain_[channel] += gainCoefficient * (target - gateGain_[channel]);
  return gateGain_[channel];
}

float DistortionEngine::ApplyDcBlock(float sample, int channel) {
  return ApplyOnePoleHighpass(sample, channel, dcBlockerCoefficient_,
                              dcInputState_, dcOutputState_);
}

float DistortionEngine::ApplyTightHighpass(float sample, int channel) {
  return ApplyOnePoleHighpass(sample, channel, tightHighpassCoefficient_,
                              tightInputState_, tightOutputState_);
}

float DistortionEngine::ApplyPreEmphasis(float sample, int channel) {
  float boosted = ProcessBiquad(sample, channel, &preMidBoostFilter_);
  return ProcessBiquad(boosted, channel, &preHighShelfFilter_);
}

float DistortionEngine::ApplyFirstClip(float sample) {
  return tanhf(sample);
}

float DistortionEngine::ApplySecondClip(float sample) {
  float driven = sample * kSecondClipGain;
  if (driven > 1.0f) {
    return 1.0f;
  }
  if (driven < -1.0f) {
    return -1.0f;
  }
  return 1.5f * driven - 0.5f * driven * driven * driven;
}

float DistortionEngine::ApplyClipStages(float sample, int channel) {
  float stage1[2];
  float stage2[4];

  Upsample(sample, channel, &upsampleStage1_, stage1);
  Upsample(stage1[0], channel, &upsampleStage2_, &stage2[0]);
  Upsample(stage1[1], channel, &upsampleStage2_, &stage2[2]);

  for (int i = 0; i < 4; i++) {
    stage2[i] = ApplySecondClip(ApplyFirstClip(stage2[i]));
  }

  stage1[0] = Downsample(&stage2[0], channel, &downsampleStage2_);
  stage1[1] = Downsample(&stage2[2], channel, &downsampleStage2_);

  return Downsample(stage1, channel, &downsampleStage1_);
}

float DistortionEngine::ApplyMidScoop(float sample, int channel) {
  return ProcessBiquad(sample, channel, &midScoopFilter_);
}

float DistortionEngine::ApplyFizzLowpass(float sample, int channel) {
  fizzState_[channel] +=
      fizzLowpassCoefficient_ * (sample - fizzState_[channel]);
  return fizzState_[channel];
}

float DistortionEngine::ApplyLowShelf(float sample, int channel) {
  return ProcessBiquad(sample, channel, &lowShelfFilter_);
}

float DistortionEngine::ApplyHighShelf(float sample, int channel) {
  return ProcessBiquad(sample, channel, &highShelfFilter_);
}

float DistortionEngine::OutputClamp(float sample) {
  if (!(sample >= -FLT_MAX && sample <= FLT_MAX)) {
    return 0.0f;
  }

  return Clamp(sample, -1.0f, 1.0f);
}

float DistortionEngine::ProcessSample(float sample, int channel) {
  if (!(sample >= -FLT_MAX && sample <= FLT_MAX)) {
    return 0.0f;
  }

  float dcBlocked = ApplyDcBlock(sample, channel);
  float gateGain = UpdateNoiseGate(dcBlocked, channel);
  float tightened = ApplyTightHighpass(dcBlocked, channel);
  float emphasized = ApplyPreEmphasis(tightened, channel);
  float driven = emphasized * distortionGain_;
  float clipped = ApplyClipStages(driven, channel);
  float scooped = ApplyMidScoop(clipped, channel);
  float smoothed = ApplyFizzLowpass(scooped, channel);
  float lowShaped = ApplyLowShelf(smoothed, channel);
  float shaped = ApplyHighShelf(lowShaped, channel);
  float output = shaped * levelGain_ * gateGain;

  return OutputClamp(output);
}
