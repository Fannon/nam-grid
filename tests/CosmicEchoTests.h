#pragma once
#include "SlotCosmicEcho.h"
#include "EffectPreview.h"

void TestCosmicEcho()
{
  for (double rate : {44100., 48000., 96000., 192000.})
    for (int style=0; style<3; ++style)
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
            c.cosmicStyle=(segment+style)%3;
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
  std::array<std::vector<double>,3> impulses;
  for (int style=0; style<3; ++style)
  {
    SlotCosmicEcho effect; effect.Reset(rate);
    UtilityControls c; c.cosmicStyle=style; c.cosmic={40.,0.,0.,0.,12000.,0.,100.};
    std::vector<double> left(20000),right(left.size()); left[0]=1.;
    effect.Process(left.data(),right.data(),static_cast<int>(left.size()),c);
    int first=-1;
    for (int i=0; i<left.size(); ++i) if (std::abs(left[i])>1.e-10) { first=i; break; }
    const auto plan=effect_preview::Cosmic(40.,0.,style,rate);
    const int expected=static_cast<int>(plan.seconds[0]*rate)*(style==1 ? 8 : 1)+(style==1 ? 32 : 0);
    if (std::abs(first-expected)>1) std::cerr<<"Cosmic style="<<style<<" arrival="<<first<<" expected="<<expected<<'\n';
    assert(first>=0 && std::abs(first-expected)<=1);
    for (double x:right) assert(x==0.);
    assert(plan.count==(style==2 ? 4 : 16));
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
      c.cosmicStyle=(style+1)%3;
      left.assign(4096,0.); right.assign(4096,0.);
      effect.Process(left.data(),right.data(),static_cast<int>(left.size()),c);
      for (int i=1000; i<left.size(); ++i) assert(left[i]==0. && right[i]==0.);
    }

    effect.Reset(rate); c.cosmicStyle=style; c.cosmic={80.,65.,65.,80.,6000.,25.,100.};
    impulses[style].assign(96000,0.); right.assign(96000,0.); impulses[style][0]=1.;
    effect.Process(impulses[style].data(),right.data(),96000,c);
  }
  for (int a=0; a<3; ++a) for (int b=a+1; b<3; ++b)
  {
    double difference=0.;
    for (int i=0; i<96000; ++i) difference+=std::pow(impulses[a][i]-impulses[b][i],2.);
    assert(difference>.01);
  }
  std::cout<<"Cosmic Echo: three distinct styles, 44.1-192 kHz automation/partitioning, impulse timing, bounded tails, exact dry and allocation-free style changes passed\n";
}
