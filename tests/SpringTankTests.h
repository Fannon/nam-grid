#pragma once
#include "SlotGuitarEffects.h"

// Spring Tank: Dispersive. Every echo is a chirp (higher bands arrive later), Drip sets its
// strength, the 1 kHz band decays as set, and the wet level matches Classic.
namespace spring_tank_test
{
inline std::vector<double> Band(const std::vector<double>& x, double rate, double hz, double q)
{
  const double w = 2. * grid_fx::pi * hz / rate, alpha = std::sin(w) / (2. * q), a0 = 1. + alpha;
  double x1 = 0., x2 = 0., y1 = 0., y2 = 0.;
  std::vector<double> y(x.size());
  for (size_t i = 0; i < x.size(); ++i)
  {
    const double v = (alpha * x[i] - alpha * x2 + 2. * std::cos(w) * y1 - (1. - alpha) * y2) / a0;
    x2 = x1; x1 = x[i]; y2 = y1; y1 = v; y[i] = v;
  }
  return y;
}
// Time of the strongest band-passed response within the first echo, in milliseconds.
inline double Arrival(const std::vector<double>& ir, double rate, double hz)
{
  const auto y = Band(ir, rate, hz, 4.);
  int best = 0; double peak = 0.;
  for (int i = 0; i < static_cast<int>(.12 * rate); ++i)
    if (std::abs(y[i]) > peak) { peak = std::abs(y[i]); best = i; }
  return 1000. * best / rate;
}
inline double DecayT20(const std::vector<double>& ir, double rate)
{
  const auto y = Band(ir, rate, 1000., .7);
  std::vector<double> energy(y.size() + 1);
  for (size_t i = y.size(); i-- > 0;) energy[i] = energy[i + 1] + y[i] * y[i];
  size_t first = 0, last = 0;
  for (size_t i = 0; i < y.size(); ++i)
  {
    if (!first && energy[i] <= energy[0] * std::pow(10., -.5)) first = i;
    if (energy[i] <= energy[0] * std::pow(10., -2.5)) { last = i; break; }
  }
  assert(last > first);
  return 3. * (last - first) / rate;
}
inline UtilityControls Controls(double tank, double decay, double drip)
{
  UtilityControls c; c.spring = {decay, 3., drip, 100., 0.}; c.reverb[2] = 12000.;
  c.extra.springTank = tank; c.extra.springPredelay = 0.;
  return c;
}
inline std::vector<double> Impulse(double rate, double tank, double decay, double drip, double seconds)
{
  SlotSpring spring; spring.Reset(rate);
  const auto c = Controls(tank, decay, drip);
  std::vector<double> l(static_cast<size_t>(rate * seconds)), r(l.size());
  l[0] = r[0] = .5;
  spring.Process(l.data(), r.data(), static_cast<int>(l.size()), c);
  return l;
}
} // namespace spring_tank_test

inline void TestSpringTank()
{
  using namespace spring_tank_test;
  for (double rate : {44100., 48000., 96000.})
  {
    // Drip 100%: 4 kHz arrives well after 1.5 kHz, and 3.5 kHz after 700 Hz (measured 21.5 and 9.3 ms
    // at 48 kHz). Drip 0% turns the stages into plain delays: every band arrives together.
    const auto dripping = Impulse(rate, 1., 1.8, 100., .2), flat = Impulse(rate, 1., 1.8, 0., .2);
    assert(Arrival(dripping, rate, 4000.) - Arrival(dripping, rate, 1500.) > 10.);
    assert(Arrival(dripping, rate, 3500.) - Arrival(dripping, rate, 700.) > 4.);
    for (double hz : {700., 1500., 3500.}) assert(std::abs(Arrival(flat, rate, hz) - Arrival(flat, rate, 300.)) < 1.);
    // Decay: the 1 kHz band reaches -60 dB at the set time (measured within 10%).
    for (double decay : {.5, 1.8, 4.})
    {
      const double measured = DecayT20(Impulse(rate, 1., decay, 60., 2. * decay + 1.), rate);
      assert(measured > .8 * decay && measured < 1.2 * decay);
    }
    // Wet level of guitar-band playing (noise low-passed at 3 kHz, so every rate gets the same
    // material) matches Classic, so switching keeps loudness.
    std::array<double, 2> rms{};
    for (int tank : {0, 1})
    {
      SlotSpring spring; spring.Reset(rate);
      const auto c = Controls(tank, 1.8, 60.);
      std::vector<double> l(static_cast<size_t>(rate * 4.)), r(l.size());
      uint32_t seed = 1u;
      const double pole = std::exp(-2. * grid_fx::pi * 3000. / rate);
      double s1 = 0., s2 = 0.;
      for (size_t i = 0; i < l.size() / 2; ++i)
      {
        seed = seed * 1664525u + 1013904223u;
        s1 = .2 * (seed * (2. / 4294967295.) - 1.) + pole * (s1 - .2 * (seed * (2. / 4294967295.) - 1.));
        s2 = s1 + pole * (s2 - s1);
        l[i] = r[i] = s2;
      }
      spring.Process(l.data(), r.data(), static_cast<int>(l.size()), c);
      double sum = 0.; for (double v : l) sum += v * v;
      rms[tank] = std::sqrt(sum / l.size());
    }
    assert(std::abs(20. * std::log10(rms[1] / rms[0])) < 1.); // measured -0.7 to +0.2 dB
    // Host blocks do not change the result, and processing never allocates.
    SlotSpring whole, split; whole.Reset(rate); split.Reset(rate);
    const auto c = Controls(1., 1.8, 70.);
    std::vector<double> a(8192), b(8192), ar(8192), br(8192);
    for (int i = 0; i < 8192; ++i) a[i] = b[i] = .2 * std::sin(i * .031), ar[i] = br[i] = .15 * std::sin(i * .017);
    whole.Process(a.data(), ar.data(), 8192, c);
    for (int at = 0, k = 0; at < 8192; ++k)
    {
      const int n = std::min(8192 - at, std::array<int, 5>{1, 37, 64, 255, 512}[k % 5]);
      allocations = deletions = 0; tracking = true;
      split.Process(b.data() + at, br.data() + at, n, c);
      tracking = false; assert(allocations == 0 && deletions == 0);
      at += n;
    }
    assert(a == b && ar == br);
  }
  std::cout << "Spring tank: dispersive chirps follow Drip, 1 kHz decay, Classic-matched level, blocks and allocation at 44.1-96 kHz passed\n";
}
