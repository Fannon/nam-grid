#pragma once
#include "SlotTimeEffects.h"

// Chorus 3 voices: voices at different delays beat against each other, so a steady tone
// gains the moving, denser envelope a single moving delay cannot produce, while
// broadband material keeps the wet level of one voice.
inline void TestChorusVoices()
{
  const auto render = [](double rate, double voices, bool noise, double depth) {
    SlotModulation chorus; chorus.Reset(rate);
    UtilityControls c; c.modulation = {.8, depth, 0., 0., 100.}; c.extra.chorusDelay = 18.; c.extra.chorusVoices = voices;
    const int count = static_cast<int>(rate * 2.);
    std::vector<double> x(count);
    uint32_t seed = 12345u;
    for (int i = 0; i < count; ++i)
    {
      seed = seed * 1664525u + 1013904223u;
      x[i] = noise ? .25 * (seed * (2. / 4294967295.) - 1.) : .25 * std::sin(2. * grid_fx::pi * 2000. * i / rate);
    }
    chorus.Process(x.data(), count, SlotEffect::Chorus, c);
    return std::vector<double>(x.begin() + static_cast<long>(rate * .1), x.end()); // skip the delay fill
  };
  const auto rms = [](const std::vector<double>& x) { double s = 0.; for (double v : x) s += v * v; return std::sqrt(s / x.size()); };
  for (double rate : {44100., 48000., 96000.})
  {
    // A 2 kHz tone through 10 ms RMS windows: one voice stays flat, three beat.
    for (double voices : {0., 1.})
    {
      const auto y = render(rate, voices, false, 60.);
      const int window = static_cast<int>(rate * .01);
      double low = 1.e9, high = 0.;
      for (size_t start = 0; start + window <= y.size(); start += window)
      {
        double sum = 0.;
        for (size_t i = start; i < start + window; ++i) sum += y[i] * y[i];
        low = std::min(low, sum); high = std::max(high, sum);
      }
      const double swing = 10. * std::log10(high / low);
      assert(voices == 0. ? swing < .5 : swing > 6.); // measured 0.07 / 12.1 dB
    }
    // Switching modes keeps the level of moving broadband material (measured within 0.06 dB).
    for (double depth : {35., 100.})
      assert(std::abs(20. * std::log10(rms(render(rate, 1., true, depth)) / rms(render(rate, 0., true, depth)))) < .5);
  }
  std::cout << "Chorus voices: 3 voices beat on a steady tone, keep broadband level at 44.1-96 kHz passed\n";
}
