#pragma once
#include "SlotEQ.h"
#include "EffectParameters.h"

// Reuses the same MIT AudioDSPTools peak coefficients and smoothed cut filters as SlotEQ.
class SlotParametricEQ
{
public:
  void Reset(double rate)
  {
    mRate = std::max(1., rate);
    mSmoothing = std::exp(-1. / (.005 * mRate));
    mCurrent = {800., 0., .7};
    mCounter = 0;
    mDirty = false;
    mBell.Reset();
    Update(); // Allocate coefficient storage outside audio.
    mCuts.Reset(rate);
    mCuts.Process(nullptr, 0, true, {});
  }
  void Process(double* samples, int frames, const UtilityControls& c, EQControls cuts)
  {
    cuts.presence = 0.;
    mCuts.Process(samples, frames, true, cuts);
    const std::array<double, 3> target{
      std::clamp(c.frequency, 80., 8000.), std::clamp(c.gain, -12., 12.), std::clamp(c.q, .3, 4.)};
    for (int i = 0; i < frames; ++i)
    {
      for (int j = 0; j < 3; ++j)
      {
        const double next =
          std::abs(mCurrent[j] - target[j]) < 1.e-7 ? target[j] : target[j] + mSmoothing * (mCurrent[j] - target[j]);
        mDirty |= next != mCurrent[j];
        mCurrent[j] = next;
      }
      if (mCounter++ == 0 && mDirty)
      {
        Update();
        mDirty = false;
      }
      mCounter %= 16;
      samples[i] = mBell.Tick(samples[i]);
    }
  }

private:
  void Update()
  {
    mBell.SetParams(
      recursive_linear_filter::BiquadParams(mRate, std::min(mCurrent[0], .4 * mRate), mCurrent[2], mCurrent[1]));
  }
  ScalarFilter<recursive_linear_filter::Peaking> mBell;
  SlotEQ mCuts;
  std::array<double, 3> mCurrent{};
  double mRate = 48000., mSmoothing = 0.;
  int mCounter = 0;
  bool mDirty = false;
};

class SlotBoost
{
public:
  void Reset(double rate)
  {
    mSmooth = std::exp(-1. / (.005 * std::max(1., rate)));
    mGain = 1.;
    mTilt = 0.;
    mCount = 0;
    mRate = rate;
    mLow.Reset();
    mHigh.Reset();
    Update();
    mCut.Reset(rate);
    mCut.Process(nullptr, 0, true, {});
  }
  void Process(double* samples, int frames, const UtilityControls& c, const EQControls& eq)
  {
    EQControls cuts;
    cuts.highPass = eq.highPass;
    mCut.Process(samples, frames, true, cuts);
    const double gain = std::pow(10., std::clamp(c.boost, 0., 18.) * .05);
    const double tilt = std::clamp(c.tilt, -6., 6.);
    for (int i = 0; i < frames; ++i)
    {
      mGain = gain + mSmooth * (mGain - gain);
      mTilt = tilt + mSmooth * (mTilt - tilt);
      if (mCount++ % 16 == 0)
        Update();
      mCount %= 16;
      samples[i] = mHigh.Tick(mLow.Tick(samples[i])) * mGain;
    }
  }

private:
  void Update()
  {
    const double hz = std::min(800., .4 * mRate);
    mLow.SetParams(recursive_linear_filter::BiquadParams(mRate, hz, .707, -mTilt));
    mHigh.SetParams(recursive_linear_filter::BiquadParams(mRate, hz, .707, mTilt));
  }
  ScalarFilter<recursive_linear_filter::LowShelf> mLow;
  ScalarFilter<recursive_linear_filter::HighShelf> mHigh;
  SlotEQ mCut;
  double mSmooth = 0., mGain = 1., mTilt = 0., mRate = 48000.;
  int mCount = 0;
};

class SlotEnvelopeFilter
{
public:
  void Reset(double rate)
  {
    mRate = rate;
    mSmooth = std::exp(-1. / (.005 * rate));
    mAttack = std::exp(-1. / (.003 * rate));
    mRelease = std::exp(-1. / (.08 * rate));
    mEnv = 0.;
    mCut = 2500.;
    mQ = .7;
    mDepth = 0.;
    mCount = 0;
    mFilter.Reset();
    Update();
  }
  void Process(double* samples, int frames, const UtilityControls& c)
  {
    for (int i = 0; i < frames; ++i)
    {
      const double peak = std::abs(samples[i]);
      mEnv = peak + (peak > mEnv ? mAttack : mRelease) * (mEnv - peak);
      mDepth = std::clamp(c.envelope, 0., 4.) + mSmooth * (mDepth - std::clamp(c.envelope, 0., 4.));
      const double hz =
        std::min(.4 * mRate, std::clamp(c.cutoff, 40., 12000.) * std::exp2(mDepth * std::min(1., mEnv * 4.)));
      mCut = hz + mSmooth * (mCut - hz);
      mQ = std::clamp(c.resonance, .5, 4.) + mSmooth * (mQ - std::clamp(c.resonance, .5, 4.));
      if (mCount++ % 16 == 0)
        Update();
      mCount %= 16;
      samples[i] = mFilter.Tick(samples[i]);
    }
  }

private:
  void Update() { mFilter.SetParams(recursive_linear_filter::BiquadParams(mRate, std::min(.4 * mRate, mCut), mQ, 0.)); }
  ScalarFilter<CutCoefficients<false>> mFilter;
  double mRate = 48000., mSmooth = 0., mAttack = 0., mRelease = 0., mEnv = 0., mCut = 2500., mQ = .7, mDepth = 0.;
  int mCount = 0;
};

class SlotAutoSwell
{
public:
  void Reset(double rate)
  {
    mRate = rate;
    mEnv = mGain = mHold = 0.;
    mOpen = false;
  }
  void Process(double* samples, int frames, const UtilityControls& c)
  {
    const double threshold = std::pow(10., std::clamp(c.threshold, -72., -12.) / 20.);
    const double attack = std::exp(-1. / (.001 * mRate)), release = std::exp(-1. / (.03 * mRate));
    const double rise = 1. / (mRate * .001 * std::clamp(c.rise, 10., 1200.));
    const double close = std::exp(-1. / (mRate * .001 * std::clamp(c.release, 20., 800.)));
    for (int i = 0; i < frames; ++i)
    {
      const double peak = std::abs(samples[i]);
      mEnv = peak + (peak > mEnv ? attack : release) * (mEnv - peak);
      if (mEnv >= threshold)
      {
        mOpen = true;
        mHold = .04 * mRate;
      }
      else if (mEnv < threshold * .707 && --mHold <= 0.)
      {
        mOpen = false;
        mHold = 0.;
      }
      mGain = mOpen ? std::min(1., mGain + rise) : mGain * close;
      if (mGain < 1.e-30)
        mGain = 0.;
      samples[i] *= mGain;
    }
  }

private:
  double mRate = 48000., mEnv = 0., mGain = 0., mHold = 0.;
  bool mOpen = false;
};
