#pragma once
#include "SlotTimeEffects.h"
#include "dsp/MinimumPhaseOversampling.h"
#include <bit>
#include <cstdint>

// Original waveshapers. Style changes crossfade their outputs instead of moving
// through intermediate styles. Independent channel instances preserve stereo.
class SlotDistortion
{
public:
  // Static shaper before tone filtering, wet/dry mix and output trim. Shared with
  // the editor so gain, bias and style changes draw the same transfer function.
  static double Transfer(double input, int style, double drive, double bias)
  { return Shape(input * drive + bias, style) - Shape(bias, style); }
  // Automatic quality keeps a similar internal rate as the host rate rises.
  static int FactorForRate(double rate)
  { return rate < 88200. ? 4 : rate < 176400. ? 2 : 1; }
  static std::complex<double> LinearResponse(double hz, double rate)
  {
    const int factor = FactorForRate(rate);
    const auto highZ = std::polar(1., -2. * grid_fx::pi * hz / (factor * rate));
    const auto filter = factor == 4 ? grid_oversampling::FIR<4>::Response(hz, factor * rate)
                      : factor == 2 ? grid_oversampling::FIR<2>::Response(hz, factor * rate)
                                    : std::complex<double>(1.);
    // Unity-slope ADAA is the average of adjacent high-rate samples. The two
    // resampling filters each contribute their complex response to the wet path.
    return filter * filter * .5 * (1. + highZ);
  }
  int OversamplingFactor() const { return mFactor; }
  // Explicit factors/ADAA are for offline quality comparisons, not saved controls.
  void Reset(double rate, int factor = 0, bool adaa = true)
  {
    mRate = std::max(1., rate);
    mFactor = factor == 1 || factor == 2 || factor == 4 || factor == 8 ? factor : FactorForRate(mRate);
    mADAA = adaa;
    if (mFactor == 2) mOversample2.Reset();
    if (mFactor == 4) mOversample4.Reset();
    if (mFactor == 8) mOversample8.Reset();
    mPreviousInput = 0.;
    mIntegralMask = mBiasMask = 0;
    mIntegralDriveKey = mBiasKey = 0;
    mSmooth = std::exp(-1. / (.02 * mRate));
    mDC = std::exp(-2. * grid_fx::pi * 5. / mRate);
    mControls.Reset(mRate);
    mWeights = {};
    mPrevious = mHighPass = mLowPass = 0.;
    mFirst = true;
    mCached = {-1., -1., -100.};
  }
  // The uncached instantiation is an offline numerical regression oracle;
  // production callers always use the cached default with no runtime switch.
  template <bool CacheIntegrals = true>
  void Process(double* samples, int frames, const UtilityControls& controls)
  {
    mControls.Target(controls.distortion, uDistStyle);
    const int style = static_cast<int>(std::clamp(controls.distortion[0], 0., double(effect_modes::distortionCount - 1)));
    if (mFirst) { mWeights[style] = 1.; mFirst = false; }
    for (int i = 0; i < frames; ++i)
    {
      const auto& c = mControls.Tick();
      // Every smoothed sample, cached on value: 16-sample steps made audible zipper noise.
      if (c[1] != mCached[0]) mDrive = std::pow(10., c[1] / 20.);
      if (c[2] != mCached[1]) mTone = std::exp(-2. * grid_fx::pi * std::min(c[2], mRate * .4) / mRate);
      if (c[5] != mCached[2]) mOutput = std::pow(10., c[5] / 20.);
      mCached = {c[1], c[2], c[5]};
      const double dry = samples[i], bias = c[3] * .004;
      for (int j = 0; j < effect_modes::distortionCount; ++j)
        mWeights[j] = grid_fx::Flush((j == style ? 1. : 0.) + mSmooth * (mWeights[j] - (j == style ? 1. : 0.)));
      const auto shape = [&](double input) {
        double wet = 0.;
        if constexpr (!CacheIntegrals)
        {
          for (int j = 0; j < effect_modes::distortionCount; ++j)
            if (mWeights[j] != 0.)
              wet += mWeights[j] * (mADAA ? Antialiased(input, mPreviousInput, j, mDrive, bias)
                                        : Transfer(input, j, mDrive, bias));
          mPreviousInput = input;
          return wet;
        }
        const auto driveKey = std::bit_cast<uint64_t>(mDrive), biasKey = std::bit_cast<uint64_t>(bias);
        const bool sameCurve = driveKey == mIntegralDriveKey && biasKey == mBiasKey;
        if (biasKey != mBiasKey) mBiasMask = 0;
        const double x = input * mDrive + bias, before = mPreviousInput * mDrive + bias;
        const double delta = x - before;
        const bool smallInterval = std::abs(delta) <= 1.e-5 * std::max({1., std::abs(x), std::abs(before)});
        unsigned nextMask = 0;
        for (int j = 0; j < effect_modes::distortionCount; ++j)
          if (mWeights[j] != 0.)
          {
            if (!mADAA) { wet += mWeights[j] * Transfer(input, j, mDrive, bias); continue; }
            const unsigned bit = 1u << j;
            // Reuse F(previous) only when it was evaluated for this exact
            // curve in the preceding high-rate sample. Automation, style
            // crossfades and the small-interval fallback invalidate that reuse.
            double averaged;
            if (smallInterval) averaged = Shape(.5 * (x + before), j);
            else
            {
              const double current = Integral(x, j);
              const double previous = sameCurve && (mIntegralMask & bit) ? mIntegrals[j] : Integral(before, j);
              averaged = (current - previous) / delta;
              mIntegrals[j] = current;
              nextMask |= bit;
            }
            if (!(mBiasMask & bit)) { mBiasOffsets[j] = Shape(bias, j); mBiasMask |= bit; }
            wet += mWeights[j] * (averaged - mBiasOffsets[j]);
          }
        mIntegralMask = nextMask;
        mIntegralDriveKey = driveKey; mBiasKey = biasKey;
        mPreviousInput = input;
        return wet;
      };
      const double wet = mFactor == 2 ? mOversample2.Tick(dry, shape)
                       : mFactor == 4 ? mOversample4.Tick(dry, shape)
                       : mFactor == 8 ? mOversample8.Tick(dry, shape) : shape(dry);
      // Reject the remaining signal-dependent DC from asymmetric distortion.
      mHighPass = grid_fx::Flush(wet - mPrevious + mDC * mHighPass);
      mPrevious = wet;
      mLowPass = grid_fx::Flush((1. - mTone) * mHighPass + mTone * mLowPass);
      samples[i] = (dry + c[4] * .01 * (mLowPass - dry)) * mOutput;
    }
  }

  // First-order ADAA follows Parker, Zavalishin & Le Bivic (DAFx 2016):
  // https://www.dafx.de/paper-archive/2016/dafxpapers/20-DAFx-16_paper_41-PN.pdf
  // Original analytic primitives below; no external implementation copied.
  // Antiderivative antialiasing averages the shaper over the
  // line between adjacent high-rate samples. Current gain/bias apply to both
  // ends so parameter motion is not mistaken for an audio interval.
  static double Antialiased(double input, double previous, int style, double drive, double bias)
  {
    const double x = input * drive + bias, before = previous * drive + bias;
    const double delta = x - before;
    const double shaped = std::abs(delta) <= 1.e-5 * std::max({1., std::abs(x), std::abs(before)})
      ? Shape(.5 * (x + before), style) : (Integral(x, style) - Integral(before, style)) / delta;
    return shaped - Shape(bias, style);
  }

private:
  static double LogCosh(double x)
  { return std::abs(x) + std::log1p(std::exp(-2. * std::abs(x))) - std::log(2.); }
  static double Integral(double x, int style)
  {
    const double magnitude = std::abs(x);
    switch (static_cast<effect_modes::DistortionStyle>(style))
    {
      case effect_modes::DistortionStyle::Hard:
        return magnitude <= 1. ? .5 * x * x : magnitude - .5;
      case effect_modes::DistortionStyle::Fuzz:
        return magnitude - std::log1p(6. * magnitude) / 6.;
      case effect_modes::DistortionStyle::Foldback:
      {
        const double t = x + 1. - 4. * std::floor((x + 1.) / 4.);
        return t <= 2. ? .5 * t * t - t : 3. * t - .5 * t * t - 4.;
      }
      case effect_modes::DistortionStyle::Diode: return x >= 0. ? LogCosh(x) : .25 * LogCosh(2. * x);
      case effect_modes::DistortionStyle::Rectify: return std::copysign(LogCosh(x), x);
      default: return LogCosh(x);
    }
  }
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
  grid_oversampling::FIR<2> mOversample2;
  grid_oversampling::FIR<4> mOversample4;
  grid_oversampling::FIR<8> mOversample8;
  int mFactor = 4;
  bool mADAA = true;
  double mPreviousInput = 0.;
  std::array<double, effect_modes::distortionCount> mIntegrals{}, mBiasOffsets{};
  unsigned mIntegralMask = 0, mBiasMask = 0;
  uint64_t mIntegralDriveKey = 0, mBiasKey = 0;
  grid_fx::SmoothControls<6> mControls;
  std::array<double, effect_modes::distortionCount> mWeights{};
  std::array<double, 3> mCached{};
  double mRate = 48000., mSmooth = 0., mDC = 0.;
  double mPrevious = 0., mHighPass = 0., mLowPass = 0.;
  double mDrive = 1., mTone = 0., mOutput = 1.;
  bool mFirst = true;
};
