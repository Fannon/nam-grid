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
  const auto timeLabel = [&](double seconds) {
    WDL_String text; text.SetFormatted(32, seconds < .1 ? "%.1f ms" : "%.1f s", seconds < .1 ? seconds * 1000. : seconds);
    PlotFooter(g,plot,"Parameter preview",text.Get());
  };
  WDL_String text;
  grid();
  if (juno)
  {
    const int mode = static_cast<int>(value(uJunoMode));
    text.SetFormatted(64, "%s | 1.7-5.4 ms", mode == 0 ? "I" : mode == 1 ? "I+II" : "II"); caption(text.Get());
    text.SetFormatted(80,"L / R delays | Bright %.0f%% | Wet %.0f%%",value(uJunoBrightness),value(uJunoMix)); PlotDetail(g,plot,text.Get());
    for (int voice = 0; voice < 2; ++voice)
      if ((voice == 0 && mode != 2) || (voice == 1 && mode != 0))
        for (bool right : {true, false})
          curve([&](double t) {
            const double phase = t * 2. * (voice ? .863 : .513);
            const double triangle = grid_hq::Tri(phase - std::floor(phase));
            return 1.66 + 3.69 * (right ? 1. - triangle : triangle);
          }, 1., 6., (right ? ampColor : color).WithOpacity(voice ? .65f : 1.f));
    timeLabel(2.);
  }
  else if (rotary)
  {
    const double speed = value(uRotarySpeed) >= .5 ? value(uRotaryFast) : value(uRotarySlow);
    const double spread = value(uRotarySpread) * .01;
    text.SetFormatted(64, "Horn %.1f / Drum %.1f Hz", speed, speed * .73); caption(text.Get());
    text.SetFormatted(80,"Ramp %.1fs | Drive %.0fdB | Wet %.0f%%",value(uRotaryAccel),value(uRotaryDrive),value(uRotaryMix)); PlotDetail(g,plot,text.Get());
    for (bool right : {true, false})
    {
      curve([&](double t) { return .72 + .28 * std::cos(2. * grid_fx::pi * (t * 2. * speed + (right ? .25 * spread : 0.))); },
            0., 1.1, right ? ampColor : color);
      curve([&](double t) { return .78 + .22 * std::sin(2. * grid_fx::pi * t * 2. * speed * .73 + (right ? spread : 0.)); },
            0., 1.1, (right ? ampColor : color).WithOpacity(.4f), 1.f);
    }
    timeLabel(2.);
  }
  else if (ring)
  {
    const double frequency = value(uRingFrequency);
    text.SetFormatted(64, "Carrier %.0f Hz", frequency); caption(text.Get());
    const double seconds = std::min(2., 4. / frequency);
    UtilityControls controls; for(int i=0;i<4;++i) controls.ring[i]=value(uRingFrequency+i);
    PlotDetail(g,plot,"DC input: gain -1 to +1, with tone / mix");
    curve([&](double t) { return effect_preview::RingGain(frequency*seconds*t,controls,sampleRate); },-1.1,1.1,color);
    timeLabel(seconds);
  }
  else if (vibe)
  {
    text.SetFormatted(64, "4 stages | %.2f Hz", rate); caption(text.Get());
    text.SetFormatted(80,"30 Hz - 18k | %s | %.0f%% wet",value(uVibeMode)>=.5 ? "Vibrato" : "Chorus",value(uModMix)); PlotDetail(g,plot,text.Get());
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
    UtilityControls controls;
    for (int i=0;i<5;++i) controls.modulation[i]=value(uModRate+i);
    PlotDetail(g,plot,"Frozen sweep phases | small-signal response");
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
    text.SetFormatted(64, envelope ? "%.2f Hz + envelope" : "Gain | %.2f Hz", rate); caption(text.Get());
    const auto gain = [&](double t, double speed) {
      const double shape = 1. + 5. * character;
      return 1. - .5 * depth * (1. + std::tanh(shape * std::sin(2. * grid_fx::pi * t * 2. * speed)) / std::tanh(shape));
    };
    PlotDetail(g,plot,envelope ? "0 - 100% gain | faint: loud-note rate" : "0 - 100% gain | includes dry/wet mix");
    if (envelope) curve([&](double t) { return 1.-value(uModMix)*.01+value(uModMix)*.01*gain(t, rate*(1.+character*3.)); },0.,1.1,color.WithOpacity(.25f));
    curve([&](double t) { return 1. - value(uModMix) * .01 + value(uModMix) * .01 * gain(t, rate); }, 0., 1.1, color);
    timeLabel(2.);
  }
  else
  {
    const bool flange = effect == SlotEffect::Flanger, vibrato = effect == SlotEffect::Vibrato;
    const double center = flange ? 3. : vibrato ? 8. : 18.;
    const double excursion = flange ? 2.5 : 6.;
    text.SetFormatted(64, "Delay | %.2f Hz", rate); caption(text.Get());
    text.SetFormatted(80,vibrato ? "0 - 16 ms | motion changes pitch" : "0 - 26 ms | L / R voices | Wet %.0f%%",value(uModMix));
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
      }, 0., flange ? 6. : vibrato ? 16. : 26.,
      right ? ampColor : color);
    }
    timeLabel(2.);
  }
  return true;
}
} // namespace grid_ui
