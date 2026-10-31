#pragma once
#include "SlotOperations.h"
#include "SignalMonitor.h"
#include <cstring>

inline void TestSignalMonitor()
{
  using namespace signal_monitor;
  Accumulator levels;
  for (int i=0;i<4800;++i)
  {
    const double s=std::sin(2.*grid_fx::pi*500.*i/48000.);
    levels.Push(.5*s,-.25*s);
  }
  Meter meter; meter.Receive(levels,48000.);
  assert(std::abs(meter.RMS(0)-.5/std::sqrt(2.))<1.e-12);
  assert(std::abs(meter.RMS(1)-.25/std::sqrt(2.))<1.e-12);
  assert(std::abs(meter.Correlation()+1.)<1.e-12);
  assert(meter.peak[0]>.499 && meter.hold[1]>.249);
  meter.Reset(); meter.Receive({},48000.);
  assert(meter.Correlation()==0. && meter.RMS(0)==0.);
  Accumulator clipped; clipped.Push(1.2,-1.1); meter.Receive(clipped,48000.);
  assert(meter.hold[0]==1.2 && meter.hold[1]==1.1);

  // Stereo capture alignment: the right FFT and newest scope samples must
  // describe the same times, even after wraparound.
  SignalCapture capture;
  for (int i=0;i<5000;++i) capture.Push(i,-i);
  grid_eq_signal::Window window;
  assert(capture.Take(window));
  for(size_t i=0;i<window.samples.size();++i)
    assert(window.samples[i]==-window.right[i]);

  for(double rate:{44100.,48000.,96000.,192000.})
  {
    SlotConfig config;config.effect=SlotEffect::SignalMonitor;
    NAMStage stage;stage.Publish(PrepareSlot(config,rate,128).dsp);stage.ResetStopped(rate,128);
    assert(stage.Latency()==0);
    SignalCapture samples;stage.AttachCapture(&samples);
    for(int block=0;block<80;++block)
    {
      const int frames=block%3==0?1:block%3==1?37:128;
      std::array<double,128> l{},r{};
      for(int i=0;i<frames;++i){l[i]=.3*std::sin((block*128+i)*.071);r[i]=-.2*std::cos((block*128+i)*.043);}
      l[0]=-0.;const auto dryL=l,dryR=r;
      const bool enabled=block%5!=0, visible=block%7!=0;
      stage.EnableMetering(visible);
      allocations=deletions=0;tracking=true;
      stage.Process(l.data(),frames,enabled,0.,0.,{},{},{},r.data());
      tracking=false;
      assert(allocations==0 && deletions==0);
      assert(std::memcmp(l.data(),dryL.data(),sizeof(l))==0 && std::memcmp(r.data(),dryR.data(),sizeof(r))==0);
      const auto observed=stage.TakeMeter();
      assert(observed.monitor.frames==static_cast<uint64_t>(enabled&&visible?frames:0));
    }
    stage.AttachCapture(nullptr);
    std::array<double,128> l{},r{};
    stage.Process(l.data(),128,true,0.,0.,{},{},{},r.data());
    assert(stage.TakeMeter().monitor.frames==0); // No hidden measurement work.
  }
}
