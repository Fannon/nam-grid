#pragma once
#include "SlotTimeEffects.h"

// Original MIT NAM Grid implementation. A normalized TPT state-variable band-pass
// feeds both the detector and the subtractive bell. Modulating its contribution,
// rather than interpolating direct-form biquad coefficients, keeps the topology
// stable and the gain-zero path exactly dry. TPT SVF equations: Andrew Simper,
// https://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf (equations, no copied code).
class SlotDynamicEQ
{
public:
  void Reset(double rate)
  {
    mRate = std::max(1., rate); mControls.Reset(mRate);
    for (auto& s : mState) s = {};
    mPower = mReduction = mMaxReduction = 0.; mCounter = 0;
  }
  void Process(double* left, double* right, int frames, const UtilityControls& c, bool = false)
  {
    mControls.Target(c.dynamicEQ, uDynamicEQFrequency);
    mMaxReduction = 0.;
    for (int i = 0; i < frames; ++i)
    {
      const auto& v = mControls.Tick();
      if (mCounter++ % 16 == 0)
      {
        const double frequency = std::clamp(v[0], 20., .4 * mRate);
        const double g = std::tan(grid_fx::pi * frequency / mRate);
        mK = 1. / std::clamp(v[1], .2, 12.);
        // About two band periods: fast enough for pick bursts, slow enough that
        // low bands do not modulate their own cycles. 0.5 ms to 25 ms.
        mRMS = std::exp(-1. / (std::clamp(2. / frequency, .0005, .025) * mRate));
        mA = 1. / (1. + g * (g + mK)); mG = g;
        mAttack = std::exp(-1. / (.001 * v[4] * mRate));
        mRelease = std::exp(-1. / (.001 * v[5] * mRate));
      }
      mCounter %= 16;
      const double dryL = left[i], dryR = right ? right[i] : dryL;
      const double bandL = Band(dryL, mState[0]);
      const double bandR = right ? Band(dryR, mState[1]) : bandL;
      // Max power never cancels anti-phase stereo. Peak-calibrated RMS on a sine.
      const double power = 2. * std::max(bandL * bandL, bandR * bandR);
      mPower = grid_fx::Flush(power + mRMS * (mPower - power));
      const double over = 10. * std::log10(std::max(mPower, 1.e-18)) - v[2];
      const double target = std::clamp(.75 * std::max(0., over), 0., v[3]); // 4:1, bounded range.
      const double coefficient = target > mReduction ? mAttack : mRelease;
      mReduction = grid_fx::Flush(target + coefficient * (mReduction - target));
      const double blend = v[7] * .01;
      mMaxReduction = std::max(mMaxReduction, mReduction * blend);
      const double amount = blend * (mGain(-mReduction * .05, [](double x) { return std::pow(10., x); }) - 1.);
      const double output = mOutput(v[6] * .05, [](double x) { return std::pow(10., x); });
      left[i] = (dryL + amount * bandL) * output;
      if (right) right[i] = (dryR + amount * bandR) * output;
    }
  }
  double Reduction() const { return mMaxReduction; }
private:
  struct State { double a = 0., b = 0.; };
  double Band(double x, State& s) const
  {
    const double band = mA * (s.a + mG * (x - s.b));
    const double low = s.b + mG * band;
    s.a = grid_fx::Flush(2. * band - s.a); s.b = grid_fx::Flush(2. * low - s.b);
    return mK * band;
  }
  std::array<State, 2> mState{};
  grid_fx::SmoothControls<8> mControls;
  grid_fx::Memo mGain, mOutput;
  double mRate = 48000., mG = 0., mK = 1., mA = 1., mRMS = 0., mPower = 0.;
  double mAttack = 0., mRelease = 0., mReduction = 0., mMaxReduction = 0.;
  int mCounter = 0;
};
