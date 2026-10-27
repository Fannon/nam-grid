#pragma once
#include "SlotCosmicEcho.h"
#include "EffectPreview.h"

void TestCosmicEcho()
{
  for (double rate : {44100., 48000., 96000., 192000.})
    for (int style=0; style<effect_modes::cosmicCount; ++style)
    {
      const auto render = [&](bool tiny, bool automate, bool dry) {
        SlotCosmicEcho effect; effect.Reset(rate);
        UtilityControls c; c.cosmicStyle=style; if (dry) c.cosmic[6]=0.;
        const int count=static_cast<int>(rate*.4);
        std::array<std::vector<double>, 2> audio{std::vector<double>(count),std::vector<double>(count)};
        for (int i=0; i<count; ++i) { audio[0][i]=.1*std::sin(i*.071); audio[1][i]=.08*std::cos(i*.053); }
        for (int pos=0, block=0; pos<count;)
        {
          const int segment=pos/2048;
          if (automate)
          {
            c.cosmicStyle=(segment+style)%effect_modes::cosmicCount;
            for (int p=0; p<7; ++p)
              c.cosmic[p]=segment%2 ? kEffectParameters[uCosmicTime+p].maximum : kEffectParameters[uCosmicTime+p].minimum;
          }
          const int n=std::min(std::min(count,(segment+1)*2048)-pos,
            tiny ? std::array<int,5>{1,3,64,127,256}[block++%5] : 512);
          allocations=deletions=0; tracking=true; Eigen::internal::set_is_malloc_allowed(false);
          effect.Process(audio[0].data()+pos,audio[1].data()+pos,n,c);
          Eigen::internal::set_is_malloc_allowed(true); tracking=false;
          assert(allocations==0 && deletions==0); pos+=n;
        }
        return audio;
      };
      for (bool automate : {false,true})
      {
        const auto a=render(false,automate,false), b=render(true,automate,false);
        for (int ch=0; ch<2; ++ch) for (int i=0; i<a[ch].size(); ++i)
        {
          assert(std::isfinite(b[ch][i]) && std::abs(b[ch][i])<32.);
          assert(std::abs(a[ch][i]-b[ch][i])<1.e-11);
        }
      }
      const auto dry=render(true,false,true);
      for (int i=0; i<dry[0].size(); ++i)
      {
        assert(std::abs(dry[0][i]-.1*std::sin(i*.071))<1.e-12);
        assert(std::abs(dry[1][i]-.08*std::cos(i*.053))<1.e-12);
      }
    }

  constexpr double rate=48000.;
  std::array<std::vector<double>,effect_modes::cosmicCount> impulses;
  for (int style=0; style<effect_modes::cosmicCount; ++style)
  {
    SlotCosmicEcho effect; effect.Reset(rate);
    UtilityControls c; c.cosmicStyle=style; c.cosmic={40.,0.,0.,0.,12000.,0.,100.};
    std::vector<double> left(20000),right(left.size()); left[0]=1.;
    effect.Process(left.data(),right.data(),static_cast<int>(left.size()),c);
    int first=-1;
    for (int i=0; i<left.size(); ++i) if (std::abs(left[i])>1.e-10) { first=i; break; }
    const auto plan=effect_preview::Cosmic(40.,0.,style,rate);
    int expected=static_cast<int>(std::round(plan.seconds[0]*rate));
    if (style==static_cast<int>(effect_modes::CosmicStyle::Sagittarius)) expected=expected*8+32;
    if (style==static_cast<int>(effect_modes::CosmicStyle::Centaurus)) expected=expected*4+32;
    if (style==static_cast<int>(effect_modes::CosmicStyle::GreatAnnihilator))
      expected=static_cast<int>(std::round((plan.seconds[0]+plan.seconds[1]+plan.seconds[2]+plan.seconds[3])*rate));
    if (style==static_cast<int>(effect_modes::CosmicStyle::AuroraHall))
    {
      double earliest=1.e9;
      for (int i=0; i<8; i+=2) earliest=std::min(earliest,plan.seconds[i]+plan.seconds[8+i]);
      expected=static_cast<int>(std::round(earliest*rate));
    }
    const int onsetTolerance=style==static_cast<int>(effect_modes::CosmicStyle::AuroraHall) ? 3 : 1; // Two fractional reads in series.
    if (std::abs(first-expected)>onsetTolerance) std::cerr<<"Cosmic style="<<style<<" arrival="<<first<<" expected="<<expected<<'\n';
    assert(first>=0 && std::abs(first-expected)<=onsetTolerance);
    for (double x:right) assert(x==0.);
    assert(plan.count==grid_cosmic::LineCount(style));
    const auto spread=effect_preview::Cosmic(40.,100.,style,rate);
    assert(spread.seconds[0]==plan.seconds[0] && spread.seconds[spread.count-1]<spread.seconds[0]*.2);

    // Listenability/stability checks over filled tanks, including the extreme
    // feedback setting and minimum line lengths with maximum modulation.
    for (double time : {20.,8000.})
    {
      effect.Reset(rate); c.cosmicStyle=style; c.cosmic={time,100.,98.,100.,16000.,100.,100.};
      std::array<double,512> l{},r{};
      double energy=0.;
      for (int pos=0; pos<12*48000; pos+=512)
      {
        const int n=std::min(512,12*48000-pos);
        for (int i=0; i<n; ++i) { l[i]=pos+i<48000 ? .1*std::sin((pos+i)*.071) : 0.; r[i]=0.; }
        allocations=deletions=0; tracking=true; Eigen::internal::set_is_malloc_allowed(false);
        effect.Process(l.data(),r.data(),n,c);
        Eigen::internal::set_is_malloc_allowed(true); tracking=false;
        assert(allocations==0 && deletions==0);
        for (int i=0; i<n; ++i)
        {
          assert(std::isfinite(l[i]) && std::isfinite(r[i]) && std::abs(l[i])<32. && std::abs(r[i])<32.);
          energy+=l[i]*l[i]+r[i]*r[i];
        }
      }
      assert(energy>.001);
      // The defined transition drops the old tail, without stale ring contents
      // leaking back out after logical clearing. Mix remains fully wet.
      c.cosmicStyle=(style+1)%effect_modes::cosmicCount;
      left.assign(4096,0.); right.assign(4096,0.);
      effect.Process(left.data(),right.data(),static_cast<int>(left.size()),c);
      for (int i=1000; i<left.size(); ++i) assert(left[i]==0. && right[i]==0.);
    }

    effect.Reset(rate); c.cosmicStyle=style; c.cosmic={80.,65.,65.,80.,6000.,25.,100.};
    impulses[style].assign(96000,0.); right.assign(96000,0.); impulses[style][0]=1.;
    effect.Process(impulses[style].data(),right.data(),96000,c);
  }
  for (int a=0; a<effect_modes::cosmicCount; ++a) for (int b=a+1; b<effect_modes::cosmicCount; ++b)
  {
    double difference=0.;
    for (int i=0; i<96000; ++i) difference+=std::pow(impulses[a][i]-impulses[b][i],2.);
    assert(difference>.01);
  }
  // Aurora Hall must supply a real sparse-to-dense transition, including at Warp 0,
  // without the mid-Density energy hole caused by crossfading a diffuser.
  const auto hall = [](double density, double feedback, double rate, double warp) {
    grid_cosmic::Core core; core.Reset(rate);
    core.Configure({120.,warp,feedback,density,6000.,0.,100.},
                   static_cast<int>(effect_modes::CosmicStyle::AuroraHall),false);
    std::vector<grid_cosmic::Core::Stereo> out(static_cast<int>(rate*4.));
    for (int i=0; i<out.size(); ++i) out[i]=core.Tick({i==0 ? 1. : 0.,0.});
    return out;
  };
  for (double warp : {0.,65.})
  {
    double sparseDensity=0., sparseEnergy=0.;
    for (double density : {0.,30.,60.,100.})
    {
      const auto out=hall(density,85.,48000.,warp);
      double energy=0., squares=0.; int above=0;
      for (const auto& x:out) energy+=x[0]*x[0]+x[1]*x[1];
      for (int i=9600; i<24000; ++i) squares+=out[i][0]*out[i][0];
      const double rms=std::sqrt(squares/14400.);
      for (int i=9600; i<24000; ++i) above+=std::abs(out[i][0])>rms;
      const double echoDensity=above/14400.;
      if (density==0.) { sparseDensity=echoDensity; sparseEnergy=energy; }
      else assert(energy>.4*sparseEnergy && energy<2.*sparseEnergy);
      if (density==100.) assert(echoDensity>3.*sparseDensity && echoDensity>.12);
    }
  }
  double shortTail=0.;
  for (double feedback : {35.,98.})
  {
    const auto out=hall(100.,feedback,48000.,65.);
    double early=0., late=0.;
    for (int i=0; i<out.size(); ++i)
    {
      const double energy=out[i][0]*out[i][0]+out[i][1]*out[i][1];
      if (i<24000) early+=energy;
      if (i>=96000) late+=energy;
    }
    if (feedback==35.) shortTail=late/early;
    else assert(late/early>.05 && late/early>100.*shortTail);
  }
  // All Aurora Hall timing offsets are expressed in seconds, including the short
  // diffuser offsets. A host-rate change must not resize the hall.
  double firstSeconds=0.;
  for (double hostRate : {44100.,48000.,96000.,192000.})
  {
    const auto out=hall(0.,0.,hostRate,0.);
    size_t first=0; while (first<out.size() && std::abs(out[first][0])<1.e-10) ++first;
    assert(first<out.size());
    const double seconds=first/hostRate;
    if (firstSeconds==0.) firstSeconds=seconds;
    else assert(std::abs(seconds-firstSeconds)<.0001);
  }
  // With stationary controls and Motion off, Aurora Hall must not fold a steady
  // treble sine into other frequencies (particularly after NAM/drive stages).
  for (double hostRate : {44100.,48000.,96000.,192000.})
  {
    grid_cosmic::Core core; core.Reset(hostRate);
    core.Configure({20.,65.,35.,100.,16000.,0.,100.},
                   static_cast<int>(effect_modes::CosmicStyle::AuroraHall),false);
    constexpr int count=32768;
    const int bin=static_cast<int>(10000.*count/hostRate)|1;
    const double omega=2.*grid_fx::pi*bin/count;
    const int warm=static_cast<int>(2.*hostRate);
    std::array<double,2> sine{}, cosine{}, squares{};
    for (int i=0; i<warm+count; ++i)
    {
      const auto out=core.Tick({.2*std::sin(omega*i),0.});
      if (i<warm) continue;
      for (int ch=0; ch<2; ++ch)
      {
        sine[ch]+=out[ch]*std::sin(omega*i);
        cosine[ch]+=out[ch]*std::cos(omega*i);
        squares[ch]+=out[ch]*out[ch];
      }
    }
    for (int ch=0; ch<2; ++ch)
    {
      const double tonePower=2.*(sine[ch]*sine[ch]+cosine[ch]*cosine[ch])/(count*count);
      const double residual=std::sqrt(std::max(0.,squares[ch]/count-tonePower));
      assert(tonePower>1.e-10 && residual<2.e-7);
    }
  }
  std::cout<<"Cosmic Echo: distinct styles, 44.1-192 kHz automation/partitioning, impulse timing, bounded tails, exact dry and allocation-free style changes passed\n";
}

void TestCosmicBypassDrain()
{
  // Sagittarius can hold an impulse for eight eight-second stages. Scale the
  // sample rate down for a fast deterministic test of the real-time policy.
  constexpr double rate=1000.;
  const auto render=[&](bool bypass, effect_modes::CosmicStyle style) {
    SlotConfig config; config.effect=SlotEffect::CosmicEcho;
    auto prepared=PrepareSlot(config,rate,100);
    NAMStage stage; stage.Publish(std::move(prepared.dsp)); stage.ResetStopped(rate,100);
    UtilityControls c; c.cosmicStyle=static_cast<int>(style);
    c.cosmic={8000.,0.,0.,0.,200.,0.,100.};
    std::array<double,100> l{},r{};
    double energy=0.; allocations=deletions=0;
    for(int block=0;block<800;++block)
    {
      l.fill(0.); r.fill(0.); if(block==0) l[0]=1.;
      tracking=true; Eigen::internal::set_is_malloc_allowed(false);
      stage.Process(l.data(),100,!bypass || block==0 || block>=700,0.,0.,{},{},c,r.data());
      Eigen::internal::set_is_malloc_allowed(true); tracking=false;
      if(block>=700 || !bypass) for(double x:l) energy+=x*x;
    }
    assert(allocations==0 && deletions==0);
    return energy;
  };
  for (auto style : {effect_modes::CosmicStyle::Sagittarius,effect_modes::CosmicStyle::AuroraHall})
  {
    assert(render(false,style)>1.e-6); // The tank really contains a pending impulse.
    assert(render(true,style)<1.e-24); // No stale impulse after 70 seconds bypassed.
  }
}
