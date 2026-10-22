#pragma once
#include "EffectPresentation.h"
#include "TempoSync.h"
#include "dsp/reverb/CosmicReverb.h"
#include <algorithm>
#include <cmath>
#include <complex>

// Settled-control sketches, not measurements of live audio or hidden LFO phase.
namespace effect_preview
{
struct CosmicLines { std::array<double, 16> seconds{}; int count=0; };
inline CosmicLines Cosmic(double milliseconds, double warpPercent, int style, double rate)
{
  CosmicLines plan; style=std::clamp(style, 0, effect_modes::cosmicCount-1); plan.count=grid_cosmic::LineCount(style);
  for (int i=0; i<plan.count; ++i)
    plan.seconds[i]=grid_cosmic::BaseSamples(milliseconds, warpPercent*.01, style, i, rate)/rate;
  return plan;
}
inline double DelaySeconds(const UtilityControls& c)
{ return c.sync ? grid_tempo::Seconds(c.tempo, c.division) : std::clamp(c.delay[0], 5., 4000.) * .001; }
inline double LowPass(double hz, double cutoff, double rate, int poles = 1)
{
  const double a = std::exp(-2. * 3.14159265358979323846 * std::min(cutoff, .4 * rate) / rate);
  const double magnitude = std::abs((1. - a) / (1. - a * std::polar(1., -2. * 3.14159265358979323846 * hz / rate)));
  return std::pow(magnitude, poles);
}
inline double DistortionTone(double hz, const UtilityControls& c, double rate)
{
  const auto z = std::polar(1., -2. * 3.14159265358979323846 * hz / rate);
  const double low = std::exp(-2. * 3.14159265358979323846 * std::min(c.distortion[2], .4 * rate) / rate);
  const double dc = std::exp(-2. * 3.14159265358979323846 * 5. / rate);
  const auto wet = ((1. - z) / (1. - dc * z)) * ((1. - low) / (1. - low * z));
  const double mix = c.distortion[4] * .01;
  return std::abs(1. - mix + mix * wet) * std::pow(10., c.distortion[5] / 20.);
}
struct Echo
{
  double seconds = 0., left = 0., right = 0., treble = 0.;
};
struct Echoes
{
  std::array<Echo, 96> taps{};
  size_t count = 0;
  double horizon = 1., dry = 1.;
};
inline Echoes Delay(SlotEffect effect, const UtilityControls& c, double rate)
{
  Echoes result;
  const bool dual = effect == SlotEffect::DualDelay;
  const double time = DelaySeconds(c), ratio = dual ? std::clamp(c.dual[0], .25, 2.) : 1.;
  result.horizon = std::max(1., 4. * time * std::max(1., ratio));
  const double gain = std::pow(10., c.delayOutput / 20.), mix = std::clamp(c.delay[4] * .01, 0., 1.);
  result.dry = (1. - mix) * gain;
  const double feedback = std::clamp(c.delay[1] * .01, 0., .82);
  const double age = effect == SlotEffect::AnalogDelay ? 1. - .6 * c.delay[3] * .01 : 1.;
  const double brightness = LowPass(5000., c.delay[2] * age, rate, effect == SlotEffect::AnalogDelay ? 2 : 1);
  const double balance = std::clamp(c.dual[1] * .01, 0., 1.);
  const double left = dual ? std::min(1., 2. * (1. - balance)) : 1.;
  const double right = dual ? std::min(1., 2. * balance) : 1.;
  const auto add = [&](double seconds, double l, double r, int passes) {
    if (seconds <= result.horizon && result.count < result.taps.size())
      result.taps[result.count++] = {seconds, l, r, std::pow(brightness, passes)};
  };
  double amplitude = mix * gain, elapsed = 0.;
  for (int repeat = 1; repeat <= 12 && amplitude > 1.e-5; ++repeat)
  {
    if (c.pingPong)
    {
      const bool onRight = repeat % 2 == 0;
      elapsed += time * (onRight ? ratio : 1.);
      add(elapsed, onRight ? 0. : amplitude * left, onRight ? amplitude * right : 0., repeat);
    }
    else
    {
      if (effect == SlotEffect::MultiHeadDelay)
      {
        const auto& heads = effect_modes::TapeHeads(c.tapeHeads).gains;
        for (int head = 0; head < 4; ++head)
          if (heads[0][head] > 0. || heads[1][head] > 0.)
            add((repeat - 1 + (head + 1) * .25) * time, amplitude * heads[0][head], amplitude * heads[1][head], repeat);
      }
      else
      {
        add(repeat * time, amplitude * left, dual ? 0. : amplitude * right, repeat);
        if (dual) add(repeat * time * ratio, 0., amplitude * right, repeat);
      }
    }
    amplitude *= feedback;
  }
  return result;
}
inline double CapturePeriod(SlotEffect effect, double milliseconds, double fadeMilliseconds)
{
  const double window = milliseconds * .001;
  return effect == SlotEffect::Freeze ? window - std::min(window * .25, fadeMilliseconds * .001) : window;
}
} // namespace effect_preview
