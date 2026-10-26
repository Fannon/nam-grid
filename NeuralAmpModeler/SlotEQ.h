#pragma once
#include <array>
#include <type_traits>
#include "../AudioDSPTools/dsp/RecursiveLinearFilter.h"

struct EQControls
{
  double bass = 0., mid = 0., treble = 0., highPass = 0., lowPass = 20000., presence = 0.;
  double irLevel = -18.;
  double midFrequency = 800.;
};

// RBJ Audio EQ Cookbook low/high-pass equations (12 dB/octave, Butterworth Q).
// Mathematical formulas: https://www.w3.org/TR/audio-eq-cookbook/
// Formula reference copyright (c) 2021 W3C (MIT, ERCIM, Keio, Beihang).
// New C++ implementation; reference notice retained in ThirdPartyNotices.txt.
template <bool HighPass>
class CutCoefficients : public recursive_linear_filter::Biquad
{
public:
  void SetParams(const recursive_linear_filter::BiquadParams& params) override
  {
    const double omega = params.GetOmega0(), cosine = params.GetCosW(omega), alpha = params.GetAlpha(omega);
    const double b0 = (1. + (HighPass ? cosine : -cosine)) * .5;
    this->_AssignCoefficients(1. + alpha, -2. * cosine, 1. - alpha, b0, (HighPass ? -2. : 2.) * b0, b0);
  }
};

// Reuse MIT AudioDSPTools' coefficient implementations, with scalar histories instead
// of its multichannel buffer API. Coefficient updates and ticks allocate nothing.
template <class Filter>
class ScalarFilter : public Filter
{
  static_assert(std::is_base_of_v<recursive_linear_filter::Biquad, Filter>,
                "ScalarFilter has exactly two input/output history samples and requires a biquad");
public:
  void Reset()
  {
    mX = {};
    mY = {};
  }
  double Tick(double x)
  {
    double y = this->mInputCoefficients[0] * x;
    for (size_t i = 1; i < this->mInputCoefficients.size(); ++i)
      y += this->mInputCoefficients[i] * mX[i - 1];
    for (size_t i = 1; i < this->mOutputCoefficients.size(); ++i)
      y += this->mOutputCoefficients[i] * mY[i - 1];
    mX[1] = mX[0];
    mX[0] = x;
    mY[1] = mY[0];
    mY[0] = std::abs(y) < 1.e-30 ? 0. : y;
    return mY[0];
  }

private:
  std::array<double, 2> mX{}, mY{};
};

class SlotEQ
{
public:
  void Reset(double rate)
  {
    mRate = std::max(1., rate);
    mSmoothing = std::exp(-1. / (.005 * mRate));
    mBass.Reset();
    mMid.Reset();
    mTreble.Reset();
    mHP.Reset();
    mLP.Reset();
    mFirst = true;
    mDirty = false;
    mCounter = 0;
  }
  void Process(double* samples, int frames, bool cuts, const EQControls& c)
  {
    const std::array<double, 4> target =
      cuts ? std::array<double, 4>{std::clamp(c.highPass, 0., 300.), std::clamp(c.lowPass, 2000., 20000.),
                                   std::clamp(c.presence, -6., 6.), 800.}
           : std::array<double, 4>{
               std::clamp(c.bass, -12., 12.), std::clamp(c.mid, -12., 12.), std::clamp(c.treble, -12., 12.), std::clamp(c.midFrequency, 100., 4000.)};
    if (mFirst)
    {
      mCurrent = target;
      mHPMix = target[0] > 0. ? 1. : 0.;
      mLPMix = target[1] < 20000. ? 1. : 0.;
      Update(cuts);
      mFirst = false;
    }
    for (int i = 0; i < frames; ++i)
    {
      bool changed = false;
      for (int j = 0; j < 4; ++j)
      {
        const double next =
          std::abs(mCurrent[j] - target[j]) < 1.e-7 ? target[j] : target[j] + mSmoothing * (mCurrent[j] - target[j]);
        changed |= next != mCurrent[j];
        mCurrent[j] = next;
      }
      mDirty |= changed;
      if (mCounter++ == 0 && mDirty)
      {
        Update(cuts);
        mDirty = false;
      }
      mCounter %= 16;
      if (cuts)
      {
        mHPMix = Approach(mHPMix, target[0] > 0. ? 1. : 0.);
        mLPMix = Approach(mLPMix, target[1] < 20000. ? 1. : 0.);
        const double hp = mHP.Tick(samples[i]);
        const double x = samples[i] + mHPMix * (hp - samples[i]);
        const double lp = mLP.Tick(x);
        samples[i] = mTreble.Tick(x + mLPMix * (lp - x));
      }
      else
        samples[i] = mTreble.Tick(mMid.Tick(mBass.Tick(samples[i])));
    }
  }

private:
  double Approach(double value, double target) const
  {
    const double step = 1. / (.005 * mRate);
    return value < target ? std::min(target, value + step) : std::max(target, value - step);
  }
  void Update(bool cuts)
  {
    const auto frequency = [this](double hz) { return std::min(hz, .4 * mRate); };
    if (cuts)
    {
      mHP.SetParams(recursive_linear_filter::BiquadParams(mRate, std::max(1., mCurrent[0]), .7071067811865476, 0.));
      mLP.SetParams(
        recursive_linear_filter::BiquadParams(mRate, std::min(.49 * mRate, mCurrent[1]), .7071067811865476, 0.));
      mTreble.SetParams(recursive_linear_filter::BiquadParams(mRate, frequency(3500.), .707, mCurrent[2]));
    }
    else
    {
      mBass.SetParams(recursive_linear_filter::BiquadParams(mRate, frequency(150.), .707, mCurrent[0]));
      mMid.SetParams(recursive_linear_filter::BiquadParams(mRate, frequency(mCurrent[3]), .7, mCurrent[1]));
      mTreble.SetParams(recursive_linear_filter::BiquadParams(mRate, frequency(3000.), .707, mCurrent[2]));
    }
  }
  ScalarFilter<recursive_linear_filter::LowShelf> mBass;
  ScalarFilter<recursive_linear_filter::Peaking> mMid;
  ScalarFilter<recursive_linear_filter::HighShelf> mTreble;
  ScalarFilter<CutCoefficients<true>> mHP;
  ScalarFilter<CutCoefficients<false>> mLP;
  std::array<double, 4> mCurrent{};
  double mRate = 48000., mSmoothing = 0., mHPMix = 0., mLPMix = 0.;
  int mCounter = 0;
  bool mFirst = true, mDirty = false;
};
