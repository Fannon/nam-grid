#pragma once
#include "SlotTimeEffects.h"
#include "dsp/PedalDrive.h"

// Voiced, oversampled drive, with independent stereo instances in NAMStage.
// Low keep subtracts a complementary low band before the entire drive/EQ path
// and restores it afterward. Both restored lows and outer dry match FIR delay.
class SlotPedalDrive
{
public:
  static constexpr int latency = grid_pedal::Oversampler4x::latency;
  void Reset(double rate)
  {
    mRate = std::max(1., rate);
    mControls.Reset(mRate);
    mOversampler.Reset();
    mCoefficients.Reset(4. * mRate);
    mVoices = {};
    mWeights = {};
    mDry = {};
    mLow = {};
    mLowState = mBodyState = mPrevious = mDC = mToneState = 0.;
    mCounter = mDelayIndex = 0;
    mFirst = true;
    mCached.fill(-1.e30);
  }
  void Process(double* samples, int frames, const UtilityControls& controls)
  {
    if (frames <= 0)
      return;
    mControls.Target(controls.pedal, uPedalStyle);
    const int style = static_cast<int>(std::clamp(controls.pedal[0], 0., double(effect_modes::pedalCount - 1)));
    if (mFirst)
    {
      mWeights[style] = 1.;
      mFirst = false;
    }
    else if (mWeights[style] == 0.)
      mVoices[style] = {};
    for (int i = 0; i < frames; ++i)
    {
      const auto& c = mControls.Tick();
      if (mCounter == 0)
      {
        if (c[1] != mCached[1])
        {
          mDrive = std::pow(10., c[1] * .05);
          mRootDrive = std::sqrt(mDrive);
        }
        if (c[2] != mCached[2])
          mTonePole = std::exp(-2. * grid_pedal::pi * std::min(c[2], .4 * mRate) / (4. * mRate));
        if (c[3] != mCached[3])
          mBody = std::pow(10., c[3] * .05) - 1.;
        if (c[4] != mCached[4])
          mSplitPole = std::exp(-2. * grid_pedal::pi * std::min(c[4], .4 * mRate) / mRate);
        if (c[6] != mCached[6])
          mOutput = std::pow(10., c[6] * .05);
        mCached = c;
      }
      mCounter = (mCounter + 1) % 16;
      const bool settled = mWeights[style] == 1.;
      if (!settled)
      {
        double others = 0.;
        for (int j = 0; j < effect_modes::pedalCount; ++j)
          if (j != style)
          {
            mWeights[j] = std::max(0., mWeights[j] - 1. / (.02 * mRate));
            others += mWeights[j];
          }
        mWeights[style] = 1. - others;
      }
      const double input = samples[i];
      mLowState = grid_pedal::Flush(input + mSplitPole * (mLowState - input));
      const double kept = mLowState * c[5] * .01;
      const double dry = mDry[mDelayIndex], low = mLow[mDelayIndex];
      mDry[mDelayIndex] = input;
      mLow[mDelayIndex] = kept;
      mDelayIndex = (mDelayIndex + 1) % latency;
      const double processed = mOversampler.Tick(input - kept, [&](double x) {
        mBodyState = grid_pedal::Flush(x + mCoefficients.poles[grid_pedal::Coefficients::Body] * (mBodyState - x));
        x += mBody * mBodyState;
        double wet = 0.;
        if (settled)
          wet = mVoices[style].Tick(x, style, mDrive, mRootDrive, c[1] / 36., mCoefficients);
        else
          for (int j = 0; j < effect_modes::pedalCount; ++j)
            if (mWeights[j] > 0.)
              wet += mWeights[j] * mVoices[j].Tick(x, j, mDrive, mRootDrive, c[1] / 36., mCoefficients);
        mDC = grid_pedal::Flush(wet - mPrevious + mCoefficients.dc * mDC);
        mPrevious = wet;
        mToneState = grid_pedal::Flush(mDC + mTonePole * (mToneState - mDC));
        return mToneState;
      });
      samples[i] = (dry + c[7] * .01 * (processed + low - dry)) * mOutput;
    }
  }

private:
  grid_fx::SmoothControls<8> mControls;
  grid_pedal::Oversampler4x mOversampler;
  grid_pedal::Coefficients mCoefficients;
  std::array<grid_pedal::Voice, effect_modes::pedalCount> mVoices{};
  std::array<double, effect_modes::pedalCount> mWeights{};
  std::array<double, latency> mDry{}, mLow{};
  std::array<double, 8> mCached{};
  double mRate = 48000., mLowState = 0., mBodyState = 0., mPrevious = 0., mDC = 0., mToneState = 0.;
  double mDrive = 1., mRootDrive = 1., mTonePole = 0., mSplitPole = 0., mBody = 0., mOutput = 1.;
  int mCounter = 0, mDelayIndex = 0;
  bool mFirst = true;
};
