#pragma once
#include "EffectPreview.h"

namespace grid_ui
{
template<class Value>
bool DrawEffectSetup(IGraphics& g, SlotEffect effect, const IRECT& plot, IColor color,
                     Value value, double tempo, double rate)
{
  const auto caption = [&](const char* text) { g.DrawText(Text(11.f, color), text, IRECT(plot.L, 490, plot.R, 518)); };
  const auto foot = [&](const char* text) { g.DrawText(Text(11.f, muted, EAlign::Far), text, IRECT(plot.L, plot.B - 10, plot.R, plot.B)); };
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
    g.DrawText(Text(14.f,color),text.Get(),IRECT(plot.L,490,plot.R,514));
    const int motion=static_cast<int>(value(uDelayMotion));
    if(effect==SlotEffect::DiffusedDelay) text.SetFormatted(64,"Diff %.0f%% | Mod %d%%",value(uDelayDiffusion),motion);
    else if(effect==SlotEffect::TapeDelay || heads) text.SetFormatted(64,"Mod %d%% | Sat %.0f%%",motion,c.delay[3]);
    else if(effect==SlotEffect::AnalogDelay) text.SetFormatted(64,"Age %.0f%% | Mod %d%%",c.delay[3],motion);
    else text.SetFormatted(64,"High cut %.1f kHz",c.delay[2]/1000.);
    g.DrawText(Text(12.f,muted),text.Get(),IRECT(plot.L,515,plot.R-50,536));
    g.DrawText(Text(12.f,delayColor),stereo ? "L" : "L=R",IRECT(plot.R-45,515,plot.R-23,536));
    if(stereo) g.DrawText(Text(12.f,ampColor),"R",IRECT(plot.R-17,515,plot.R,536));
    double maximum = .001;
    for (size_t i = 0; i < echoes.count; ++i)
      maximum = std::max({maximum, echoes.taps[i].left, echoes.taps[i].right});
    const IRECT chart(plot.L,542,plot.R,668);
    const auto stem = [&](double x, double height, IColor ink, float width) {
      const float px = chart.L + chart.W() * static_cast<float>(std::clamp(x, 0., 1.));
      g.DrawLine(ink, px, chart.B, px, chart.B - chart.H() * static_cast<float>(height * .9 / maximum), nullptr, width);
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
          g.FillRect(color.WithOpacity(.2f),IRECT(a,chart.B-chart.H()*static_cast<float>(height/maximum)*(1.f-k/12.f),b,chart.B));
        }
      }
      if(effect!=SlotEffect::DigitalDelay && motion>0 && height>0.)
      {
        const float px=chart.L+chart.W()*static_cast<float>(x),width=motion*.035f;
        g.FillRect(color.WithOpacity(.12f),IRECT(std::max(chart.L,px-width),chart.B-chart.H()*static_cast<float>(height/maximum)*.9f,
          std::min(chart.R,px+width),chart.B));
      }
      if(shared && tap.left>0.) stem(x,tap.left,delayColor,2.f);
      else
      {
        if(tap.left>0.) stem(x,tap.left,delayColor,2.f);
        if(tap.right>0.) stem(x,tap.right,ampColor,2.f);
      }
    }
    g.DrawLine(border,chart.L,chart.B,chart.R,chart.B);
    g.DrawText(Text(12.f,muted),stereoHeads ? "Cross-feedback" : "Relative taps",IRECT(plot.L,671,plot.R-50,690));
    text.SetFormatted(32,"%.1fs",echoes.horizon);
    g.DrawText(Text(12.f,muted,EAlign::Far),text.Get(),IRECT(plot.R-50,671,plot.R,690));
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
    if (freeze) text.SetFormatted(64, "%s | %.0f ms loop", value(uFreezeHold) >= .5 ? "Hold" : "Ready", period * 1000.);
    else text.SetFormatted(64, reverse ? "Reverse | %.0f ms" : "Slice | %.0f ms", window);
    caption(text.Get());
    const double duty = freeze || reverse ? 1. : value(uStutterDuty) * .01;
    const double overlap = std::min(.25, fade / std::max(1., window));
    const double wet = value(freeze ? uFreezeMix : reverse ? uDelayMix : uStutterMix) * .01;
    curve([&](double x) {
      const double phase = std::fmod(x * 3., 1.);
      const double envelope = std::clamp(std::min(phase / std::max(.001, overlap), (duty - phase) / std::max(.001, overlap)), 0., 1.);
      return .12 + .7 * wet * (freeze ? 1. : envelope);
    }, color);
    for (int i = 0; i < 3; ++i)
    {
      const float left = plot.L + plot.W() * i / 3.f, right = left + plot.W() / 3.f;
      g.DrawRect(color.WithOpacity(.25f), IRECT(right - plot.W() / 3.f * static_cast<float>(overlap), plot.T, right, plot.B - 11));
      if (reverse)
      {
        line((i + .8) / 3., .5, (i + .2) / 3., .5, muted);
        line((i + .2) / 3., .5, (i + .35) / 3., .65, muted);
      }
    }
    foot(freeze ? "Overlap shortens loop" : reverse ? "Fresh reversed windows" : "Gate / edge fade");
    return true;
  }
  if (effect == SlotEffect::Octaver)
  {
    text.SetFormatted(64, "Octaves | high cut %.0f Hz", value(uOctTone)); caption(text.Get());
    const char* labels[]{"Dry", "-1 oct", "-2 oct"};
    const double gain = std::pow(10., value(uOctOutput) / 20.);
    for (int i = 0; i < 3; ++i)
    {
      const float x = plot.L + plot.W() * (i + .5f) / 3.f;
      const double height = value(uOctDry + i) * .01 * gain;
      g.FillRect((i ? color : muted).WithOpacity(.75f), IRECT(x - 9, plot.B - 12 - plot.H() * static_cast<float>(std::min(.75, .65 * height)), x + 9, plot.B - 12));
      g.DrawText(Text(11.f, muted, EAlign::Center), labels[i], IRECT(x - 25, plot.B - 11, x + 25, plot.B));
    }
    return true;
  }
  if (effect == SlotEffect::Doubler)
  {
    text.SetFormatted(64, "+/-%.1f ct | %.0f ms", value(uDoubleDetune), value(uDoubleDelay)); caption(text.Get());
    const double spread = value(uDoubleSpread) * .01, mix = value(uDoubleMix) * .01;
    line(0., .5, 1., .5, border);
    for (int voice : {-1, 1})
    {
      // Width changes stereo placement, not the actual pitch offsets. Mix
      // changes prominence; a dry blend must not appear to retune the voices.
      const double x = .5 + voice * .25 * spread, y = .5 + voice * value(uDoubleDetune) / 60.;
      const auto ink = (voice < 0 ? muted : color).WithOpacity(.2f + .8f * static_cast<float>(mix));
      line(.5, .5, x, y, ink.WithOpacity(.4f));
      line(x - .12, y, x + .12, y, ink, 2.f);
    }
    foot("L/R voice spread"); return true;
  }
  if (effect == SlotEffect::AutoSwell)
  {
    const double rise = value(uSwellRise), release = value(uSwellRelease), total = 1.5 * (rise + release);
    text.SetFormatted(64, "Trigger %.0f dB", value(uSwellThreshold)); caption(text.Get());
    curve([&](double x) { const double t = x * total; return .1 + .8 * (t < rise ? t / rise : std::exp(-(t - rise) / release)); }, color);
    text.SetFormatted(64, "Rise / release | %.1f s", total * .001); foot(text.Get()); return true;
  }
  if (effect == SlotEffect::FrequencyShifter)
  {
    text.SetFormatted(64, "Shift %+.0f Hz / pass", value(uShiftFrequency)); caption(text.Get());
    const double shift = value(uShiftFrequency), side = value(uShiftSide) * .01;
    const double upper = std::max(1200., 880. + std::abs(shift) * 3.);
    const double mix = value(uShiftMix) * .01, feedback = value(uShiftFeedback) * .01;
    for (int partial = 1; partial <= 4; ++partial)
    {
      const double dryHz = 220. * partial;
      line(dryHz / upper, 0., dryHz / upper, .6 * (1. - mix), muted);
      for (int pass = 1; pass <= 3; ++pass)
        for (int sign : {-1, 1})
        {
          const double hz = std::abs(dryHz + sign * pass * shift);
          const double amplitude = mix * .65 * .5 * (1. + sign * side) * std::pow(feedback, pass - 1);
          if (hz <= upper) line(hz / upper, 0., hz / upper, amplitude, color, pass == 1 ? 2.f : 1.f);
        }
    }
    foot("220 Hz harmonic sketch"); return true;
  }
  if (effect == SlotEffect::Utility)
  {
    text.SetFormatted(64, "M/S | side %.0f%%", 100. + value(uUtilityStereo)); caption(text.Get());
    const double width = 1. + value(uUtilityStereo) * .01, balance = value(uUtilityBalance) * .01;
    const double gain = std::pow(10., value(uUtilityLevel) / 20.);
    for (int channel = 0; channel < 2; ++channel)
    {
      const double level = gain * (1. - std::max(0., channel ? -balance : balance));
      const double y = channel ? .3 : .7, end = .5 + (channel ? 1. : -1.) * .24 * width;
      line(.5, .5, end, y, channel ? muted : color, static_cast<float>(1. + std::min(3., level)));
      const bool inverted = value(channel ? uUtilityInvertR : uUtilityInvertL) >= .5;
      g.DrawText(Text(11.f, channel ? muted : color, EAlign::Center), inverted ? "-" : "+",
                 IRECT(plot.L + plot.W() * static_cast<float>(end) - 8, plot.B - plot.H() * static_cast<float>(y) - 8,
                       plot.L + plot.W() * static_cast<float>(end) + 8, plot.B - plot.H() * static_cast<float>(y) + 8));
    }
    text.SetFormatted(64, "Offset %.2f ms", value(uUtilityDelay)); foot(text.Get()); return true;
  }
  return false;
}
} // namespace grid_ui
