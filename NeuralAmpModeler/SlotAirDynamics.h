#pragma once
#include "SlotTimeEffects.h"
#include "dsp/airwindows/outboard/DigitalBlack.h"
#include "dsp/airwindows/outboard/CStripGate.h"

inline bool IsAirDynamics(SlotEffect effect)
{ return effect == SlotEffect::DigitalBlack; }

// Digital Black Gate: DigitalBlack's darkening closure or CStrip's re-arming gate.
class SlotAirDynamics
{
public:
  void Reset(double rate, SlotEffect effect)
  {
    mRate = std::max(1., rate);
    mEffect = effect;
    mControls.Reset(mRate);
    mBlack.Reset(mRate);
    mHysteresis.Reset(mRate);
    mCounter = 0;
    mMode = -1;
    mVoice = 1.;
    mMinimum = 1.;
    mLastThresholdDB = std::numeric_limits<double>::quiet_NaN();
    mLastHoldMs = -1.;
  }
  void Process(double* left, double* right, int frames, const UtilityControls& c)
  {
    mControls.Target(c.black, uBlackThreshold);
    const int mode = std::clamp(c.blackMode, 0, 1);
    if (mMode < 0)
      mMode = mode;
    mMinimum = 1.;
    for (int i = 0; i < frames; ++i)
    {
      const auto& v = mControls.Tick();
      const double step = 1. / (.01 * mRate);
      mVoice = mode == mMode ? std::min(1., mVoice + step) : std::max(0., mVoice - step);
      if (mVoice == 0. && mode != mMode)
      {
        mBlack.Reset(mRate);
        mHysteresis.Reset(mRate);
        mMode = mode;
        mCounter = 0;
        mLastThresholdDB = std::numeric_limits<double>::quiet_NaN();
        mLastHoldMs = -1.;
      }
      if (mCounter == 0)
      {
        if (v[0] != mLastThresholdDB)
        {
          mLastThresholdDB = v[0];
          const double threshold = std::pow(10., v[0] / 20.);
          mBlack.Set({std::pow(std::clamp(3. * (threshold - .00018), 0., 1.), .25), 1.});
          mHysteresis.Set(threshold);
        }
        if (c.blackHold != mLastHoldMs)
        {
          mLastHoldMs = c.blackHold;
          mBlack.SetHold(c.blackHold * .001);
          mHysteresis.SetHold(c.blackHold * .001);
        }
      }
      mCounter = (mCounter + 1) % 32;
      const double dryL = left[i], dryR = right ? right[i] : 0.;
      double wetL = dryL, wetR = dryR;
      if (right)
      {
        if (mMode == 0)
          mBlack.Tick(wetL, wetR);
        else
          mHysteresis.Tick(wetL, wetR);
      }
      else
      {
        if (mMode == 0)
          mBlack.TickMono(wetL);
        else
          mHysteresis.TickMono(wetL);
      }
      const double mix = v[1] * .01 * mVoice;
      const double blendedL = mix == 0. ? dryL : dryL + mix * (wetL - dryL);
      const double blendedR = right ? (mix == 0. ? dryR : dryR + mix * (wetR - dryR)) : 0.;
      // The rectified fill makes Opening alone an inaccurate gain tap.
      // Measure actual stereo amplitude attenuation before user Output.
      const double inPower = right ? dryL * dryL + dryR * dryR : dryL * dryL;
      double gain;
      if (mix == 0. || (blendedL == dryL && (!right || blendedR == dryR)))
        gain = 1.;
      else if (inPower <= 1.e-20)
      {
        const double opening = mMode == 0
          ? (right ? std::max(mBlack.Opening(0), mBlack.Opening(1)) : mBlack.Opening(0))
          : (right ? std::max(mHysteresis.Opening(0), mHysteresis.Opening(1)) : mHysteresis.Opening(0));
        gain = 1. + mix * (opening - 1.);
      }
      else if (blendedL == 0. && (!right || blendedR == 0.))
        gain = 0.;
      else if (!right)
        gain = std::abs(blendedL / dryL);
      else
        gain = std::sqrt((blendedL * blendedL + blendedR * blendedR) / inPower);
      mMinimum = std::min(mMinimum, std::clamp(gain, 0., 1.));
      const double output = mOutput(v[2] / 20., [](double x) { return std::pow(10., x); });
      left[i] = blendedL * output;
      if (right)
        right[i] = blendedR * output;
    }
  }
  double Reduction() const { return -20. * std::log10(std::max(1.e-6, mMinimum)); }

private:
  airwindows_outboard::DigitalBlack::Core mBlack;
  airwindows_outboard::CStripGate::Core mHysteresis;
  grid_fx::SmoothControls<3> mControls;
  grid_fx::Memo mOutput;
  SlotEffect mEffect = SlotEffect::DigitalBlack;
  double mRate = 48000., mVoice = 1., mMinimum = 1.;
  double mLastThresholdDB = std::numeric_limits<double>::quiet_NaN(), mLastHoldMs = -1.;
  int mCounter = 0, mMode = -1;
};
