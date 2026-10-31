#pragma once
#include "SlotTimeEffects.h"
#include <numeric>

namespace harmonic_tremolo_test
{
inline std::vector<double> Tone(double rate, double hz)
{
  std::vector<double> a(static_cast<size_t>(rate));
  for (size_t i = 0; i < a.size(); ++i) a[i] = .3 * std::sin(2. * grid_fx::pi * hz * i / rate);
  return a;
}
inline std::vector<double> Render(double rate, double hz, UtilityControls c, int block = 37, SlotEffect effect = SlotEffect::HarmonicTremolo)
{
  SlotModulation fx; fx.Reset(rate);
  auto a = Tone(rate, hz);
  for (size_t i = 0; i < a.size(); i += block)
    fx.Process(a.data() + i, static_cast<int>(std::min<size_t>(block, a.size() - i)), effect, c);
  return a;
}
// Correlation of 10 ms RMS envelopes over the second half second.
inline double EnvelopeCorrelation(const std::vector<double>& a, const std::vector<double>& b, double rate)
{
  const size_t hop = static_cast<size_t>(rate * .01);
  std::vector<double> x, y;
  for (size_t i = a.size() / 2; i + hop <= a.size(); i += hop)
  {
    double p = 0., q = 0.;
    for (size_t j = i; j < i + hop; ++j) { p += a[j] * a[j]; q += b[j] * b[j]; }
    x.push_back(std::sqrt(p)); y.push_back(std::sqrt(q));
  }
  const double mx = std::accumulate(x.begin(), x.end(), 0.) / x.size(), my = std::accumulate(y.begin(), y.end(), 0.) / y.size();
  double sxy = 0., sxx = 0., syy = 0.;
  for (size_t i = 0; i < x.size(); ++i) { sxy += (x[i] - mx) * (y[i] - my); sxx += (x[i] - mx) * (x[i] - mx); syy += (y[i] - my) * (y[i] - my); }
  return sxy / std::sqrt(sxx * syy);
}
}
void TestHarmonicTremolo()
{
  using namespace harmonic_tremolo_test;
  for (double rate : {44100., 48000., 96000., 192000.})
  {
    UtilityControls c; c.modulation = {4., 100., 0., 0., 100.}; c.extra.harmonicCrossover = 500.;
    // Bass and treble take turns: their level envelopes move in opposite phase, where an
    // ordinary tremolo moves both together.
    const auto bass = Render(rate, 80., c), treble = Render(rate, 4000., c);
    assert(EnvelopeCorrelation(bass, treble, rate) < -.8);
    assert(EnvelopeCorrelation(Render(rate, 80., c, 37, SlotEffect::Tremolo), Render(rate, 4000., c, 37, SlotEffect::Tremolo), rate) > .8);
    // Crossover moves the split: a 400 Hz tone follows the bass band with a 2 kHz split
    // and the treble band with a 200 Hz split.
    auto split = c; split.extra.harmonicCrossover = 2000.;
    assert(EnvelopeCorrelation(Render(rate, 400., split), bass, rate) > .8);
    split.extra.harmonicCrossover = 200.;
    assert(EnvelopeCorrelation(Render(rate, 400., split), treble, rate) > .8);
    assert(bass == Render(rate, 80., c, 1)); // Block-size independent.
    // Depth 0 and Mix 0 return the input exactly.
    const auto dry = Tone(rate, 80.);
    auto neutral = c; neutral.modulation[1] = 0.;
    assert(Render(rate, 80., neutral) == dry);
    neutral = c; neutral.modulation[4] = 0.;
    assert(Render(rate, 80., neutral) == dry);
    SlotModulation fx; fx.Reset(rate);
    std::array<double, 512> block{};
    for (size_t i = 0; i < block.size(); ++i) block[i] = .2 * std::sin(i * .1);
    allocations = deletions = 0; tracking = true;
    fx.Process(block.data(), 512, SlotEffect::HarmonicTremolo, c);
    tracking = false;
    assert(allocations == 0 && deletions == 0);
    for (double x : block) assert(std::isfinite(x));
  }
}
