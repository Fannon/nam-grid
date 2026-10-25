#pragma once
#include "EffectPreview.h"
#include "ImpulseProbe.h"

namespace grid_ui
{
// Cosmic Echo's measured wet response to one impulse (ImpulseProbe), on a dB scale
// over a fixed time window, above the base delay-line sketch. Separate spikes are
// distinct echoes; Warp spreads them, Density and Feedback blur them into a smooth
// wash. Until the current measurement arrives, the previous one is shown dimmed.
template<class Value>
void DrawCosmicSetup(IGraphics& g, const IRECT& plot, Value value, const ImpulseProbe::Result& measured, bool current, double rate)
{
  const int style=std::clamp(static_cast<int>(value(uCosmicStyle)),0,effect_modes::cosmicCount-1);
  const auto plan=effect_preview::Cosmic(value(uCosmicTime),value(uCosmicWarp),style,rate);
  const IRECT response(plot.L,plot.T,plot.R,plot.T+std::round(plot.H()*.56f));
  const IRECT lines(plot.L,response.B+17.f,plot.R,plot.B);
  WDL_String text;
  PlotTitle(g,plot,delayColor,"Measured echo response / delay lines");
  PlotDetail(g,plot,"Measured response above; delay-line network below. Hover each view for details.");
  PlotRegionDetail(g,IRECT(plot.L,plot.T,plot.R,response.B+14.f),
    "Measured wet impulse, relative to its strongest peak (0 to -48 dB). Warp spreads echoes; Density fills the tail.");
  text.SetFormatted(320,"%d delay lines at their base times. Connections show Density; halos show Modulation. Colours identify network groups; coincident lines share a mark.",plan.count);
  PlotRegionDetail(g,IRECT(plot.L,response.B+14.f,plot.R,grid_layout::plotFooterBottom),text.Get());
  PlotGrid(g,response);
  g.DrawLine(border,response.L,response.B,response.R,response.B);
  if(measured.valid)
  {
    float loudest=0.f;
    for(const float peak : measured.peak) loudest=std::max(loudest,peak);
    const auto y=[&](float peak) {
      const double db=20.*std::log10(std::max(1.e-9,static_cast<double>(peak/std::max(loudest,1.e-9f))));
      return response.B-response.H()*static_cast<float>(std::clamp((db+48.)/48.,0.,1.));
    };
    const float width=response.W()/ImpulseProbe::bins, opacity=current ? 1.f : .35f;
    for(int i=0;i<ImpulseProbe::bins;++i)
    {
      const float x=response.L+width*i, top=y(measured.peak[i]);
      g.FillRect(delayColor.WithOpacity(.25f*opacity),IRECT(x,top,x+width,response.B));
      if(i) g.DrawLine(delayColor.WithOpacity(opacity),x-width*.5f,y(measured.peak[i-1]),x+width*.5f,top,nullptr,1.5f);
    }
  }
  if(!current)
    g.DrawText(Text(13.f,muted,EAlign::Center),"Measuring...",IRECT(response.L,response.T,response.R,response.T+20));
  if(measured.seconds<1.) text.SetFormatted(32,"%.0f ms",measured.seconds*1000.);
  else text.SetFormatted(32,"%.1f s",measured.seconds);
  g.DrawText(Text(11.f,muted),"Measured impulse | 0 to -48 dB",IRECT(plot.L,response.B+1.f,plot.R-75.f,response.B+14.f));
  if(measured.valid)
    g.DrawText(Text(11.f,muted,EAlign::Far),text.Get(),IRECT(plot.R-75.f,response.B+1.f,plot.R,response.B+14.f));

  // Restore the original network sketch underneath the measured response. Its
  // shorter time axis shows base line lengths, not every repeat in the wet tail.
  const double horizon=effect_preview::TimeHorizon(plan.seconds[0]*1.03), density=value(uCosmicDensity)*.01;
  const double motion=(500./48000.)*std::pow(value(uCosmicMotion)*.01,2.);
  g.DrawLine(border,lines.L,lines.B,lines.R,lines.B);
  for(int i=0;i<plan.count;++i)
  {
    const float x=lines.L+lines.W()*static_cast<float>(plan.seconds[i]/horizon);
    const float height=lines.H()*static_cast<float>(.3+.5*density);
    const int group=style==static_cast<int>(effect_modes::CosmicStyle::Centaurus) || style==static_cast<int>(effect_modes::CosmicStyle::GreatAnnihilator) ? (i/4)%2 : i%2;
    const auto ink=group ? ampColor : delayColor;
    const float spread=std::clamp(static_cast<float>(lines.W()*motion/horizon),0.f,12.f);
    if(spread>0.) g.FillRect(ink.WithOpacity(.12f),IRECT(std::max(lines.L,x-spread),lines.B-height,std::min(lines.R,x+spread),lines.B));
    g.DrawLine(ink,x,lines.B,x,lines.B-height,nullptr,2.f);
    if(i && density>.01)
    {
      const float previous=lines.L+lines.W()*static_cast<float>(plan.seconds[i-1]/horizon);
      g.DrawLine(delayColor.WithOpacity(static_cast<float>(density*.3)),previous,lines.B-height,x,lines.B-height*.55f);
    }
  }
  // Colours identify network groups; they are not direct left/right output taps.
  text.SetFormatted(32,"%.0f ms",horizon*1000.);
  PlotFooter(g,plot,"Network sketch | colours = line groups",text.Get());
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
  const double brightness=native ? plate ? .5 : value(hall ? uAirHallTone : uSpaceBrightness)*.01
                                  : std::clamp(value(shimmer ? uShimmerTone : uReverbTone)/12000.,.05,1.);
  const double motion=space ? value(uSpaceDetune)*.01 : spring ? value(uSpringDrip)*.01 : native ? 0. : value(shimmer ? uShimmerMod : uReverbMotion)*.01;
  const double early=hall ? value(uAirHallEarly)*.01 : space ? 0. : effect==SlotEffect::RoomReverb ? .8 : plate ? .2 : spring ? .5 : .4;
  WDL_String text;
  if(native) text.Set("Relative decay envelope");
  else if(frozen) text.Set("Shimmer | frozen tail");
  else if(shimmer) text.Set("Decay / pitch layers");
  else if(spring) text.Set("Reflections / decay");
  else text.Set("Decay by frequency");
  PlotTitle(g,plot,color,text.Get());
  if(shimmer) text.Set("The decay envelope and successive pitch layers share a logarithmic time axis. Freezing holds the tail without adding new pitch layers.");
  else if(spring) text.Set("Reflections feed the spring's decay; Drip and Dwell change their shape.");
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
  // The sketch describes the wet tank's shape at full scale; typical low wet
  // settings would otherwise hide its decay.
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
  if(native) text.Set("Relative tank | amber: early reflections");
  else if(spring) text.Set("Reflections + decay | log time");
  else if(shimmer) text.Set("Decay + pitch layers | log time");
  else text.SetFormatted(80,"Low %.1f s | high %.1f s",lowDecay,highDecay);
  WDL_String end;
  if(!native) end.SetFormatted(32,"%.1f s (log)",horizon);
  else end.Set("Relative");
  PlotFooter(g,plot,text.Get(),end.Get());
}
} // namespace grid_ui
