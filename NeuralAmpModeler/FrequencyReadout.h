#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

// UI frequency axes, independent of tuning detection and audio processing.
namespace frequency_readout
{
inline double Hertz(double position, double low, double high, bool logarithmic = true)
{
  position = std::clamp(position, 0., 1.);
  return logarithmic ? low * std::pow(high / low, position) : low + position * (high - low);
}
inline std::string Label(double hz)
{
  char text[64];
  if (!(hz > 0.) || !std::isfinite(hz)) return "0 Hz";
  const int midi = static_cast<int>(std::lround(69. + 12. * std::log2(hz / 440.)));
  constexpr const char* names[]{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  const int octave = static_cast<int>(std::floor(midi / 12.)) - 1;
  std::snprintf(text, sizeof(text), hz < 1000. ? "%.1f Hz | %s%d" : "%.0f Hz | %s%d",
                hz, names[(midi % 12 + 12) % 12], octave);
  return text;
}
} // namespace frequency_readout
