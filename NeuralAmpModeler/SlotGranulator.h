#pragma once
#include "dsp/Granulator.h"

// Granulator slot: smoothed controls, Mode, Freeze, drain and the rig's dry-mix law around
// the granular engine. Its 4 s stereo buffer is allocated in Reset, off the audio thread.
class SlotGranulator
{
public:
  void Reset(double rate)
  {
    mEngine.Reset(rate); mControls.Reset(rate); mMixLaw.Reset(rate); mDrain.Reset(rate);
    mCounter = 0;
  }
  void Process(double* left, double* right, int frames, const UtilityControls& controls, bool draining = false)
  {
    mControls.Target(controls.granulator, uGranulatorPosition);
    const int mode = std::clamp(controls.granulatorMode, 0, effect_modes::granulatorCount - 1);
    // A draining slot records its silence, so a frozen buffer cannot return on re-enable.
    const bool frozen = controls.granulatorFreeze && !draining;
    const double period = controls.sync ? grid_tempo::Seconds(controls.tempo, controls.division) : 0.;
    for (int i = 0; i < frames; ++i)
    {
      mMixLaw.Tick(controls.constantDry);
      // Position, Size, Activity, Shape, Filter, Repeats, Space, Mix.
      const auto& v = mControls.Tick();
      // Repeats fade over 5 ms when draining starts.
      const double live = mDrain.Tick(draining);
      if (mCounter == 0)
        mEngine.Configure({mode, v[0] * .01, v[1], v[2] * .01, v[3] * .01, v[4] * .01, v[5] * .01, v[6] * .01, period});
      mCounter = (mCounter + 1) % 32;
      const grid_granular::Stereo dry{left[i], right ? right[i] : left[i]};
      const auto wet = mEngine.Tick(dry, right != nullptr, frozen, live);
      const double mix = v[7] * .01;
      left[i] = mMixLaw.Apply(dry[0] + mix * ((right ? wet[0] : .5 * (wet[0] + wet[1])) - dry[0]), dry[0], mix, 1.);
      if (right) right[i] = mMixLaw.Apply(dry[1] + mix * (wet[1] - dry[1]), dry[1], mix, 1.);
    }
  }
  int ActiveGrains() const { return mEngine.Active(); }

private:
  grid_granular::Engine mEngine;
  grid_fx::SmoothControls<8> mControls;
  grid_fx::MixLaw mMixLaw;
  grid_fx::DrainRamp mDrain;
  int mCounter = 0;
};
