#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include "EQSignalSpectrum.h"

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
// Level history in fixed 50 ms columns of audio frames, so the time axis stays
// exact however meter packets are batched. Each column keeps the louder channel's
// sample peak and RMS; the column being filled is shown live.
struct History
{
  static constexpr size_t columns = 400;
  static constexpr double columnSeconds = .05;
  struct Column
  {
    float peak = 0.f, rms = 0.f;
    bool resumed = false; // Monitoring restarted here after a pause.
  };
  std::array<Column, columns> done{}; // Ring of completed columns; oldest at write.
  size_t write = 0, count = 0;
  double frames = 0., energy = 0.;
  float peak = 0.f;
  bool resumed = false;
  void Reset() { *this = {}; }
  void Resume() { if (count || frames > 0.) resumed = true; }
  void Receive(const Accumulator& a, double rate)
  {
    if (!a.frames || !(rate > 0.)) return;
    const double width = std::max(1., rate * columnSeconds);
    const double total = static_cast<double>(a.frames), power = std::max(a.energy[0], a.energy[1]);
    const float loudest = static_cast<float>(std::max(a.peak[0], a.peak[1]));
    for (double left = total; left > 0.;)
    {
      if (frames >= width) { Commit(); continue; } // After a sample-rate change.
      const double take = std::min(left, width - frames);
      energy += power * take / total; frames += take; left -= take;
      peak = std::max(peak, loudest);
      if (frames >= width) Commit();
    }
  }
  Column Current() const { return {peak, frames > 0. ? static_cast<float>(std::sqrt(energy / frames)) : 0.f, resumed}; }
  // Completed column by age: 0 is the newest finished column.
  const Column& Ago(size_t age) const { return done[(write + columns - 1 - age) % columns]; }
private:
  void Commit()
  {
    done[write] = Current();
    write = (write + 1) % columns; count = std::min(columns, count + 1);
    frames = energy = 0.; peak = 0.f; resumed = false;
  }
};

// Spectrum at many log-spaced points: the loudest bin across wide high bands, and
// interpolation between bins at low frequencies, where one bin spans several points.
inline constexpr size_t spectrumPoints = 384;
inline constexpr float spectrumFloor = -72.f;
inline double SpectrumUpper(double rate) { return std::min(20000., .49 * rate); }
inline double PointHz(size_t point, double rate)
{ return 20. * std::pow(SpectrumUpper(rate) / 20., static_cast<double>(point) / (spectrumPoints - 1)); }
inline std::array<float, spectrumPoints> Detailed(const std::array<float, grid_eq_signal::windowSize / 2>& magnitudes, double rate)
{
  std::array<float, spectrumPoints> db{};
  const double binHz = rate / grid_eq_signal::windowSize, last = magnitudes.size() - 1.;
  const double halfStep = std::pow(SpectrumUpper(rate) / 20., .5 / (spectrumPoints - 1));
  for (size_t i = 0; i < spectrumPoints; ++i)
  {
    const double hz = PointHz(i, rate), low = std::clamp(hz / halfStep / binHz, 1., last), high = std::clamp(hz * halfStep / binHz, 1., last);
    float magnitude = 0.f;
    if (high - low >= 1.)
      for (auto k = static_cast<size_t>(std::ceil(low)); k <= static_cast<size_t>(high); ++k) magnitude = std::max(magnitude, magnitudes[k]);
    else
    {
      const double position = std::clamp(hz / binHz, 1., last - 1.);
      const auto k = static_cast<size_t>(position);
      const double t = position - k;
      magnitude = static_cast<float>(magnitudes[k] + t * (magnitudes[k + 1] - magnitudes[k]));
    }
    db[i] = std::clamp(20.f * std::log10(std::max(1.e-8f, magnitude)), spectrumFloor, 0.f);
  }
  return db;
}

// Persistent narrow peaks in a long-term spectrum: local maxima standing at least
// 6 dB above the mean of their surrounding third-octaves. Broad tone and isolated
// noise do not qualify; the strongest three, a third-octave apart, are kept.
struct Resonance { size_t point = 0; float db = spectrumFloor, prominence = 0.f; };
inline size_t FindResonances(const std::array<float, spectrumPoints>& db, double rate, std::array<Resonance, 3>& found)
{
  const double octavesPerPoint = std::log2(SpectrumUpper(rate) / 20.) / (spectrumPoints - 1);
  const int third = std::max(2, static_cast<int>(std::lround(1. / 3. / octavesPerPoint)));
  const int core = std::max(1, third / 4), n = static_cast<int>(spectrumPoints);
  std::array<Resonance, spectrumPoints> candidates{};
  size_t count = 0;
  for (int i = third; i < n - third; ++i)
  {
    const double hz = PointHz(static_cast<size_t>(i), rate);
    if (hz < 40. || hz > 16000. || db[i] < spectrumFloor + 12.f) continue;
    bool top = true;
    for (int j = i - core; j <= i + core && top; ++j) top = j == i || db[j] < db[i] || (db[j] == db[i] && j > i);
    if (!top) continue;
    double sum = 0.; int used = 0;
    for (int j = i - third; j <= i + third; ++j)
      if (std::abs(j - i) > core) { sum += db[j]; ++used; }
    const float prominence = db[i] - static_cast<float>(sum / used);
    if (prominence >= 6.f) candidates[count++] = {static_cast<size_t>(i), db[i], prominence};
  }
  std::sort(candidates.begin(), candidates.begin() + count,
            [](const Resonance& a, const Resonance& b) { return a.prominence > b.prominence; });
  size_t kept = 0;
  for (size_t c = 0; c < count && kept < found.size(); ++c)
  {
    bool apart = true;
    for (size_t k = 0; k < kept; ++k)
      apart &= std::abs(static_cast<int>(candidates[c].point) - static_cast<int>(found[k].point)) >= third;
    if (apart) found[kept++] = candidates[c];
  }
  return kept;
}

// UI-side spectrum state: live display with instant rise and gentle fall, plus a
// 6-second power average over playing (silence does not pull it down) for resonances.
struct SpectrumView
{
  std::array<float, spectrumPoints> live{}, average{};
  std::array<double, spectrumPoints> power{};
  double averaged = 0.; // Seconds of signal in the average.
  void Reset() { *this = {}; }
  void Receive(const std::array<float, spectrumPoints>& now, bool fresh, double seconds)
  {
    for (size_t i = 0; i < spectrumPoints; ++i)
      live[i] = fresh ? std::max(now[i], .65f * live[i] + .35f * now[i]) : now[i];
    if (*std::max_element(now.begin(), now.end()) < spectrumFloor + 12.f) return;
    seconds = std::clamp(seconds, 0., .5);
    const double blend = averaged > 0. ? -std::expm1(-seconds / 6.) : 1.;
    for (size_t i = 0; i < spectrumPoints; ++i)
    {
      power[i] += blend * (std::pow(10., now[i] * .1) - power[i]);
      average[i] = static_cast<float>(std::clamp(10. * std::log10(std::max(1.e-12, power[i])), static_cast<double>(spectrumFloor), 0.));
    }
    averaged += seconds;
  }
  bool Settled() const { return averaged >= 2.; }
};
} // namespace signal_monitor
