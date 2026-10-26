#pragma once
#include "EffectParameters.h"
#include "SlotEQ.h"
#include <algorithm>
#include <cmath>
#include <vector>

// Per-channel wet processing. Allocation and coefficient storage are prepared
// before publication; automation smooths on a continuous sample schedule.
class SlotIR
{
public:
  void Reset(double rate)
  {
    mRate = std::max(1., rate);
    mSmoothing = std::exp(-1. / (.005 * mRate));
    mDelay.assign(static_cast<size_t>(std::ceil(.5 * mRate)) + 2, 0.);
    mWrite = 0;
    mHP.Reset(); mLP.Reset();
    mLow = 0.; mHigh = 20000.; mTime = 0.;
    mHPMix = mLPMix = 0.; mPolarity = 1.;
    mCounter = 0; mFirst = true;
    Update();
  }
  void Process(double* samples, int frames, const UtilityControls& c)
  {
    const double low = std::clamp(c.irLowCut, 0., 2000.);
    const double high = std::clamp(c.irHighCut, 200., 20000.);
    const double time = std::clamp(c.irPredelay, 0., 500.) * .001 * mRate;
    const double polarity = c.irInvert ? -1. : 1.;
    if (mFirst)
    {
      mLow = low; mHigh = high; mTime = time; mPolarity = polarity;
      mHPMix = low > 0. ? 1. : 0.; mLPMix = high < 20000. ? 1. : 0.;
      Update(); mFirst = false;
    }
    for (int i = 0; i < frames; ++i)
    {
      const bool changed = mLow != low || mHigh != high;
      mLow = Smooth(mLow, low); mHigh = Smooth(mHigh, high);
      mTime = Smooth(mTime, time);
      mPolarity += std::clamp(polarity - mPolarity, -2. / (.005 * mRate), 2. / (.005 * mRate));
      if (mCounter == 0 && changed) Update();
      mCounter = (mCounter + 1) % 16;
      mHPMix = Smooth(mHPMix, low > 0. ? 1. : 0.);
      mLPMix = Smooth(mLPMix, high < 20000. ? 1. : 0.);
      mDelay[mWrite] = samples[i];
      const size_t whole = static_cast<size_t>(mTime);
      const double fraction = mTime - whole;
      const size_t recent = (mWrite + mDelay.size() - whole) % mDelay.size();
      const size_t older = (recent + mDelay.size() - 1) % mDelay.size();
      double x = mDelay[recent] + fraction * (mDelay[older] - mDelay[recent]);
      if (++mWrite == mDelay.size()) mWrite = 0;
      const double hp = mHP.Tick(x);
      x += mHPMix * (hp - x);
      const double lp = mLP.Tick(x);
      samples[i] = (x + mLPMix * (lp - x)) * mPolarity;
    }
  }
private:
  double Smooth(double value, double target) const
  { return std::abs(value - target) < 1.e-7 ? target : target + mSmoothing * (value - target); }
  void Update()
  {
    using Params = recursive_linear_filter::BiquadParams;
    mHP.SetParams(Params(mRate, std::min(.49 * mRate, std::max(1., mLow)), .7071067811865476, 0.));
    mLP.SetParams(Params(mRate, std::min(.49 * mRate, mHigh), .7071067811865476, 0.));
  }
  ScalarFilter<CutCoefficients<true>> mHP;
  ScalarFilter<CutCoefficients<false>> mLP;
  std::vector<double> mDelay;
  size_t mWrite = 0;
  double mRate = 48000., mSmoothing = 0., mLow = 0., mHigh = 20000., mTime = 0.;
  double mHPMix = 0., mLPMix = 0., mPolarity = 1.;
  int mCounter = 0;
  bool mFirst = true;
};
