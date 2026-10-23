#pragma once
#include "EffectPreview.h"

namespace grid_ui
{
template<class Value>
bool DrawEffectSetup(IGraphics& g, SlotEffect effect, const IRECT& plot, IColor color,
                     Value value, double tempo, double rate, float inputLevel = 0.f)
{
  const auto caption = [&](const char* text) { PlotTitle(g,plot,color,text); };
  const auto foot = [&](const char* text) { PlotFooter(g,plot,text); };
  const auto line = [&](double x1, double y1, double x2, double y2, IColor ink, float width = 1.5f) {
    g.DrawLine(ink, plot.L + plot.W() * static_cast<float>(std::clamp(x1, 0., 1.)),
                    plot.B - plot.H() * static_cast<float>(std::clamp(y1, 0., 1.)),
                    plot.L + plot.W() * static_cast<float>(std::clamp(x2, 0., 1.)),
                    plot.B - plot.H() * static_cast<float>(std::clamp(y2, 0., 1.)), nullptr, width);
  };
  const auto curve = [&](auto function, IColor ink) {
    double previous = function(0.);
    for (int i = 1; i <= 160; ++i)
    {
      const double x = i / 160., current = function(x);
      line((i - 1) / 160., previous, x, current, ink); previous = current;
    }
  };
  WDL_String text;
  if (effect == SlotEffect::ConsoleEQ)
  {
    caption("Band density | not a linear EQ curve");
    line(0., .5, 1., .5, muted);
    const char* names[]{"Bass", "Mid", "Treble"};
    const int params[]{uConsoleBass, uConsoleMid, uConsoleTreble};
    for (int i = 0; i < 3; ++i)
    {
      const double density = value(params[i]);
      const float x = plot.L+plot.W()*(i+.5f)/3.f;
      const float y = plot.MH()-static_cast<float>(density/6.)*(plot.H()*.5f-24.f);
      g.FillRect(color.WithOpacity(.65f),IRECT(x-16,std::min(y,plot.MH()),x+16,std::max(y,plot.MH())+1));
      text.SetFormatted(64,"%s %+.1f",names[i],density);
      g.DrawText(Text(13.f,muted,EAlign::Center),text.Get(),IRECT(x-48,plot.B-18,x+48,plot.B));
    }
    text.SetFormatted(96,"Bass %.0f Hz | Treble %.0f Hz",value(uConsoleBassFreq),value(uConsoleTrebleFreq));
    PlotDetail(g,plot,text.Get());
    text.SetFormatted(96,"Cuts: low %.0f%% | high open %.0f%%",value(uConsoleLowCut),value(uConsoleHighCut));
    foot(text.Get());
    return true;
  }
  if (effect == SlotEffect::Wah)
  {
    UtilityControls wah;
    for (int i=0;i<5;++i) wah.wah[i]=value(uWahPosition+i);
    const double center=effect_preview::WahCenter(wah,rate),upper=std::min(20000.,.45*rate);
    text.SetFormatted(80,"Wah | peak %.0f Hz",center); caption(text.Get());
    PlotDetail(g,plot,"Position / register move peak; Q narrows it");
    PlotFrequencyAxis(g,plot,upper,"-24 / +24 dB");
    curve([&](double x) {
      const double hz=20.*std::pow(upper/20.,x);
      return .5+20.*std::log10(std::max(1.e-9,effect_preview::WahMagnitude(hz,wah,rate)))/48.;
    },color);
    const float peakX=plot.L+plot.W()*static_cast<float>(std::log(center/20.)/std::log(upper/20.));
    g.DrawLine(color.WithOpacity(.3f),peakX,plot.T,peakX,plot.B);
    return true;
  }
  if (effect == SlotEffect::Bitcrusher)
  {
    UtilityControls crush;
    for (int i=0;i<5;++i) crush.crusher[i]=value(uCrushBits+i);
    caption("Quantize / sample hold / high cut");
    text.SetFormatted(80,"%.0f bits | %.1fk samples/s",crush.crusher[0],std::min(rate,crush.crusher[1])/1000.);
    PlotDetail(g,plot,text.Get()); PlotGrid(g,plot);
    const auto reference=effect_preview::CrusherReference(crush,rate);
    const double duration=std::max(384.,std::ceil(rate*3./220.))/rate;
    PlotCurve(g,plot,[&](double x) { return .8*std::sin(2.*grid_fx::pi*220.*duration*x); },-2.,2.,muted.WithOpacity(.6f),1.f);
    for (size_t i=1;i<reference.size();++i)
      line((i-1)/384.,.5+reference[i-1]/4.,i/384.,.5+reference[i]/4.,color);
    PlotFooter(g,plot,"220 Hz reference: dry / wet mix","+/-2");
    return true;
  }
  UtilityControls c;
  c.sync = value(uTempoSync) >= .5; c.tempo = tempo; c.division = static_cast<int>(value(uTempoDivision));
  for (int i = 0; i < 5; ++i) c.delay[i] = value(uDelayTime + i);
  c.delayOutput = value(uDelayOutput); c.dual = {value(uDualRatio), value(uDualBalance)};
  c.pingPong = value(uDelayPingPong) >= .5;
  if (effect == SlotEffect::MultiHeadDelay || effect == SlotEffect::DiffusedDelay) c.pingPong = false;
  c.tapeHeads = static_cast<int>(value(uTapeHeads));
  if (IsDelay(effect) && effect != SlotEffect::ReverseDelay)
  {
    const auto echoes = effect_preview::Delay(effect, c, rate);
    const bool heads=effect==SlotEffect::MultiHeadDelay;
    const bool stereoHeads=heads && effect_modes::TapeHeads(c.tapeHeads).crossFeedback;
    const bool stereo=stereoHeads || c.pingPong || effect==SlotEffect::DualDelay;
    const char* name=heads ? effect_modes::tapeHeadNames[std::clamp(c.tapeHeads,0,7)]
                          : effect==SlotEffect::DiffusedDelay ? "Diffused" : c.pingPong ? "Ping-pong" : "Echoes";
    text.SetFormatted(64,"%s | %.0f%% wet",name,c.delay[4]);
    caption(text.Get());
    const int motion=static_cast<int>(value(uDelayMotion));
    if(effect==SlotEffect::DiffusedDelay) text.SetFormatted(64,"Diff %.0f%% | Mod %d%%",value(uDelayDiffusion),motion);
    else if(effect==SlotEffect::TapeDelay || heads) text.SetFormatted(64,"Mod %d%% | Sat %.0f%%",motion,c.delay[3]);
    else if(effect==SlotEffect::AnalogDelay) text.SetFormatted(64,"Age %.0f%% | Mod %d%%",c.delay[3],motion);
    else text.SetFormatted(64,"High cut %.1fk | %+.0f dB out",c.delay[2]/1000.,c.delayOutput);
    g.DrawText(Text(13.f,muted),text.Get(),IRECT(plot.L,515,plot.R-50,536));
    if(effect==SlotEffect::DiffusedDelay && (value(uDelayDiffusion)>0. || motion>0))
      g.DrawText(Text(13.f,color,EAlign::Far),"Stereo",IRECT(plot.R-50,515,plot.R,536));
    else
    {
      g.DrawText(Text(13.f,delayColor),stereo ? "L" : "L=R",IRECT(plot.R-45,515,plot.R-23,536));
      if(stereo) g.DrawText(Text(13.f,ampColor),"R",IRECT(plot.R-17,515,plot.R,536));
    }
    const IRECT chart=plot;
    const auto amplitudeY=[&](double amplitude) {
      return chart.B-chart.H()*static_cast<float>(std::clamp((20.*std::log10(std::max(1.e-9,amplitude))+60.)/72.,0.,1.));
    };
    PlotGrid(g,chart);
    const auto stem = [&](double x, double height, double treble, IColor ink, float offset) {
      const float px = std::clamp(chart.L+chart.W()*static_cast<float>(x)+offset,chart.L+2.f,chart.R-2.f);
      g.DrawLine(ink.WithOpacity(.5f),px,chart.B,px,amplitudeY(height),nullptr,4.f);
      g.DrawLine(ink,px,chart.B,px,amplitudeY(height*treble),nullptr,1.5f);
    };
    // One marker per channel/tap. Shared L/R taps draw once. Modulation halos,
    // diffusion clusters and the M/S badges are explicitly setup cues.
    for (size_t i = 0; i < echoes.count; ++i)
    {
      const auto& tap = echoes.taps[i];
      const double x = tap.seconds / echoes.horizon;
      const bool shared=std::abs(tap.left-tap.right)<1.e-10;
      const double height=std::max(tap.left,tap.right);
      if (effect == SlotEffect::DiffusedDelay && height>0.)
      {
        const float smear=static_cast<float>(value(uDelayDiffusion)*.001);
        const float start=chart.L+chart.W()*static_cast<float>(x);
        for(int k=0;k<12;++k)
        {
          const float a=start+chart.W()*smear*k/12.f,b=std::min(chart.R,start+chart.W()*smear*(k+1)/12.f);
          if(a>=chart.R) break;
          g.FillRect(color.WithOpacity(.15f),IRECT(a,amplitudeY(height*std::exp(-k*.3)),b,chart.B));
        }
      }
      if(effect!=SlotEffect::DigitalDelay && motion>0 && height>0.)
      {
        const float px=chart.L+chart.W()*static_cast<float>(x),width=motion*.035f;
        g.FillRect(color.WithOpacity(.12f),IRECT(std::max(chart.L,px-width),amplitudeY(height),
          std::min(chart.R,px+width),chart.B));
      }
      if(shared && tap.left>0.) stem(x,tap.left,tap.treble,delayColor,0.f);
      else
      {
        if(tap.left>0.) stem(x,tap.left,tap.treble,delayColor,-2.f);
        if(tap.right>0.) stem(x,tap.right,tap.treble,ampColor,2.f);
      }
    }
    g.DrawLine(border,chart.L,chart.B,chart.R,chart.B);
    if(echoes.horizon<1.) text.SetFormatted(32,"%.0f ms",echoes.horizon*1000.);
    else text.SetFormatted(32,"%.1f s",echoes.horizon);
    PlotFooter(g,plot,"Bright: 5k | -60 to +12 dB",text.Get());
    return true;
  }
  if (effect == SlotEffect::Freeze || effect == SlotEffect::Stutter || effect == SlotEffect::ReverseDelay)
  {
    const bool freeze = effect == SlotEffect::Freeze, reverse = effect == SlotEffect::ReverseDelay;
    const double window = freeze ? value(uFreezeWindow)
                          : c.sync ? 1000. * grid_tempo::Seconds(tempo, c.division)
                          : value(reverse ? uDelayTime : uStutterTime);
    const double fade = reverse ? 5. : value(freeze ? uFreezeFade : uStutterFade);
    const double period = effect_preview::CapturePeriod(effect, window, fade);
    if (freeze) text.SetFormatted(64, "%s | %.0f ms loop", value(uFreezeHold) >= .5 ? "Hold requested" : "Hold off", period * 1000.);
    else text.SetFormatted(64, reverse ? "Reverse | %.0f ms" : "Slice | %.0f ms", window);
    caption(text.Get());
    const double duty = freeze || reverse ? 1. : value(uStutterDuty) * .01;
    const double overlap = std::min(.25, fade / std::max(1., window));
    const double wet = value(freeze ? uFreezeMix : reverse ? uDelayMix : uStutterMix) * .01;
    PlotDetail(g,plot,freeze ? "Dry stays full; traces show seam blend" : reverse ? "Arrows: playback | shaded: seam blend" : "Wet gate + dry blend | 3 windows");
    PlotGrid(g,plot);
    curve([&](double x) {
      const double phase = std::fmod(x * 3., 1.);
      const double envelope = std::clamp(std::min(phase / std::max(.001, overlap), (duty - phase) / std::max(.001, overlap)), 0., 1.);
      if (freeze || reverse)
        return wet*(phase<1.-overlap ? 1. : (1.-phase)/overlap);
      return 1.-wet+wet*envelope;
    }, color);
    if (freeze || reverse)
      curve([&](double x) { return wet*std::max(0.,(std::fmod(x*3.,1.)-(1.-overlap))/overlap); },ampColor);
    for (int i = 0; i < 3; ++i)
    {
      const float left = plot.L + plot.W() * i / 3.f, right = left + plot.W() / 3.f;
      g.FillRect(color.WithOpacity(.08f), IRECT(right - plot.W() / 3.f * static_cast<float>(overlap), plot.T, right, plot.B));
      if (reverse)
      {
        line((i + .8) / 3., .5, (i + .2) / 3., .5, muted);
        line((i + .2) / 3., .5, (i + .35) / 3., .65, muted);
      }
    }
    text.SetFormatted(64,"%.0f ms",3.*period*1000.);
    WDL_String note;
    if(freeze || reverse) note.SetFormatted(80,"High cut %.1fk | %s",value(freeze ? uFreezeTone : uDelayTone)/1000.,freeze ? "dry + held" : "reverse seams");
    else note.Set("0 - 100% gain");
    PlotFooter(g,plot,note.Get(),text.Get());
    return true;
  }
  if (effect == SlotEffect::Octaver)
  {
    text.SetFormatted(64, "Octaves | high cut %.0f Hz", value(uOctTone)); caption(text.Get());
    PlotDetail(g,plot,"Layer gains; high cut affects subs only");
    const char* labels[]{"Dry", "-1 oct", "-2 oct"};
    const double gain = std::pow(10., value(uOctOutput) / 20.);
    for (int i = 0; i < 3; ++i)
    {
      const float x = plot.L + plot.W() * (i + .5f) / 3.f;
      const double height = value(uOctDry + i) * .01 * gain;
      g.FillRect((i ? color : muted).WithOpacity(.75f), IRECT(x - 9, plot.B - 22 - (plot.H()-22)*static_cast<float>(height/2.), x + 9, plot.B - 22));
      g.DrawText(Text(13.f, muted, EAlign::Center), labels[i], IRECT(x - 25, plot.B - 16, x + 25, plot.B));
    }
    PlotFooter(g,plot,"Gain scale 0 - 2x (+6 dB)");
    return true;
  }
  if (effect == SlotEffect::Doubler)
  {
    text.SetFormatted(64, "+/-%.1f ct | %.0f ms", value(uDoubleDetune), value(uDoubleDelay)); caption(text.Get());
    PlotDetail(g,plot,"Pitch up/down | horizontal: stereo spread");
    const double spread = value(uDoubleSpread) * .01;
    line(0., .5, 1., .5, border);
    for (int voice : {-1, 1})
    {
      // Width changes stereo placement, not the actual pitch offsets. Mix is
      // in the readout so a quiet wet setting does not hide the voice positions.
      const double x = .5 + voice * .25 * spread, y = .5 - voice * value(uDoubleDetune) / 60.;
      const auto ink = voice < 0 ? color : ampColor;
      line(.5, .5, x, y, ink.WithOpacity(.4f));
      line(x - .12, y, x + .12, y, ink, 2.f);
    }
    text.SetFormatted(64,"Wet %.0f%% | Out %+.0f dB",value(uDoubleMix),value(uDoubleOutput));
    foot(text.Get()); return true;
  }
  if (effect == SlotEffect::AutoSwell)
  {
    const double rise = value(uSwellRise), release = value(uSwellRelease), hold = 40.;
    const double threshold = value(uSwellThreshold);
    const double level = 20. * std::log10(std::max(1.e-8f, inputLevel));
    const IRECT meter(plot.L, plot.T, plot.L + 66.f, plot.B);
    const IRECT envelope(meter.R + 16.f, plot.T, plot.R, plot.B);
    text.SetFormatted(64, "Trigger %.0f dB", threshold); caption(text.Get());
    PlotDetail(g,plot,"Rise above Thr | hold resets above Thr | release below Thr -3 dB");
    PlotGrid(g,envelope);
    const auto meterY = [&](double db) {
      return meter.B - meter.H() * static_cast<float>(std::clamp((db + 72.) / 72., 0., 1.));
    };
    g.DrawRect(border,meter);
    g.FillRect(color.WithOpacity(.65f),IRECT(meter.L + 18.f,meterY(level),meter.R - 18.f,meter.B));
    const float thresholdY = meterY(threshold);
    for(float x=meter.L;x<meter.R;x+=7.f)
      g.DrawLine(ampColor,x,thresholdY,std::min(x+4.f,meter.R),thresholdY);
    g.DrawText(Text(12.f,muted,EAlign::Center),"IN",IRECT(meter.L,meter.B-17.f,meter.R,meter.B));
    // Give the fixed 40 ms hold enough room to be legible even with a long rise
    // or release. Each labeled section has its own time scale.
    const float riseEnd = envelope.L + envelope.W() * .35f;
    const float holdEnd = envelope.L + envelope.W() * .58f;
    g.FillRect(color.WithOpacity(.06f),IRECT(riseEnd,envelope.T,holdEnd,envelope.B));
    PlotCurve(g,envelope,[&](double x) {
      return x<.35 ? x/.35 : x<.58 ? 1. : std::exp(-3.*(x-.58)/.42);
    },0.,1.,color);
    for (float x : {riseEnd,holdEnd})
      g.DrawLine(border.WithOpacity(.7f),x,envelope.T,x,envelope.B);
    text.SetFormatted(32,"Rise %.0f ms",rise);
    g.DrawText(Text(12.f,muted,EAlign::Center),text.Get(),IRECT(envelope.L,envelope.B-17.f,riseEnd,envelope.B));
    text.SetFormatted(32,"Hold %.0f ms",hold);
    g.DrawText(Text(12.f,muted,EAlign::Center),text.Get(),IRECT(riseEnd,envelope.B-17.f,holdEnd,envelope.B));
    text.SetFormatted(32,"Release %.0f ms tau",release);
    g.DrawText(Text(12.f,muted,EAlign::Center),text.Get(),IRECT(holdEnd,envelope.B-17.f,envelope.R,envelope.B));
    PlotFooter(g,plot,"Sections use separate time scales","IN dBFS"); return true;
  }
  if (effect == SlotEffect::FrequencyShifter)
  {
    text.SetFormatted(64, "Shift %+.0f Hz / pass", value(uShiftFrequency)); caption(text.Get());
    const double shift = value(uShiftFrequency), side = value(uShiftSide) * .01;
    const double upper = std::max(1200., 880. + std::abs(shift));
    const double mix = value(uShiftMix) * .01, feedback = value(uShiftFeedback) * .01;
    text.SetFormatted(96,"FB %.0f%% / %.0f ms | Spread %.0f%%",feedback*100.,value(uShiftDelay),value(uShiftStereo));
    PlotDetail(g,plot,text.Get()); PlotGrid(g,plot);
    for (int partial = 1; partial <= 4; ++partial)
    {
      const double dryHz = 220. * partial;
      line(dryHz / upper, 0., dryHz / upper, .6 * (1. - mix), muted);
      if(std::abs(shift)<1.e-6)
        line(dryHz/upper,0.,dryHz/upper,mix*.65*effect_preview::LowPass(dryHz,value(uShiftTone),rate,1,.45),color,2.f);
      else
        for (int sign : {-1, 1})
        {
          const double hz = std::abs(dryHz + sign * shift);
          const double amplitude = mix * .65 * .5 * (1. + sign * side)*effect_preview::LowPass(hz,value(uShiftTone),rate,1,.45);
          if (hz <= upper) line(hz / upper, 0., hz / upper, amplitude, sign>0 ? color : ampColor,2.f);
        }
    }
    text.SetFormatted(32,"0 - %.1fk Hz",upper/1000.); PlotFooter(g,plot,"First pass: 220 Hz harmonics",text.Get()); return true;
  }
  if (effect == SlotEffect::Utility)
  {
    text.SetFormatted(64, "M/S | side %.0f%%", 100. + value(uUtilityStereo)); caption(text.Get());
    PlotDetail(g,plot,"Width scales existing stereo; mono stays mono");
    const double width = 1. + value(uUtilityStereo) * .01, balance = value(uUtilityBalance) * .01;
    const double gain = std::pow(10., value(uUtilityLevel) / 20.);
    for (int channel = 0; channel < 2; ++channel)
    {
      const double level = gain * (1. - std::max(0., channel ? -balance : balance));
      const double y = channel ? .3 : .7, end = .5 + (channel ? 1. : -1.) * .24 * width;
      const auto ink=(channel ? ampColor : color).WithOpacity(static_cast<float>(.15+.85*std::min(1.,level)));
      line(.5, .5, end, y, ink, static_cast<float>(1. + std::min(3., level)));
      const bool inverted = value(channel ? uUtilityInvertR : uUtilityInvertL) >= .5;
      g.DrawText(Text(13.f, ink, EAlign::Center), inverted ? "-" : "+",
                 IRECT(plot.L + plot.W() * static_cast<float>(end) - 8, plot.B - plot.H() * static_cast<float>(y) - 8,
                       plot.L + plot.W() * static_cast<float>(end) + 8, plot.B - plot.H() * static_cast<float>(y) + 8));
    }
    text.SetFormatted(64,"L / R | both delayed %.2f ms",value(uUtilityDelay)); foot(text.Get()); return true;
  }
  return false;
}
} // namespace grid_ui
