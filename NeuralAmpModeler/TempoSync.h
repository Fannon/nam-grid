#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace grid_tempo
{
inline constexpr std::array<const char*, 9> names{"1/16", "1/8T", "1/8", "1/8D", "1/4", "1/4D", "1/2", "1/2D", "1/1"};
inline constexpr std::array<double, 9> beats{.25, 1. / 3., .5, .75, 1., 1.5, 2., 3., 4.};
inline double Tempo(double bpm)
{ return std::isfinite(bpm) && bpm > 0. ? std::clamp(bpm, 30., 300.) : 120.; }
inline double Seconds(double bpm, int division)
{ return 60. * beats[std::clamp(division, 0, 8)] / Tempo(bpm); }
} // namespace grid_tempo
