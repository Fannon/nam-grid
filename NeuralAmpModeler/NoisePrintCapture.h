#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#if (defined(_M_X64) || defined(__SSE2__)) && !defined(NAM_NOISE_PRINT_SCALAR)
  #include <emmintrin.h>
#endif

namespace noise_print
{
// Original MIT NAM Grid implementation. Summarize raw band power during Learn,
// then cap exceptional time buckets at four times each band's median. Counts
// preserve time weighting when a long capture coalesces adjacent buckets.
class RobustCapture
{
public:
  static constexpr int bands = 96;
  static constexpr int capacity = 32;

  void Reset(double rate)
  {
    mChunk = std::max<uint64_t>(1, static_cast<uint64_t>(.04 * rate));
    mUsed = 0;
    mPending = 0;
    mSum = {};
    mCounts = {};
  }

  // Reuse the detector's raw powers; no second band-power calculation.
  void Accumulate(int band, double power) { mSum[band] += power; }
#if (defined(_M_X64) || defined(__SSE2__)) && !defined(NAM_NOISE_PRINT_SCALAR)
  void Accumulate(int band, __m128d power)
  {
    _mm_storeu_pd(&mSum[band], _mm_add_pd(_mm_loadu_pd(&mSum[band]), power));
  }
#endif

  void Advance()
  {
    if (++mPending < mChunk)
      return;
    if (mUsed == capacity)
    {
      for (int i = 0; i < capacity / 2; ++i)
      {
        for (int k = 0; k < bands; ++k)
          mBuckets[i][k] = mBuckets[2 * i][k] + mBuckets[2 * i + 1][k];
        mCounts[i] = mCounts[2 * i] + mCounts[2 * i + 1];
      }
      mUsed = capacity / 2;
      mChunk *= 2;
    }
    mBuckets[mUsed] = mSum;
    mCounts[mUsed++] = mPending;
    mPending = 0;
    mSum = {};
  }

  std::array<double, bands> Finish(const std::array<double, bands>& fallback) const
  {
    // Very short captures retain the existing arithmetic. A useful robust
    // estimate needs enough independent summaries to distinguish an outlier.
    if (mUsed < 8)
      return fallback;
    std::array<double, bands> result{};
    for (int k = 0; k < bands; ++k)
    {
      std::array<double, capacity + 1> sorted{};
      for (int i = 0; i < mUsed; ++i)
        sorted[i] = mBuckets[i][k] / mCounts[i];
      int count = mUsed;
      if (mPending)
        sorted[count++] = mSum[k] / mPending;
      std::sort(sorted.begin(), sorted.begin() + count);
      const double limit = std::max(1.e-30, sorted[count / 2] * 4.);
      double sum = 0.;
      uint64_t samples = 0;
      for (int i = 0; i < mUsed; ++i)
      {
        sum += std::min(mBuckets[i][k], limit * mCounts[i]);
        samples += mCounts[i];
      }
      if (mPending)
      {
        sum += std::min(mSum[k], limit * mPending);
        samples += mPending;
      }
      result[k] = sum / samples;
    }
    return result;
  }

private:
  std::array<std::array<double, bands>, capacity> mBuckets{};
  std::array<double, bands> mSum{};
  std::array<uint64_t, capacity> mCounts{};
  uint64_t mChunk = 1920, mPending = 0;
  int mUsed = 0;
};
} // namespace noise_print
