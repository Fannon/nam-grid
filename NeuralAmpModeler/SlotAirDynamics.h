#pragma once
#include "SlotTimeEffects.h"
#include "dsp/airwindows/outboard/DigitalBlack.h"
#include "dsp/airwindows/outboard/CStripGate.h"
#include "dsp/airwindows/outboard/Pressure6.h"

inline bool IsAirDynamics(SlotEffect effect)
{ return effect == SlotEffect::DigitalBlack || effect == SlotEffect::Pressure; }

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
    mPressure.Reset(mRate);
    mCounter = 0;
    mMode = -1;
    mVoice = 1.;
    mMinimum = 1.;
  }
  void Process(double* left, double* right, int frames, const UtilityControls& c)
  {
    const bool gate = mEffect == SlotEffect::DigitalBlack;
    mControls.Target(gate ? c.black : c.pressure, gate ? uBlackThreshold : uPressureCompress);
    const int mode = std::clamp(c.blackMode, 0, 1);
    if (mMode < 0)
      mMode = mode;
    mMinimum = 1.;
    for (int i = 0; i < frames; ++i)
    {
      const auto& v = mControls.Tick();
      if (gate)
      {
        const double step = 1. / (.01 * mRate);
        mVoice = mode == mMode ? std::min(1., mVoice + step) : std::max(0., mVoice - step);
        if (mVoice == 0. && mode != mMode)
        {
          mBlack.Reset(mRate);
          mHysteresis.Reset(mRate);
          mMode = mode;
          mCounter = 0;
        }
      }
      if (mCounter == 0)
      {
        if (gate)
        {
          const double threshold = std::pow(10., v[0] / 20.);
          mBlack.Set({std::pow(std::clamp(3. * (threshold - .00018), 0., 1.), .25), 1.});
          mHysteresis.Set(threshold);
        }
        else
          mPressure.Set({v[0] * .01, 1.}); // retain the native 0.9 nonlinear blend
      }
      mCounter = (mCounter + 1) % 32;
      const double dryL = left[i], dryR = right ? right[i] : dryL;
      double wetL = dryL, wetR = dryR;
      if (gate)
      {
        if (mMode == 0)
          mBlack.Tick(wetL, wetR);
        else
          mHysteresis.Tick(wetL, wetR);
      }
      else
        mPressure.Tick(wetL, wetR); // one shared max(L,R) detector
      const double mix = v[1] * .01 * (gate ? mVoice : 1.);
      const double blendedL = mix == 0. ? dryL : dryL + mix * (wetL - dryL);
      const double blendedR = mix == 0. ? dryR : dryR + mix * (wetR - dryR);
      if (gate)
      {
        // The rectified fill makes Opening alone an inaccurate gain tap.
        // Measure actual stereo amplitude attenuation before user Output.
        const double inPower = dryL * dryL + dryR * dryR;
        const double opening = mMode == 0 ? std::max(mBlack.Opening(0), mBlack.Opening(1))
                                          : std::max(mHysteresis.Opening(0), mHysteresis.Opening(1));
        const double gain = inPower > 1.e-20 ? std::sqrt((blendedL * blendedL + blendedR * blendedR) / inPower)
                                             : 1. + mix * (opening - 1.);
        mMinimum = std::min(mMinimum, std::clamp(gain, 0., 1.));
      }
      else
        mMinimum = std::min(mMinimum, 1. + mix * (mPressure.Gain() - 1.));
      const double output = std::pow(10., v[2] / 20.);
      left[i] = blendedL * output;
      if (right)
        right[i] = blendedR * output;
    }
  }
  double Reduction() const { return -20. * std::log10(std::max(1.e-6, mMinimum)); }

private:
  airwindows_outboard::DigitalBlack::Core mBlack;
  airwindows_outboard::CStripGate::Core mHysteresis;
  airwindows_outboard::Pressure6::Core mPressure;
  grid_fx::SmoothControls<3> mControls;
  SlotEffect mEffect = SlotEffect::DigitalBlack;
  double mRate = 48000., mVoice = 1., mMinimum = 1.;
  int mCounter = 0, mMode = -1;
};
