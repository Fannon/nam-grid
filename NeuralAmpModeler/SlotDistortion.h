#pragma once
#include "SlotTimeEffects.h"

// Original waveshapers. Style changes crossfade their outputs instead of moving
// through intermediate styles. Independent channel instances preserve stereo.
class SlotDistortion
{
public:
  void Reset(double rate)
  {
    mRate = std::max(1., rate);
    mSmooth = std::exp(-1. / (.02 * mRate));
    mDC = std::exp(-2. * grid_fx::pi * 5. / mRate);
    mControls.Reset(mRate);
    mWeights = {};
    mPrevious = mHighPass = mLowPass = 0.;
    mCounter = 0; mFirst = true;
    mCached = {-1., -1., -100.};
  }
  void Process(double* samples, int frames, const UtilityControls& controls)
  {
    mControls.Target(controls.distortion, uDistStyle);
    const int style = static_cast<int>(std::clamp(controls.distortion[0], 0., 3.));
    if (mFirst) { mWeights[style] = 1.; mFirst = false; }
    for (int i = 0; i < frames; ++i)
    {
      const auto& c = mControls.Tick();
      if (mCounter == 0)
      {
        if (c[1] != mCached[0]) mDrive = std::pow(10., c[1] / 20.);
        if (c[2] != mCached[1]) mTone = std::exp(-2. * grid_fx::pi * std::min(c[2], mRate * .4) / mRate);
        if (c[5] != mCached[2]) mOutput = std::pow(10., c[5] / 20.);
        mCached = {c[1], c[2], c[5]};
      }
      mCounter = (mCounter + 1) % 16;
      const double dry = samples[i], bias = c[3] * .004, x = dry * mDrive + bias;
      double wet = 0.;
      for (int j = 0; j < 4; ++j)
      {
        mWeights[j] = grid_fx::Flush((j == style ? 1. : 0.) + mSmooth * (mWeights[j] - (j == style ? 1. : 0.)));
        if (mWeights[j] != 0.) wet += mWeights[j] * (Shape(x, j) - Shape(bias, j));
      }
      // Reject the remaining signal-dependent DC from asymmetric distortion.
      mHighPass = grid_fx::Flush(wet - mPrevious + mDC * mHighPass);
      mPrevious = wet;
      mLowPass = grid_fx::Flush((1. - mTone) * mHighPass + mTone * mLowPass);
      samples[i] = (dry + c[4] * .01 * (mLowPass - dry)) * mOutput;
    }
  }

private:
  static double Shape(double x, int style)
  {
    switch (style)
    {
      case 0: return std::tanh(x);
      case 1: return std::clamp(x, -1., 1.);
      case 2: return std::copysign(1. - 1. / (1. + 6. * std::abs(x)), x);
      default:
        const double t = x + 1. - 4. * std::floor((x + 1.) / 4.);
        return 1. - std::abs(t - 2.);
    }
  }
  grid_fx::SmoothControls<6> mControls;
  std::array<double, 4> mWeights{};
  std::array<double, 3> mCached{};
  double mRate = 48000., mSmooth = 0., mDC = 0.;
  double mPrevious = 0., mHighPass = 0., mLowPass = 0.;
  double mDrive = 1., mTone = 0., mOutput = 1.;
  int mCounter = 0;
  bool mFirst = true;
};
