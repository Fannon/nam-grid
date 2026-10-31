#pragma once
#include "SlotTimeEffects.h"

// Original MIT NAM Grid design using the general differential-envelope approach to
// transient shaping, not SPL circuitry or code. A stereo-linked 3 ms power average
// feeds a peak envelope. Two followers derived from it are compared in dB, so the
// action does not depend on pickup level and a steady note gives no difference:
//  - attack: the envelope against a copy that rises over 15 ms and falls with it,
//    so the difference covers the first milliseconds of a note;
//  - sustain: a copy that holds each new note's peak and falls with Recovery against
//    the envelope, so the difference grows as a note decays below its peak.
// No lookahead, audio buffer, crossover, or compensation delay.
class SlotTransientShaper
{
public:
  void Reset(double rate)
  {
    mRate = std::max(1., rate); mControls.Reset(mRate);
    mPower = mEnvelope = mRise = mHold = mDB = mGainDB = mMaxReduction = 0.; mCounter = 0;
    mAverage = Pole(3.); mAttack = Pole(.5); mRelease = Pole(25.); mRiseTime = Pole(15.);
    mGainAttack = Pole(.3); mGainRelease = Pole(2.); mGainLag = Pole(1.);
  }
  void Process(double* l, double* r, int n, const UtilityControls& c, bool = false)
  {
    mControls.Target(c.transientShaper, uTransientShaperAttack); mMaxReduction = 0.;
    for (int i = 0; i < n; ++i)
    {
      const auto& v = mControls.Tick();
      if (mCounter++ % 16 == 0) mRecovery = Pole(v[2]);
      mCounter %= 16;
      const double dryL = l[i], dryR = r ? r[i] : dryL;
      const double power = std::max(dryL * dryL, dryR * dryR);
      mPower = Follow(mPower, power, mAverage);
      mEnvelope = Follow(mEnvelope, mPower, mPower > mEnvelope ? mAttack : mRelease);
      mRise = mEnvelope > mRise ? Follow(mRise, mEnvelope, mRiseTime) : mEnvelope;
      // A new pick (the envelope rising 3 dB faster than the slow copy) re-arms the hold, so
      // Sustain acts on the decay of this note rather than on the previous note's peak.
      mHold = mEnvelope > mHold || mEnvelope > 2. * mRise ? mEnvelope : Follow(mHold, mEnvelope, mRecovery);
      // Sensitivity 0..100% maps a 12 dB to a 1.5 dB envelope difference onto full action.
      const double span = 12. * std::exp2(-3. * v[3] * .01);
      // A 0.5 dB deadband ignores the envelope's ripple on held low notes.
      const double rise = 10. * std::log10((mEnvelope + kFloor) / (mRise + kFloor)) - .5;
      const double fall = 10. * std::log10((mHold + kFloor) / (mEnvelope + kFloor)) - .5;
      // Fade control action from -80 to -60 dBFS power. Silence stays exact and
      // faint noise is not lifted by the sustain control. The instantaneous power
      // opens it on a note's first sample, independent of the averaging.
      const double active = std::clamp((std::max(mHold, power) - 1.e-8) / 9.9e-7, 0., 1.);
      const double target = active * (v[0] * std::clamp(rise / span, 0., 1.) + v[1] * std::clamp(fall / span, 0., 1.));
      mDB = Follow(mDB, target, target > mDB ? mGainAttack : mGainRelease);
      mGainDB = Follow(mGainDB, mDB, mGainLag);
      const double gain = mGain(mGainDB * .05, [](double x) { return std::pow(10., x); });
      const double blend = v[5] * .01, output = mOutput(v[4] * .05, [](double x) { return std::pow(10., x); });
      mMaxReduction = std::max(mMaxReduction, std::max(0., -mGainDB) * blend);
      const double applied = (1. + blend * (gain - 1.)) * output;
      l[i] = dryL * applied; if (r) r[i] = dryR * applied;
    }
  }
  double Reduction() const { return mMaxReduction; }
private:
  static constexpr double kFloor = 1.e-30;
  double Pole(double ms) const { return std::exp(-1. / (.001 * ms * mRate)); }
  static double Follow(double current, double target, double pole)
  { return grid_fx::Flush(target + pole * (current - target)); }
  grid_fx::SmoothControls<6> mControls;
  grid_fx::Memo mGain, mOutput;
  double mRate = 48000., mPower = 0., mEnvelope = 0., mRise = 0., mHold = 0., mDB = 0., mGainDB = 0., mMaxReduction = 0.;
  double mAverage = 0., mAttack = 0., mRelease = 0., mRiseTime = 0., mRecovery = 0.;
  double mGainAttack = 0., mGainRelease = 0., mGainLag = 0.;
  int mCounter = 0;
};
