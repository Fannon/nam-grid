#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>

// Parameter callbacks only invalidate. Values and seen revisions belong to audio.
// A change during Refresh stays pending for the next block; no lock or allocation.
template <class Controls, std::size_t Count>
class AudioControlCache
{
public:
  static_assert(std::atomic<uint64_t>::is_always_lock_free);
  AudioControlCache() { mSeen.fill(std::numeric_limits<uint64_t>::max()); }
  void Invalidate(std::size_t slot) { mRevision[slot].fetch_add(1, std::memory_order_release); }
  void InvalidateAll()
  {
    for (std::size_t slot = 0; slot < Count; ++slot)
      Invalidate(slot);
  }
  template <class Read>
  void Refresh(std::size_t slot, Read&& read)
  {
    const auto revision = mRevision[slot].load(std::memory_order_acquire);
    if (revision == mSeen[slot])
      return;
    read(mValues[slot]);
    mSeen[slot] = revision;
  }
  auto& Values() { return mValues; } // audio only, or host stopped

private:
  std::array<Controls, Count> mValues{};
  std::array<std::atomic<uint64_t>, Count> mRevision{};
  std::array<uint64_t, Count> mSeen{};
};
