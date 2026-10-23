#pragma once
#include "EffectPreview.h"

namespace grid_ui
{
template<class Value>
bool DrawEffectSetup(IGraphics& g, SlotEffect effect, const IRECT& plot, IColor color,
                     Value value, double tempo, double rate)
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
    // Delay modulation moves timing by well under a pixel at these horizons; what
    // you hear is pitch. Excursion and LFO rates follow each delay's DSP.
    const double cents=effect==SlotEffect::DiffusedDelay ? effect_preview::SweepCents(motion*.01*.0007,.31)
                      : effect==SlotEffect::AnalogDelay ? effect_preview::SweepCents(motion*.01*.0008,.67)
                      : effect==SlotEffect::TapeDelay ? effect_preview::SweepCents(motion*.01*.0015,.67,2.05)
                      : heads ? effect_preview::SweepCents(motion*.01*.001,.67,2.05) : 0.;
    // Digital Delay ducks only the wet output: 1 / (1 + 12 x Ducking x envelope).
    const double duck=effect==SlotEffect::DigitalDelay ? 1./(1.+12.*c.delay[3]*.01*.25) : 1.;
    if(effect==SlotEffect::DiffusedDelay) text.SetFormatted(64,"Diff %.0f%% | Mod +/-%.1f ct",value(uDelayDiffusion),cents);
    else if(effect==SlotEffect::TapeDelay || heads) text.SetFormatted(64,"Mod +/-%.1f ct | Sat %.0f%%",cents,c.delay[3]);
    else if(effect==SlotEffect::AnalogDelay) text.SetFormatted(64,"Age %.0f%% | Mod +/-%.1f ct",c.delay[3],cents);
    else if(duck<.999) text.SetFormatted(64,"Ducked %.1f dB at -12 dBFS",20.*std::log10(duck));
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
      // Ducked level while playing at -12 dBFS; the full stem returns when you stop.
      if(duck<.999) g.DrawLine(grid_ui::text,px-4.f,amplitudeY(height*duck),px+4.f,amplitudeY(height*duck),nullptr,1.5f);
    };
    // One marker per channel/tap. Shared L/R taps draw once. Diffusion clusters
    // and the M/S badges are explicitly setup cues.
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
    PlotFooter(g,plot,duck<.999 ? "Tick: ducked | -60 to +12 dB" : "Bright: 5k | -60 to +12 dB",text.Get());
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
    if (reverse)
    {
      // Each window is captured, then played backwards during the next one:
      // what you hear lags what you play by one to two windows.
      PlotDetail(g,plot,"Top: input | below: plays reversed one window later");
      const char* names[]{"A","B","C","D"};
      const float width = plot.W() / 4.f, top = plot.T + 6.f, mid = plot.MH(), bottom = plot.B - 6.f;
      for (int i = 0; i < 4; ++i)
      {
        const float left = plot.L + width * i;
        g.DrawRect(muted.WithOpacity(.7f), IRECT(left + 2.f, top, left + width - 2.f, mid - 4.f));
        g.DrawText(Text(13.f, muted, EAlign::Center), names[i], IRECT(left, top, left + width, mid - 4.f));
        if (i == 0) continue;
        const IRECT played(left + 2.f, mid + 4.f, left + width - 2.f, bottom);
        g.FillRect(color.WithOpacity(static_cast<float>(.12 + .5 * wet)), played);
        text.SetFormatted(16, "%s rev", names[i - 1]);
        g.DrawText(Text(13.f, color, EAlign::Center), text.Get(), played.GetFromTop(played.H() * .6f));
        const float arrowY = played.B - 6.f;
        g.DrawLine(color, played.R - 6.f, arrowY, played.L + 6.f, arrowY);
        g.DrawLine(color, played.L + 6.f, arrowY, played.L + 11.f, arrowY - 3.f);
        g.DrawLine(color, played.L + 6.f, arrowY, played.L + 11.f, arrowY + 3.f);
      }
      text.SetFormatted(64,"%.0f ms",4.*period*1000.);
      WDL_String note; note.SetFormatted(80,"High cut %.1fk | %.0f%% wet",value(uDelayTone)/1000.,wet*100.);
      PlotFooter(g,plot,note.Get(),text.Get());
      return true;
    }
    // Stutter replays one slice for four windows before grabbing a new one.
    const int windows = freeze ? 3 : 4;
    PlotDetail(g,plot,freeze ? "Dry stays full; traces show seam blend" : "Gate + dry blend | new slice every 4");
    PlotGrid(g,plot);
    curve([&](double x) {
      const double phase = std::fmod(x * windows, 1.);
      const double envelope = std::clamp(std::min(phase / std::max(.001, overlap), (duty - phase) / std::max(.001, overlap)), 0., 1.);
      if (freeze || reverse)
        return wet*(phase<1.-overlap ? 1. : (1.-phase)/overlap);
      return 1.-wet+wet*envelope;
    }, color);
    if (freeze)
      curve([&](double x) { return wet*std::max(0.,(std::fmod(x*3.,1.)-(1.-overlap))/overlap); },ampColor);
    for (int i = 0; i < windows; ++i)
    {
      const float left = plot.L + plot.W() * i / windows, right = left + plot.W() / windows;
      g.FillRect(color.WithOpacity(.08f), IRECT(right - plot.W() / windows * static_cast<float>(overlap), plot.T, right, plot.B));
    }
    if (!freeze)
      for (float x : {plot.L + 1.f, plot.R - 1.f})
      {
        g.DrawLine(ampColor, x, plot.T, x, plot.B, nullptr, 2.f);
        g.DrawText(Text(11.f, ampColor, x < plot.MW() ? EAlign::Near : EAlign::Far), "new slice",
                   IRECT(x < plot.MW() ? x + 3.f : x - 60.f, plot.T, x < plot.MW() ? x + 60.f : x - 3.f, plot.T + 12.f));
      }
    text.SetFormatted(64,"%.0f ms",windows*period*1000.);
    WDL_String note;
    if(freeze) note.SetFormatted(80,"High cut %.1fk | dry + held",value(uFreezeTone)/1000.);
    else note.Set("0 - 100% gain");
    PlotFooter(g,plot,note.Get(),text.Get());
    return true;
  }
  if (effect == SlotEffect::Octaver)
  {
    text.SetFormatted(64, "Octaves | high cut %.0f Hz", value(uOctTone)); caption(text.Get());
    PlotDetail(g,plot,"110 Hz note: subs are flip-flop squares; Tone rounds them");
    // The tracker flips a square on each rising crossing (-1) and on every
    // second flip (-2); the subs follow the input envelope through Tone's low-pass.
    const char* labels[]{"Dry", "-1", "-2"};
    const double gain = std::pow(10., value(uOctOutput) / 20.);
    const double pole = std::exp(-2. * grid_fx::pi * std::min(value(uOctTone), .45 * rate) / rate);
    const int samples = static_cast<int>(rate * 4. / 110.);
    for (int layer = 0; layer < 3; ++layer)
    {
      const double level = value(uOctDry + layer) * .01 * gain;
      const float center = plot.T + plot.H() * (layer + .5f) / 3.f, height = plot.H() / 6.f - 2.f;
      g.DrawText(Text(11.f, muted), labels[layer], IRECT(plot.L, center - 7.f, plot.L + 20.f, center + 7.f));
      double low = 0., previousY = center;
      for (int i = 0; i <= samples; ++i)
      {
        const double phase = 110. * i / rate;
        const double dry = std::sin(2. * grid_fx::pi * phase);
        const double square = layer == 1 ? (std::fmod(phase, 2.) < 1. ? 1. : -1.) : (std::fmod(phase, 4.) < 2. ? 1. : -1.);
        low = (layer ? square : dry) + (layer ? pole : 0.) * (low - (layer ? square : dry));
        const double y = center - height * std::clamp(level * .5 * (layer ? low : dry), -1., 1.);
        if (i)
          g.DrawLine((layer ? color : muted).WithOpacity(level > 0. ? 1.f : .25f),
                     plot.L + 22.f + (plot.W() - 22.f) * (i - 1) / samples, static_cast<float>(previousY),
                     plot.L + 22.f + (plot.W() - 22.f) * i / samples, static_cast<float>(y));
        previousY = y;
      }
    }
    PlotFooter(g,plot,"Lane height = layer gain (0 - 2x)","36 ms");
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
  if (effect == SlotEffect::FrequencyShifter)
  {
    text.SetFormatted(64, "Shift %+.0f Hz / pass", value(uShiftFrequency)); caption(text.Get());
    const double shift = value(uShiftFrequency), side = value(uShiftSide) * .01;
    const double mix = value(uShiftMix) * .01, feedback = value(uShiftFeedback) * .01;
    // Each feedback pass shifts again by Shift, after Delay, through High cut.
    // Columns are passes; dot height is frequency, brightness is level.
    constexpr int passes = 8;
    const double cascadeUpper = std::max(1200., 880. + passes * std::abs(shift));
    text.SetFormatted(96,"Pass every %.0f ms | FB %.0f%% | Spread %.0f%%",value(uShiftDelay),feedback*100.,value(uShiftStereo));
    PlotDetail(g,plot,text.Get()); PlotGrid(g,plot);
    const auto columnX = [&](int pass) { return (pass + .5) / (passes + 1.); };
    for (int partial = 1; partial <= 4; ++partial)
    {
      const double dryHz = 220. * partial;
      line(columnX(0) - .03, dryHz / cascadeUpper, columnX(0) + .03, dryHz / cascadeUpper, muted.WithOpacity(static_cast<float>(.3 + .7 * (1. - mix))), 2.f);
      for (int sign : {1, -1})
      {
        const double weight = std::abs(shift) < 1.e-6 ? (sign > 0 ? 1. : 0.) : .5 * (1. + sign * side);
        double hz = dryHz, amplitude = mix;
        for (int pass = 1; pass <= passes && weight > 0.; ++pass)
        {
          const double next = std::abs(hz + sign * shift);
          amplitude *= weight * effect_preview::LowPass(next, value(uShiftTone), rate, 1, .45) * (pass > 1 ? feedback : 1.);
          if (amplitude < .01 || next > cascadeUpper) break;
          line(columnX(pass - 1), hz / cascadeUpper, columnX(pass), next / cascadeUpper,
               (sign > 0 ? color : ampColor).WithOpacity(static_cast<float>(std::min(1., .15 + amplitude))), 1.f);
          hz = next;
        }
      }
    }
    text.SetFormatted(32,"0 - %.1fk Hz",cascadeUpper/1000.);
    PlotFooter(g,plot,"Passes 0-8 | up colour, down amber",text.Get()); return true;
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
