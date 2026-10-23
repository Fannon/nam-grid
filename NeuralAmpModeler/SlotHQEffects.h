#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include "SlotTimeEffects.h"

// Original high-quality grid effects. The algorithms are informed by the MIT
// Shimwash (shimmer reverb), Junologue Chorus (Juno BBD ensemble) and Ruminant
// (frequency-shifting delay) projects, but this is a rate-scaled, double
// precision implementation with nam-grid's control smoothing and lifecycle.
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
    mGainA = .70710678;
    mGainB = .70710678;
    mFirst = true;
  }
  void Process(double* l, double* r, int n, const UtilityControls& c)
  {
    mControls.Target(c.juno, uJunoMode);
    const double gainCoefficient = std::exp(-1. / (.01 * mRate));
    for (int i = 0; i < n; ++i)
    {
      const auto& v = mControls.Tick();
      const int mode = std::clamp(static_cast<int>(std::lround(v[0])), 0, 2);
      constexpr double modeGains[3][2]{{1., 0.}, {.70710678, .70710678}, {0., 1.}};
      if (mFirst)
      {
        mGainA = modeGains[mode][0];
        mGainB = modeGains[mode][1];
        mFirst = false;
      }
      mGainA = modeGains[mode][0] + gainCoefficient * (mGainA - modeGains[mode][0]);
      mGainB = modeGains[mode][1] + gainCoefficient * (mGainB - modeGains[mode][1]);
      const double brightness = v[2] * .01;
      const double preCutoff = std::min(.45 * mRate, 2000. + brightness * brightness * 18000.);
      const double postCutoff = std::min(.45 * mRate, 6000. + brightness * brightness * 14000.);
      const double preCoefficient = std::exp(-2. * grid_fx::pi * preCutoff / mRate);
      const double postCoefficient = std::exp(-2. * grid_fx::pi * postCutoff / mRate);
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
      mPost[0] = grid_fx::Flush(wetL + postCoefficient * (mPost[0] - wetL));
      mPost[1] = grid_fx::Flush(wetR + postCoefficient * (mPost[1] - wetR));
      wetL = mPost[0];
      wetR = mPost[1];
      const double mix = v[1] * .01, dryGain = std::sqrt(std::max(0., 1. - mix)), wetGain = std::sqrt(mix);
      l[i] = dryL * dryGain + wetL * wetGain;
      if (r)
        r[i] = dryR * dryGain + wetR * wetGain;
    }
  }

private:
  grid_fx::DelayLine mLine;
  grid_fx::SmoothControls<3> mControls;
  double mRate = 48000., mPre = 0., mPost[2]{}, mPhase[2]{}, mGainA = 1., mGainB = 0.;
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
  }
  void Process(double* l, double* r, int n, const UtilityControls& c, bool draining = false)
  {
    mControls.Target(c.shimmer, uShimmerMix);
    const double freezeTarget = (!draining && c.shimmer[6] >= .5) ? 1. : 0.;
    const double coefficient = std::exp(-2. * grid_fx::pi * 100. / mRate);
    for (int i = 0; i < n; ++i)
    {
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
      const double dryL = l[i], dryR = r ? r[i] : dryL;
      mInputLow[0] = grid_fx::Flush(dryL + coefficient * (mInputLow[0] - dryL));
      mInputLow[1] = grid_fx::Flush(dryR + coefficient * (mInputLow[1] - dryR));
      const double inputL = (.75 * (dryL - mInputLow[0]) + .25 * (dryR - mInputLow[1])) * (1. - mFreeze)
                            + mFeedback[0] * (draining ? 0. : 1.);
      const double inputR = (.25 * (dryL - mInputLow[0]) + .75 * (dryR - mInputLow[1])) * (1. - mFreeze)
                            + mFeedback[1] * (draining ? 0. : 1.);
      const double wetL = mTank[0].Process(inputL);
      const double wetR = mTank[1].Process(inputR);
      const double pitch = v[3] * mTank[0].live;
      const double shiftedL = mShifter.ProcessSample(wetL, 0, pitch);
      const double shiftedR = mShifter.ProcessSample(wetR, 1, pitch);
      const double amount = v[4] * .01 * .55;
      mFeedback[0] = std::clamp(amount * mTank[0].live * shiftedL, -4., 4.);
      mFeedback[1] = std::clamp(amount * mTank[1].live * shiftedR, -4., 4.);
      const double mix = v[0] * .01;
      l[i] = dryL + mix * (wetL - dryL);
      if (r)
        r[i] = dryR + mix * (wetR - dryR);
    }
  }

private:
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
  double mRate = 48000., mInputLow[2]{}, mFeedback[2]{}, mFreeze = 0.;
  int mCounter = 0;
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
      mHistory[ch].fill(0.);
      mIndex[ch] = 0;
      mLow[ch] = 0.;
    }
    mCosine = 1.;
    mSine = 0.;
    mCosOffset = 1.;
    mSinOffset = 0.;
    mCounter = 0;
    for (double& value : mHilbert)
      value = 0.;
    for (int k = 1; k < 32; k += 2)
    {
      const double window = .54 - .46 * std::cos(2. * grid_fx::pi * (31 + k) / 62.);
      const double value = 2. * window / (grid_fx::pi * k);
      mHilbert[31 + k] = value;
      mHilbert[31 - k] = -value;
    }
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
      const double side = v[3] * .01, feedback = draining ? 0. : v[1] * .01;
      const double dryL = l[i], dryR = r ? r[i] : dryL;
      double wet[2]{};
      for (int ch = 0; ch < 2; ++ch)
      {
        const double dry = ch ? dryR : dryL;
        const double delayed = mLine[ch].Read(delaySamples);
        auto& history = mHistory[ch];
        const int index = mIndex[ch];
        history[index] = delayed;
        double quadrature = 0.;
        for (int j = 1; j < 63; j += 2)
          quadrature += mHilbert[j] * history[(index - j) & 63];
        const double inPhase = history[(index - 31) & 63];
        const double cosine = ch ? rightCosine : mCosine, sine = ch ? rightSine : mSine;
        const double upper = inPhase * cosine - quadrature * sine;
        const double lower = inPhase * cosine + quadrature * sine;
        const double weight = .5 * (side + 1.);
        double shifted = lower + (upper - lower) * weight;
        mLow[ch] = grid_fx::Flush(
          shifted + std::exp(-2. * grid_fx::pi * std::min(.45 * mRate, v[4]) / mRate) * (mLow[ch] - shifted));
        shifted = mLow[ch];
        mLine[ch].Push(dry + feedback * SoftLimit(shifted));
        wet[ch] = shifted;
        mIndex[ch] = (index + 1) & 63;
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
  std::array<std::array<double, 64>, 2> mHistory{};
  std::array<double, 63> mHilbert{};
  grid_fx::SmoothControls<7> mControls;
  double mRate = 48000., mCosine = 1., mSine = 0., mCosOffset = 1., mSinOffset = 0.;
  double mLow[2]{};
  int mIndex[2]{};
  int mCounter = 0;
};
} // namespace grid_hq
