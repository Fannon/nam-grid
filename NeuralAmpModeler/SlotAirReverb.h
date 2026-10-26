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
    mRate = std::max(1., rate); mEffect = effect; mControls.Reset(mRate); mDuck.Reset(mRate); mDucker.Reset(mRate);
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
    mInputLow = {}; mCounter = 0; mOutput = 1.;
  }
  void Process(double* left, double* right, int frames, const UtilityControls& controls, bool draining = false)
  {
    mDuck.Target({controls.extra.reverbDuck}, uReverbDuck);
    const auto& values = mEffect == SlotEffect::AirPlate ? controls.airPlate
                         : mEffect == SlotEffect::AirHall ? controls.airHall : controls.space;
    mControls.Target(values, mEffect == SlotEffect::AirPlate ? uAirPlateInput
                             : mEffect == SlotEffect::AirHall ? uAirHallSustain : uSpaceReplace);
    for (int i = 0; i < frames; ++i)
    {
      const auto& v = mControls.Tick();
      if (mCounter == 0)
      {
        mOutput = std::pow(10., v[6] / 20.);
        if (mPlate)
        {
          mPlate->Set({std::pow(10., v[0] / 20.), draining ? 0. : v[1] * .01, v[2] * .01, 0., 1.});
          mLowCoefficient = std::exp(-2. * grid_fx::pi * std::max(1., v[5]) / mRate);
        }
        if (mHall) mHall->Set({draining ? 0. : v[0] * .01, v[1] * .005, v[2] * .01, v[3] * .01, v[4] * .01, 1.});
        if (mSpace) mSpace->Set({draining ? 1. : v[0] * .01, v[1] * .01, v[2] * .01, v[4] * .01, v[3] * .01, 1.});
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
      const double duck = mDucker.Tick(dryL, dryR, mPlate ? mDuck.Tick()[0] : 0.);
      wetL *= duck; wetR *= duck;
      if (!right) wetL = .5 * (wetL + wetR);
      left[i] = (dryL + mix * (wetL - dryL)) * mOutput;
      if (right) right[i] = (dryR + mix * (wetR - dryR)) * mOutput;
    }
  }
private:
  std::unique_ptr<airwindows_reverb::kPlate140::Core> mPlate;
  std::unique_ptr<airwindows_reverb::kGuitarHall2::Core> mHall;
  std::unique_ptr<airwindows_reverb::Galactic3::Core> mSpace;
  std::array<grid_fx::DelayLine, 2> mPredelay;
  grid_fx::SmoothControls<7> mControls;
  grid_fx::SmoothControls<1> mDuck;
  grid_fx::WetDucker mDucker;
  std::array<double, 2> mInputLow{};
  SlotEffect mEffect = SlotEffect::AirPlate;
  double mRate = 48000., mOutput = 1., mLowCoefficient = 0.;
  int mCounter = 0;
};
