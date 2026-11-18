#pragma once
#include "EffectPreview.h"

// Small, settled-control models for the editor. These describe one repeat
// or follow idealized feedback paths without running the audio processor.
namespace delay_preview
{
inline constexpr int bins = 128;
using Trace = std::array<double, bins>;

struct Tap
{
  double time, level, pan;
};
template<class Value>
std::array<Tap, 4> PatternTaps(Value value)
{
  std::array<Tap, 4> taps{};
  for (int i = 0; i < 4; ++i)
    taps[i] = {.01 * value(uTapTime1 + i), .01 * value(uTapLevel1 + i), .01 * value(uTapPan1 + i)};
  return taps;
}
inline double TapGain(const Tap& tap, int channel, double width)
{
  return tap.level * (1. - std::max(0., (channel ? -tap.pan : tap.pan) * width));
}

inline int DelayBin(double time, double horizon)
{
  return std::clamp(static_cast<int>(std::lround(time / horizon * (bins - 1))), 1, bins - 1);
}

// An impulse through the four nominal tap delays, with the first arrivals removed
// from the drawn trace. Binning is for display, and omits filters, motion and drive.
inline std::array<Trace, 2> PatternReturns(const std::array<Tap, 4>& taps, int mode, double feedback, double width)
{
  std::array<Trace, 4> written{};
  std::array<Trace, 2> output{};
  std::array<int, 4> time{};
  double sum = 0., power = 0.;
  for (int tap = 0; tap < 4; ++tap)
  {
    time[tap] = DelayBin(taps[tap].time, 3.);
    written[tap][0] = 1.;
    sum += taps[tap].level;
    power += taps[tap].level * taps[tap].level;
  }
  const double norm = 1. / std::sqrt(std::max(1., power));
  for (int bin = 1; bin < bins; ++bin)
  {
    std::array<double, 4> reads{};
    double swell = 0., total = 0.;
    for (int tap = 0; tap < 4; ++tap)
    {
      const int source = bin - time[tap];
      if (source >= 0) reads[tap] = written[tap][source];
      swell += reads[tap] * taps[tap].level;
      total += reads[tap];
      if (source > 0)
        for (int ch = 0; ch < 2; ++ch)
          output[ch][bin] += reads[tap] * TapGain(taps[tap], ch, width) * norm;
    }
    for (int tap = 0; tap < 4; ++tap)
    {
      const double returned = mode == 0 ? reads[3] : mode == 2 ? .5 * total - reads[tap] : swell / std::max(1., sum);
      written[tap][bin] = feedback * returned;
    }
  }
  return output;
}

inline Trace TapeRepeats(int mode, double feedback, double time, double horizon)
{
  Trace send{}, wet{};
  std::array<int, 3> heads{};
  int count = 0;
  for (int head = 0; head < 3; ++head)
    if (spaceHeads[mode] & (1u << head))
      heads[count++] = DelayBin(time * HeadFraction(head), horizon);
  const double headGain = 1. / std::sqrt(static_cast<double>(count));
  send[0] = 1.;
  for (int bin = 1; bin < bins; ++bin)
  {
    for (int h = 0; h < count; ++h)
      if (bin >= heads[h]) wet[bin] += headGain * send[bin - heads[h]];
    send[bin] = feedback * headGain * wet[bin];
  }
  return wet;
}

inline double BBDBandwidth(int mode, double ms, double highCut)
{
  constexpr std::array widths{.70, 1., .96, 1.5, .84};
  return std::min(highCut, 4300. * std::clamp(std::sqrt(330. / ms), .38, 1.65) * widths[mode]);
}

inline double Decibels(double gain) { return 20. * std::log10(std::max(gain, 1.e-6)); }

struct PitchPass
{
  static constexpr int count = 4;
  int repeats;
  std::array<double, count + 1> weights{};
};

inline std::array<PitchPass, PitchPass::count> PitchVoices(double shiftBlend, double feedback)
{
  std::array<PitchPass, PitchPass::count> passes{};
  PitchPass current{};
  current.weights[0] = 1.;
  for (auto& next : passes)
  {
    next.repeats = current.repeats + 1;
    for (int shifted = 0; shifted <= current.repeats; ++shifted)
    {
      const double gain = current.weights[shifted] * (current.repeats == 0 ? 1. : feedback);
      next.weights[shifted] += gain * (1. - shiftBlend);
      next.weights[shifted + 1] += gain * shiftBlend;
    }
    current = next;
  }
  return passes;
}

inline std::complex<double> OnePole(double cutoff, double rate, std::complex<double> z)
{
  const double a = std::exp(-2. * grid_fx::pi * std::min(cutoff, .45 * rate) / rate);
  return (1. - a) / (1. - a * z);
}

inline double FilterEffect(int mode, double frequency, double rate, double center, double depth, double resonance)
{
  const auto z = std::polar(1., -2. * grid_fx::pi * frequency / rate);
  if (mode == 0)
  {
    const auto band = OnePole(center * 1.8, rate, z) - OnePole(center * .57, rate, z);
    return std::abs((1. - depth) + depth * (1.15 + .85 * resonance) * band);
  }
  const double g = std::tan(grid_fx::pi * std::min(.4 * rate, center) / rate), a = (1. - g) / (1. + g);
  const auto phase = (z - a) / (1. - a * z);
  const double blend = depth * (.5 + .5 * resonance);
  return std::abs((1. - blend) + blend * phase * phase * phase * phase);
}

} // namespace delay_preview
