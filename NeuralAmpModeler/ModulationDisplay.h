#pragma once
#include <complex>

// Parameter previews over a labeled time window; these deliberately do not
// invent a running audio LFO phase while the transport/processor is stopped.
namespace grid_ui
{
template<class Value>
bool DrawModulationSetup(IGraphics& g, SlotEffect effect, const IRECT& plot, IColor color,
                         Value value, double tempo, double sampleRate)
{
  const bool classic = IsModulation(effect), juno = effect == SlotEffect::JunoChorus;
  const bool vibe = effect == SlotEffect::UniVibe, rotary = effect == SlotEffect::Rotary;
  const bool ring = effect == SlotEffect::RingMod;
  if (!classic && !juno && !vibe && !rotary && !ring) return false;
  double rate = value(uModRate);
  if ((classic || vibe) && value(uTempoSync) >= .5)
    rate = 1. / grid_tempo::Seconds(tempo, static_cast<int>(value(uTempoDivision)));
  const double depth = value(uModDepth) * .01, character = value(uModCharacter) * .01;
  const auto caption = [&](const char* text) { g.DrawText(Text(14.f, color), text, IRECT(plot.L, 490, plot.R, 518)); };
  const auto curve = [&](auto function, double low, double high, IColor ink, float width = 1.5f) {
    const auto y = [&](double x) { return plot.B - plot.H() * static_cast<float>(std::clamp((x - low) / (high - low), 0., 1.)); };
    double previous = function(0.);
    for (int i = 1; i <= 192; ++i)
    {
      const double t = i / 192.;
      const double current = function(t);
      g.DrawLine(ink, plot.L + plot.W() * (i - 1) / 192.f, y(previous), plot.L + plot.W() * i / 192.f, y(current), nullptr, width);
      previous = current;
    }
  };
  const auto grid = [&] {
    for (int i = 0; i <= 2; ++i)
      g.DrawLine(border.WithOpacity(.4f), plot.L, plot.T + plot.H() * i / 2.f, plot.R, plot.T + plot.H() * i / 2.f);
  };
  const auto timeLabel = [&](double seconds) {
    WDL_String text; text.SetFormatted(32, seconds < .1 ? "%.1f ms" : "%.1f s", seconds < .1 ? seconds * 1000. : seconds);
    g.DrawText(Text(13.f, muted, EAlign::Far), text.Get(), IRECT(plot.R - 42, plot.B - 17, plot.R, plot.B));
  };
  WDL_String text;
  grid();
  if (juno)
  {
    const int mode = static_cast<int>(value(uJunoMode));
    text.SetFormatted(64, "%s | 1.7-5.4 ms", mode == 0 ? "I" : mode == 1 ? "I+II" : "II"); caption(text.Get());
    for (int voice = 0; voice < 2; ++voice)
      if ((voice == 0 && mode != 2) || (voice == 1 && mode != 0))
        for (bool right : {true, false})
          curve([&](double t) {
            const double phase = t * 2. * (voice ? .863 : .513);
            const double triangle = grid_hq::Tri(phase - std::floor(phase));
            return 1.66 + 3.69 * (right ? 1. - triangle : triangle);
          }, 1., 6., (right ? muted : color).WithOpacity((voice ? .65f : 1.f) * (.25f + .75f * value(uJunoMix) * .01)));
    timeLabel(2.);
  }
  else if (rotary)
  {
    const double speed = value(uRotarySpeed) >= .5 ? value(uRotaryFast) : value(uRotarySlow);
    const double spread = value(uRotarySpread) * .01;
    text.SetFormatted(64, "Horn %.1f / Drum %.1f Hz", speed, speed * .73); caption(text.Get());
    for (bool right : {true, false})
    {
      curve([&](double t) { return .72 + .28 * std::cos(2. * grid_fx::pi * (t * 2. * speed + (right ? .25 * spread : 0.))); },
            0., 1.1, (right ? muted : color).WithOpacity(.3f + .7f * value(uRotaryMix) * .01));
      curve([&](double t) { return .78 + .22 * std::sin(2. * grid_fx::pi * t * 2. * speed * .73 + (right ? spread : 0.)); },
            0., 1.1, (right ? muted : color).WithOpacity(.35f), 1.f);
    }
    timeLabel(2.);
  }
  else if (ring)
  {
    const double frequency = value(uRingFrequency), amount = value(uRingDepth) * .01;
    text.SetFormatted(64, "Carrier %.0f Hz", frequency); caption(text.Get());
    const double seconds = std::min(2., 4. / frequency);
    curve([&](double t) { return 1. - amount + amount * std::sin(2. * grid_fx::pi * frequency * seconds * t); },
          -1.1, 1.1, color.WithOpacity(.3f + .7f * value(uRingMix) * .01));
    timeLabel(seconds);
  }
  else if (vibe)
  {
    text.SetFormatted(64, "4 stages | %.2f Hz", rate); caption(text.Get());
    constexpr double centers[]{180., 430., 1050., 2900.};
    for (int stage = 0; stage < 4; ++stage)
      curve([&](double t) {
        const double phase = t * 2. * rate;
        const double lamp = (std::sin(2. * grid_fx::pi * phase) + .28 * std::sin(4. * grid_fx::pi * phase + .6)) * depth;
        return std::log2(std::min(.4 * sampleRate, centers[stage] * std::exp2(lamp * (1.2 + character * 1.2))));
      }, std::log2(30.), std::log2(std::min(18000., .45 * sampleRate)), color.WithOpacity(.4f + .2f * stage));
    timeLabel(2.);
  }
  else if (effect == SlotEffect::Phaser || effect == SlotEffect::Flanger)
  {
    const bool flange = effect == SlotEffect::Flanger;
    text.SetFormatted(64, flange ? "Comb sweep | %.2f Hz" : "6-stage sweep | %.2f Hz", rate); caption(text.Get());
    const double feedback = value(uModFeedback) * .01 * (flange ? 1. - 2. * character : 1.), mix = value(uModMix) * .01;
    // Small-signal response at the center and both ends of the modulation sweep.
    // The one-sample feedback delay and six DSP allpass stages are included.
    for (double lfo : {-1., 1., 0.})
    {
      const double cutoff = std::min(.35 * sampleRate, 650. * std::exp2(lfo * depth * 2.2 + character));
      const double tangent = std::tan(grid_fx::pi * cutoff / sampleRate), a = (tangent - 1.) / (tangent + 1.);
      curve([&](double t) {
        const double frequency = 40. * std::exp(std::log(std::min(16000., .45 * sampleRate) / 40.) * t);
        const auto z = std::polar(1., -2. * grid_fx::pi * frequency / sampleRate);
        const double wobble = (1. - .25 * character) * lfo + .25 * character * (lfo == 0. ? 1. : -1.) * std::sin(.7);
        const auto delay = std::polar(1., -2. * grid_fx::pi * frequency * (.003 + .0025 * depth * wobble));
        const auto allpass = flange ? delay : std::pow((a + z) / (1. + a * z), 6);
        const auto wet = allpass / (1. - feedback * (flange ? std::complex<double>(1., 0.) : z) * allpass);
        return 20. * std::log10(std::max(1.e-6, std::abs(1. - mix + mix * wet)));
      }, -30., 12., color.WithOpacity(lfo == 0. ? 1.f : .3f), lfo == 0. ? 1.8f : 1.f);
    }
    g.DrawText(Text(13.f, muted), "40 Hz", IRECT(plot.L, plot.B - 17, plot.L + 40, plot.B));
    g.DrawText(Text(13.f, muted, EAlign::Far), "16 kHz", IRECT(plot.R - 40, plot.B - 17, plot.R, plot.B));
  }
  else if (effect == SlotEffect::Tremolo)
  {
    const bool envelope = value(uTempoSync) < .5 && character > 0.;
    text.SetFormatted(64, envelope ? "%.2f Hz + envelope" : "Gain | %.2f Hz", rate); caption(text.Get());
    const auto gain = [&](double t, double speed) {
      const double shape = 1. + 5. * character;
      return 1. - .5 * depth * (1. + std::tanh(shape * std::sin(2. * grid_fx::pi * t * 2. * speed)) / std::tanh(shape));
    };
    if (envelope) curve([&](double t) { return gain(t, rate * (1. + character * 3.)); }, 0., 1.1, color.WithOpacity(.25f));
    curve([&](double t) { return 1. - value(uModMix) * .01 + value(uModMix) * .01 * gain(t, rate); }, 0., 1.1, color);
    timeLabel(2.);
  }
  else
  {
    const bool flange = effect == SlotEffect::Flanger, vibrato = effect == SlotEffect::Vibrato;
    const double center = flange ? 3. : vibrato ? 8. : 18.;
    const double excursion = flange ? 2.5 : 6.;
    text.SetFormatted(64, "Delay | %.2f Hz", rate); caption(text.Get());
    for (bool right : {true, false})
    {
      if (right && effect != SlotEffect::Chorus) continue;
      const double spread = right ? .005 * value(uChorusSpread) : 0.;
      curve([&](double t) {
        const double phase = t * 2. * rate + spread;
        const double wobble = (1. - .25 * character) * std::sin(2. * grid_fx::pi * phase)
                                + .25 * character * std::sin(4. * grid_fx::pi * phase + .7);
        return center + excursion * depth * wobble;
      }, 0., flange ? 6. : vibrato ? 16. : 26.,
      (right ? muted : color).WithOpacity(vibrato ? 1.f : .3f + .7f * value(uModMix) * .01));
    }
    text.SetFormatted(32, "%.0f ms", center);
    g.DrawText(Text(13.f, muted), text.Get(), IRECT(plot.L, plot.B - 17, plot.L + 40, plot.B));
    timeLabel(2.);
  }
  return true;
}
} // namespace grid_ui
