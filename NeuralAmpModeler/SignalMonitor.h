#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// Measurement only. No processing buffer, gain, filtering or audio writes.
namespace signal_monitor
{
struct Accumulator
{
  std::array<double, 2> energy{}, peak{};
  double product = 0.;
  uint64_t frames = 0;
  void Push(double l, double r)
  {
    l = std::isfinite(l) ? std::clamp(l, -1.e6, 1.e6) : 0.;
    r = std::isfinite(r) ? std::clamp(r, -1.e6, 1.e6) : 0.;
    energy[0] += l*l; energy[1] += r*r; product += l*r; ++frames;
    peak[0] = std::max(peak[0], std::abs(l)); peak[1] = std::max(peak[1], std::abs(r));
  }
};
// UI-side 300 ms exponential RMS/correlation; peaks cover every audio sample
// received in meter packets, not merely the displayed FFT/waveform window.
struct Meter
{
  std::array<double, 2> power{}, peak{}, hold{};
  double product = 0.;
  bool first = true;
  void Reset() { *this = {}; }
  void Receive(const Accumulator& a, double rate)
  {
    if (!a.frames || !(rate > 0.)) return;
    const double blend = first ? 1. : -std::expm1(-static_cast<double>(a.frames) / (.3*rate));
    for (int c=0;c<2;++c)
    {
      power[c] += blend*(a.energy[c]/a.frames-power[c]);
      peak[c] = a.peak[c]; hold[c] = std::max(hold[c],peak[c]);
    }
    product += blend*(a.product/a.frames-product); first = false;
  }
  double RMS(int ch) const { return std::sqrt(std::max(0.,power[ch])); }
  double Correlation() const
  { return power[0]>1.e-20 && power[1]>1.e-20 ? std::clamp(product/std::sqrt(power[0]*power[1]),-1.,1.) : 0.; }
  static double DB(double amplitude) { return 20.*std::log10(std::max(1.e-6,amplitude)); }
};
} // namespace signal_monitor
