#pragma once
#include "EffectPreview.h"

namespace grid_ui
{
// Parameter sketches share a time field and readable text; no synthetic audio
// history or measured impulse response is implied by the reflection marks.
template<class Value>
void DrawCosmicSetup(IGraphics& g, const IRECT& plot, Value value, double rate)
{
  const int style=std::clamp(static_cast<int>(value(uCosmicStyle)),0,effect_modes::cosmicCount-1);
  const auto plan=effect_preview::Cosmic(value(uCosmicTime),value(uCosmicWarp),style,rate);
  const double horizon=plan.seconds[0]*1.03, density=value(uCosmicDensity)*.01, mix=value(uCosmicMix)*.01;
  const double motion=.010416667*std::pow(value(uCosmicMotion)*.01,2.);
  const IRECT field(plot.L,542,plot.R,668);
  WDL_String text;
  text.SetFormatted(64,"%s | %.0f ms",effect_modes::cosmicNames[style],value(uCosmicTime));
  g.DrawText(Text(14.f,delayColor),text.Get(),IRECT(plot.L,490,plot.R,514));
  text.SetFormatted(64,"Warp %.0f%% | Density %.0f%%",value(uCosmicWarp),value(uCosmicDensity));
  g.DrawText(Text(12.f,muted),text.Get(),IRECT(plot.L,515,plot.R,536));
  g.DrawLine(border,field.L,field.B,field.R,field.B);
  for(int i=0;i<plan.count;++i)
  {
    const float x=field.L+field.W()*static_cast<float>(plan.seconds[i]/horizon);
    const float height=field.H()*static_cast<float>(.3+.65*density);
    const int channel=style==static_cast<int>(effect_modes::CosmicStyle::Centaurus) || style==static_cast<int>(effect_modes::CosmicStyle::GreatAnnihilator) ? (i/4)%2 : i%2;
    const auto ink=(channel ? ampColor : delayColor).WithOpacity(static_cast<float>(.25+.75*mix));
    const float spread=std::clamp(static_cast<float>(field.W()*motion/horizon),0.f,12.f);
    if(spread>0.) g.FillRect(ink.WithOpacity(.12f),IRECT(std::max(field.L,x-spread),field.B-height,std::min(field.R,x+spread),field.B));
    g.DrawLine(ink,x,field.B,x,field.B-height,nullptr,2.f);
    if(i && density>.01)
    {
      const float previous=field.L+field.W()*static_cast<float>(plan.seconds[i-1]/horizon);
      g.DrawLine(delayColor.WithOpacity(static_cast<float>(density*.3)),previous,field.B-height,x,field.B-height*.55f);
    }
  }
  g.DrawText(Text(12.f,delayColor),"L",IRECT(field.L,field.T,field.L+12,field.T+17));
  g.DrawText(Text(12.f,ampColor),"R lines",IRECT(field.L+16,field.T,field.L+70,field.T+17));
  g.DrawText(Text(12.f,muted,EAlign::Far),effect_modes::cosmicTopology[style],IRECT(field.L+76,field.T,field.R,field.T+17));
  text.SetFormatted(64,"Mod %.0f%% | FB %.0f%%",value(uCosmicMotion),value(uCosmicFeedback));
  g.DrawText(Text(12.f,muted),text.Get(),IRECT(plot.L,671,plot.R,690));
}

template<class Value>
void DrawSpaceSetup(IGraphics& g, SlotEffect effect, const IRECT& plot, IColor color, Value value)
{
  const bool spring=effect==SlotEffect::SpringReverb, shimmer=effect==SlotEffect::ShimmerReverb;
  const bool plate=effect==SlotEffect::AirPlate, hall=effect==SlotEffect::AirHall, space=effect==SlotEffect::GalacticSpace;
  const bool native=plate || hall || space;
  const bool frozen=shimmer && value(uShimmerFreeze)>=.5;
  double size=spring || shimmer ? 1. : native ? space ? value(uSpaceSize)*.01 : 100./value(plate ? uAirPlateResolution : uAirHallResolution)
                                                               : value(uReverbSize)*.01;
  const double decay=spring ? value(uSpringDecay) : shimmer ? value(uShimmerDecay) : native ? 1. : value(uReverbDecay);
  const double sustain=plate ? value(uAirPlateSustain)*.01 : hall ? value(uAirHallSustain)*.01 : space ? 1.-value(uSpaceReplace)*.01 : 1.;
  const double pre=spring || shimmer || hall || space ? 0. : value(plate ? uAirPlatePredelay : uReverbPredelay)*.001;
  const double mix=value(plate ? uAirPlateMix : hall ? uAirHallMix : space ? uSpaceMix : spring ? uSpringMix : shimmer ? uShimmerMix : uReverbMix)*.01;
  const double brightness=native ? plate ? .5 : value(hall ? uAirHallTone : uSpaceBrightness)*.01
                                  : std::clamp(value(shimmer ? uShimmerTone : uReverbTone)/12000.,.05,1.);
  const double motion=space ? value(uSpaceDetune)*.01 : spring ? value(uSpringDrip)*.01 : native ? 0. : value(shimmer ? uShimmerMod : uReverbMotion)*.01;
  const double early=hall ? value(uAirHallEarly)*.01 : space ? 0. : effect==SlotEffect::RoomReverb ? .8 : plate ? .2 : spring ? .5 : .4;
  WDL_String text;
  if(space) text.SetFormatted(64,"Size %.0f%% | Replace %.0f%%",value(uSpaceSize),value(uSpaceReplace));
  else if(native) text.SetFormatted(64,"Sustain %.0f%% | Res %.0f%%",sustain*100.,value(plate ? uAirPlateResolution : uAirHallResolution));
  else if(frozen) text.Set("Shimmer | frozen tail");
  else if(spring || shimmer) text.SetFormatted(64,"%s | %.1f s",spring ? "Spring" : "Shimmer",decay);
  else text.SetFormatted(64,"Size %.0f%% | %.1f s",size*100.,decay);
  g.DrawText(Text(14.f,color),text.Get(),IRECT(plot.L,490,plot.R,514));
  g.DrawText(Text(12.f,ampColor),"Early",IRECT(plot.L,515,plot.L+40,536));
  g.DrawText(Text(12.f,color),native ? "/ late: relative sketch" : "/ late: decay sketch",IRECT(plot.L+40,515,plot.R,536));
  const IRECT field(plot.L,542,plot.R,668);
  const double horizon=native ? 1. : std::max(.25,decay+pre);
  const double onset=native ? std::min(.2,pre) : pre;
  const auto timeX=[&](double t) {
    return field.L+field.W()*static_cast<float>(std::log1p(std::max(0.,t)/.025)/std::log1p(horizon/.025));
  };
  const double earlySpan=native ? .04*size : .055*size*(effect==SlotEffect::HallReverb ? 2. : 1.);
  const double lateStart=std::min(horizon*.45,onset+earlySpan*.7);
  const double tailScale=native ? .2+.8*sustain : 1.;
  const double lateHeight=.8*mix;
  for(int i=0;i<64;++i)
  {
    const double t=horizon*std::expm1(std::log1p(horizon/.025)*i/64.)/std::expm1(std::log1p(horizon/.025));
    if(t<lateStart) continue;
    const double progress=(t-lateStart)/std::max(.01,horizon-lateStart);
    const double envelope=frozen ? 1. : std::max(0.,1.-progress/tailScale);
    const float x=timeX(t),y=field.B-field.H()*static_cast<float>(lateHeight*envelope);
    g.FillRect(color.WithOpacity(.16f),IRECT(x,y,std::min(field.R,x+field.W()/64.f+1.f),field.B));
    g.FillCircle(color.WithOpacity(.75f),x,y,.8f);
    const float dark=field.B-field.H()*static_cast<float>(lateHeight*std::max(0.,1.-progress/std::max(.1,tailScale*brightness)));
    if(i%3==0) g.FillCircle(muted,x,dark,.8f);
  }
  const double position=hall ? value(uAirHallPosition)*.01 : .5;
  for(int i=0;i<9;++i)
  {
    const double t=std::min(horizon,onset+earlySpan*(.15+i*.1+position*.02*std::sin(i*2.)));
    const float x=timeX(t),height=field.H()*static_cast<float>(early*mix*(.8-.055*i));
    g.DrawLine(ampColor.WithOpacity(.85f),x,field.B,x,field.B-height,nullptr,1.5f);
    if(motion>.01) g.FillCircle(color.WithOpacity(.25f),x+static_cast<float>(motion*3.*std::sin(i)),field.B-height,1.5f);
  }
  g.DrawLine(border,field.L,field.B,field.R,field.B);
  if(shimmer)
  {
    const float direction=value(uShimmerPitch)>=0. ? -1.f : 1.f;
    const float x=field.L+field.W()*.7f,y=field.T+field.H()*.35f;
    g.DrawLine(color,x,y-direction*5,x,y+direction*5,nullptr,1.5f);
    g.DrawLine(color,x-3,y+direction*2,x,y+direction*5);
    g.DrawLine(color,x+3,y+direction*2,x,y+direction*5);
  }
  if(native) text.SetFormatted(64,"Wet %.0f%% | relative scale",mix*100.);
  else text.SetFormatted(64,"Pre %.0f ms | Wet %.0f%%",pre*1000.,mix*100.);
  g.DrawText(Text(12.f,muted),text.Get(),IRECT(plot.L,671,plot.R,690));
}
} // namespace grid_ui
