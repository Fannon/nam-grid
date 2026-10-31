#pragma once
#include "SlotOperations.h"
#include "SignalMonitor.h"
#include <cstring>

inline void TestGoniometerBounds()
{
  using namespace signal_monitor;
  // Utility's scope: every peak-normalized point stays inside the box.
  for (int a = -10; a <= 10; ++a)
    for (int b = -10; b <= 10; ++b)
    {
      const auto [side, mid] = Goniometer(a / 10., b / 10.);
      assert(std::abs(side) <= 1. && std::abs(mid) <= 1.);
    }
  const auto mono = Goniometer(1., 1.), inverted = Goniometer(1., -1.), left = Goniometer(1., 0.);
  assert(mono.first == 0. && mono.second == 1.);            // Full-scale mono reaches the top.
  assert(inverted.first == -1. && inverted.second == 0.);   // Opposite polarity lies across.
  assert(left.first < 0. && left.second == -left.first);    // Left only: the upper-left "L" diagonal.
}

inline void TestSignalMonitorHistory()
{
  using namespace signal_monitor;
  // Columns follow audio frames, not packet sizes: 2400 frames at 48 kHz each.
  History history;
  const auto packet = [](uint64_t frames, double level) {
    Accumulator a;
    for (uint64_t i = 0; i < frames; ++i) a.Push(i % 2 ? level : -level, .5 * level);
    return a;
  };
  history.Receive(packet(2000, .25), 48000.);
  assert(history.count == 0 && std::abs(history.Current().peak - .25f) < 1.e-6f);
  // This packet straddles the column edge; its peak marks both columns.
  history.Receive(packet(800, 1.2), 48000.);
  assert(history.count == 1 && history.Ago(0).peak >= 1.f && history.Current().peak >= 1.f);
  assert(std::abs(history.Current().rms - 1.2f) < 1.e-5f); // Louder channel's RMS.
  history.Receive(packet(4 * 2400, .1), 48000.);
  assert(history.count == 5 && std::abs(history.Ago(0).peak - .1f) < 1.e-6f && history.Ago(3).peak >= 1.f);
  history.Resume(); history.Receive(packet(2400, .1), 48000.);
  assert(history.Ago(0).resumed && !history.Ago(1).resumed && !history.Current().resumed);
  // A rate change cannot leave the current column wider than a new column.
  History changed; changed.Receive(packet(4000, .2), 96000.); changed.Receive(packet(10, .2), 44100.);
  assert(changed.count == 1);
  for (int i = 0; i < 500; ++i) history.Receive(packet(2400, .1), 48000.);
  assert(history.count == History::columns);
  History empty; empty.Resume(); empty.Receive(packet(10, .1), 48000.);
  assert(!empty.Current().resumed); // Nothing to separate before the first audio.
}

inline void TestSignalMonitorSpectrum()
{
  using namespace signal_monitor;
  constexpr double rate = 48000.;
  const auto sine = [](double hz, double amplitude) {
    std::array<float, grid_eq_signal::windowSize> samples{};
    for (size_t i = 0; i < samples.size(); ++i)
      samples[i] = static_cast<float>(amplitude * std::sin(2. * grid_fx::pi * hz * i / rate));
    return samples;
  };
  const auto nearest = [&](double hz) {
    size_t best = 0;
    for (size_t i = 0; i < spectrumPoints; ++i)
      if (std::abs(std::log(PointHz(i, rate) / hz)) < std::abs(std::log(PointHz(best, rate) / hz))) best = i;
    return best;
  };
  // A sine reads its peak amplitude at its point, both where bands span many
  // bins and where low points interpolate inside one bin.
  for (double hz : {1000., 5000., 60.})
  {
    const auto db = Detailed(grid_eq_signal::Magnitudes(sine(hz, .5)), rate);
    const size_t at = nearest(hz);
    // Within the Hann window's scalloping loss: 1.42 dB for a sine halfway between bins.
    assert(std::abs(db[at] + 6.02f) < 1.5f);
    assert(*std::max_element(db.begin(), db.end()) < .01f);
    assert(db[nearest(hz * 4.)] < -40.f);
  }
  // The refactored EQ bands match the detailed points for the same signal.
  const auto bands = grid_eq_signal::Analyze(sine(1000., .5), rate);
  assert(std::abs(*std::max_element(bands.begin(), bands.end()) + 6.02f) < 1.5f);

  // Resonances: narrow peaks above a sloped floor; broad bumps never qualify.
  std::array<float, spectrumPoints> db{};
  for (size_t i = 0; i < spectrumPoints; ++i)
    db[i] = -30.f - 10.f * static_cast<float>(i) / spectrumPoints
            + 8.f * std::exp(-std::pow((static_cast<float>(i) - 60.f) / 40.f, 2.f));
  const auto narrow = [&](size_t point, float height) {
    for (int d = -2; d <= 2; ++d) db[point + d] += height * (1.f - .3f * std::abs(d));
  };
  narrow(150, 12.f); narrow(260, 9.f); narrow(265, 7.f); narrow(320, 4.f);
  std::array<Resonance, 3> found{};
  const size_t count = FindResonances(db, rate, found);
  assert(count == 2 && found[0].point == 150 && found[1].point == 260); // 265 is too close and less prominent; 320 is too small.
  assert(found[0].prominence > found[1].prominence && found[1].prominence >= 6.f);
  std::array<float, spectrumPoints> quiet{};
  quiet.fill(spectrumFloor);
  assert(FindResonances(quiet, rate, found) == 0);

  // The long-term average ignores silence and needs two seconds of signal.
  SpectrumView view;
  std::array<float, spectrumPoints> loud{}; loud.fill(-20.f);
  for (int i = 0; i < 30; ++i) view.Receive(loud, true, .05);
  assert(!view.Settled() && std::abs(view.average[10] + 20.f) < 1.e-4f);
  for (int i = 0; i < 200; ++i) view.Receive(quiet, true, .05);
  assert(!view.Settled() && std::abs(view.average[10] + 20.f) < 1.e-4f);
  assert(view.live[10] < -60.f); // The live curve still falls.
  for (int i = 0; i < 20; ++i) view.Receive(loud, true, .1);
  assert(view.Settled());
}

inline void TestSignalMonitor()
{
  using namespace signal_monitor;
  TestGoniometerBounds();
  TestSignalMonitorHistory();
  TestSignalMonitorSpectrum();
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
