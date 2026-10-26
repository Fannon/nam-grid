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
  if (effect == SlotEffect::Wah)
  {
    UtilityControls wah;
    for (int i=0;i<5;++i) wah.wah[i]=value(uWahPosition+i);
    wah.extra.wahSweep=value(uWahSweep);
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
    caption("Sample hold / quantization");
    PlotDetail(g,plot,"One 220 Hz cycle, 100% wet");
    // The classic crusher picture: the held, quantized staircase filled to zero over the dry sine.
    const auto cycle=effect_preview::CrusherReference(crush,rate);
    const size_t count=cycle.held.size();
    const auto x=[&](size_t i) { return plot.L+plot.W()*static_cast<float>(i)/static_cast<float>(count-1); };
    const auto y=[&](double v) { return plot.MH()-plot.H()*.5f*static_cast<float>(std::clamp(v,-1.,1.)); };
    g.DrawLine(muted.WithOpacity(.5f),plot.L,plot.MH(),plot.R,plot.MH());
    for (size_t i=0;i+1<count;++i)
    {
      const float top=y(cycle.held[i]);
      g.FillRect(color.WithOpacity(.18f),IRECT(x(i),std::min(top,plot.MH()),x(i+1),std::max(top,plot.MH())));
    }
    for (size_t i=1;i<count;++i)
      g.DrawLine(muted.WithOpacity(.7f),x(i-1),y(cycle.dry[i-1]),x(i),y(cycle.dry[i]),nullptr,1.f);
    for (size_t i=0;i+1<count;++i)
    {
      g.DrawLine(color,x(i),y(cycle.held[i]),x(i+1),y(cycle.held[i]),nullptr,1.5f);
      if (cycle.held[i+1]!=cycle.held[i]) g.DrawLine(color,x(i+1),y(cycle.held[i]),x(i+1),y(cycle.held[i+1]),nullptr,1.5f);
    }
    const bool cut=crush.crusher[2]<.45*rate-1.;
    if (cut)
      for (size_t i=1;i<count;++i)
        g.DrawLine(grid_ui::text.WithOpacity(.8f),x(i-1),y(cycle.wet[i-1]),x(i),y(cycle.wet[i]),nullptr,1.2f);
    PlotFooter(g,plot,"Gray: dry | fill: crushed",cut ? "Line: High cut" : "");
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
    const int motion=static_cast<int>(value(uDelayMotion));
    // Delay modulation moves timing by well under a pixel at these horizons; what
    // you hear is pitch. Excursion and LFO rates follow each delay's DSP.
    const double cents=effect==SlotEffect::DiffusedDelay ? effect_preview::SweepCents(motion*.01*.0007,.31)
                      : effect==SlotEffect::AnalogDelay ? effect_preview::SweepCents(motion*.01*.0008,.67)
                      : effect==SlotEffect::TapeDelay ? effect_preview::SweepCents(motion*.01*.0015,.67,2.05)
                      : heads ? effect_preview::SweepCents(motion*.01*.001,.67,2.05) : 0.;
    // Digital Delay ducks only the wet output: 1 / (1 + 12 x Ducking x envelope).
    const double duck=effect==SlotEffect::DigitalDelay ? 1./(1.+12.*c.delay[3]*.01*.25) : 1.;
    if(cents>.01)
    {
      text.SetFormatted(80,"%s | modulation +/-%.1f ct",stereo ? "Stereo repeats" : "Wet repeats",cents);
      caption(text.Get());
    }
    else caption(stereo ? "Stereo repeats" : "Wet repeats");
    PlotDetail(g,plot,"Cyan: left/shared repeats; amber: right. Thin lines show energy above 5 kHz; diffusion fills out the echoes. White ticks show ducked wet level at -12 dBFS input.");
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
    PlotFooter(g,plot,duck<.999 ? "Tick: ducked | -60 to +12 dB" : "Thin line: above 5 kHz | -60 to +12 dB",text.Get());
    return true;
  }
  if (effect == SlotEffect::Freeze || effect == SlotEffect::Stutter || effect == SlotEffect::ReverseDelay)
  {
    const bool freeze = effect == SlotEffect::Freeze, reverse = effect == SlotEffect::ReverseDelay;
    const double window = freeze ? value(uFreezeWindow)
                          : c.sync ? 1000. * grid_tempo::Seconds(tempo, c.division)
                          : value(reverse ? uDelayTime : uStutterTime);
    const double fade = reverse ? value(uReverseEnvelope) : value(freeze ? uFreezeFade : uStutterFade);
    const double period = effect_preview::CapturePeriod(effect, window, fade);
    if (freeze) text.SetFormatted(64, "%s | %.0f ms loop", value(uFreezeHold) >= .5 ? "Hold requested" : "Hold off", period * 1000.);
    else text.Set(reverse ? "Reverse windows" : "Slice pattern");
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
      WDL_String note; note.Set("Upper: input | lower: reversed output");
      PlotFooter(g,plot,note.Get(),text.Get());
      return true;
    }
    // Stutter replays one slice for four windows before grabbing a new one.
    const int windows = freeze ? 3 : 4;
    PlotDetail(g,plot,freeze ? "Dry stays full; Fade in/out shape the held layer; traces show seam blend" : value(uStutterHold) >= .5 ? "Held slice | Gate + dry blend" : "Gate + dry blend | new slice every 4");
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
    if (!freeze && value(uStutterHold) < .5)
      for (float x : {plot.L + 1.f, plot.R - 1.f})
      {
        g.DrawLine(ampColor, x, plot.T, x, plot.B, nullptr, 2.f);
        g.DrawText(Text(11.f, ampColor, x < plot.MW() ? EAlign::Near : EAlign::Far), "new slice",
                   IRECT(x < plot.MW() ? x + 3.f : x - 60.f, plot.T, x < plot.MW() ? x + 60.f : x - 3.f, plot.T + 12.f));
      }
    text.SetFormatted(64,"%.0f ms",windows*period*1000.);
    WDL_String note;
    if(freeze) note.Set("Dry + held | seam blend");
    else note.Set("0 - 100% gain");
    PlotFooter(g,plot,note.Get(),text.Get());
    return true;
  }
  if (effect == SlotEffect::Octaver)
  {
    caption("Octave waveforms");
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
    caption("Pitch / stereo sketch");
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
    foot("Lower / upper pitch | horizontal: stereo"); return true;
  }
  if (effect == SlotEffect::FrequencyShifter)
  {
    caption("Successive frequency shifts");
    const double shift = value(uShiftFrequency), side = value(uShiftSide) * .01;
    const double mix = value(uShiftMix) * .01, feedback = value(uShiftFeedback) * .01;
    // Each feedback pass shifts again by Shift, after Delay, through High cut.
    // Columns are passes; dot height is frequency, brightness is level.
    constexpr int passes = 8;
    const double cascadeUpper = std::max(1200., 880. + passes * std::abs(shift));
    PlotDetail(g,plot,"Columns are successive feedback passes; height is frequency and brightness is level. Colour shifts up, amber shifts down; each pass follows Delay and High cut."); PlotGrid(g,plot);
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
  return false;
}
} // namespace grid_ui
