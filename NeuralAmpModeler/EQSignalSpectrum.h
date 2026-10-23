#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

// UI-thread only: one 1024-point FFT of the latest EQ output window per meter
// update. The audio thread only copies samples into the existing meter packet.
namespace grid_eq_signal
{
constexpr size_t windowSize = 1024;
constexpr size_t bands = 64;

inline std::array<float, bands> Analyze(const std::array<float, windowSize>& samples, double rate)
{
  std::array<std::complex<float>, windowSize> bins{};
  constexpr float pi = 3.14159265358979323846f;
  for (size_t i = 0; i < windowSize; ++i)
  {
    const float hann = .5f - .5f * std::cos(2.f * pi * static_cast<float>(i) / static_cast<float>(windowSize - 1));
    bins[i] = {samples[i] * hann, 0.f};
  }
  for (size_t i = 1, j = 0; i < windowSize; ++i)
  {
    size_t bit = windowSize >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) std::swap(bins[i], bins[j]);
  }
  for (size_t length = 2; length <= windowSize; length <<= 1)
  {
    const auto step = std::polar(1.f, -2.f * pi / static_cast<float>(length));
    for (size_t base = 0; base < windowSize; base += length)
    {
      std::complex<float> phase{1.f, 0.f};
      for (size_t j = 0; j < length / 2; ++j)
      {
        const auto even = bins[base + j], odd = bins[base + j + length / 2] * phase;
        bins[base + j] = even + odd;
        bins[base + j + length / 2] = even - odd;
        phase *= step;
      }
    }
  }
  std::array<float, bands> result{};
  const double upper = std::min(20000., .49 * rate);
  for (size_t band = 0; band < bands; ++band)
  {
    const double hz = 20. * std::pow(upper / 20., static_cast<double>(band) / (bands - 1));
    const double position = std::clamp(hz * windowSize / rate, 1., static_cast<double>(windowSize / 2 - 1));
    const size_t low = static_cast<size_t>(position), high = std::min(low + 1, windowSize / 2 - 1);
    const float blend = static_cast<float>(position - low);
    const float magnitude = 4.f * ((1.f - blend) * std::abs(bins[low]) + blend * std::abs(bins[high]))
                          / static_cast<float>(windowSize);
    result[band] = std::clamp(20.f * std::log10(std::max(1.e-8f, magnitude)), -72.f, 0.f);
  }
  return result;
}
} // namespace grid_eq_signal
