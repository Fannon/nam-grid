#pragma once
#include "SlotGraphicEQ.h"

namespace graphic_eq_test
{
inline std::vector<double> Render(double rate, double hz, const UtilityControls& c, int block = 37)
{
  SlotGraphicEQ fx; fx.Reset(rate);
  std::vector<double> a(static_cast<size_t>(rate));
  for (size_t i = 0; i < a.size(); ++i) a[i] = .2 * std::sin(2. * grid_fx::pi * hz * i / rate);
  for (size_t i = 0; i < a.size(); i += block)
    fx.Process(a.data() + i, nullptr, static_cast<int>(std::min<size_t>(block, a.size() - i)), c);
  return a;
}
inline double Gain(double rate, double hz, const UtilityControls& c)
{
  const auto wet = Render(rate, hz, c);
  double p = 0.;
  for (size_t i = wet.size() / 2; i < wet.size(); ++i) p += wet[i] * wet[i];
  return 10. * std::log10(p / (wet.size() / 2) / (.02));
}
}
void TestGraphicEQ()
{
  using namespace graphic_eq_test;
  for (double rate : {44100., 48000., 96000., 192000.})
  {
    // Flat bands and Level 0 return the input exactly.
    UtilityControls flat;
    std::vector<double> dry(static_cast<size_t>(rate));
    for (size_t i = 0; i < dry.size(); ++i) dry[i] = .2 * std::sin(2. * grid_fx::pi * 440. * i / rate);
    assert(Render(rate, 440., flat) == dry);
    for (int voicing = 0; voicing < 3; ++voicing)
      for (int band = 0; band < graphic_eq::kBands; ++band)
      {
        const double hz = graphic_eq::kCentres[voicing][band];
        if (hz > .45 * rate) continue;
        // Each slider moves its own centre by its full amount and agrees with the display.
        UtilityControls c; c.graphicVoicing = voicing; c.graphicEQ[band] = 12.;
        assert(std::abs(Gain(rate, hz, c) - 12.) < .3);
        std::array<double, graphic_eq::kBands + 2> gains{}; gains[band] = 12.;
        assert(std::abs(20. * std::log10(graphic_eq::Magnitude(hz, voicing, gains)) - 12.) < .01);
        // A neighbour two bands away barely moves: the bands are about an octave wide.
        if (band + 2 < graphic_eq::kBands && graphic_eq::kCentres[voicing][band + 2] < .45 * rate)
          assert(std::abs(Gain(rate, graphic_eq::kCentres[voicing][band + 2], c)) < 2.);
      }
    // Voicing moves the bands: the guitar voicing's first slider leaves 40 Hz nearly alone;
    // the bass voicing's first slider is centred there.
    UtilityControls guitar; guitar.graphicEQ[0] = 12.;
    auto bass = guitar; bass.graphicVoicing = 1;
    assert(Gain(rate, 40., bass) > Gain(rate, 40., guitar) + 5.);
    UtilityControls level; level.graphicEQ[7] = -6.;
    assert(std::abs(Gain(rate, 1000., level) + 6.) < .05);
    UtilityControls shaped; shaped.graphicEQ = {-9., -4., 0., 2., 3., 1., -1., 2.};
    assert(Render(rate, 220., shaped) == Render(rate, 220., shaped, 1)); // Block-size independent.
    SlotGraphicEQ fx; fx.Reset(rate);
    std::array<double, 512> l{}, r{};
    for (size_t i = 0; i < l.size(); ++i) { l[i] = .2 * std::sin(i * .1); r[i] = -l[i]; }
    allocations = deletions = 0; tracking = true;
    fx.Process(l.data(), r.data(), 512, shaped);
    tracking = false;
    assert(allocations == 0 && deletions == 0);
    for (size_t i = 0; i < l.size(); ++i) assert(l[i] == -r[i] && std::isfinite(l[i]));
  }
}
