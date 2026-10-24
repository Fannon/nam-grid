#pragma once
#include "EffectPreview.h"
#include "ImpulseProbe.h"

namespace grid_ui
{
// Cosmic Echo's measured wet response to one impulse (ImpulseProbe), on a dB scale
// over a fixed time window. Separate spikes are distinct echoes; Warp spreads them,
// Density and Feedback blur them into a smooth wash. Until the measurement for the
// current settings arrives, the previous one is shown dimmed.
template<class Value>
void DrawCosmicSetup(IGraphics& g, const IRECT& plot, Value value, const ImpulseProbe::Result& measured, bool current)
{
  const int style=std::clamp(static_cast<int>(value(uCosmicStyle)),0,effect_modes::cosmicCount-1);
  WDL_String text;
  text.SetFormatted(64,"%s | %.0f ms",effect_modes::cosmicNames[style],value(uCosmicTime));
  PlotTitle(g,plot,delayColor,text.Get());
  text.SetFormatted(80,"Warp %.0f%% | Density %.0f%% | FB %.0f%%",value(uCosmicWarp),value(uCosmicDensity),value(uCosmicFeedback));
  PlotDetail(g,plot,text.Get());
  PlotGrid(g,plot);
  g.DrawLine(border,plot.L,plot.B,plot.R,plot.B);
  if(measured.valid)
  {
    float loudest=0.f;
    for(const float peak : measured.peak) loudest=std::max(loudest,peak);
    const auto y=[&](float peak) {
      const double db=20.*std::log10(std::max(1.e-9,static_cast<double>(peak/std::max(loudest,1.e-9f))));
      return plot.B-plot.H()*static_cast<float>(std::clamp((db+48.)/48.,0.,1.));
    };
    const float width=plot.W()/ImpulseProbe::bins, opacity=current ? 1.f : .35f;
    for(int i=0;i<ImpulseProbe::bins;++i)
    {
      const float x=plot.L+width*i, top=y(measured.peak[i]);
      g.FillRect(delayColor.WithOpacity(.25f*opacity),IRECT(x,top,x+width,plot.B));
      if(i) g.DrawLine(delayColor.WithOpacity(opacity),x-width*.5f,y(measured.peak[i-1]),x+width*.5f,top,nullptr,1.5f);
    }
  }
  if(!current)
    g.DrawText(Text(13.f,muted,EAlign::Center),"Measuring...",IRECT(plot.L,plot.T,plot.R,plot.T+20));
  if(measured.seconds<1.) text.SetFormatted(32,"%.0f ms",measured.seconds*1000.);
  else text.SetFormatted(32,"%.1f s",measured.seconds);
  PlotFooter(g,plot,"Measured impulse | 0 to -48 dB",measured.valid ? text.Get() : "");
}

template<class Value>
void DrawSpaceSetup(IGraphics& g, SlotEffect effect, const IRECT& plot, IColor color, Value value, double rate = 48000.)
{
  const bool spring=effect==SlotEffect::SpringReverb, shimmer=effect==SlotEffect::ShimmerReverb;
  const bool plate=effect==SlotEffect::AirPlate, hall=effect==SlotEffect::AirHall, space=effect==SlotEffect::GalacticSpace;
  const bool native=plate || hall || space;
  const bool frozen=shimmer && value(uShimmerFreeze)>=.5;
  double size=spring || shimmer ? 1. : native ? space ? (.1+1.77*value(uSpaceSize)*.01)*100./value(uSpaceResolution) : 100./value(plate ? uAirPlateResolution : uAirHallResolution)
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
  PlotTitle(g,plot,color,text.Get());
  if(shimmer) text.SetFormatted(80,"Pitch %+.0fst | Amount %.0f%%%s",value(uShimmerPitch),frozen ? 0. : value(uShimmerAmount),frozen ? " (frozen)" : "");
  else if(spring) text.SetFormatted(80,"Drip %.0f%% | Dwell %+.0fdB",value(uSpringDrip),value(uSpringDwell));
  else if(native) text.Set("Relative tank | dots: HF | amber: early");
  if(native || spring || shimmer) PlotDetail(g,plot,text.Get());
  const IRECT field=plot;
  const double horizon=native ? 1. : effect_preview::TimeHorizon(std::max(.25,decay+pre));
  const double onset=native ? std::min(.2,pre) : pre;
  const auto timeX=[&](double t) {
    return field.L+field.W()*static_cast<float>(std::log1p(std::max(0.,t)/.025)/std::log1p(horizon/.025));
  };
  const double earlySpan=native ? .04*size : .055*size*(effect==SlotEffect::HallReverb ? 2. : 1.);
  const double lateStart=std::min(horizon*.45,onset+earlySpan*.7);
  const double tailScale=native ? .2+.8*sustain : 1.;
  // Algorithmic reverbs: exact decay per band from each core. Room/Hall shelve
  // lows (<200 Hz) and highs (>Damping) to fixed ratios; Plate/Cloud damp once
  // per loop pass, so a fixed 5 kHz reference shows Damping x Size x Decay.
  const bool banded=!native && !spring && !shimmer;
  const bool fdn=effect==SlotEffect::RoomReverb || effect==SlotEffect::HallReverb;
  const double damping=value(uReverbTone), highHz=fdn ? damping*1.01 : 5000.;
  const double lowDecay=banded ? effect_preview::ReverbDecay(effect,100.,decay,size,damping,rate) : decay;
  const double highDecay=banded ? effect_preview::ReverbDecay(effect,highHz,decay,size,damping,rate) : decay;
  if(banded)
  {
    if(fdn) text.SetFormatted(80,"Amber <200 Hz %.1f s | gray >%.1fk %.1f s",lowDecay,damping/1000.,highDecay);
    else text.SetFormatted(80,"Fill: mid | gray: 5 kHz decays in %.1f s",highDecay);
    PlotDetail(g,plot,text.Get());
  }
  float previousLow=0.f,previousHigh=0.f,previousX=0.f; bool first=true;
  // The sketch describes the wet tank's shape. Mix belongs in the readout;
  // scaling the drawing by a typical 5-30% wet setting hides its decay.
  const double lateHeight=.8;
  for(int i=0;i<64;++i)
  {
    const double t=horizon*std::expm1(std::log1p(horizon/.025)*i/64.)/std::expm1(std::log1p(horizon/.025));
    if(t<lateStart) continue;
    const double progress=(t-lateStart)/std::max(.01,horizon-lateStart);
    const double envelope=frozen ? 1. : native ? std::max(0.,1.-progress/tailScale)
        : std::clamp((60.+effect_preview::TailDB(t-lateStart,decay))/60.,0.,1.);
    const float x=timeX(t),y=field.B-field.H()*static_cast<float>((native || frozen ? lateHeight : .9)*envelope);
    g.FillRect(color.WithOpacity(.16f),IRECT(x,y,std::min(field.R,x+field.W()/64.f+1.f),field.B));
    g.FillCircle(color.WithOpacity(.75f),x,y,.8f);
    // Movement/dispersion belongs to the tank, not the unmodulated early path.
    if(motion>.01 && i%4==0)
      g.FillCircle(color.WithOpacity(.2f),std::clamp(x+static_cast<float>(motion*3.*std::sin(i)),field.L,field.R),y,2.f);
    if(banded)
    {
      const auto band=[&](double seconds) {
        return field.B-field.H()*static_cast<float>(.9*std::clamp((60.+effect_preview::TailDB(t-lateStart,seconds))/60.,0.,1.));
      };
      const float low=band(lowDecay),high=band(highDecay);
      if(!first)
      {
        if(fdn) g.DrawLine(ampColor.WithOpacity(.8f),previousX,previousLow,x,low,nullptr,1.2f);
        g.DrawLine(muted,previousX,previousHigh,x,high,nullptr,1.2f);
      }
      previousLow=low; previousHigh=high; previousX=x; first=false;
      continue;
    }
    const float dark=frozen ? y : field.B-field.H()*static_cast<float>(std::min(double((field.B-y)/field.H()),lateHeight*std::max(0.,1.-progress/std::max(.1,tailScale*brightness))));
    if(i%3==0) g.FillCircle(muted,x,dark,.8f);
  }
  const double position=hall ? value(uAirHallPosition)*.01 : .5;
  for(int i=0;i<9 && !frozen;++i)
  {
    const double t=std::min(horizon,onset+earlySpan*(hall ? .25+1.5*position : 1.)*(.15+i*.1));
    const float x=timeX(t),height=field.H()*static_cast<float>(early*(.8-.055*i));
    g.DrawLine(ampColor.WithOpacity(.85f),x,field.B,x,field.B-height,nullptr,1.5f);
  }
  g.DrawLine(border,field.L,field.B,field.R,field.B);
  if(shimmer && !frozen && value(uShimmerAmount)>0. && std::abs(value(uShimmerPitch))>.1)
  {
    // Every pass through the tank is shifted by Pitch again and fed back at
    // Amount x .55: layers stack at Pitch, 2x Pitch, ... each quieter.
    const double pitch=value(uShimmerPitch),gain=value(uShimmerAmount)*.01*.55;
    const float left=field.R-58.f,right=field.R-30.f,center=field.MH();
    const auto semitoneY=[&](double semitones) { return center-field.H()*.48f*static_cast<float>(std::clamp(semitones/48.,-1.,1.)); };
    g.DrawLine(muted,left,center,right,center,nullptr,1.5f);
    double level=1.;
    for(int layer=1;layer<=4 && std::abs(layer*pitch)<=48.;++layer)
    {
      level*=gain;
      const float y=semitoneY(layer*pitch);
      g.DrawLine(color.WithOpacity(static_cast<float>(std::clamp(.2+3.*level,.2,1.))),left,y,right,y,nullptr,2.f);
      text.SetFormatted(16,"%+.0f",layer*pitch);
      g.DrawText(Text(11.f,muted),text.Get(),IRECT(right+3.f,y-6.f,field.R,y+6.f));
    }
  }
  if(native || spring)
    text.SetFormatted(64,"Wet %.0f%% | Out %+.0f dB",mix*100.,value(plate ? uAirPlateOutput : hall ? uAirHallOutput : space ? uSpaceOutput : uSpringOutput));
  else if(shimmer) text.SetFormatted(64,"Wet %.0f%% | Log time",mix*100.);
  else text.SetFormatted(64,"Wet %.0f%% | HP %.0f Hz",mix*100.,value(uReverbHighPass));
  WDL_String end;
  if(!native) end.SetFormatted(32,"%.1f s (log)",horizon);
  else end.Set("Relative");
  PlotFooter(g,plot,text.Get(),end.Get());
}
} // namespace grid_ui
