#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

// Graphic EQ voicings, shared by the processor, the editor and the docs metadata.
namespace graphic_eq
{
inline constexpr int kBands = 7;
inline constexpr std::array<std::array<double, kBands>, 3> kCentres{{
  {100., 200., 400., 800., 1600., 3200., 6400.},   // Guitar: octave bands
  {40., 80., 160., 320., 800., 2500., 6000.},      // Bass: fundamentals, growl, attack, string noise
  {63., 160., 400., 1000., 2500., 6300., 16000.}}};// Full range: 1.33-octave ISO centres
inline constexpr std::array<std::array<const char*, kBands>, 3> kLabels{{
  {"100 Hz", "200 Hz", "400 Hz", "800 Hz", "1.6 kHz", "3.2 kHz", "6.4 kHz"},
  {"40 Hz", "80 Hz", "160 Hz", "320 Hz", "800 Hz", "2.5 kHz", "6 kHz"},
  {"63 Hz", "160 Hz", "400 Hz", "1 kHz", "2.5 kHz", "6.3 kHz", "16 kHz"}}};
inline int Voicing(double value) { return std::clamp(static_cast<int>(value + .5), 0, 2); }
// Bell Q from the band's distance to its neighbours (half the span between them, in octaves).
inline double Q(int voicing, int band)
{
  const auto& f = kCentres[Voicing(voicing)];
  const double below = band > 0 ? f[band] / f[band - 1] : f[band + 1] / f[band];
  const double above = band < kBands - 1 ? f[band + 1] / f[band] : f[band] / f[band - 1];
  const double octaves = .5 * (std::log2(below) + std::log2(above));
  return 1. / (2. * std::sinh(.5 * std::log(2.) * octaves));
}
// Analog-prototype magnitude of the whole EQ at hz, for the display.
inline double Magnitude(double hz, int voicing, const std::array<double, kBands + 2>& c)
{
  std::complex<double> total(1., 0.);
  for (int b = 0; b < kBands; ++b)
  {
    const double k = 1. / Q(voicing, b), w = hz / kCentres[Voicing(voicing)][b];
    const std::complex<double> s(0., w), band = k * s / (s * s + k * s + 1.);
    total *= 1. + (std::pow(10., c[b] * .05) - 1.) * band;
  }
  return std::abs(total) * std::pow(10., c[kBands] * .05);
}
} // namespace graphic_eq
