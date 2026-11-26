#pragma once
#include "dsp/OversampledMix.h"
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
    mRate = std::isfinite(rate) ? std::clamp(rate, 1., 1'000'000.) : 48000.;
    mSmooth = std::exp(-1. / (.005 * mRate));
    mGain = 1.;
    mTilt = 0.;
    mCount = 0;
    mLow.Reset();
    mHigh.Reset();
    mMid.Reset(); mMidGain = 0.;
    Update();
    mCoefficientsDirty = false;
    mCut.Reset(mRate);
    mCut.Process(nullptr, 0, true, {});
  }
  void Process(double* samples, int frames, const UtilityControls& c, const EQControls& eq)
  {
    EQControls cuts;
    cuts.highPass = eq.highPass;
    mCut.Process(samples, frames, true, cuts);
    const double gain = std::pow(10., (std::isfinite(c.boost) ? std::clamp(c.boost, 0., 18.) : 0.) * .05);
    const double tilt = std::isfinite(c.tilt) ? std::clamp(c.tilt, -6., 6.) : 0.;
    const double midGain = std::isfinite(c.extra.boostMidGain) ? std::clamp(c.extra.boostMidGain, -12., 12.) : 0.;
    for (int i = 0; i < frames; ++i)
    {
      mGain = gain + mSmooth * (mGain - gain);
      const double previousTilt = mTilt, previousMidGain = mMidGain;
      mTilt = tilt + mSmooth * (mTilt - tilt);
      mMidGain = midGain + mSmooth * (mMidGain - midGain);
      mCoefficientsDirty |= mTilt != previousTilt || mMidGain != previousMidGain;
      if (mCount++ % 16 == 0 && mCoefficientsDirty)
      {
        Update();
        mCoefficientsDirty = false;
      }
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
  bool mCoefficientsDirty = false;
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
  // The explicit factor is an offline comparison hook for the aliasing tests,
  // not a saved control; production always uses FactorForRate.
  void Reset(double rate, int factor = 0)
  {
    mRate = std::max(1., rate);
    mFactor = factor == 1 || factor == 2 || factor == 4 ? factor : FactorForRate(rate);
    if (mFactor == 2)
    {
      mOversample4.reset(); mDry4.reset();
      if (!mOversample2) mOversample2 = std::make_unique<grid_oversampling::FIR<2, 32>>();
      if (!mDry2) mDry2 = std::make_unique<grid_oversampling::LinearReference<2>>();
      mOversample2->Reset(); mDry2->Reset();
    }
    else if (mFactor == 4)
    {
      mOversample2.reset(); mDry2.reset();
      if (!mOversample4) mOversample4 = std::make_unique<grid_oversampling::FIR<4, 32>>();
      if (!mDry4) mDry4 = std::make_unique<grid_oversampling::LinearReference<4>>();
      mOversample4->Reset(); mDry4->Reset();
    }
    else
    {
      mOversample2.reset(); mOversample4.reset();
      mDry2.reset(); mDry4.reset();
    }
    mMixBypass.Reset(mRate);
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
    mMixBypass.Target(mix > 0.);
    for (int i = 0; i < frames; ++i)
    {
      mDrive = drive + mSmooth * (mDrive - drive);
      mBias = bias + mSmooth * (mBias - bias);
      mMix = mix + mSmooth * (mMix - mix);
      mOutput = output + mSmooth * (mOutput - output);
      const double offset = mBiasOffset(mBias, [](double v) { return std::tanh(v); });
      const auto shape = [&](double x) { return (std::tanh(x * mDrive + mBias) - offset) / (1. - offset * offset); };
      double shaped = mFactor == 2 ? mOversample2->Tick(samples[i], shape)
                    : mFactor == 4 ? mOversample4->Tick(samples[i], shape) : shape(samples[i]);
      const double cutoff = mPostCut.Tick()[0];
      const double pole = mPostPole(-6.283185307179586 * std::min(cutoff, .45 * mRate) / mRate, [](double x) { return std::exp(x); });
      mPostLow = grid_fx::Flush(shaped + pole * (mPostLow - shaped));
      const double cutBlend = std::clamp((20000. - cutoff) / 1000., 0., 1.);
      shaped += cutBlend * (mPostLow - shaped);
      mDC = shaped + mDCCoefficient * (mDC - shaped);
      if (std::abs(mDC) < 1.e-30)
        mDC = 0.;
      double blended = dry[i] + mMix * (shaped - mDC - dry[i]);
      if (mFactor > 1)
      {
        if (mFactor == 2) mDry2->Push(dry[i]); else mDry4->Push(dry[i]);
        mMixBypass.Tick();
        if (mMix != 1. && !mMixBypass.IsDry())
        {
          const double alignedDry = mFactor == 2 ? mDry2->Filtered() : mDry4->Filtered();
          blended = alignedDry + mMix * (shaped - mDC - alignedDry);
        }
        blended = mMixBypass.Apply(dry[i], blended);
      }
      samples[i] = blended * mOutput;
    }
  }

private:
  std::unique_ptr<grid_oversampling::FIR<2, 32>> mOversample2;
  std::unique_ptr<grid_oversampling::FIR<4, 32>> mOversample4;
  std::unique_ptr<grid_oversampling::LinearReference<2>> mDry2;
  std::unique_ptr<grid_oversampling::LinearReference<4>> mDry4;
  grid_oversampling::MixBypass mMixBypass;
  grid_fx::Memo mBiasOffset;
  int mFactor = 4;
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
    mRate = std::isfinite(rate) && rate > 0. ? std::clamp(rate, 1., 1'000'000.) : 48000.;
    mSmooth = std::exp(-1. / (.005 * mRate));
    mAttack = std::exp(-1. / (.003 * mRate));
    mRelease = std::exp(-1. / (.08 * mRate)); mDecayMs = 80.;
    mEnv = 0.;
    mOutput = mMix = 1.; mFirstLevel = true;
    mCut = 2500.;
    mQ = .7;
    mDepth = 0.;
    mWeights = {1., 0., 0.}; mFirstMode = true;
    mCount = 0;
    mLastSensParam = mLastOutParam = std::numeric_limits<double>::quiet_NaN();
    mSweep = {};
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
    // Sensitivity scales the detector only; 0 dB keeps the released response, which
    // sweeps fully from -12 dBFS peaks. Decay is the release time constant.
    const double decay = std::clamp(c.extra.filterDecay, 20., 2000.);
    if (decay != mDecayMs) { mDecayMs = decay; mRelease = std::exp(-1. / (.001 * decay * mRate)); }
    if (c.extra.filterSensitivity != mLastSensParam)
    {
      mLastSensParam = c.extra.filterSensitivity;
      mSensitivity = 4. * std::pow(10., std::clamp(c.extra.filterSensitivity, -24., 24.) / 20.);
    }
    const double sensitivity = mSensitivity;
    const double direction = c.extra.filterDown >= .5 ? -1. : 1.;
    if (c.extra.filterOutput != mLastOutParam)
    {
      mLastOutParam = c.extra.filterOutput;
      mOutputTarget = std::pow(10., std::clamp(c.extra.filterOutput, -24., 24.) / 20.);
    }
    const double output = mOutputTarget;
    const double mix = std::clamp(c.extra.filterMix, 0., 100.) * .01;
    if (mFirstLevel) { mOutput = output; mMix = mix; mFirstLevel = false; }
    for (int i = 0; i < frames; ++i)
    {
      const double dry = samples[i], peak = std::abs(dry);
      mEnv = peak + (peak > mEnv ? mAttack : mRelease) * (mEnv - peak);
      if (std::abs(mEnv) < 1.e-30) mEnv = 0.;
      const double depth = direction * std::clamp(c.envelope, 0., 4.);
      mDepth = depth + mSmooth * (mDepth - depth);
      const double maxCutoff = .4 * mRate;
      const double mod = mDepth * std::min(1., mEnv * sensitivity);
      const double factor = mod == 0. ? 1. : mSweep(mod, [](double x) { return std::exp2(x); });
      const double hz = std::clamp(std::clamp(c.cutoff, 40., 12000.) * factor,
                                   std::min(20., maxCutoff), maxCutoff);
      mCut = hz + mSmooth * (mCut - hz);
      mQ = std::clamp(c.resonance, .5, 4.) + mSmooth * (mQ - std::clamp(c.resonance, .5, 4.));
      if (mCount++ % 16 == 0 && (std::min(.4 * mRate, mCut) != mAppliedCut || mQ != mAppliedQ))
        Update();
      mCount %= 16;
      const double low = mFilter.Tick(samples[i]), high = mHighPass.Tick(samples[i]), band = mBandPass.Tick(samples[i]);
      for (int j = 0; j < 3; ++j)
      {
        const double target = j == mode ? 1. : 0.;
        mWeights[j] = std::abs(mWeights[j] - target) < 1.e-12 ? target : target + mSmooth * (mWeights[j] - target);
      }
      // Band-pass keeps a unity-gain peak up to Q 1 and gains Q above it, like an SVF's
      // band output, so resonant settings are not far quieter than the other modes.
      const double wet = low + mWeights[1] * (high - low) + mWeights[2] * (band * std::max(1., mAppliedQ) - low);
      mOutput = output + mSmooth * (mOutput - output);
      mMix = mix + mSmooth * (mMix - mix);
      samples[i] = (mMix == 1. ? wet : dry + mMix * (wet - dry)) * mOutput;
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
  double mDecayMs = 80., mOutput = 1., mMix = 1.;
  double mLastSensParam = std::numeric_limits<double>::quiet_NaN(), mSensitivity = 4.;
  double mLastOutParam = std::numeric_limits<double>::quiet_NaN(), mOutputTarget = 1.;
  grid_fx::Memo mSweep;
  bool mFirstLevel = true;
  int mCount = 0;
  double mAppliedCut = 2500., mAppliedQ = .7;
};

// A volume pedal: Position (or the slot's MIDI CC) sweeps the gain from Heel level to Toe
// level along a power curve in amplitude. Curve 0 is an audio taper (position squared, so
// -12 dB at half travel); -100 rises fast (square root) and +100 holds back (8th power).
// Heel level -60 dB mutes. Smoothing hides the 128 steps of a MIDI pedal.
class SlotVolumePedal
{
public:
  void Reset(double rate)
  {
    mRate = std::isfinite(rate) ? std::clamp(rate, 1., 1'000'000.) : 48000.;
    mFirst = true; mSmoothingMs = -1.; mPosition = 1.;
  }
  static double Gain(double position, const std::array<double, 6>& v)
  {
    const double k = std::exp2(1. + 2. * std::clamp(v[1], -100., 100.) * .01);
    const double heel = v[2] <= -60. ? 0. : std::pow(10., std::clamp(v[2], -60., 0.) / 20.);
    const double toe = std::pow(10., std::clamp(v[3], -24., 12.) / 20.);
    return heel + (toe - heel) * std::pow(std::clamp(position, 0., 1.), k);
  }
  void Process(double* x, int frames, const UtilityControls& c)
  {
    const auto& v = c.extra.volumePedal;
    mPosition = c.midiPedal >= 0. ? std::min(1., c.midiPedal) : std::clamp(v[0], 0., 100.) * .01;
    const double target = Gain(mPosition, v);
    if (v[4] != mSmoothingMs)
    {
      mSmoothingMs = v[4];
      mSmooth = std::exp(-1. / (.001 * std::clamp(v[4], 1., 500.) * mRate));
    }
    if (mFirst) { mGain = target; mFirst = false; }
    for (int i = 0; i < frames; ++i)
    {
      mGain = target + mSmooth * (mGain - target);
      if (std::abs(mGain - target) < 1.e-9) mGain = target;
      x[i] *= mGain;
    }
  }
  double Position() const { return mPosition; }
  double CurrentGain() const { return mGain; }

private:
  double mRate = 48000., mSmooth = 0., mSmoothingMs = -1., mGain = 1., mPosition = 1.;
  bool mFirst = true;
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
    mLastThresholdDB = std::numeric_limits<double>::quiet_NaN();
    mLastRiseMs = mLastReleaseMs = -1.;
    mValueMoving = true;
    mLastGain = mLastCurve = -1.;
    mLastShaped = 0.;
    mTarget = {};
  }
  void Process(double* samples, int frames, const UtilityControls& c, double* right = nullptr)
  {
    mExtras.Target({c.extra.swellCurve, c.extra.swellHold, c.extra.swellMix}, std::array<int, 3>{uSwellCurve, uSwellHold, uSwellMix});
    // Smooth the derived threshold/rates so automation has no block-sized steps.
    // Exponentials stay outside the sample loop and the envelope stays per sample.
    const double thresh = std::clamp(c.threshold, -72., -12.);
    const double rise = std::clamp(c.rise, 10., 2000.);
    const double release = std::clamp(c.release, 20., 800.);
    if (thresh != mLastThresholdDB) { mLastThresholdDB = thresh; mTarget[0] = std::pow(10., thresh / 20.); mValueMoving = true; }
    if (rise != mLastRiseMs) { mLastRiseMs = rise; mTarget[1] = 1. / (mRate * .001 * rise); mValueMoving = true; }
    if (release != mLastReleaseMs) { mLastReleaseMs = release; mTarget[2] = std::exp(-1. / (mRate * .001 * release)); mValueMoving = true; }
    if (mFirst) { mValue = mTarget; mFirst = false; mValueMoving = false; }
    for (int i = 0; i < frames; ++i)
    {
      if (mValueMoving)
      {
        bool moving = false;
        for (int j = 0; j < 3; ++j)
        {
          const double next = mTarget[j] + mSmoothing * (mValue[j] - mTarget[j]);
          moving |= next != mValue[j];
          mValue[j] = next;
        }
        mValueMoving = moving;
      }
      const auto& extra = mExtras.Tick();
      const double peak = right ? std::max(std::abs(samples[i]), std::abs(right[i])) : std::abs(samples[i]);
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
      double shaped = mGain;
      if (extra[0] != 1. && mGain != 0. && mGain != 1.)
      {
        if (mGain != mLastGain || extra[0] != mLastCurve)
        {
          mLastGain = mGain;
          mLastCurve = extra[0];
          mLastShaped = std::pow(mGain, extra[0]);
        }
        shaped = mLastShaped;
      }
      const double applied = extra[2] == 100. ? shaped : 1. + extra[2] * .01 * (shaped - 1.);
      samples[i] *= applied;
      if (right) right[i] *= applied;
    }
  }

private:
  grid_fx::SmoothControls<3> mExtras;
  double mRate = 48000., mEnv = 0., mGain = 0., mHold = 0.;
  std::array<double, 3> mValue{}, mTarget{};
  double mAttack = 0., mRelease = 0., mSmoothing = 0.;
  double mLastThresholdDB = std::numeric_limits<double>::quiet_NaN(), mLastRiseMs = -1., mLastReleaseMs = -1.;
  double mLastGain = -1., mLastCurve = -1., mLastShaped = 0.;
  bool mFirst = true, mValueMoving = true;
  bool mOpen = false;
};
