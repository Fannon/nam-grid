#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include "SlotTimeEffects.h"

// Original high-quality grid effects. The algorithms are informed by the MIT
// Shimwash (shimmer reverb), Junologue Chorus (Juno BBD ensemble) and Ruminant
// (frequency-shifting delay) projects, but this is a rate-scaled, double
// precision implementation with NAM Grid's control smoothing and lifecycle.
// Reference revisions consulted (MIT at each revision; see ThirdPartyNotices.txt):
//   twinpowers/schwung-shimwash             a27d7a215d9cb112d27c5eeb34d31c9e3304087f
//   charlesvestal/schwung-junologue-chorus  0b1ecd9d09daad91b5726899bd5e4cfe3f97cf06
//   mestela/schwung-ruminant                d6cdedcc1febc426bb3a06eeca74b768198f446c
namespace grid_hq
{
inline double SoftLimit(double x)
{
  const double x2 = x * x;
  return x * (27. + x2) / (27. + 9. * x2);
}
inline double Tri(double phase)
{ return phase < .5 ? phase * 2. : 2. - phase * 2.; }

// Vintage I+II: Juno-60 recordings measured at 9.75 Hz and 3.3-3.7 ms:
// https://github.com/pendragon-andyh/Juno60/blob/master/Chorus/README.md
// Sine-like fast LFO, with the second BBD no longer inverted:
// https://www.florian-anwander.de/roland_string_choruses/
// The pinned Junologue reference above instead blends I and II; retained as mode 1.
class JunoChorus
{
public:
  void Reset(double rate)
  {
    mRate = std::max(1., rate);
    mLine.Reset(static_cast<int>(mRate * .012) + 8);
    mControls.Reset(mRate);
    mPre = mPost[0] = mPost[1] = 0.;
    mPhase[0] = mPhase[1] = 0.;
    mVintagePhase = mVintageGain = 0.;
    mWidth.Reset(rate);
    mGainA = .70710678;
    mGainB = .70710678;
    mMixSmoothing = std::exp(-1. / (.02 * mRate));
    mFirst = true;
  }
  void Process(double* l, double* r, int n, const UtilityControls& c)
  {
    mControls.Target(c.juno, uJunoMode);
    mWidth.Target({c.extra.junoWidth}, uJunoWidth);
    const double gainCoefficient = std::exp(-1. / (.01 * mRate));
    // Smooth the equal-power gains themselves: sqrt of a smoothed mix rising from 0%
    // has an infinite initial slope, so the wet voice jumped in.
    const double mixTarget = std::clamp(c.juno[1], kEffectParameters[uJunoMix].minimum, kEffectParameters[uJunoMix].maximum) * .01;
    const double dryTarget = std::sqrt(std::max(0., 1. - mixTarget)), wetTarget = std::sqrt(mixTarget);
    for (int i = 0; i < n; ++i)
    {
      const auto& v = mControls.Tick();
      const int mode = std::clamp(static_cast<int>(std::lround(v[0])), 0, 3);
      constexpr double modeGains[4][2]{{1., 0.}, {.70710678, .70710678}, {0., 1.}, {0., 0.}};
      const double vintageTarget = mode == 3 ? 1. : 0.;
      if (mFirst)
      {
        mVintageGain = vintageTarget;
        mGainA = modeGains[mode][0];
        mGainB = modeGains[mode][1];
        mDryGain = dryTarget;
        mWetGain = wetTarget;
        mFirst = false;
      }
      mGainA = modeGains[mode][0] + gainCoefficient * (mGainA - modeGains[mode][0]);
      mGainB = modeGains[mode][1] + gainCoefficient * (mGainB - modeGains[mode][1]);
      mVintageGain = vintageTarget + gainCoefficient * (mVintageGain - vintageTarget);
      if (std::abs(mVintageGain - vintageTarget) < 1.e-12) mVintageGain = vintageTarget;
      const double brightness = v[2] * .01;
      const double preCutoff = std::min(.45 * mRate, 2000. + brightness * brightness * 18000.);
      const double postCutoff = std::min(.45 * mRate, 6000. + brightness * brightness * 14000.);
      const double preCoefficient =
        mPreCoefficient(-2. * grid_fx::pi * preCutoff / mRate, [](double x) { return std::exp(x); });
      const double postCoefficient =
        mPostCoefficient(-2. * grid_fx::pi * postCutoff / mRate, [](double x) { return std::exp(x); });
      const double dryL = l[i], dryR = r ? r[i] : dryL;
      const double mono = SoftLimit(.5 * (dryL + dryR));
      mPre = grid_fx::Flush(mono + preCoefficient * (mPre - mono));
      mLine.Push(mPre);
      mPhase[0] += .513 / mRate;
      mPhase[1] += .863 / mRate;
      if (mPhase[0] >= 1.)
        mPhase[0] -= 1.;
      if (mPhase[1] >= 1.)
        mPhase[1] -= 1.;
      constexpr double minimum = .00166, range = .00369;
      const double tap1L = mLine.Read(mRate * (minimum + range * Tri(mPhase[0])));
      const double tap1R = mLine.Read(mRate * (minimum + range * (1. - Tri(mPhase[0]))));
      const double tap2L = mLine.Read(mRate * (minimum + range * Tri(mPhase[1])));
      const double tap2R = mLine.Read(mRate * (minimum + range * (1. - Tri(mPhase[1]))));
      double wetL = tap1L * mGainA + tap2L * mGainB;
      double wetR = tap1R * mGainA + tap2R * mGainB;
      // Crossfade a separate tap; changing mode never jumps a running delay.
      // Skip the extra oscillator/read entirely for rigs using the original modes.
      if (mVintageGain != 0.)
      {
        mVintagePhase += 9.75 / mRate;
        if (mVintagePhase >= 1.) mVintagePhase -= 1.;
        const double delay = .0035 + .0002 * std::sin(2. * grid_fx::pi * mVintagePhase);
        const double vintage = mLine.Read(mRate * delay) * mVintageGain;
        wetL += vintage;
        wetR += vintage;
      }
      mPost[0] = grid_fx::Flush(wetL + postCoefficient * (mPost[0] - wetL));
      mPost[1] = grid_fx::Flush(wetR + postCoefficient * (mPost[1] - wetR));
      wetL = mPost[0];
      wetR = mPost[1];
      const double width = mWidth.Tick()[0] * .01;
      if (width != 1.)
      {
        const double mid = .5 * (wetL + wetR), side = .5 * (wetL - wetR) * width;
        wetL = mid + side; wetR = mid - side;
      }
      // Snap the last rounding step so Mix 0% stays exactly dry after automation.
      const auto approach = [&](double gain, double target) {
        const double next = target + mMixSmoothing * (gain - target);
        return std::abs(next - target) < 1.e-12 ? target : next;
      };
      mDryGain = approach(mDryGain, dryTarget);
      mWetGain = approach(mWetGain, wetTarget);
      l[i] = dryL * mDryGain + wetL * mWetGain;
      if (r)
        r[i] = dryR * mDryGain + wetR * mWetGain;
    }
  }

private:
  grid_fx::DelayLine mLine;
  grid_fx::SmoothControls<3> mControls;
  grid_fx::SmoothControls<1> mWidth;
  grid_fx::Memo mPreCoefficient, mPostCoefficient;
  double mRate = 48000., mPre = 0., mPost[2]{}, mPhase[2]{}, mGainA = 1., mGainB = 0.;
  double mDryGain = .70710678, mWetGain = .70710678, mMixSmoothing = 0.;
  double mVintagePhase = 0., mVintageGain = 0.;
  bool mFirst = true;
};

class PitchShifter
{
public:
  void Reset(double rate, double windowSeconds = .05, double rightOffset = .37)
  {
    mRate = std::max(1., rate);
    mWindow = std::max(64, static_cast<int>(mRate * windowSeconds));
    for (auto& line : mLine)
      line.Reset(mWindow + 8);
    mPhase[0] = 0.;
    mPhase[1] = rightOffset - std::floor(rightOffset);
    mRatio = 1.;
    mSmoothing = std::exp(-1. / (.01 * mRate));
  }
  double ProcessSample(double input, int channel, double semitones)
  {
    const int ch = channel != 0 ? 1 : 0;
    const double target = mTarget[ch](semitones / 12., [](double x) { return std::exp2(x); });
    mRatio = target + mSmoothing * (mRatio - target);
    mLine[ch].Push(input);
    if (std::abs(mRatio - 1.) < 1.e-5)
      return input;
    mPhase[ch] += (1. - mRatio) / mWindow;
    if (mPhase[ch] < 0.)
      mPhase[ch] += 1.;
    if (mPhase[ch] >= 1.)
      mPhase[ch] -= 1.;
    const double phase = mPhase[ch];
    const double weight = .5 - .5 * std::cos(2. * grid_fx::pi * phase);
    double second = phase + .5;
    if (second >= 1.)
      second -= 1.;
    return weight * mLine[ch].Read(phase * mWindow) + (1. - weight) * mLine[ch].Read(second * mWindow);
  }

private:
  std::array<grid_fx::DelayLine, 2> mLine;
  std::array<grid_fx::Memo, 2> mTarget;
  double mRate = 48000., mRatio = 1., mSmoothing = 0., mPhase[2]{};
  int mWindow = 2048;
};

class Shimmer
{
public:
  void Reset(double rate)
  {
    mMixLaw.Reset(rate);
    mRate = std::max(1., rate);
    mControls.Reset(mRate);
    mTank[0].Reset(mRate, 1.);
    mTank[1].Reset(mRate, 1.037);
    mShifter.Reset(mRate, .05, .37);
    for (int ch = 0; ch < 2; ++ch)
    {
      mInputLow[ch] = 0.;
      mFeedback[ch] = 0.;
    }
    mFreeze = 0.;
    mCounter = 0;
    mDrain.Reset(mRate);
  }
  void Process(double* l, double* r, int n, const UtilityControls& c, bool draining = false)
  {
    mControls.Target(c.shimmer, uShimmerMix);
    const double freezeTarget = (!draining && c.shimmer[6] >= .5) ? 1. : 0.;
    const double coefficient = std::exp(-2. * grid_fx::pi * 100. / mRate);
    for (int i = 0; i < n; ++i)
    {
      mMixLaw.Tick(c.constantDry);
      const auto& v = mControls.Tick();
      mFreeze += std::clamp(freezeTarget - mFreeze, -1. / (.01 * mRate), 1. / (.01 * mRate));
      if (mCounter++ % 32 == 0)
      {
        const double live = 1. - mFreeze;
        for (int ch = 0; ch < 2; ++ch)
        {
          auto& tank = mTank[ch];
          for (int j = 0; j < 8; ++j)
          {
            const double normal = std::pow(10., -3. * tank.length[j] / std::max(.25, v[1]));
            tank.feedback[j] = std::min(.9995, normal + mFreeze * (.9995 - normal));
          }
          tank.damping = std::exp(-2. * grid_fx::pi * std::min(.45 * mRate, v[2] + mFreeze * (12000. - v[2])) / mRate);
          tank.modDepth = v[5] * .01 * .000625 * mRate;
          tank.live = live;
        }
      }
      mCounter %= 32;
      const double dryL = l[i], dryR = r ? r[i] : dryL, live = mDrain.Tick(draining);
      mInputLow[0] = grid_fx::Flush(dryL + coefficient * (mInputLow[0] - dryL));
      mInputLow[1] = grid_fx::Flush(dryR + coefficient * (mInputLow[1] - dryR));
      const double inputL = (.75 * (dryL - mInputLow[0]) + .25 * (dryR - mInputLow[1])) * (1. - mFreeze)
                            + mFeedback[0] * live;
      const double inputR = (.25 * (dryL - mInputLow[0]) + .75 * (dryR - mInputLow[1])) * (1. - mFreeze)
                            + mFeedback[1] * live;
      const double wetL = mTank[0].Process(inputL);
      const double wetR = mTank[1].Process(inputR);
      const double pitch = v[3] * mTank[0].live;
      const double shiftedL = mShifter.ProcessSample(wetL, 0, pitch);
      const double shiftedR = mShifter.ProcessSample(wetR, 1, pitch);
      const double amount = v[4] * .01 * .55;
      mFeedback[0] = std::clamp(amount * mTank[0].live * shiftedL, -4., 4.);
      mFeedback[1] = std::clamp(amount * mTank[1].live * shiftedR, -4., 4.);
      const double mix = v[0] * .01;
      l[i] = mMixLaw.Apply(dryL + mix * (wetL - dryL), dryL, mix, 1.);
      if (r)
        r[i] = mMixLaw.Apply(dryR + mix * (wetR - dryR), dryR, mix, 1.);
    }
  }

private:
  grid_fx::MixLaw mMixLaw;
  struct Tank
  {
    std::array<grid_fx::DelayLine, 8> line;
    std::array<grid_fx::DelayLine, 4> diffuser;
    std::array<double, 8> length{}, damp{}, feedback{}, phase{};
    std::array<double, 4> diffuserDelay{};
    double damping = 0., modDepth = 0., live = 1., mRate = 48000.;
    void Reset(double rate, double scale)
    {
      mRate = std::max(1., rate);
      constexpr std::array<double, 8> base{.0297, .0371, .0411, .0437, .0531, .0617, .0719, .0797};
      constexpr std::array<double, 4> diffusion{.0043, .0067, .0091, .0137};
      for (int j = 0; j < 8; ++j)
      {
        length[j] = base[j] * scale;
        line[j].Reset(static_cast<int>(mRate * (.09 * scale + .002)) + 8);
        damp[j] = 0.;
        phase[j] = j * .137;
      }
      for (int j = 0; j < 4; ++j)
      {
        diffuserDelay[j] = diffusion[j] * scale;
        diffuser[j].Reset(static_cast<int>(mRate * diffuserDelay[j]) + 8);
      }
    }
    double Process(double input)
    {
      for (int j = 0; j < 4; ++j)
      {
        const double delayed = diffuser[j].Read(mRate * diffuserDelay[j]);
        const double y = delayed - .6 * input;
        diffuser[j].Push(input + .6 * y);
        input = y;
      }
      std::array<double, 8> taps{};
      double wet = 0.;
      for (int j = 0; j < 8; ++j)
      {
        phase[j] += (.13 + .043 * j) / mRate;
        if (phase[j] >= 1.)
          phase[j] -= 1.;
        const double motion = modDepth * std::sin(2. * grid_fx::pi * phase[j]);
        const double tap = line[j].Read(mRate * length[j] + motion);
        damp[j] = grid_fx::Flush(tap + damping * (damp[j] - tap));
        taps[j] = damp[j] * feedback[j];
        wet += (j % 2 ? -1. : 1.) * tap * .125;
      }
      for (int span = 1; span < 8; span *= 2)
        for (int base = 0; base < 8; base += span * 2)
          for (int j = 0; j < span; ++j)
          {
            const double a = taps[base + j], b = taps[base + j + span];
            taps[base + j] = a + b;
            taps[base + j + span] = a - b;
          }
      for (int j = 0; j < 8; ++j)
        line[j].Push(taps[j] * .3535533905932738 + input * (j % 3 == 0 ? -.2 : .2));
      return wet * 1.8;
    }
  };
  Tank mTank[2];
  PitchShifter mShifter;
  grid_fx::SmoothControls<7> mControls;
  grid_fx::DrainRamp mDrain;
  double mRate = 48000., mInputLow[2]{}, mFeedback[2]{}, mFreeze = 0.;
  int mCounter = 0;
};

// Analytic-signal pair from two chains of second-order allpass sections, using
// Olli Niemitalo's published polyphase IIR Hilbert transformer coefficients
// (https://yehar.com/blog/?p=368). The quadrature output lags the in-phase output
// by 90 degrees within about 0.7 degrees from 30 Hz to 20 kHz at 44.1/48 kHz
// (60 Hz at 96 kHz), with unity magnitude and no lookahead.

class HilbertPair
{
public:
  void Reset() { mInPhase = {}; mQuadrature = {}; mDelayed = 0.; }
  // Returns {in-phase, quadrature}.
  std::array<double, 2> Process(double x)
  {
    const double quadrature = mDelayed;
    mDelayed = Chain(mQuadrature, quadratureCoefficients, x);
    return {Chain(mInPhase, inPhaseCoefficients, x), quadrature};
  }

private:
  struct Section { double x1 = 0., x2 = 0., y1 = 0., y2 = 0.; };
  using Sections = std::array<Section, 4>;
  static constexpr std::array<double, 4> inPhaseCoefficients{.4021921162426, .8561710882420, .9722909545651,
                                                             .9952884791278};
  static constexpr std::array<double, 4> quadratureCoefficients{.6923878, .9360654322959, .9882295226860,
                                                                .9987488452737};
  static double Chain(Sections& sections, const std::array<double, 4>& a, double x)
  {
    for (size_t j = 0; j < sections.size(); ++j)
    {
      auto& s = sections[j];
      const double y = grid_fx::Flush(a[j] * a[j] * (x + s.y2) - s.x2);
      s.x2 = s.x1; s.x1 = x;
      s.y2 = s.y1; s.y1 = y;
      x = y;
    }
    return x;
  }
  Sections mInPhase{}, mQuadrature{};
  double mDelayed = 0.;
};

class FrequencyShifter
{
public:
  void Reset(double rate)
  {
    mRate = std::max(1., rate);
    mControls.Reset(mRate);
    for (int ch = 0; ch < 2; ++ch)
    {
      mLine[ch].Reset(static_cast<int>(mRate * 1.05) + 8);
      mHilbert[ch].Reset();
      mLow[ch] = 0.;
    }
    mCosine = 1.;
    mSine = 0.;
    mCosOffset = 1.;
    mSinOffset = 0.;
    mCounter = 0;
    mDrain.Reset(mRate);
  }
  void Process(double* l, double* r, int n, const UtilityControls& c, bool draining = false)
  {
    mControls.Target(c.shift, uShiftFrequency);
    for (int i = 0; i < n; ++i)
    {
      const auto& v = mControls.Tick();
      if (mCounter++ % 32 == 0)
      {
        const double offset = std::clamp(v[5], 0., 100.) * .01 * grid_fx::pi;
        mCosOffset = std::cos(offset);
        mSinOffset = std::sin(offset);
      }
      mCounter %= 32;
      const double radians = 2. * grid_fx::pi * std::clamp(v[0], -2000., 2000.) / mRate;
      const double stepCos = std::cos(radians), stepSin = std::sin(radians);
      const double rightCosine = mCosine * mCosOffset - mSine * mSinOffset;
      const double rightSine = mSine * mCosOffset + mCosine * mSinOffset;
      const double delaySamples = std::max(1., v[2] * .001 * mRate);
      const double side = v[3] * .01, feedback = v[1] * .01 * mDrain.Tick(draining);
      const double dryL = l[i], dryR = r ? r[i] : dryL;
      const double lowPole = mLowPole(-2. * grid_fx::pi * std::min(.45 * mRate, v[4]) / mRate,
                                     [](double x) { return std::exp(x); });
      double wet[2]{};
      for (int ch = 0; ch < 2; ++ch)
      {
        const double dry = ch ? dryR : dryL;
        const auto [inPhase, quadrature] = mHilbert[ch].Process(mLine[ch].Read(delaySamples));
        const double cosine = ch ? rightCosine : mCosine, sine = ch ? rightSine : mSine;
        const double upper = inPhase * cosine - quadrature * sine;
        const double lower = inPhase * cosine + quadrature * sine;
        const double weight = .5 * (side + 1.);
        double shifted = lower + (upper - lower) * weight;
        mLow[ch] = grid_fx::Flush(
          shifted + lowPole * (mLow[ch] - shifted));
        shifted = mLow[ch];
        mLine[ch].Push(dry + feedback * SoftLimit(shifted));
        wet[ch] = shifted;
      }
      const double mix = v[6] * .01;
      l[i] = dryL + mix * (wet[0] - dryL);
      if (r)
        r[i] = dryR + mix * (wet[1] - dryR);
      const double nextCosine = mCosine * stepCos - mSine * stepSin;
      mSine = mSine * stepCos + mCosine * stepSin;
      mCosine = nextCosine;
    }
  }

private:
  std::array<grid_fx::DelayLine, 2> mLine;
  std::array<HilbertPair, 2> mHilbert;
  grid_fx::SmoothControls<7> mControls;
  grid_fx::Memo mLowPole;
  grid_fx::DrainRamp mDrain;
  double mRate = 48000., mCosine = 1., mSine = 0., mCosOffset = 1., mSinOffset = 0.;
  double mLow[2]{};
  int mCounter = 0;
};
} // namespace grid_hq
