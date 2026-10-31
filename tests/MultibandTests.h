#pragma once
#include "SlotMultiband.h"

namespace multiband_test
{
inline std::vector<double> Tones(double rate, std::initializer_list<std::pair<double, double>> tones)
{
  std::vector<double> a(static_cast<size_t>(rate));
  for (size_t i = 0; i < a.size(); ++i)
    for (const auto& [hz, level] : tones) a[i] += level * std::sin(2. * grid_fx::pi * hz * i / rate);
  return a;
}
inline std::vector<double> Render(double rate, std::vector<double> a, const UtilityControls& c, int block = 37)
{
  SlotMultiband fx; fx.Reset(rate);
  for (size_t i = 0; i < a.size(); i += block)
    fx.Process(a.data() + i, nullptr, static_cast<int>(std::min<size_t>(block, a.size() - i)), c);
  return a;
}
// Level change of one frequency over the second half, from a correlation with that sine.
inline double Change(const std::vector<double>& wet, const std::vector<double>& dry, double hz, double rate)
{
  const auto amplitude = [&](const std::vector<double>& x) {
    double s = 0., c = 0.;
    for (size_t i = x.size() / 2; i < x.size(); ++i)
    { s += x[i] * std::sin(2. * grid_fx::pi * hz * i / rate); c += x[i] * std::cos(2. * grid_fx::pi * hz * i / rate); }
    return std::hypot(s, c);
  };
  return 20. * std::log10(amplitude(wet) / amplitude(dry));
}
}
void TestMultiband()
{
  using namespace multiband_test;
  for (double rate : {44100., 48000., 96000., 192000.})
  {
    UtilityControls neutral;
    const auto mixed = Tones(rate, {{80., .02}, {4000., .5}});
    // Neutral bands sum to an allpass of the input: every frequency keeps its level. Depth 0
    // is that same sum whatever the other controls do, so Depth never comb filters.
    for (double hz : {40., 150., 400., 2500., 8000.})
    {
      const auto tone = Tones(rate, {{hz, .3}});
      assert(std::abs(Change(Render(rate, tone, neutral), tone, hz, rate)) < .01);
    }
    const auto passed = Render(rate, mixed, neutral);
    auto dry = neutral; dry.multiband = {0., 100., 100., 100., 12., -12., 12., 0.};
    assert(Render(rate, mixed, dry) == passed);
    // Downward acts per band: a loud 4 kHz tone is compressed, a quiet 80 Hz tone is not.
    auto down = neutral; down.multiband[1] = 100.;
    const auto pressed = Render(rate, mixed, down);
    assert(Change(pressed, mixed, 4000., rate) < -10. && std::abs(Change(pressed, mixed, 80., rate)) < .5);
    assert(pressed == Render(rate, mixed, down, 1)); // Block-size independent.
    // Upward raises a quiet band, but not one below the -60 dBFS noise guard, nor silence.
    auto up = neutral; up.multiband[2] = 100.;
    const auto quiet = Tones(rate, {{1000., .003}});
    assert(Change(Render(rate, quiet, up), quiet, 1000., rate) > 6.);
    const auto hiss = Tones(rate, {{1000., .00003}});
    assert(std::abs(Change(Render(rate, hiss, up), hiss, 1000., rate)) < .1);
    const std::vector<double> silence(static_cast<size_t>(rate), 0.);
    assert(Render(rate, silence, up) == silence);
    // Band levels apply after compression; 50 Hz sits well inside the 150 Hz low band.
    auto lift = neutral; lift.multiband[4] = 6.;
    const auto bass = Tones(rate, {{50., .02}});
    assert(std::abs(Change(Render(rate, bass, lift), bass, 50., rate) - 6.) < .1);
    // Stereo link: opposite polarity stays opposite; no allocation while processing.
    SlotMultiband fx; fx.Reset(rate);
    std::array<double, 512> l{}, r{};
    for (size_t i = 0; i < l.size(); ++i) { l[i] = .4 * std::sin(i * .1); r[i] = -l[i]; }
    auto both = neutral; both.multiband = {100., 60., 60., 100., 3., 0., -3., 0.};
    allocations = deletions = 0; tracking = true;
    fx.Process(l.data(), r.data(), 512, both);
    tracking = false;
    assert(allocations == 0 && deletions == 0);
    for (size_t i = 0; i < l.size(); ++i) assert(l[i] == -r[i] && std::isfinite(l[i]));
  }
}
