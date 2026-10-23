#pragma once
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstddef>

// Trace connection rules inspired by trace-tuner/src/ui.rs at
// 2e1ff89243698b8f4fca33dc4505a2cd749d9e67 (MIT, Simon Heimler, 2026).
// See ThirdPartyNotices.txt. History stores frequency so the UI can
// reinterpret existing points after a reference change.
namespace grid_tuner
{
inline constexpr int historyLength = 384;
inline constexpr long long historyDurationMs = 6000;
struct HistoryPoint
{
  double frequency = 0.;
  long long milliseconds = 0;
};
struct Pitch
{
  int note = 0;
  double cents = 0.;
  bool valid = false;
};
inline Pitch ToPitch(double frequency, double reference)
{
  if (!std::isfinite(frequency) || frequency < 35. || frequency > 1600.
      || !std::isfinite(reference) || reference <= 0.) return {};
  const double midi = 69. + 12. * std::log2(frequency / reference);
  const int note = static_cast<int>(std::round(midi));
  return {note, 100. * (midi - note), true};
}
inline bool Connect(Pitch previous, Pitch current)
{
  return previous.valid && current.valid && previous.note == current.note
         && std::abs(current.cents - previous.cents) <= 35.;
}
// One detection-worker writer. Atomic entries allow a UI snapshot without locks,
// including when the ring wraps. A sequence guards each complete point.
class History
{
public:
  void Append(double frequency, long long milliseconds, unsigned generation)
  {
    auto& entry = mEntries[mWrite % historyLength];
    entry.sequence.fetch_add(1);
    entry.frequency.store(frequency);
    entry.milliseconds.store(milliseconds);
    entry.generation.store(generation);
    entry.sequence.fetch_add(1);
    ++mWrite;
    mPublished.store(mWrite);
  }
  std::array<HistoryPoint, historyLength> Read(long long now, unsigned generation) const
  {
    std::array<HistoryPoint, historyLength> result{};
    const auto end = mPublished.load();
    for (int i = 0; i < historyLength; ++i)
    {
      const auto& entry = mEntries[(end + i) % historyLength];
      const auto sequence = entry.sequence.load();
      if (!sequence || (sequence & 1)) continue;
      const HistoryPoint point{entry.frequency.load(), entry.milliseconds.load()};
      const auto epoch = entry.generation.load();
      if (sequence == entry.sequence.load() && epoch == generation
          && point.milliseconds <= now && now - point.milliseconds <= historyDurationMs)
        result[i] = point;
    }
    return result;
  }
private:
  struct Entry
  {
    std::atomic<unsigned> sequence{0}, generation{0};
    std::atomic<double> frequency{0.};
    std::atomic<long long> milliseconds{0};
  };
  std::array<Entry, historyLength> mEntries{};
  size_t mWrite = 0;
  std::atomic<size_t> mPublished{0};
};
} // namespace grid_tuner
