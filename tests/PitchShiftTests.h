#pragma once
#include "SlotPitchShift.h"
#include "SpectralAnalysis.h"
#include "SlotOperations.h"
#include <cstring>
namespace pitch_shift_test
{
inline std::vector<double> Render(double rate, double hz, double interval, double fine = 0., int block = 37)
{
  SlotPitchShift fx; fx.Reset(rate); UtilityControls c; c.pitchShift = {interval, fine, 16000., 0., 100.};
  std::vector<double> x(static_cast<size_t>(rate * 1.5));
  for (size_t i = 0; i < x.size(); ++i) x[i] = .2 * std::sin(2. * grid_fx::pi * hz * i / rate);
  for (size_t i = 0; i < x.size(); i += block)
    fx.Process(x.data() + i, nullptr, static_cast<int>(std::min<size_t>(block, x.size() - i)), c);
  return x;
}
inline double PeakHz(const std::vector<double>& x, double rate)
{
  constexpr size_t n = 32768;
  std::vector<double> window(n);
  for (size_t i = 0; i < n; ++i)
    window[i] = x[x.size() - n + i] * (.5 - .5 * std::cos(2. * grid_fx::pi * i / (n - 1)));
  const auto bins = spectral_test::FFT(window);
  size_t best = 1;
  for (size_t i = 2; i < n / 2; ++i) if (std::norm(bins[i]) > std::norm(bins[best])) best = i;
  const double a = std::log(std::max(1.e-30, std::abs(bins[best-1])));
  const double b = std::log(std::max(1.e-30, std::abs(bins[best])));
  const double c = std::log(std::max(1.e-30, std::abs(bins[best+1])));
  const double offset = .5 * (a - c) / (a - 2. * b + c);
  return (best + std::clamp(offset, -.5, .5)) * rate / n;
}
}
inline void TestPitchShift()
{
  using namespace pitch_shift_test;
  for (double rate : {44100., 48000., 96000., 192000.})
  {
    for (double hz : {82.4069, 220., 523.251})
      for (double interval : {-12., 7., 12.})
      {
        const auto audio = Render(rate, hz, interval);
        const double wanted = hz * std::exp2(interval / 12.), measured = PeakHz(audio, rate);
        const double cents = 1200. * std::log2(measured / wanted);
        std::cout << "Pitch " << rate << ' ' << hz << ' ' << interval << ": " << measured << " Hz (" << cents << " cents)\n";
        assert(std::abs(cents) < 3.); // Accurate pitch, not merely energy in a wide target band.
        double energy = 0.;
        for (size_t i = audio.size()/2; i < audio.size(); ++i) { assert(std::isfinite(audio[i]) && std::abs(audio[i]) < .25); energy += audio[i]*audio[i]; }
        assert(energy / (audio.size()/2) > .003); // No sustained-note cancellation.
      }
    for (double interval : {-24., 24.})
    {
      const auto extreme = Render(rate, 220., interval);
      const double wanted = 220. * std::exp2(interval / 12.);
      assert(std::abs(1200. * std::log2(PeakHz(extreme, rate) / wanted)) < 3.);
    }
    const auto detuned = Render(rate, 440., 0., 8.);
    assert(std::abs(1200.*std::log2(PeakHz(detuned,rate)/440.) - 8.) < 3.);
    assert(Render(rate, 220., 7., 0., 1) == Render(rate, 220., 7., 0., 128));
    SlotPitchShift fx; fx.Reset(rate); UtilityControls c; c.pitchShift = {7., 0., 10000., 0., 35.};
    std::array<double,128> l{},r{};
    for(int b=0;b<100;++b)
    {
      for(int i=0;i<128;++i){ l[i]=.3*std::sin((b*128+i)*.071);r[i]=-l[i]; }
      allocations=deletions=0;tracking=true;fx.Process(l.data(),r.data(),128,c);tracking=false;
      assert(allocations==0 && deletions==0);
      for(int i=0;i<128;++i)assert(l[i]==-r[i]);
    }
    // Silent right stays silent. Pitching never folds stereo to mono.
    fx.Reset(rate);
    for(int b=0;b<100;++b)
    {
      l.fill(.2);r.fill(0.);fx.Process(l.data(),r.data(),128,c);
      for(double x:r)assert(x==0.);
    }
    // Mix 0 and neutral unison at max High cut are exact, including signed zero.
    for (double mix : {0., 100.})
    {
      fx.Reset(rate); c.pitchShift={mix==0.?7.:0.,0.,16000.,0.,mix};
      for(int i=0;i<128;++i)l[i]=.3*std::sin(i*.17);l[0]=-0.;const auto dry=l;
      fx.Process(l.data(),nullptr,128,c);assert(std::memcmp(l.data(),dry.data(),sizeof(l))==0);
    }
    // Returning Mix to zero becomes exact dry after smoothing, even with grain history.
    fx.Reset(rate); c.pitchShift={7.,0.,10000.,0.,100.};l.fill(.2);fx.Process(l.data(),nullptr,128,c);
    c.pitchShift[4]=0.;
    for(int b=0;b<static_cast<int>(rate/128)+1;++b){l.fill(.2);fx.Process(l.data(),nullptr,128,c);}
    l.fill(.1);l[0]=-0.;const auto afterFade=l;fx.Process(l.data(),nullptr,128,c);
    assert(std::memcmp(l.data(),afterFade.data(),sizeof(l))==0);
    // Slot reports zero compensation delay, and the dry pick is immediate.
    SlotConfig config;config.effect=SlotEffect::PitchShift;
    NAMStage stage;stage.Publish(PrepareSlot(config,rate,128).dsp);stage.ResetStopped(rate,128);
    assert(stage.Latency()==0);c.pitchShift={7.,0.,10000.,0.,25.};l.fill(0.);l[0]=.2;
    stage.Process(l.data(),128,true,0.,0.,{},{},c);
    assert(std::abs(l[0]-.15)<1.e-12);
    // Finite grain memory/filter decay; silence does not ring indefinitely.
    for(int b=0;b<static_cast<int>(rate*.3/128)+2;++b){l.fill(0.);stage.Process(l.data(),128,true,0.,0.,{},{},c);}
    for(double x:l)assert(std::abs(x)<1.e-12);
  }
}
