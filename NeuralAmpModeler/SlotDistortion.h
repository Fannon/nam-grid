#pragma once
#include "SlotTimeEffects.h"

// Original waveshapers. Style changes crossfade their outputs instead of moving
// through intermediate styles. Independent channel instances preserve stereo.
class SlotDistortion
{
public:
  // Static shaper before tone filtering, wet/dry mix and output trim. Shared with
  // the editor so gain, bias and style changes draw the same transfer function.
  static double Transfer(double input, int style, double drive, double bias)
  { return Shape(input * drive + bias, style) - Shape(bias, style); }
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
    const int style = static_cast<int>(std::clamp(controls.distortion[0], 0., double(effect_modes::distortionCount - 1)));
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
      const double dry = samples[i], bias = c[3] * .004;
      double wet = 0.;
      for (int j = 0; j < effect_modes::distortionCount; ++j)
      {
        mWeights[j] = grid_fx::Flush((j == style ? 1. : 0.) + mSmooth * (mWeights[j] - (j == style ? 1. : 0.)));
        if (mWeights[j] != 0.) wet += mWeights[j] * Transfer(dry, j, mDrive, bias);
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
    switch (static_cast<effect_modes::DistortionStyle>(style))
    {
      case effect_modes::DistortionStyle::Soft: return std::tanh(x);
      case effect_modes::DistortionStyle::Hard: return std::clamp(x, -1., 1.);
      case effect_modes::DistortionStyle::Fuzz: return std::copysign(1. - 1. / (1. + 6. * std::abs(x)), x);
      case effect_modes::DistortionStyle::Foldback:
      {
        const double t = x + 1. - 4. * std::floor((x + 1.) / 4.);
        return 1. - std::abs(t - 2.);
      }
      // Equal small-signal slope with a lower negative shoulder. This is an
      // original asymmetric shaper, not a physical diode-circuit model.
      case effect_modes::DistortionStyle::Diode: return x >= 0. ? std::tanh(x) : .5 * std::tanh(2. * x);
      case effect_modes::DistortionStyle::Rectify: return std::abs(std::tanh(x));
      default: return std::tanh(x);
    }
  }
  grid_fx::SmoothControls<6> mControls;
  std::array<double, effect_modes::distortionCount> mWeights{};
  std::array<double, 3> mCached{};
  double mRate = 48000., mSmooth = 0., mDC = 0.;
  double mPrevious = 0., mHighPass = 0., mLowPass = 0.;
  double mDrive = 1., mTone = 0., mOutput = 1.;
  int mCounter = 0;
  bool mFirst = true;
};
