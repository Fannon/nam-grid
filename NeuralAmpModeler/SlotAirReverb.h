#pragma once
#include "SlotTimeEffects.h"
#include "dsp/airwindows/reverbs/kPlate140.h"
#include "dsp/airwindows/reverbs/kGuitarHall2.h"
#include "dsp/airwindows/reverbs/Galactic3.h"
#include <memory>

inline bool IsAirReverb(SlotEffect effect)
{ return effect == SlotEffect::AirPlate || effect == SlotEffect::AirHall || effect == SlotEffect::GalacticSpace; }

// Audition modules: retain Airwindows' native sustain/resolution controls rather
// than presenting an uncalibrated decay in seconds. Only the selected core is
// allocated, on the preparation thread. Core mixing is wet-only; the outer mix
// and output follow the same convention as the other grid effects.
class SlotAirReverb
{
public:
  void Reset(double rate, SlotEffect effect)
  {
    mMixLaw.Reset(rate);
    mRate = std::max(1., rate); mEffect = effect; mControls.Reset(mRate);
    mPlate.reset(); mHall.reset(); mSpace.reset();
    if (effect == SlotEffect::AirPlate)
    {
      mPlate = std::make_unique<airwindows_reverb::kPlate140::Core>(); mPlate->Reset(mRate);
      for (auto& delay : mPredelay) delay.Reset(static_cast<int>(mRate * .201));
    }
    if (effect == SlotEffect::AirHall)
    { mHall = std::make_unique<airwindows_reverb::kGuitarHall2::Core>(); mHall->Reset(mRate); }
    if (effect == SlotEffect::GalacticSpace)
    { mSpace = std::make_unique<airwindows_reverb::Galactic3::Core>(); mSpace->Reset(mRate); }
    mInputLow = {}; mCounter = 0; mOutput = 1.; mDrain.Reset(mRate); mLive = 1.;
  }
  void Process(double* left, double* right, int frames, const UtilityControls& controls, bool draining = false)
  {
    const auto& values = mEffect == SlotEffect::AirPlate ? controls.airPlate
                         : mEffect == SlotEffect::AirHall ? controls.airHall : controls.space;
    mControls.Target(values, mEffect == SlotEffect::AirPlate ? uAirPlateInput
                             : mEffect == SlotEffect::AirHall ? uAirHallSustain : uSpaceReplace);
    for (int i = 0; i < frames; ++i)
    {
      mMixLaw.Tick(controls.constantDry);
      const auto& v = mControls.Tick();
      mOutput = mOutputGain(v[6] / 20., [](double x) { return std::pow(10., x); });
      // Feedback fades over 5 ms when draining starts; follow it every sample while it moves.
      const double live = mDrain.Tick(draining);
      if (mCounter == 0 || live != mLive)
      {
        mLive = live;
        if (mPlate)
        {
          mPlate->Set({std::pow(10., v[0] / 20.), v[1] * .01 * live, v[2] * .01, 0., 1.});
          mLowCoefficient = std::exp(-2. * grid_fx::pi * std::max(1., v[5]) / mRate);
        }
        if (mHall) mHall->Set({v[0] * .01 * live, v[1] * .005, v[2] * .01, v[3] * .01, v[4] * .01, 1.});
        if (mSpace) mSpace->Set({live == 1. ? v[0] * .01 : 1. - live * (1. - v[0] * .01), v[1] * .01, v[2] * .01, v[4] * .01, v[3] * .01, 1.});
      }
      mCounter = (mCounter + 1) % 32;
      const double dryL = left[i], dryR = right ? right[i] : dryL;
      double wetL = dryL, wetR = dryR;
      if (mPlate)
      {
        double* wet[2]{&wetL, &wetR};
        for (int ch = 0; ch < 2; ++ch)
        {
          mInputLow[ch] = grid_fx::Flush(*wet[ch] + mLowCoefficient * (mInputLow[ch] - *wet[ch]));
          const double input = *wet[ch] - (v[5] > .01 ? mInputLow[ch] : 0.);
          *wet[ch] = v[3] < .001 ? input : mPredelay[ch].Read(v[3] * .001 * mRate);
          mPredelay[ch].Push(input);
        }
        mPlate->Tick(wetL, wetR);
      }
      if (mHall) mHall->Tick(wetL, wetR);
      if (mSpace) mSpace->Tick(wetL, wetR);
      const double mix = v[mPlate ? 4 : 5] * .01;
      if (!right) wetL = .5 * (wetL + wetR);
      left[i] = mMixLaw.Apply((dryL + mix * (wetL - dryL)) * mOutput, dryL, mix, mOutput);
      if (right) right[i] = mMixLaw.Apply((dryR + mix * (wetR - dryR)) * mOutput, dryR, mix, mOutput);
    }
  }
private:
  grid_fx::MixLaw mMixLaw;
  std::unique_ptr<airwindows_reverb::kPlate140::Core> mPlate;
  std::unique_ptr<airwindows_reverb::kGuitarHall2::Core> mHall;
  std::unique_ptr<airwindows_reverb::Galactic3::Core> mSpace;
  std::array<grid_fx::DelayLine, 2> mPredelay;
  grid_fx::SmoothControls<7> mControls;
  std::array<double, 2> mInputLow{};
  SlotEffect mEffect = SlotEffect::AirPlate;
  double mRate = 48000., mOutput = 1., mLowCoefficient = 0.;
  grid_fx::Memo mOutputGain;
  grid_fx::DrainRamp mDrain;
  double mLive = 1.;
  int mCounter = 0;
};
