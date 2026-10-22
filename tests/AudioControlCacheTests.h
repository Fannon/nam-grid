#pragma once
#include "../NeuralAmpModeler/AudioControlCache.h"

inline void TestAudioControlCache()
{
  AudioControlCache<int, 2> cache;
  int reads = 0;
  const auto read = [&](int& value) { value = ++reads; };
  cache.Refresh(0, read);
  cache.Refresh(0, read);
  assert(reads == 1 && cache.Values()[0] == 1);
  cache.Refresh(1, read);
  assert(reads == 2 && cache.Values()[1] == 2);
  cache.Invalidate(0);
  cache.Refresh(1, read);
  assert(reads == 2); // A slot change must not rebuild unrelated banks.
  cache.Refresh(0, [&](int& value) {
    value = ++reads;
    cache.Invalidate(0); // A control callback races the in-progress snapshot.
  });
  cache.Refresh(0, read);
  assert(reads == 4 && cache.Values()[0] == 4); // No lost invalidation.
  cache.InvalidateAll();
  allocations = deletions = 0;
  tracking = true;
  cache.Refresh(0, read);
  cache.Refresh(1, read);
  tracking = false;
  assert(reads == 6 && allocations == 0 && deletions == 0);
  std::cout << "Audio controls: per-slot cache, concurrent invalidation and no allocations passed\n";
}
