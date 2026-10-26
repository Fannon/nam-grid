#pragma once
#include "dsp/reverb/ReverbCores.h"

// Common musical controls around distinct stereo cores. Reset prepares only the
// selected algorithm. All clocks, coefficient quanta and fades are sample-based.
class SlotReverb
{
public:
  static constexpr double SpaceScale(SlotEffect effect)
  { return effect == SlotEffect::RoomReverb ? .55 : effect == SlotEffect::HallReverb ? 1.1 : 1.; }
  static constexpr double kMaximumSize = 2.2;
  void Reset(double rate, SlotEffect effect = SlotEffect::RoomReverb)
  {
    mRate = std::max(1., rate); mEffect = effect; mCounter = 0;
    mControls.Reset(mRate); mExtras.Reset(mRate); mStereo.Reset(mRate);
    mInputLow = {}; mWetLow = {}; mMusical.Reset(mRate); mDucker.Reset(mRate);
    for (auto& line : mPredelay) line.Reset(mRate, .201);
    if (effect == SlotEffect::PlateReverb) mPlate.Reset(mRate);
    else if (effect == SlotEffect::CloudReverb) mCloud.Reset(mRate);
    else mFDN.Reset(mRate, effect == SlotEffect::HallReverb);
  }
  void Process(double* left, double* right, int frames, const UtilityControls& controls)
  {
    mControls.Target(controls.reverb, uReverbDecay);
    mMusical.Target({controls.extra.reverbDuck}, uReverbDuck);
    mExtras.Target({controls.reverbMotion, controls.reverbHighPass}, uReverbMotion);
    mStereo.Target({controls.stereoReverb ? 1. : 0.}, uReverbStereo);
    for (int i = 0; i < frames; ++i)
    {
      const auto& v = mControls.Tick(); const auto& extra = mExtras.Tick();
      const double width = mStereo.Tick()[0];
      const auto& musical = mMusical.Tick();
      if (mCounter == 0)
      {
        mLowCoefficient = grid_reverb::Coefficient(extra[1], mRate);
        mToneCoefficient = grid_reverb::Coefficient(v[2], mRate);
        const double size = v[1] * .01, motion = extra[0] * .01;
        if (mEffect == SlotEffect::PlateReverb) mPlate.Configure(size, v[0], v[2], motion);
        else if (mEffect == SlotEffect::CloudReverb) mCloud.Configure(size, v[0], v[2], motion);
        else mFDN.Configure(size, v[0], v[2], motion);
      }
      mCounter = (mCounter + 1) % 32;
      const grid_reverb::Stereo dry{left[i], right ? right[i] : left[i]};
      grid_reverb::Stereo input = dry;
      for (int ch = 0; ch < 2; ++ch)
      {
        const double low = grid_reverb::LowPass(input[ch], mInputLow[ch], mLowCoefficient);
        input[ch] -= extra[1] > .01 ? low : 0.;
        const double delayed = mPredelay[ch].Read(std::max(1., v[3] * .001 * mRate));
        mPredelay[ch].Push(input[ch]);
        if (v[3] >= .001) input[ch] = delayed;
      }
      auto wet = mEffect == SlotEffect::PlateReverb ? mPlate.Tick(input)
                 : mEffect == SlotEffect::CloudReverb ? mCloud.Tick(input) : mFDN.Tick(input);
      for (int ch = 0; ch < 2; ++ch) wet[ch] = grid_reverb::LowPass(wet[ch], mWetLow[ch], mToneCoefficient);
      const double duck = mDucker.Tick(dry[0], dry[1], mEffect == SlotEffect::RoomReverb ? 0. : musical[0]);
      const double mid = .5 * (wet[0] + wet[1]) * duck, side = .5 * (wet[0] - wet[1]) * width * duck;
      const double mix = v[4] * .01;
      left[i] = dry[0] + mix * ((right ? mid + side : mid) - dry[0]);
      if (right) right[i] = dry[1] + mix * (mid - side - dry[1]);
    }
  }
  bool ReadsFit() const
  {
    return !mPredelay[0].Clipped() && !mPredelay[1].Clipped()
      && !(mEffect == SlotEffect::PlateReverb ? mPlate.Clipped() : mEffect == SlotEffect::CloudReverb ? mCloud.Clipped() : mFDN.Clipped());
  }
private:
  grid_reverb::DiffusionFDN mFDN;
  grid_reverb::PlateTank mPlate;
  grid_reverb::CloudTank mCloud;
  std::array<grid_reverb::MovingDelay, 2> mPredelay;
  grid_reverb::Stereo mInputLow{}, mWetLow{};
  grid_fx::SmoothControls<5> mControls;
  grid_fx::SmoothControls<2> mExtras;
  grid_fx::SmoothControls<1> mStereo;
  grid_fx::SmoothControls<1> mMusical;
  grid_fx::WetDucker mDucker;
  SlotEffect mEffect = SlotEffect::RoomReverb;
  double mRate = 48000., mLowCoefficient = 0., mToneCoefficient = 0.;
  int mCounter = 0;
};
