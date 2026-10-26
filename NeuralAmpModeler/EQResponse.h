#pragma once
#include "SlotUtility.h"
#include <complex>

// Control-thread response model using the processors' actual coefficient classes.
// Shows settled settings, including the DSP frequency clamps and cut bypasses.
namespace grid_eq_response
{
template <class Filter>
class Biquad : public Filter
{
public:
  std::complex<double> At(double hz, double rate) const
  {
    const auto z = std::polar(1., -2. * MATH_PI * hz / rate);
    return (this->mInputCoefficients[0] + z * this->mInputCoefficients[1] + z * z * this->mInputCoefficients[2])
           / (1. - z * this->mOutputCoefficients[1] - z * z * this->mOutputCoefficients[2]);
  }
};

class Boost
{
public:
  Boost(double rate, double gain, double tilt, double lowCut, double midGain = 0., double midFrequency = 800.) : mRate(rate),mGain(std::pow(10.,gain*.05)),mCut(lowCut>0.)
  {
    using Params=recursive_linear_filter::BiquadParams;
    mLow.SetParams(Params(rate,std::min(800.,.4*rate),.707,-tilt));
    mHigh.SetParams(Params(rate,std::min(800.,.4*rate),.707,tilt));
    mHP.SetParams(Params(rate,std::max(1.,lowCut),.7071067811865476,0.));
    mMid.SetParams(Params(rate,std::min(midFrequency,.4*rate),.7,midGain));
  }
  std::complex<double> At(double frequency) const
  { return mGain*mLow.At(frequency,mRate)*mHigh.At(frequency,mRate)*mMid.At(frequency,mRate)*(mCut ? mHP.At(frequency,mRate) : std::complex<double>(1.,0.)); }
private:
  double mRate,mGain;
  Biquad<recursive_linear_filter::Peaking> mMid;
  bool mCut;
  Biquad<recursive_linear_filter::LowShelf> mLow;
  Biquad<recursive_linear_filter::HighShelf> mHigh;
  Biquad<CutCoefficients<true>> mHP;
};

class Response
{
public:
  Response(double rate, SlotEffect effect, const EQControls& eq, const UtilityControls& utility)
  : mRate(std::max(1., rate)), mEffect(effect), mHPOn(eq.highPass > 0.), mLPOn(eq.lowPass < 20000.)
  {
    mOutput = std::pow(10., utility.extra.eqOutput * .05);
    using Params = recursive_linear_filter::BiquadParams;
    const auto frequency = [&](double hz) { return std::min(hz, .4 * mRate); };
    mBass.SetParams(Params(mRate, frequency(150.), .707, std::clamp(eq.bass, -12., 12.)));
    mMid.SetParams(Params(mRate, frequency(eq.midFrequency), .7, std::clamp(eq.mid, -12., 12.)));
    mTreble.SetParams(Params(mRate, frequency(effect == SlotEffect::ToneEQ ? 3000. : 3500.), .707,
                            effect == SlotEffect::ToneEQ ? std::clamp(eq.treble, -12., 12.)
                            : effect == SlotEffect::CutEQ ? std::clamp(eq.presence, -6., 6.) : 0.));
    const auto filterHz = std::max(.001, std::min(utility.cutoff, .4 * mRate));
    const auto filterQ = std::clamp(utility.resonance, .5, 4.);
    const bool parametric = effect == SlotEffect::ParametricEQ;
    const double lowQ = parametric ? std::clamp(utility.paramLowQ, .1, 4.) : .7071067811865476;
    const double highQ = parametric ? std::clamp(utility.paramHighQ, .1, 4.) : .7071067811865476;
    mLowShelfOn = parametric && utility.paramLowMode == 1;
    mLowShelf.SetParams(Params(mRate, std::min(std::max(1., std::clamp(eq.highPass, 0., 300.)), .4 * mRate),
                             lowQ, std::clamp(utility.paramShelfGain, -12., 12.)));
    mHP.SetParams(effect == SlotEffect::SynthFilter ? Params(mRate, filterHz, filterQ, 0.)
                   : Params(mRate, std::max(1., std::clamp(eq.highPass, 0., 300.)), lowQ, 0.));
    mLP.SetParams(effect == SlotEffect::SynthFilter
                   ? Params(mRate, filterHz, filterQ, 0.)
                   : Params(mRate, std::min(.49 * mRate, std::clamp(eq.lowPass, 2000., 20000.)), highQ, 0.));
    const auto center = frequency(std::clamp(utility.frequency, 20., 20000.));
    const auto q = std::clamp(utility.q, .1, 20.);
    mBell.SetParams(Params(mRate, center, q, std::clamp(utility.gain, -12., 12.)));
    mNotch.SetParams(Params(mRate, center, q, 0.));
    mNotchOn = utility.eqMode >= .5;
    mBandPass.SetParams(Params(mRate, filterHz, filterQ, 0.));
    mFilterMode = std::clamp(utility.filterMode, 0, 2);
  }
  double Magnitude(double hz) const
  {
    if (mEffect == SlotEffect::SynthFilter)
      return std::abs(mFilterMode == 1 ? mHP.At(hz, mRate) : mFilterMode == 2 ? mBandPass.At(hz, mRate) : mLP.At(hz, mRate));
    if (mEffect == SlotEffect::ToneEQ)
      return mOutput * std::abs(mBass.At(hz, mRate) * mMid.At(hz, mRate) * mTreble.At(hz, mRate));
    auto result = mTreble.At(hz, mRate);
    if (mHPOn) result *= mLowShelfOn ? mLowShelf.At(hz, mRate) : mHP.At(hz, mRate);
    if (mLPOn) result *= mLP.At(hz, mRate);
    if (mEffect == SlotEffect::ParametricEQ)
      result *= mNotchOn ? mNotch.At(hz, mRate) : mBell.At(hz, mRate);
    return (mEffect == SlotEffect::CutEQ ? mOutput : 1.) * std::abs(result);
  }
private:
  double mRate, mOutput = 1.;
  SlotEffect mEffect;
  bool mHPOn, mLPOn, mNotchOn, mLowShelfOn = false;
  int mFilterMode = 0;
  Biquad<recursive_linear_filter::LowShelf> mBass, mLowShelf;
  Biquad<recursive_linear_filter::Peaking> mMid, mBell;
  Biquad<recursive_linear_filter::HighShelf> mTreble;
  Biquad<CutCoefficients<true>> mHP;
  Biquad<CutCoefficients<false>> mLP;
  Biquad<NotchCoefficients> mNotch;
  Biquad<BandPassCoefficients> mBandPass;
};
}
