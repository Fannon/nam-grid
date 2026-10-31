#pragma once
#include "SlotDynamicEQ.h"

namespace dynamic_eq_test
{
inline std::vector<double> Render(double rate, double hz, double level, UtilityControls c, int block = 37)
{
  SlotDynamicEQ fx; fx.Reset(rate);
  std::vector<double> audio(static_cast<size_t>(rate));
  for (size_t i = 0; i < audio.size(); ++i) audio[i] = level * std::sin(2. * grid_fx::pi * hz * i / rate);
  for (size_t i = 0; i < audio.size(); i += block)
    fx.Process(audio.data() + i, nullptr, static_cast<int>(std::min<size_t>(block, audio.size()-i)), c);
  return audio;
}
inline double Energy(const std::vector<double>& a)
{ double e=0.; for(size_t i=a.size()/2;i<a.size();++i) e+=a[i]*a[i]; return e; }
}
void TestDynamicEQ()
{
  using namespace dynamic_eq_test;
  for (double rate : {44100.,48000.,96000.,192000.})
  {
    UtilityControls c; c.dynamicEQ = {800.,2.,-24.,12.,2.,100.,0.,100.};
    auto neutral=c; neutral.dynamicEQ[3]=0.;
    const auto loud=Render(rate,800.,.5,c), dry=Render(rate,800.,.5,neutral);
    assert(Energy(loud) < .09*Energy(dry)); // At least ~10.5 dB reduction on loud center tone.
    const auto quiet=Render(rate,800.,.001,c), quietDry=Render(rate,800.,.001,neutral);
    assert(quiet==quietDry); // No static cut below threshold.
    assert(Energy(Render(rate,80.,.5,c)) > .95*Energy(Render(rate,80.,.5,neutral)));
    assert(loud==Render(rate,800.,.5,c,1)); // Continuous control schedule independent of blocks.
    neutral.dynamicEQ[3]=12.; neutral.dynamicEQ[7]=0.;
    assert(Render(rate,800.,.5,neutral)==dry);
    SlotDynamicEQ fx; fx.Reset(rate);
    std::array<double,256> l{},r{}; l[0]=.5; r[0]=-.5;
    tracking=true; fx.Process(l.data(),r.data(),256,c); tracking=false;
    assert(l[0]!=0.); // First input sample is returned immediately, no buffering.
    for(size_t i=0;i<l.size();++i) assert(l[i]==-r[i] && std::isfinite(l[i]));
    assert(allocations==0 && deletions==0);
    // A loud 60 Hz tone under fast settings: the detector spans about two band periods,
    // so the cut does not follow the waveform and add distortion to low notes. A fixed
    // 3 ms detector measured -31 dB third harmonic here; this one about -49 dB.
    UtilityControls low; low.dynamicEQ = {60.,1.,-14.,18.,.2,10.,0.,100.};
    SlotDynamicEQ steady; steady.Reset(rate);
    std::vector<double> tone(static_cast<size_t>(rate));
    for (size_t i = 0; i < tone.size(); ++i) tone[i] = .5 * std::sin(2. * grid_fx::pi * 60. * i / rate);
    for (size_t i = 0; i < tone.size(); i += 64) steady.Process(tone.data() + i, nullptr, static_cast<int>(std::min<size_t>(64, tone.size() - i)), low);
    const auto harmonic = [&](double hz) {
      double s = 0., c = 0.;
      for (size_t i = tone.size() / 2; i < tone.size(); ++i)
      { s += tone[i] * std::sin(2. * grid_fx::pi * hz * i / rate); c += tone[i] * std::cos(2. * grid_fx::pi * hz * i / rate); }
      return std::hypot(s, c);
    };
    assert(steady.Reduction() > 4. && harmonic(180.) < .01 * harmonic(60.));
  }
}
