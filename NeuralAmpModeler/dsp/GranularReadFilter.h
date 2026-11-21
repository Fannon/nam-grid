#pragma once
#include <array>
#include <cmath>
#include <cstddef>

namespace grid_granular
{
// Fractional-delay low-pass kernels for the fifth, octave, octave + fifth and
// double octave. All kernels operate on recorded history at the input rate.
struct ReadFilter
{
  static constexpr int taps = 40, phases = 256;
  using Kernel = std::array<float, taps>;
  using Band = std::array<Kernel, phases + 1>;
  std::array<Band, 4> bands{};

  ReadFilter()
  {
    constexpr double pi = 3.14159265358979323846;
    constexpr double semitones[]{7., 12., 19., 24.};
    for (std::size_t b = 0; b < bands.size(); ++b)
    {
      const double cutoff = .45 / std::exp2(semitones[b] / 12.);
      for (int phase = 0; phase <= phases; ++phase)
      {
        auto& kernel = bands[b][phase];
        double sum = 0.;
        for (int i = 0; i < taps; ++i)
        {
          const double x = i - (taps / 2 - 1) - static_cast<double>(phase) / phases;
          const double window = .42 + .5 * std::cos(2. * pi * x / taps) + .08 * std::cos(4. * pi * x / taps);
          const double sinc = x == 0. ? 2. * cutoff : std::sin(2. * pi * cutoff * x) / (pi * x);
          kernel[i] = static_cast<float>(window * sinc);
          sum += kernel[i];
        }
        for (float& weight : kernel) weight = static_cast<float>(weight / sum);
      }
    }
  }

  // Interval families stay well apart even at the largest random detune.
  const Band* ForPitch(double semitones) const
  {
    if (semitones < 6.) return nullptr;
    if (semitones < 10.) return &bands[0];
    if (semitones < 16.) return &bands[1];
    if (semitones < 22.) return &bands[2];
    return &bands[3];
  }

  static double Read(const float* samples, const Kernel& kernel)
  {
    std::array<float, 4> sum{};
    for (int i = 0; i < taps; i += 4)
      for (int lane = 0; lane < 4; ++lane) sum[lane] += samples[i + lane] * kernel[i + lane];
    return (sum[0] + sum[1]) + (sum[2] + sum[3]);
  }
};
} // namespace grid_granular
