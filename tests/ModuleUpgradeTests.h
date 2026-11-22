#pragma once
#include "SlotOutboard.h"
#include "RoadmapEffects.h"

// Steady-state quiet sine gain of Console EQ, in dB.
static double ConsoleEQGain(const std::array<double, 7>& eq, double hz, double level = 1.e-3)
{
  constexpr double rate = 48000.;
  SlotOutboard strip; strip.Reset(rate, SlotEffect::ConsoleEQ);
  UtilityControls c; c.consoleEQ = eq;
  const int n = 48000;
  std::vector<double> x(n);
  for (int i = 0; i < n; ++i) x[i] = level * std::sin(2. * grid_fx::pi * hz * i / rate);
  const auto dry = x;
  strip.Process(x.data(), nullptr, n, c);
  double in = 0., out = 0.;
  for (int i = n / 2; i < n; ++i) { in += dry[i] * dry[i]; out += x[i] * x[i]; }
  return 10. * std::log10(out / in);
}

void TestConsoleEQCalibration()
{
  // Each knob's decibels are the quiet-signal change. The CStrip2 band split this
  // replaces gave about +10 dB for +6 on every band and left Bass near 30 Hz.
  assert(std::abs(ConsoleEQGain({6., 6., 6., 4750., 300., 100., 0.}, 1000.) - 6.) < .05);
  assert(std::abs(ConsoleEQGain({-6., -6., -6., 4750., 300., 100., 0.}, 1000.) + 6.) < .05);
  // Bass and Treble are shelves at their labelled corners.
  assert(ConsoleEQGain({0., 0., 6., 4750., 300., 100., 0.}, 50.) > 5.5);
  assert(ConsoleEQGain({0., 0., 6., 4750., 300., 100., 0.}, 5000.) < .3);
  const double atCorner = ConsoleEQGain({0., 0., 6., 4750., 300., 100., 0.}, 300.);
  assert(atCorner > 3. && atCorner < 5.);
  assert(ConsoleEQGain({0., 0., 6., 4750., 1000., 100., 0.}, 300.) > atCorner + 1.);
  assert(ConsoleEQGain({6., 0., 0., 2000., 300., 100., 0.}, 15000.) > 5.2);
  assert(ConsoleEQGain({6., 0., 0., 2000., 300., 100., 0.}, 100.) < .3);
  // Mid peaks between the corners.
  assert(ConsoleEQGain({0., 6., 0., 4750., 300., 100., 0.}, 1200.) > 5.);
  assert(ConsoleEQGain({0., 6., 0., 4750., 300., 100., 0.}, 60.) < 1.5);
  // Louder notes still saturate a boosted band: less gain than on quiet ones.
  assert(ConsoleEQGain({0., 6., 0., 4750., 300., 100., 0.}, 1200., .5) < ConsoleEQGain({0., 6., 0., 4750., 300., 100., 0.}, 1200.) - .5);
}

void TestEnvelopeFilterControls()
{
  constexpr double rate = 48000.;
  // Applied cutoff after a held note of the given peak level, then after `tail` seconds of silence.
  const auto sweep = [&](UtilityControls c, double peak, double tail = 0.) {
    SlotEnvelopeFilter f; f.Reset(rate);
    std::vector<double> x(static_cast<size_t>(.3 * rate));
    for (size_t i = 0; i < x.size(); ++i) x[i] = peak * std::sin(2. * grid_fx::pi * 220. * i / rate);
    f.Process(x.data(), static_cast<int>(x.size()), c);
    std::vector<double> silence(static_cast<size_t>(tail * rate));
    if (!silence.empty()) f.Process(silence.data(), static_cast<int>(silence.size()), c);
    return f.AppliedCutoff();
  };
  UtilityControls c; c.cutoff = 500.; c.resonance = .7; c.envelope = 2.;
  // Up (released) and Down sweeps from the resting cutoff.
  assert(sweep(c, .5) > 1900.);
  auto down = c; down.extra.filterDown = 1.;
  assert(sweep(down, .5) < 130.);
  // Sensitivity: -30 dBFS peaks barely move the filter until Sensitivity is raised.
  auto sensitive = c; sensitive.extra.filterSensitivity = 18.;
  assert(sweep(c, .03) < 650. && sweep(sensitive, .03) > 1700.);
  // Decay holds the sweep open after the note stops.
  auto slow = c; slow.extra.filterDecay = 1000.;
  auto fast = c; fast.extra.filterDecay = 20.;
  assert(sweep(slow, .5, .2) > 1200. && sweep(fast, .5, .2) < 520.);
  // Band-pass gains Q above 1, like an SVF band output: a resting sine at the cutoff.
  const auto level = [&](UtilityControls u, double hz) {
    SlotEnvelopeFilter f; f.Reset(rate);
    std::vector<double> x(static_cast<size_t>(rate));
    for (size_t i = 0; i < x.size(); ++i) x[i] = 1.e-3 * std::sin(2. * grid_fx::pi * hz * i / rate);
    const auto dry = x;
    f.Process(x.data(), static_cast<int>(x.size()), u);
    double in = 0., out = 0.;
    for (size_t i = x.size() / 2; i < x.size(); ++i) { in += dry[i] * dry[i]; out += x[i] * x[i]; }
    return 10. * std::log10(out / in);
  };
  UtilityControls band; band.filterMode = 2; band.cutoff = 1000.; band.resonance = 3.; band.envelope = 0.;
  assert(std::abs(level(band, 1000.) - 20. * std::log10(3.)) < .3);
  // Mix 0 with Output is exactly the dry signal scaled by Output.
  auto dryOnly = band; dryOnly.extra.filterMix = 0.; dryOnly.extra.filterOutput = -6.;
  assert(std::abs(level(dryOnly, 3000.) + 6.) < .01);
}

void TestRingCarrierLfo()
{
  constexpr double rate = 48000.;
  // Instantaneous carrier frequency from zero crossings of a ring-modulated DC input.
  const auto carrier = [&](UtilityControls c, double seconds) {
    grid_roadmap::Texture ring; ring.Reset(rate);
    std::vector<double> x(static_cast<size_t>(seconds * rate), .5);
    ring.Process(x.data(), static_cast<int>(x.size()), SlotEffect::RingMod, c);
    std::vector<std::pair<double, double>> hz; // (time, frequency)
    double last = -1.;
    for (size_t i = 1; i < x.size(); ++i)
      if (x[i - 1] < 0. && x[i] >= 0.)
      {
        const double t = (i - 1 + x[i - 1] / (x[i - 1] - x[i])) / rate;
        if (last >= 0.) hz.push_back({t, 1. / (t - last)});
        last = t;
      }
    return hz;
  };
  UtilityControls c; c.ring = {400., 100., 20000., 100.}; c.extra.ringLfoRate = 1.;
  const auto range = [](const std::vector<std::pair<double, double>>& hz) {
    double low = 1.e9, high = 0.;
    for (const auto& [t, f] : hz) if (t > .1) { low = std::min(low, f); high = std::max(high, f); }
    return high / low;
  };
  // Sweep 0 holds the carrier; full Sweep covers two octaves either way.
  assert(range(carrier(c, 2.)) < 1.01);
  auto swept = c; swept.extra.ringSweep = 100.;
  assert(range(carrier(swept, 2.)) > 12.);
  // Tempo sync: a quarter note at 120 BPM repeats the sweep every 0.5 s.
  auto synced = swept; synced.sync = true; synced.tempo = 120.; synced.division = 5; synced.extra.ringLfoRate = .05;
  const auto hz = carrier(synced, 2.);
  const auto at = [&](double t) {
    for (const auto& [time, f] : hz) if (time >= t) return f;
    return 0.;
  };
  assert(std::abs(at(1.1) / at(1.6) - 1.) < .05 && std::abs(at(1.225) / at(1.1) - 1.) > .3);
}
