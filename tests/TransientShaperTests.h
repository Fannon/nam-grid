#pragma once
#include "SlotTransientShaper.h"

namespace transient_test
{
// decay 12/s is a fast synthetic pluck (-104 dB/s); 1.7/s is a guitar-like -15 dB/s.
inline std::vector<double> Render(double rate, UtilityControls c, double scale=1., int block=37, double decay=12., double length=.4)
{
  SlotTransientShaper fx;fx.Reset(rate);
  std::vector<double> a(static_cast<size_t>(rate));
  for(size_t i=0;i<a.size();++i)
  {
    const double t=i/rate;
    a[i]=t<length?scale*.3*std::cos(2.*grid_fx::pi*440.*t)*std::exp(-t*decay):0.;
  }
  for(size_t i=0;i<a.size();i+=block)
    fx.Process(a.data()+i,nullptr,static_cast<int>(std::min<size_t>(block,a.size()-i)),c);
  return a;
}
inline double Energy(const std::vector<double>& a,size_t begin,size_t end)
{double e=0.;for(size_t i=begin;i<end;++i)e+=a[i]*a[i];return e;}
}
void TestTransientShaper()
{
  using namespace transient_test;
  for(double rate:{44100.,48000.,96000.,192000.})
  {
    UtilityControls c;c.transientShaper={0.,0.,100.,50.,0.,100.};
    const auto dry=Render(rate,c);
    assert(dry[0]==.3); // No lookahead or buffering, including the first sample.
    c.transientShaper[0]=9.;const auto attack=Render(rate,c);
    assert(Energy(attack,0,static_cast<size_t>(rate*.02))>1.6*Energy(dry,0,static_cast<size_t>(rate*.02)));
    assert(attack==Render(rate,c,1.,1));
    // Relative envelope action is independent of signal scale above the quiet guard.
    const auto quiet=Render(rate,c,.25);
    for(size_t i=0;i<static_cast<size_t>(rate*.02);++i)assert(std::abs(quiet[i]*4.-attack[i])<1.e-12);
    c.transientShaper[0]=0.;c.transientShaper[1]=-9.;const auto tight=Render(rate,c);
    assert(Energy(tight,static_cast<size_t>(rate*.1),static_cast<size_t>(rate*.3))<.6*Energy(dry,static_cast<size_t>(rate*.1),static_cast<size_t>(rate*.3)));
    c.transientShaper[5]=0.;assert(Render(rate,c)==dry);
    c.transientShaper[5]=100.;
    const auto window=[&](const std::vector<double>& wet,const std::vector<double>& reference,double from,double to)
    { return 10.*std::log10(Energy(wet,static_cast<size_t>(rate*from),static_cast<size_t>(rate*to))
                           /Energy(reference,static_cast<size_t>(rate*from),static_cast<size_t>(rate*to))); };
    // Attack acts on the first milliseconds of the note, not on its body.
    c.transientShaper={9.,0.,100.,50.,0.,100.};
    const auto shaped=Render(rate,c,1.,37,1.7,.9),slow=Render(rate,{},1.,37,1.7,.9);
    assert(window(shaped,slow,0.,.02)>window(shaped,slow,.05,.15)+4.);
    // Sustain shortens a guitar-like slow decay, not only a fast synthetic one.
    c.transientShaper={0.,-9.,300.,50.,0.,100.};
    assert(window(Render(rate,c,1.,37,1.7,.9),slow,.3,.8)<-2.);
    // A held low E is neither attack nor decay: once settled, no control changes it.
    // Raw-power followers rippled with the waveform and lifted it by about 1.3 dB.
    for(const auto& [a,s]:{std::pair{9.,0.},std::pair{0.,-9.},std::pair{0.,9.}})
    {
      UtilityControls held;held.transientShaper={a,s,100.,50.,0.,100.};
      SlotTransientShaper fx;fx.Reset(rate);
      std::vector<double> e(static_cast<size_t>(rate));
      for(size_t i=0;i<e.size();++i)e[i]=.3*std::sin(2.*grid_fx::pi*82.4*i/rate);
      const auto input=e;
      for(size_t i=0;i<e.size();i+=64)fx.Process(e.data()+i,nullptr,static_cast<int>(std::min<size_t>(64,e.size()-i)),held);
      assert(std::abs(window(e,input,.5,1.))<.05);
    }
    SlotTransientShaper fx;fx.Reset(rate);
    std::array<double,512> l{},r{};for(size_t i=0;i<l.size();++i){l[i]=.2*std::sin(i*.1);r[i]=-l[i];}
    c.transientShaper[5]=100.;allocations=deletions=0;tracking=true;
    fx.Process(l.data(),r.data(),512,c);tracking=false;assert(allocations==0&&deletions==0);
    for(size_t i=0;i<l.size();++i)assert(l[i]==-r[i]);
  }
}
