#pragma once
#include <algorithm>
#include <cmath>
#include <complex>
#include <span>
#include <stdexcept>
#include <vector>

// Offline test utilities only. Coherent tones need no analysis window: this
// avoids counting a window's skirts as aliases. Production DSP is not involved
// in the FFT or the reference bin calculations.
namespace spectral_test
{
inline constexpr double pi = 3.14159265358979323846;
inline double DB(double amplitude) { return 20. * std::log10(std::max(1.e-15, amplitude)); }
inline int FoldBin(int bin, int count)
{
  bin = ((bin % count) + count) % count;
  return std::min(bin, count - bin);
}
inline double ToneAmplitude(std::span<const double> audio, int bin)
{
  std::complex<double> value{};
  for (size_t i = 0; i < audio.size(); ++i)
    value += audio[i] * std::polar(1., -2. * pi * bin * double(i) / audio.size());
  return std::abs(value) * (bin == 0 || bin == audio.size() / 2 ? 1. : 2.) / audio.size();
}
inline std::vector<std::complex<double>> FFT(std::span<const double> audio)
{
  const size_t n = audio.size();
  if (n < 2 || (n & (n - 1))) throw std::runtime_error("Spectral FFT requires a power-of-two length");
  std::vector<std::complex<double>> out(audio.begin(), audio.end());
  for (size_t i = 1, j = 0; i < n; ++i)
  {
    size_t bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) std::swap(out[i], out[j]);
  }
  for (size_t length = 2; length <= n; length *= 2)
  {
    const auto step = std::polar(1., -2. * pi / length);
    for (size_t start = 0; start < n; start += length)
    {
      std::complex<double> phase = 1.;
      for (size_t j = 0; j < length / 2; ++j, phase *= step)
      {
        const auto a = out[start + j], b = phase * out[start + j + length / 2];
        out[start + j] = a + b;
        out[start + j + length / 2] = a - b;
      }
    }
  }
  return out;
}
inline std::vector<int> FoldedBins(int count, int fundamental, int maxOrder = 63)
{
  std::vector<bool> selected(count / 2 + 1);
  for (int order = 2; order <= maxOrder; ++order)
    if (order * fundamental > count / 2)
    {
      const int bin = FoldBin(order * fundamental, count);
      if (bin != 0 && bin % fundamental != 0) selected[bin] = true;
    }
  std::vector<int> result;
  for (int bin = 1; bin <= count / 2; ++bin) if (selected[bin]) result.push_back(bin);
  return result;
}
// The same physical-frequency bins can be read in a higher-rate render when
// its analysis window has proportionally more samples (the same duration).
// This avoids a reference decimator's phase and stopband contaminating the test.
inline double BandRMS(std::span<const double> audio, std::span<const int> bins)
{
  const auto spectrum = FFT(audio);
  double power = 0.;
  for (int bin : bins)
  {
    if (bin < 0 || bin > audio.size() / 2) throw std::runtime_error("Invalid spectral band bin");
    power += std::norm(spectrum[bin]) * (bin == 0 || bin == audio.size() / 2 ? 1. : 2.);
  }
  return std::sqrt(power) / audio.size();
}
struct Measurement
{
  double rms = 0., residualRMS = 0., foldedRMS = 0., fundamental = 0., peak = 0.;
  double ResidualDB() const { return DB(residualRMS / std::max(1.e-15, rms)); }
  double FoldedDB() const { return DB(foldedRMS / std::max(1.e-15, rms)); }
};
inline Measurement Measure(std::span<const double> audio, int fundamental, int maxOrder = 63, int baseCount = 0)
{
  const int n = static_cast<int>(audio.size());
  if (fundamental <= 0 || fundamental >= n / 2) throw std::runtime_error("Invalid fundamental bin");
  const int bandCount = baseCount ? baseCount : n;
  if (bandCount < 2 || bandCount > n || (bandCount & 1)) throw std::runtime_error("Invalid analysis bandwidth");
  const int limit = bandCount / 2;
  const auto spectrum = FFT(audio);
  std::vector<bool> legal(n / 2 + 1), folded(n / 2 + 1);
  legal[0] = true;
  for (int bin = fundamental; bin <= limit; bin += fundamental) legal[bin] = true;
  // A folded harmonic must not coincide with any legitimate in-band harmonic.
  // These selected bins are evidence only for stationary, harmonic processors;
  // modulation, noise and pitch effects can legitimately put energy here too.
  for (int bin : FoldedBins(bandCount, fundamental, maxOrder)) folded[bin] = true;
  Measurement result;
  double total = 0., residual = 0., aliases = 0.;
  for (double x : audio)
  {
    if (!std::isfinite(x)) throw std::runtime_error("Nonfinite spectral render");
    result.peak = std::max(result.peak, std::abs(x));
  }
  for (int bin = 1; bin <= limit; ++bin)
  {
    const double power = std::norm(spectrum[bin]) * (bin == n / 2 ? 1. : 2.) / (double(n) * n);
    total += power;
    if (!legal[bin]) residual += power;
    if (folded[bin]) aliases += power;
  }
  result.rms = std::sqrt(total); // DC excluded, including for asymmetric shapers.
  result.residualRMS = std::sqrt(residual);
  result.foldedRMS = std::sqrt(aliases);
  result.fundamental = 2. * std::abs(spectrum[fundamental]) / n;
  return result;
}
inline double SuppressionDB(std::span<const double> native, std::span<const double> filtered, int aliasBin)
{
  return DB(ToneAmplitude(native, aliasBin) / std::max(1.e-15, ToneAmplitude(filtered, aliasBin)));
}
} // namespace spectral_test
