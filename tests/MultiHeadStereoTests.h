#pragma once
#include "SlotCreativeDelays.h"
#include "EffectPreview.h"

void TestMultiHeadStereo()
{
  for (double rate : {44100.,48000.,96000.,192000.})
    for (int pattern=0; pattern<effect_modes::tapeHeadNames.size(); ++pattern)
    {
      UtilityControls c; c.delay={40.,0.,12000.,0.,100.}; c.delayMotion=0.; c.tapeHeads=pattern;
      const int count=static_cast<int>(rate*.2);
      std::vector<double> left(count),right(count),mono(count); left[0]=right[0]=mono[0]=.1;
      SlotMultiHeadDelay stereo,folded; stereo.Reset(rate); folded.Reset(rate);
      stereo.Process(left.data(),right.data(),count,c); folded.Process(mono.data(),nullptr,count,c);
      const auto preview=effect_preview::Delay(SlotEffect::MultiHeadDelay,c,rate);
      for (int ch=0; ch<2; ++ch)
      {
        int expected=-1;
        for (size_t i=0; i<preview.count; ++i)
          if ((ch ? preview.taps[i].right : preview.taps[i].left)>0.)
          { expected=static_cast<int>(std::lround(preview.taps[i].seconds*rate)); break; }
        const auto& audio=ch ? right : left;
        int first=-1; for (int i=0; i<count; ++i) if (std::abs(audio[i])>1.e-10) { first=i; break; }
        assert(expected>=0 && std::abs(first-expected)<=1);
      }
      for (int i=0; i<count; ++i) assert(std::abs(mono[i]-.5*(left[i]+right[i]))<1.e-12);
      const auto& p=effect_modes::TapeHeads(pattern);
      for (const auto& channel:p.gains)
      { double sum=0.; for (double gain:channel) { assert(gain>=0.); sum+=gain; } assert(std::abs(sum-1.)<1.e-12); }

      // Left-only input must cross to the right via last-head feedback, not
      // via an unadvertised mono sum of the dry input. Dry stereo stays exact.
      if (p.crossFeedback)
      {
        stereo.Reset(rate); left.assign(count,0.); right.assign(count,0.); left[0]=.1; c.delay[1]=50.;
        stereo.Process(left.data(),right.data(),count,c);
        int first=-1; for(int i=0;i<count;++i) if(std::abs(right[i])>1.e-10) { first=i; break; }
        assert(first>=static_cast<int>(rate*.04));
        assert(first<count);
      }
    }
  // Repeated changes through all patterns exercise smoothed routing as well as
  // head weights. Compare partitions with audio allocation/deletion guards.
  const auto render=[](bool tiny) {
    SlotMultiHeadDelay effect; effect.Reset(48000.);
    std::array<std::vector<double>,2> audio{std::vector<double>(48000),std::vector<double>(48000)};
    UtilityControls c; c.delay={5.,82.,12000.,100.,100.}; c.delayMotion=100.;
    for(int pos=0,block=0;pos<48000;)
    {
      const int segment=pos/2048; c.tapeHeads=segment%8;
      const int n=std::min(std::min(48000,(segment+1)*2048)-pos,tiny ? std::array<int,4>{1,3,64,127}[block++%4] : 512);
      for(int i=0;i<n;++i) { audio[0][pos+i]=.15*std::sin((pos+i)*.071); audio[1][pos+i]=.1*std::cos((pos+i)*.053); }
      allocations=deletions=0; tracking=true; Eigen::internal::set_is_malloc_allowed(false);
      effect.Process(audio[0].data()+pos,audio[1].data()+pos,n,c);
      Eigen::internal::set_is_malloc_allowed(true); tracking=false; assert(allocations==0 && deletions==0); pos+=n;
    }
    return audio;
  };
  const auto a=render(false),b=render(true);
  for(int ch=0;ch<2;++ch) for(int i=0;i<48000;++i)
  { assert(std::isfinite(a[ch][i]) && std::abs(a[ch][i])<2.); assert(std::abs(a[ch][i]-b[ch][i])<1.e-12); }
  std::cout<<"Multi-head: eight patterns, stereo arrivals/preview, cross-feedback, mono fold and allocation-free mode changes passed\n";
}
