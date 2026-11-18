#pragma once
#include "ExpandedDelayPreview.h"

namespace grid_ui
{
namespace pattern_view
{
inline IRECT Plot()
{ return {grid_layout::visualizationLeft + 12, grid_layout::plotTop, grid_layout::visualizationRight - 12, grid_layout::plotBottom}; }
inline IRECT Taps(const IRECT& plot) { return {plot.L + 14.f, plot.T + 12.f, plot.R - 14.f, plot.B - 66.f}; }
inline IRECT Controls(const IRECT& plot) { return {plot.L, plot.B - 58.f, plot.R, plot.B - 35.f}; }
inline IRECT Returns(const IRECT& plot) { return {plot.L, plot.B - 29.f, plot.R, plot.B}; }
inline float X(const IRECT& area, double time) { return area.L + area.W() * static_cast<float>(time); }
inline float Y(const IRECT& area, double level) { return area.B - area.H() * static_cast<float>(level); }
} // namespace pattern_view

template<class Function>
void DrawDelayTrace(IGraphics& g, const IRECT& area, Function&& gain, IColor color, float offset = 0.f)
{
  for (int i = 1; i < delay_preview::bins; ++i)
  {
    const double amplitude = std::abs(gain(i));
    if (amplitude < .001) continue;
    const float x = area.L + (area.W() - 2.f) * i / (delay_preview::bins - 1.f) + offset;
    const double db = delay_preview::Decibels(amplitude);
    const float height = area.H() * static_cast<float>(std::clamp(1. + db / 54., 0., 1.));
    g.DrawLine(color, x, area.B, x, area.B - height, nullptr, 1.6f);
  }
}

template<class Value>
void DrawExpandedDelaySetup(IGraphics& g, SlotEffect effect, const IRECT& plot, IColor color,
                            Value value, const UtilityControls& controls, double rate)
{
  using namespace delay_preview;
  const int selector = ExpandedDelayModeControl(effect);
  const auto choices = EffectChoices(selector);
  const int mode = std::clamp(static_cast<int>(value(selector)), 0, static_cast<int>(choices.size()) - 1);
  const double ms = 1000. * effect_preview::DelaySeconds(controls);
  const double feedback = .01 * value(uDelayFeedback), tone = value(uDelayTone);
  const double depth = .01 * value(uEchoDepth);
  WDL_String title, footer;
  const auto name = choices[mode];
  const auto frequencyGrid = [&](double upper) {
    PlotFrequency(g, plot, 20., upper);
    PlotGrid(g, plot);
    for (double hz : {100., 1000., 10000.})
      if (hz < upper)
      {
        const float x = plot.L + plot.W() * static_cast<float>(std::log(hz / 20.) / std::log(upper / 20.));
        g.DrawLine(border.WithOpacity(.35f), x, plot.T, x, plot.B);
      }
  };
  switch (effect)
  {
    case SlotEffect::PatternDelay:
    {
      const auto taps = PatternTaps(value);
      const auto area = pattern_view::Taps(plot), returns = pattern_view::Returns(plot);
      const double width = .01 * value(uEchoWidth);
      PlotGrid(g, area);
      PlotGrid(g, returns);
      for (int i = 1; i <= 4; ++i)
      {
        const auto x = pattern_view::X(area, .25 * i);
        g.DrawLine(border.WithOpacity(.45f), x, area.T, x, area.B);
      }
      for (const auto& tap : taps)
      {
        const float x = pattern_view::X(area, tap.time);
        for (int ch = 0; ch < 2; ++ch)
        {
          const float side = x + (ch ? 2.f : -2.f);
          const auto ink = ch ? ampColor : color;
          g.DrawLine(ink, side, area.B, side, pattern_view::Y(area, TapGain(tap, ch, width)), nullptr, 2.f);
        }
      }
      const auto echoes = PatternReturns(taps, mode, feedback, width);
      for (int ch = 0; ch < 2; ++ch)
        DrawDelayTrace(g, returns, [&](int i) { return echoes[ch][i]; },
                       (ch ? ampColor : color).WithOpacity(.65f), ch ? 1.f : 0.f);
      title.SetFormatted(72, "%s  |  First taps, 0–%.0f ms", name, ms);
      footer.SetFormatted(72, "Feedback sketch, 0–%.0f ms  |  Cyan L / amber R", 3. * ms);
      PlotDetail(g,plot,"Drag a numbered tap for timing and level. Choose a tap by number and drag on the pan track below. The lower plot estimates later echoes with the chosen feedback routing.");
      break;
    }
    case SlotEffect::SpaceTapeEcho:
    {
      const double horizon = effect_preview::TimeHorizon(std::max(2.5 * ms * .001, value(uEchoSpring) > 0. ? .22 : .01));
      const auto echoes = TapeRepeats(mode, feedback, ms * .001, horizon);
      const double spring = .01 * value(uEchoSpring);
      PlotGrid(g, plot);
      if (spring > 0.)
        PlotCurve(g, plot, [&](double x) {
          const double t = x * horizon;
          return spring * std::clamp(t / .06, 0., 1.) * std::exp(-6.907755 * std::max(0., t - .05) / 1.55);
        }, 0., 1., reverbColor.WithOpacity(.7f));
      DrawDelayTrace(g, plot, [&](int i) { return echoes[i]; }, color);
      // Upper bound from both transport derivatives; the real phases can also cancel.
      const double motion = .01 * value(uDelayMotion);
      const double wow = .43 * std::clamp(std::sqrt(420. / ms), .55, 2.5);
      const double pitchBound = effect_preview::SweepCents(motion * (.0028 + .00038 * (6.7 / .43) * 1.2), wow);
      title.SetFormatted(90, "%s  |  Tape motion up to %.0f ct", name, pitchBound);
      footer.SetFormatted(85, "0–%.0f ms  |  Tape / spring decay sketch", horizon * 1000.);
      PlotDetail(g,plot,"Cyan shows nominal head arrivals and summed feedback. Purple shows a separate spring decay guide at the current Spring level. Tape speed changes the motion rate; earlier heads move less.");
      break;
    }
    case SlotEffect::BucketBrigade:
    {
      // Repeats over time on Analog Echo's dB scale; the thin line is what survives above 2 kHz.
      const double horizon = effect_preview::TimeHorizon(4. * ms * .001);
      const double gain = std::pow(10., value(uDelayOutput) / 20.) * .01 * value(uDelayMix);
      const auto train = BBDRepeatTrain(mode, ms, tone, feedback, gain, rate, horizon);
      const auto y = [&](double amplitude) {
        return plot.B - plot.H() * static_cast<float>(std::clamp((Decibels(amplitude) + 60.) / 72., 0., 1.));
      };
      PlotGrid(g, plot);
      for (int i = 0; i < train.count; ++i)
      {
        const auto& repeat = train.repeats[i];
        const float x = std::clamp(plot.L + plot.W() * static_cast<float>(repeat.seconds / horizon), plot.L + 2.f, plot.R - 2.f);
        g.DrawLine(color.WithOpacity(.5f), x, plot.B, x, y(repeat.level), nullptr, 4.f);
        g.DrawLine(color, x, plot.B, x, y(repeat.level * repeat.treble), nullptr, 1.5f);
      }
      g.DrawLine(border, plot.L, plot.B, plot.R, plot.B);
      const double hz = value(uEchoRate) * (mode == 2 ? 4. : 1.);
      const double cents = effect_preview::SweepCents(.01 * value(uDelayMotion) * (mode == 0 ? .00035 : .0024), hz);
      title.SetFormatted(90, "%s  |  %.0f ms: %.1f kHz bandwidth%s", name, ms, train.cutoff * .001,
                         train.cutoff >= tone ? " (High cut)" : "");
      footer.SetFormatted(96, "Thin line: above 2 kHz  |  Motion %.2f Hz, up to %.0f ct  |  0–%.1f s", hz, cents, horizon);
      PlotDetail(g,plot,"Stems show the repeats on a -60 to +12 dB scale. The thin line inside each stem is the share above 2 kHz: longer Times lower the bandwidth, and every repeat loses more treble. Companding and saturation are left out.");
      break;
    }
    case SlotEffect::PitchDelay:
    {
      const auto passes = PitchVoices(.01 * value(uEchoShiftBlend), feedback);
      const double interval = value(uEchoInterval);
      const double range = std::max(12., std::abs(interval) * PitchPass::count) * 1.12;
      const bool stereo = mode == 2 && interval != 0.;
      const IRECT graph(plot.L + 39.f, plot.T + 5.f, plot.R - 7.f, plot.B - 19.f);
      const auto y = [&](double st) { return graph.MH() - graph.H() * .5f * static_cast<float>(st / range); };
      PlotGrid(g, graph);
      for (const double st : {-range / 1.12, 0., range / 1.12})
      {
        WDL_String level; level.SetFormatted(24, "%+.0f", st);
        g.DrawText(Text(11.f, muted, EAlign::Far), level.Get(), IRECT(plot.L, y(st) - 6.f, graph.L - 7.f, y(st) + 6.f));
      }
      for (const auto& pass : passes)
      {
        const float x = graph.L + graph.W() * (pass.repeats - .5f) / PitchPass::count;
        g.DrawLine(border.WithOpacity(.46f), x, graph.T, x, graph.B);
        WDL_String timing; timing.SetFormatted(32, "%.0f ms", pass.repeats * ms);
        g.DrawText(Text(11.f, muted, EAlign::Center), timing.Get(), IRECT(x - 41.f, graph.B + 2.f, x + 41.f, plot.B));
        for (int step = 0; step <= (interval == 0. ? 0 : pass.repeats); ++step)
        {
          double weight = pass.weights[step];
          if (interval == 0.)
            for (int i = 1; i <= pass.repeats; ++i) weight += pass.weights[i];
          if (weight < .001) continue;
          const float size = 2.5f + 5.f * static_cast<float>(std::sqrt(weight));
          const float alpha = .34f + .66f * static_cast<float>(std::sqrt(weight));
          if (stereo && step != 0) g.FillCircle(ampColor.WithOpacity(alpha), x, y(-interval * step), size);
          g.FillCircle(color.WithOpacity(alpha), x, y(interval * step), size);
        }
      }
      title.SetFormatted(90, "%s  |  Interval %+.0f st, shift blend %.0f%%", name, interval, value(uEchoShiftBlend));
      footer.Set(stereo ? "Pitch ladder  |  Cyan left, amber right" : "Pitch ladder  |  Nominal passes, in semitones");
      PlotDetail(g,plot,"Each column is a nominal repeat. Dots show the original pitch and possible accumulated shifts. Larger dots have more weight. Grain playback and detune affect the actual timing and sound.");
      break;
    }
    case SlotEffect::FeedbackFXDelay:
    {
      const double effectRate = mode == 3 ? value(uEchoRingRate) :
        value(uEchoRate) * (value(uEchoRateSync) >= .5 ? grid_tempo::Tempo(controls.tempo) / 60. : 1.);
      title.SetFormatted(90, "%s  |  %.2f Hz, depth %.0f%%", name, effectRate, 100. * depth);
      if (mode < 2)
      {
        const double upper = std::min(20000., .45 * rate), resonance = .01 * value(uEchoResonance);
        frequencyGrid(upper);
        for (double center : {420., 1680., 6720.})
          PlotCurve(g, plot, [&](double x) {
            const double hz = 20. * std::pow(upper / 20., x);
            return Decibels(FilterEffect(mode, hz, rate, center, depth, resonance));
          }, -36., 6., center == 1680. ? color : color.WithOpacity(.4f), 1.5f, 128);
        footer.Set("−36 / +6 dB  |  Three sweep positions");
        PlotDetail(g,plot,"Frequency response of the effect inside the loop at three positions of a sweep. Each echo passes through the current filter again.");
      }
      else
      {
        PlotGrid(g, plot);
        const double floor = mode == 3 ? -1. : 0.;
        const auto wave = [&](double x) { return std::sin(4. * grid_fx::pi * x); };
        PlotCurve(g, plot, [&](double x) { return mode == 3 ? 1. - depth + depth * wave(x) : 1. - depth * (.5 + .5 * wave(x)); },
                  floor, 1.1, color);
        if (mode == 2)
          PlotCurve(g, plot, [&](double x) { return 1. - depth * (.5 - .5 * wave(x)); }, floor, 1.1, ampColor.WithOpacity(.75f));
        footer.SetFormatted(88, "Two cycles, %.1f ms  |  %s", 2000. / effectRate, mode == 3 ? "Gain can invert" : "Cyan L / amber R");
        PlotDetail(g,plot,mode == 3 ? "The carrier multiplies each repeat. Negative gain flips polarity. A higher carrier frequency moves the sidebands farther from the played note."
                                  : "Left and right echo gains pulse in opposite directions. Depth sets the size of the pulse.");
      }
      break;
    }
    case SlotEffect::GranularDelay:
    {
      const double grain = value(uGrainLength) * (1.25 - .0075 * value(uGrainDensity));
      const double scatter = .01 * value(uGrainScatter) * std::min(ms, 420.);
      const double interval = value(uEchoInterval);
      const double duration = 2.5 * grain + scatter;
      const auto x = [&](double time) { return plot.L + plot.W() * static_cast<float>(time / duration); };
      PlotGrid(g, plot);
      for (int i = 0; i < 4; ++i)
      {
        const bool latched = mode == 2 && i >= 1;
        const double start = i * grain * .5 + (latched ? .5 : (i % 3) * .5) * scatter;
        const IRECT window(x(start), plot.T + 9.f, x(start + grain), plot.B - 21.f);
        const auto ink = latched ? ampColor : color;
        PlotCurve(g, window, [](double phase) { return .5 - .5 * std::cos(2. * grid_fx::pi * phase); }, 0., 1., ink.WithOpacity(.75f), 1.5f, 48);
        const float at = window.MW(), y = plot.B - 10.f;
        const float direction = mode == 1 ? -1.f : 1.f;
        g.DrawLine(ink, at - 7.f * direction, y, at + 7.f * direction, y, nullptr, 1.4f);
        g.DrawLine(ink, at + 7.f * direction, y, at + 3.f * direction, y - 3.f, nullptr, 1.4f);
        g.DrawLine(ink, at + 7.f * direction, y, at + 3.f * direction, y + 3.f, nullptr, 1.4f);
      }
      title.SetFormatted(90, "%s  |  Grain %.0f ms, pitch %+.0f st", name, grain, interval);
      footer.SetFormatted(90, "Grain sketch near %.0f ms  |  %.0f ms scatter span", ms, scatter);
      PlotDetail(g,plot,"Example overlap windows around the nominal Time. Arrowheads show playback direction. In Glitch, amber illustrates a fragment repeating while the latch is held.");
      break;
    }
    default: return;
  }
  PlotTitle(g, plot, color, title.Get());
  PlotFooter(g, plot, footer.Get());
}
} // namespace grid_ui
