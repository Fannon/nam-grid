#pragma once
#include "dsp/reverb/CosmicReverb.h"

// One bounded buffer bank for all three styles. A style change fades the wet
// signal down, clears histories in O(1) per line, then fades the new style up.
// Dry stereo is unaffected; no core allocation/deletion occurs during automation.
class SlotCosmicEcho
{
public:
  void Reset(double rate)
  {
    mCore.Reset(rate); mControls.Reset(rate); mCounter=0; mStyle=-1;
    mTransition=1.; mStep=1./std::max(1., .02*rate);
  }
  void Process(double* left, double* right, int frames, const UtilityControls& controls, bool draining=false)
  {
    mControls.Target(controls.cosmic, uCosmicTime);
    const int requested=std::clamp(controls.cosmicStyle, 0, 2);
    if (mStyle < 0) mStyle=requested;
    for (int i=0; i<frames; ++i)
    {
      const auto& v=mControls.Tick();
      if (requested != mStyle)
      {
        mTransition=std::max(0., mTransition-mStep);
        if (mTransition == 0.) { mCore.Clear(); mStyle=requested; mCounter=0; }
      }
      else mTransition=std::min(1., mTransition+mStep);
      if (mCounter == 0) mCore.Configure(v, mStyle, draining);
      mCounter=(mCounter+1)%32;
      const grid_cosmic::Core::Stereo dry{left[i], right ? right[i] : left[i]};
      const auto wet=mCore.Tick(dry);
      const double mix=v[6]*.01;
      left[i]=dry[0]+mix*(mTransition*(right ? wet[0] : .5*(wet[0]+wet[1]))-dry[0]);
      if (right) right[i]=dry[1]+mix*(mTransition*wet[1]-dry[1]);
    }
  }
private:
  grid_cosmic::Core mCore;
  grid_fx::SmoothControls<7> mControls;
  double mTransition=1., mStep=0.;
  int mStyle=-1, mCounter=0;
};
