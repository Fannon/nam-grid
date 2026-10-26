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
  const auto caption = [&](const char* text) { PlotTitle(g,plot,color,text); };
  const auto curve = [&](auto function, double low, double high, IColor ink, float width = 1.5f) {
    PlotCurve(g,plot,function,low,high,ink,width);
  };
  const auto grid = [&] {
    for (int i = 0; i <= 2; ++i)
      g.DrawLine(border.WithOpacity(.4f), plot.L, plot.T + plot.H() * i / 2.f, plot.R, plot.T + plot.H() * i / 2.f);
  };
  const auto timeLabel = [&](double seconds, const char* scale = "Modulation over time") {
    WDL_String text; text.SetFormatted(32, seconds < .1 ? "%.1f ms" : "%.1f s", seconds < .1 ? seconds * 1000. : seconds);
    PlotFooter(g,plot,scale,text.Get());
  };
  WDL_String text;
  grid();
  if (juno)
  {
    const int mode = static_cast<int>(value(uJunoMode));
    caption("Stereo delay sweep | 1.7-5.4 ms");
    PlotDetail(g,plot,"Left/right delay times for each active chorus voice. Cyan: left; amber: right. Faint traces show the second voice.");
    for (int voice = 0; voice < 2; ++voice)
      if ((voice == 0 && mode != 2) || (voice == 1 && mode != 0))
        for (bool right : {true, false})
          curve([&](double t) {
            const double phase = t * 2. * (voice ? .863 : .513);
            const double triangle = grid_hq::Tri(phase - std::floor(phase));
            return 1.66 + 3.69 * (right ? 1. - triangle : triangle);
          }, 1., 6., (right ? ampColor : color).WithOpacity(voice ? .65f : 1.f));
    timeLabel(2., value(uJunoWidth) == 0. ? "Wet centered | physical head sweeps" : "Physical head sweeps | Width mixes wet stereo");
  }
  else if (rotary)
  {
    const bool fast = value(uRotarySpeed) >= .5;
    const double slow = value(uRotarySlow), quick = value(uRotaryFast), speed = fast ? quick : slow;
    const double spread = value(uRotarySpread) * .01, ramp = std::max(.01, value(uRotaryAccel));
    // Horn Doppler: delay 4 +/- 1.8 ms swept at the rotor speed. Its pitch swing
    // grows with speed, which is much of the slow / fast character.
    text.SetFormatted(64, "Doppler +/-%.0f ct", effect_preview::SweepCents(.0018, speed)); caption(text.Get());
    PlotDetail(g,plot,"Left: speed ramp after switching slow/fast. Right: settled stereo gain modulation; faint traces are the drum, bright traces the horn above 800 Hz.");
    // Left: the one-pole speed change after toggling Speed. Right: settled gain.
    const IRECT rampPlot(plot.L, plot.T, plot.L + plot.W() * .36f, plot.B), am(rampPlot.R + 12.f, plot.T, plot.R, plot.B);
    const double from = fast ? slow : quick, top = std::max(slow, quick) * 1.15;
    for (double hz : {slow, quick})
    {
      const float y = rampPlot.B - rampPlot.H() * static_cast<float>(hz / top);
      for (float x = rampPlot.L; x < rampPlot.R; x += 8.f) g.DrawLine(border, x, y, std::min(x + 4.f, rampPlot.R), y);
    }
    PlotCurve(g, rampPlot, [&](double x) { return speed + (from - speed) * std::exp(-x * 5.); }, 0., top, color);
    g.DrawText(Text(11.f, muted), "speed", IRECT(rampPlot.L + 2.f, rampPlot.T, rampPlot.R, rampPlot.T + 12.f));
    for (bool right : {true, false})
    {
      PlotCurve(g, am, [&](double t) { return .72 + .28 * std::cos(2. * grid_fx::pi * (t * 2. * speed + (right ? .25 * spread : 0.))); },
            0., 1.1, right ? ampColor : color);
      PlotCurve(g, am, [&](double t) { return .78 + .22 * std::sin(2. * grid_fx::pi * t * 2. * speed * .73 + (right ? spread : 0.)); },
            0., 1.1, (right ? ampColor : color).WithOpacity(.4f), 1.f);
    }
    text.SetFormatted(64, "Ramp %.1f s | gain 2 s", 5. * ramp);
    PlotFooter(g,plot,text.Get());
  }
  else if (ring)
  {
    const double frequency = value(uRingFrequency);
    caption("Ring modulation | 220 Hz input");
    // wet = LowPass(x * (1 - Depth + Depth sin)): each partial f keeps (1 - Depth)
    // and gains f +/- carrier at Depth / 2. Those sidebands are inharmonic.
    const double depth = value(uRingFrequency + 1) * .01, mix = value(uRingFrequency + 3) * .01;
    const double tone = std::min(.45 * sampleRate, value(uRingFrequency + 2));
    const int carrierShape = static_cast<int>(value(uRingShape));
    const double upper = std::max(1200., 880. + frequency * (carrierShape ? 5. : 1.)) * 1.05;
    PlotDetail(g,plot,"A 220 Hz reference and its harmonics. Gray: retained partials; colour: carrier sidebands. High cut shapes all components.");
    const auto bar = [&](double hz, double level, IColor ink, float width) {
      if (hz <= 0. || hz > upper || level < .005) return;
      const float x = plot.L + plot.W() * static_cast<float>(hz / upper);
      g.DrawLine(ink, x, plot.B, x, plot.B - plot.H() * static_cast<float>(std::min(1., level)), nullptr, width);
    };
    for (int partial = 1; partial <= 4; ++partial)
    {
      const double hz = 220. * partial;
      bar(hz, std::abs(1. - mix + mix * (1. - depth) * effect_preview::PoleResponse(hz, tone, sampleRate)) * .8, muted, 3.f);
      for (int harmonic = 1; harmonic <= (carrierShape ? 5 : 1); harmonic += 2)
        for (int sign : {-1, 1})
      {
        const double weight = carrierShape == 1 ? 8. / (grid_fx::pi * grid_fx::pi * harmonic * harmonic) : carrierShape == 2 ? 4. / (grid_fx::pi * harmonic) : 1.;
        const double side = std::abs(hz + sign * frequency * harmonic);
        bar(side, weight * mix * depth * .5 * std::abs(effect_preview::PoleResponse(side, tone, sampleRate)) * .8, sign > 0 ? color : ampColor, 2.f);
      }
    }
    text.SetFormatted(32, "0 - %.1fk Hz", upper / 1000.);
    PlotFooter(g,plot,"Gray: kept partial | colour: sidebands",text.Get());
  }
  else if (vibe)
  {
    const bool vibrato = value(uVibeMode) >= .5;
    caption(vibrato ? "Pitch modulation" : "Moving notch response");
    // Chorus mode sums dry with four allpasses: moving notches. Vibrato mode is
    // the allpasses alone: flat magnitude, the sweep is heard as pitch.
    PlotDetail(g,plot,vibrato ? "Flat magnitude: the lamp sweep moves pitch" : "Notches at lamp low / centre / high");
    UtilityControls controls;
    for (int i = 0; i < 5; ++i) controls.modulation[i] = value(uModRate + i);
    controls.vibeMode = value(uVibeMode);
    const double upper = std::min(20000., .45 * sampleRate);
    PlotFrequencyAxis(g,plot,upper,"-30 / +6 dB");
    for (double lamp : {-1.28 * depth, 0., 1.28 * depth})
      curve([&](double t) {
        const double hz = 20. * std::pow(upper / 20., t);
        return 20. * std::log10(std::max(1.e-9, effect_preview::VibeMagnitude(hz, lamp, controls, sampleRate)));
      }, -30., 6., color.WithOpacity(lamp == 0. ? 1.f : .35f), lamp == 0. ? 1.8f : 1.f);
  }
  else if (effect == SlotEffect::Phaser || effect == SlotEffect::Flanger)
  {
    const bool flange = effect == SlotEffect::Flanger;
    text.SetFormatted(64, "%d-stage sweep", std::array<int,4>{4,6,8,12}[std::clamp(static_cast<int>(value(uPhaserStages)),0,3)]);
    caption(flange ? "Comb sweep" : text.Get());
    UtilityControls controls;
    for (int i=0;i<5;++i) controls.modulation[i]=value(uModRate+i);
    controls.extra.flangerDelay=value(uFlangerDelay); controls.extra.phaserStages=value(uPhaserStages);
    if (flange) PlotDetail(g,plot,character > .5 ? "Negative feedback also darkens (12k > 3k)" : "Frozen sweep phases | small-signal response");
    else
    {
      const double low = std::min(.35 * sampleRate, 650. * std::exp2(-depth * 2.2 + character));
      const double high = std::min(.35 * sampleRate, 650. * std::exp2(depth * 2.2 + character));
      text.SetFormatted(80, "Allpass sweep %.0f - %.0f Hz | 4 phases", low, high); PlotDetail(g,plot,text.Get());
    }
    const double upper=std::min(20000.,.45*sampleRate);
    PlotFrequencyAxis(g,plot,upper,"-30 / +12 dB");
    for (double phase : {0.,.5,.75,.25})
      curve([&](double t) {
        const double hz=20.*std::pow(upper/20.,t);
        return 20.*std::log10(std::max(1.e-9,effect_preview::ModulationMagnitude(effect,hz,phase,controls,sampleRate)));
      },-30.,12.,color.WithOpacity(phase==.25 ? 1.f : .25f),phase==.25 ? 1.8f : 1.f);
  }
  else if (effect == SlotEffect::Tremolo)
  {
    const bool envelope = value(uTempoSync) < .5 && character > 0.;
    caption(envelope ? "Gain + envelope modulation" : "Gain modulation");
    const auto gain = [&](double t, double speed) {
      const double shape = 1. + 5. * character;
      return 1. - .5 * depth * (1. + std::tanh(shape * std::sin(2. * grid_fx::pi * t * 2. * speed)) / std::tanh(shape));
    };
    PlotDetail(g,plot,envelope ? "0 - 100% gain | faint: loud-note rate" : "0 - 100% gain | includes dry/wet mix");
    if (envelope) curve([&](double t) { return 1.-value(uModMix)*.01+value(uModMix)*.01*gain(t, rate*(1.+character*3.)); },0.,1.1,color.WithOpacity(.25f));
    curve([&](double t) { return 1. - value(uModMix) * .01 + value(uModMix) * .01 * gain(t, rate); }, 0., 1.1, color);
    timeLabel(2., "Gain 0 - 100%");
  }
  else
  {
    const bool flange = effect == SlotEffect::Flanger, vibrato = effect == SlotEffect::Vibrato;
    const double center = flange ? value(uFlangerDelay) : vibrato ? 8. : value(uChorusDelay);
    const double excursion = flange ? 2.5 : 6.;
    // Rate x Depth sets the detune: pitch ratio = 1 - d(delay)/dt.
    const double cents = effect_preview::SweepCents(excursion * .001 * depth, rate, 1. + .25 * character);
    text.SetFormatted(80, "Delay modulation | pitch +/-%.1f ct", cents); caption(text.Get());
    if (vibrato) text.SetFormatted(80, "0 - 16 ms | pitch +/-%.1f ct", cents);
    else text.SetFormatted(80, "Base %.1f ms | pitch +/-%.1f ct", center, cents);
    PlotDetail(g,plot,text.Get());
    for (bool right : {true, false})
    {
      if (right && effect != SlotEffect::Chorus) continue;
      const double spread = right ? .005 * value(uChorusSpread) : 0.;
      curve([&](double t) {
        const double phase = t * 2. * rate + spread;
        const double wobble = (1. - .25 * character) * std::sin(2. * grid_fx::pi * phase)
                                + .25 * character * std::sin(4. * grid_fx::pi * phase + .7);
        return center + excursion * depth * wobble;
      }, 0., flange ? 18. : vibrato ? 16. : 42.,
      right ? ampColor : color);
    }
    timeLabel(2., vibrato ? "Delay 0 - 16 ms" : "Delay 0 - 26 ms");
  }
  return true;
}
} // namespace grid_ui
