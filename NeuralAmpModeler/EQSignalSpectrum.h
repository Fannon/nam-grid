#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

// UI-thread FFT utilities for the latest output window. The audio thread only
// copies the selected slot's samples into a separate sender; other slots'
// meter packets stay small. Module views share Signal Monitor's detailed display.
namespace grid_eq_signal
{
constexpr size_t windowSize = 4096;
constexpr size_t bands = 128;
// Waveform views use the newest scopeSize samples; Signal Monitor also needs
// a full right-channel window for a cancellation-safe stereo spectrum.
constexpr size_t scopeSize = 1024;
struct Window
{
  std::array<float, windowSize> samples{};  // Left output, oldest first.
  std::array<float, windowSize> right{};     // Right output (left for mono), oldest first.
};

// Hann-windowed bin magnitudes, scaled so a sine's peak bin reads its amplitude.
inline std::array<float, windowSize / 2> Magnitudes(const std::array<float, windowSize>& samples)
{
  std::array<std::complex<float>, windowSize> bins{};
  constexpr float pi = 3.14159265358979323846f;
  // All editors use the same window; build it once on the UI thread instead
  // of evaluating 4096 cosines for each arriving meter packet.
  static const auto hann = [] {
    std::array<float, windowSize> weights{};
    for (size_t i = 0; i < windowSize; ++i)
      weights[i] = .5f - .5f * std::cos(2.f * pi * static_cast<float>(i) / static_cast<float>(windowSize - 1));
    return weights;
  }();
  for (size_t i = 0; i < windowSize; ++i)
    bins[i] = {samples[i] * hann[i], 0.f};
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
  // A Hann window halves a sine's peak bin: 4/N reads a sine's peak amplitude.
  std::array<float, windowSize / 2> magnitudes{};
  for (size_t k = 0; k < magnitudes.size(); ++k)
    magnitudes[k] = std::abs(bins[k]) * (4.f / static_cast<float>(windowSize));
  return magnitudes;
}

inline std::array<float, bands> Analyze(const std::array<float, windowSize>& samples, double rate)
{
  const auto magnitudes = Magnitudes(samples);
  // Each log band reads the loudest bin between its edges. Above ~400 Hz a band
  // spans several bins; sampling one point would skip most harmonics.
  std::array<float, bands> result{};
  const double upper = std::min(20000., .49 * rate);
  const auto bin = [&](double hz) {
    return std::clamp(hz * windowSize / rate, 1., static_cast<double>(windowSize / 2 - 1));
  };
  for (size_t band = 0; band < bands; ++band)
  {
    const double hz = 20. * std::pow(upper / 20., static_cast<double>(band) / (bands - 1));
    const double halfStep = std::pow(upper / 20., .5 / (bands - 1));
    const auto first = static_cast<size_t>(std::lround(bin(hz / halfStep)));
    const auto last = std::max(first, static_cast<size_t>(std::lround(bin(hz * halfStep))));
    float magnitude = 0.f;
    for (size_t k = first; k <= last; ++k)
      magnitude = std::max(magnitude, magnitudes[k]);
    result[band] = std::clamp(20.f * std::log10(std::max(1.e-8f, magnitude)), -72.f, 0.f);
  }
  return result;
}
} // namespace grid_eq_signal
