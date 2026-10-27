#pragma once
#include "dsp/Antiderivative.h"
#include "dsp/MinimumPhaseOversampling.h"
#include "SlotEQ.h"
#include "EffectParameters.h"
#include "SlotTimeEffects.h"

// Reuses the same MIT AudioDSPTools peak coefficients and smoothed cut filters as SlotEQ.
class NotchCoefficients : public recursive_linear_filter::Biquad
{
public:
  void SetParams(const recursive_linear_filter::BiquadParams& p) override
  {
    const double omega = p.GetOmega0(), cosine = p.GetCosW(omega), alpha = p.GetAlpha(omega);
    _AssignCoefficients(1. + alpha, -2. * cosine, 1. - alpha, 1., -2. * cosine, 1.);
  }
};
// RBJ band-pass with constant 0 dB peak gain.
class BandPassCoefficients : public recursive_linear_filter::Biquad
{
public:
  void SetParams(const recursive_linear_filter::BiquadParams& p) override
  {
    const double omega = p.GetOmega0(), cosine = p.GetCosW(omega), alpha = p.GetAlpha(omega);
    _AssignCoefficients(1. + alpha, -2. * cosine, 1. - alpha, alpha, 0., -alpha);
  }
};
// Parametric EQ end bands. Reuses the RBJ coefficient implementations above and
// in MIT AudioDSPTools. Frequencies, Q and shelf gain smooth over 5 ms; changing
// Cut/Shelf crossfades between independently maintained filter histories.
class SlotParametricCuts
{
public:
  void Reset(double rate)
  {
    mRate = std::max(1., rate);
    mSmoothing = std::exp(-1. / (.005 * mRate));
    mCurrent = {0., 20000., .7071067811865476, .7071067811865476, 0.};
    mHP.Reset(); mLP.Reset(); mShelf.Reset();
    mHPMix = mLPMix = mShelfMix = 0.; mCounter = 0; mDirty = false;
    Update();
  }
  void Process(double* samples, int frames, const UtilityControls& c, const EQControls& eq)
  {
    const std::array<double, 5> target{std::clamp(eq.highPass, 0., 300.), std::clamp(eq.lowPass, 2000., 20000.),
      std::clamp(c.paramLowQ, .1, 4.), std::clamp(c.paramHighQ, .1, 4.), std::clamp(c.paramShelfGain, -12., 12.)};
    for (int i = 0; i < frames; ++i)
    {
      for (size_t j = 0; j < target.size(); ++j)
      {
        const double next = std::abs(mCurrent[j] - target[j]) < 1.e-7 ? target[j]
          : target[j] + mSmoothing * (mCurrent[j] - target[j]);
        mDirty |= next != mCurrent[j]; mCurrent[j] = next;
      }
      if (mCounter++ == 0 && mDirty) { Update(); mDirty = false; }
      mCounter %= 16;
      mHPMix = Approach(mHPMix, target[0] > 0. ? 1. : 0.);
      mLPMix = Approach(mLPMix, target[1] < 20000. ? 1. : 0.);
      mShelfMix = Approach(mShelfMix, c.paramLowMode == 1 ? 1. : 0.);
      const double hp = mHP.Tick(samples[i]), shelf = mShelf.Tick(samples[i]);
      const double low = hp + mShelfMix * (shelf - hp);
      const double x = samples[i] + mHPMix * (low - samples[i]);
      const double lp = mLP.Tick(x);
      samples[i] = x + mLPMix * (lp - x);
    }
  }
private:
  double Approach(double value, double target) const
  { return value + std::clamp(target - value, -1. / (.005 * mRate), 1. / (.005 * mRate)); }
  void Update()
  {
    using Params = recursive_linear_filter::BiquadParams;
    const double low = std::min(std::max(1., mCurrent[0]), .4 * mRate);
    mHP.SetParams(Params(mRate, low, mCurrent[2], 0.));
    mShelf.SetParams(Params(mRate, low, mCurrent[2], mCurrent[4]));
    mLP.SetParams(Params(mRate, std::min(.49 * mRate, mCurrent[1]), mCurrent[3], 0.));
  }
  ScalarFilter<CutCoefficients<true>> mHP;
  ScalarFilter<CutCoefficients<false>> mLP;
  ScalarFilter<recursive_linear_filter::LowShelf> mShelf;
  std::array<double, 5> mCurrent{};
  double mRate = 48000., mSmoothing = 0., mHPMix = 0., mLPMix = 0., mShelfMix = 0.;
  int mCounter = 0;
  bool mDirty = false;
};

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
    mNotch.Reset(); mMode = 0.;
    Update(); // Allocate coefficient storage outside audio.
    mCuts.Reset(rate);
  }
  void Process(double* samples, int frames, const UtilityControls& c, EQControls cuts)
  {
    mCuts.Process(samples, frames, c, cuts);
    const std::array<double, 3> target{
      std::clamp(c.frequency, 20., 20000.), std::clamp(c.gain, -12., 12.), std::clamp(c.q, .1, 20.)};
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
      const double bell = mBell.Tick(samples[i]), notch = mNotch.Tick(samples[i]);
      mMode += std::clamp((c.eqMode >= .5 ? 1. : 0.) - mMode, -1. / (.005 * mRate), 1. / (.005 * mRate));
      samples[i] = bell + mMode * (notch - bell);
    }
  }

private:
  void Update()
  {
    mBell.SetParams(
      recursive_linear_filter::BiquadParams(mRate, std::min(mCurrent[0], .4 * mRate), mCurrent[2], mCurrent[1]));
    mNotch.SetParams(recursive_linear_filter::BiquadParams(mRate, std::min(mCurrent[0], .4 * mRate), mCurrent[2], 0.));
  }
  ScalarFilter<recursive_linear_filter::Peaking> mBell;
  ScalarFilter<NotchCoefficients> mNotch;
  SlotParametricCuts mCuts;
  std::array<double, 3> mCurrent{};
  double mRate = 48000., mSmoothing = 0.;
  int mCounter = 0;
  bool mDirty = false;
  double mMode = 0.;
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
    mMid.Reset(); mMidGain = 0.;
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
      mMidGain = c.extra.boostMidGain + mSmooth * (mMidGain - c.extra.boostMidGain);
      if (mCount++ % 16 == 0)
        Update();
      mCount %= 16;
      samples[i] = mHigh.Tick(mLow.Tick(samples[i])) * mGain;
      const double mid = mMid.Tick(samples[i]);
      if (mMidGain != 0.) samples[i] = mid;
    }
  }

private:
  void Update()
  {
    const double hz = std::min(800., .4 * mRate);
    mLow.SetParams(recursive_linear_filter::BiquadParams(mRate, hz, .707, -mTilt));
    mHigh.SetParams(recursive_linear_filter::BiquadParams(mRate, hz, .707, mTilt));
    mMid.SetParams(recursive_linear_filter::BiquadParams(mRate, std::min(800., .4 * mRate), .7, mMidGain));
  }
  ScalarFilter<recursive_linear_filter::LowShelf> mLow;
  ScalarFilter<recursive_linear_filter::HighShelf> mHigh;
  ScalarFilter<recursive_linear_filter::Peaking> mMid;
  double mMidGain = 0.;
  SlotEQ mCut;
  double mSmooth = 0., mGain = 1., mTilt = 0., mRate = 48000.;
  int mCount = 0;
};

// Original tube-inspired waveshaper: biased soft saturation with a DC blocker.
// This is a character boost, not a circuit model of a particular tube pedal.
class SlotTubeBoost
{
public:
  static double Transfer(double input, double drive, double bias)
  {
    const double offset=std::tanh(bias);
    return (std::tanh(input*drive+bias)-offset)/(1.-offset*offset);
  }
  static std::complex<double> AntialiasResponse(double hz, double rate)
  {
    const int factor = FactorForRate(rate);
    const auto filter = factor == 4 ? grid_oversampling::FIR<4, 32>::Response(hz, factor * rate)
                      : factor == 2 ? grid_oversampling::FIR<2, 32>::Response(hz, factor * rate)
                                    : std::complex<double>(1.);
    return filter * filter;
  }
  static int FactorForRate(double rate) { return rate < 88200. ? 4 : rate < 176400. ? 2 : 1; }
  void Reset(double rate, int factor = 0, bool adaa = false)
  {
    mRate = std::max(1., rate);
    mFactor = factor == 1 || factor == 2 || factor == 4 ? factor : FactorForRate(rate);
    mADAA = adaa; mShaper.Reset(); mZero.Reset();
    if (mFactor == 2) mOversample2.Reset();
    if (mFactor == 4) mOversample4.Reset();
    mTone.Reset(rate);
    mSmooth = std::exp(-1. / (.005 * std::max(1., rate)));
    mDCCoefficient = std::exp(-2. * 3.14159265358979323846 * 5. / std::max(1., rate));
    mDrive = 1.;
    mBias = mDC = 0.;
    mMix = 1.;
    mOutput = 1.; mPostLow = 0.; mPostCut.Reset(rate);
  }
  void Process(double* samples, const double* dry, int frames, const UtilityControls& c, const EQControls& eq)
  {
    mPostCut.Target({c.extra.tubeHighCut}, uTubeHighCut);
    auto tone = c;
    tone.boost = 0.;
    tone.extra.boostMidGain = 0.;
    mTone.Process(samples, frames, tone, eq);
    const double drive = std::pow(10., std::clamp(c.tube[0], 0., 24.) * .05);
    const double bias = .35 * std::clamp(c.tube[1] * .01, 0., 1.);
    const double mix = std::clamp(c.tube[2] * .01, 0., 1.);
    const double output = std::pow(10., std::clamp(c.tube[3], -18., 18.) * .05);
    for (int i = 0; i < frames; ++i)
    {
      mDrive = drive + mSmooth * (mDrive - drive);
      mBias = bias + mSmooth * (mBias - bias);
      mMix = mix + mSmooth * (mMix - mix);
      mOutput = output + mSmooth * (mOutput - output);
      const double offset = mBiasOffset(mBias, [](double v) { return std::tanh(v); });
      const auto shape = [&](double x) {
        const double wet = mADAA ? mShaper.Tanh(x * mDrive + mBias) : std::tanh(x * mDrive + mBias);
        const double zero = mADAA ? mZero.Tanh(mBias) : offset;
        return (wet - zero) / (1. - offset * offset);
      };
      double shaped = mFactor == 2 ? mOversample2.Tick(samples[i], shape)
                    : mFactor == 4 ? mOversample4.Tick(samples[i], shape) : shape(samples[i]);
      const double cutoff = mPostCut.Tick()[0];
      const double pole = mPostPole(-6.283185307179586 * std::min(cutoff, .45 * mRate) / mRate, [](double x) { return std::exp(x); });
      mPostLow = shaped + pole * (mPostLow - shaped);
      if (cutoff < 20000.) shaped = mPostLow;
      mDC = shaped + mDCCoefficient * (mDC - shaped);
      if (std::abs(mDC) < 1.e-30)
        mDC = 0.;
      samples[i] = (dry[i] + mMix * (shaped - mDC - dry[i])) * mOutput;
    }
  }

private:
  grid_adaa::Average mShaper, mZero;
  grid_oversampling::FIR<2, 32> mOversample2;
  grid_oversampling::FIR<4, 32> mOversample4;
  grid_fx::Memo mBiasOffset;
  int mFactor = 4;
  bool mADAA = false;
  grid_fx::SmoothControls<1> mPostCut;
  grid_fx::Memo mPostPole;
  double mRate = 48000., mPostLow = 0.;
  SlotBoost mTone;
  double mSmooth = 0., mDCCoefficient = 0., mDrive = 1., mBias = 0., mDC = 0., mMix = 1., mOutput = 1.;
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
    mWeights = {1., 0., 0.}; mFirstMode = true;
    mCount = 0;
    mFilter.Reset();
    mHighPass.Reset(); mBandPass.Reset();
    Update();
  }
  void Process(double* samples, int frames, const UtilityControls& c)
  {
    const int mode = std::clamp(c.filterMode, 0, 2);
    if (mFirstMode && frames > 0)
    {
      mWeights = {}; mWeights[mode] = 1.; mFirstMode = false;
    }
    for (int i = 0; i < frames; ++i)
    {
      const double peak = std::abs(samples[i]);
      mEnv = peak + (peak > mEnv ? mAttack : mRelease) * (mEnv - peak);
      if (std::abs(mEnv) < 1.e-30) mEnv = 0.;
      mDepth = std::clamp(c.envelope, 0., 4.) + mSmooth * (mDepth - std::clamp(c.envelope, 0., 4.));
      const double hz =
        std::min(.4 * mRate, std::clamp(c.cutoff, 40., 12000.) * std::exp2(mDepth * std::min(1., mEnv * 4.)));
      mCut = hz + mSmooth * (mCut - hz);
      mQ = std::clamp(c.resonance, .5, 4.) + mSmooth * (mQ - std::clamp(c.resonance, .5, 4.));
      if (mCount++ % 16 == 0)
        Update();
      mCount %= 16;
      const double low = mFilter.Tick(samples[i]), high = mHighPass.Tick(samples[i]), band = mBandPass.Tick(samples[i]);
      for (int j = 0; j < 3; ++j)
      {
        const double target = j == mode ? 1. : 0.;
        mWeights[j] = std::abs(mWeights[j] - target) < 1.e-12 ? target : target + mSmooth * (mWeights[j] - target);
      }
      samples[i] = low + mWeights[1] * (high - low) + mWeights[2] * (band - low);
    }
  }
  double AppliedCutoff() const { return mAppliedCut; }
  double AppliedResonance() const { return mAppliedQ; }

private:
  void Update()
  {
    mAppliedCut = std::min(.4 * mRate, mCut); mAppliedQ = mQ;
    const recursive_linear_filter::BiquadParams params(mRate, mAppliedCut, mAppliedQ, 0.);
    mFilter.SetParams(params); mHighPass.SetParams(params); mBandPass.SetParams(params);
  }
  ScalarFilter<CutCoefficients<false>> mFilter;
  ScalarFilter<CutCoefficients<true>> mHighPass;
  ScalarFilter<BandPassCoefficients> mBandPass;
  std::array<double, 3> mWeights{1., 0., 0.};
  bool mFirstMode = true;
  double mRate = 48000., mSmooth = 0., mAttack = 0., mRelease = 0., mEnv = 0., mCut = 2500., mQ = .7, mDepth = 0.;
  int mCount = 0;
  double mAppliedCut = 2500., mAppliedQ = .7;
};

class SlotAutoSwell
{
public:
  void Reset(double rate)
  {
    mRate = rate;
    mEnv = mGain = mHold = 0.; mExtras.Reset(rate);
    mOpen = false;
    mAttack = std::exp(-1. / (.001 * rate));
    mRelease = std::exp(-1. / (.03 * rate));
    mSmoothing = std::exp(-1. / (.02 * rate));
    mFirst = true;
  }
  void Process(double* samples, int frames, const UtilityControls& c)
  {
    mExtras.Target({c.extra.swellCurve, c.extra.swellHold, c.extra.swellMix}, std::array<int, 3>{uSwellCurve, uSwellHold, uSwellMix});
    // Smooth the derived threshold/rates so automation has no block-sized steps.
    // Exponentials stay outside the sample loop and the envelope stays per sample.
    const std::array<double, 3> target{
      std::pow(10., std::clamp(c.threshold, -72., -12.) / 20.),
      1. / (mRate * .001 * std::clamp(c.rise, 10., 2000.)),
      std::exp(-1. / (mRate * .001 * std::clamp(c.release, 20., 800.)))};
    if (mFirst) { mValue = target; mFirst = false; }
    for (int i = 0; i < frames; ++i)
    {
      for (int j = 0; j < 3; ++j)
        mValue[j] = target[j] + mSmoothing * (mValue[j] - target[j]);
      const auto& extra = mExtras.Tick();
      const double peak = std::abs(samples[i]);
      mEnv = peak + (peak > mEnv ? mAttack : mRelease) * (mEnv - peak);
      if (std::abs(mEnv) < 1.e-30) mEnv = 0.;
      if (mEnv >= mValue[0])
      {
        mOpen = true;
        mHold = extra[1] * .001 * mRate;
      }
      else if (mEnv < mValue[0] * .707 && --mHold <= 0.)
      {
        mOpen = false;
        mHold = 0.;
      }
      mGain = mOpen ? std::min(1., mGain + mValue[1]) : mGain * mValue[2];
      if (mGain < 1.e-30)
        mGain = 0.;
      const double shaped = extra[0] == 1. ? mGain : std::pow(mGain, extra[0]);
      samples[i] *= extra[2] == 100. ? shaped : 1. + extra[2] * .01 * (shaped - 1.);
    }
  }

private:
  grid_fx::SmoothControls<3> mExtras;
  double mRate = 48000., mEnv = 0., mGain = 0., mHold = 0.;
  std::array<double, 3> mValue{};
  double mAttack = 0., mRelease = 0., mSmoothing = 0.;
  bool mFirst = true;
  bool mOpen = false;
};
