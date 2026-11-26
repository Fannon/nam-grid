#pragma once
#include "GridTunerHistory.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <cmath>
#include <thread>

// MPM key-peak selection adapted from trace-tuner/src/core.rs at
// 2e1ff89243698b8f4fca33dc4505a2cd749d9e67 (MIT, Simon Heimler, 2026).
// See ThirdPartyNotices.txt. Direct autocorrelation replaces RustFFT;
// all detection runs on the worker, never in the host's processing callback.
namespace grid_tuner
{
inline constexpr int window = 1536, hopSize = 256;
static_assert(std::atomic<double>::is_always_lock_free && std::atomic<size_t>::is_always_lock_free);
inline double Detect(const double* samples, double rate)
{
  std::array<double, window> x{}, energy{}, nsdf{};
  double mean = 0.;
  for (int i = 0; i < window; ++i)
  {
    if (!std::isfinite(samples[i]))
      return 0.;
    mean += samples[i];
  }
  mean /= window;
  double total = 0.;
  for (int i = 0; i < window; ++i)
  {
    x[i] = samples[i] - mean;
    total += x[i] * x[i];
    energy[i] = total;
  }
  if (total / window < 1.e-8 || !std::isfinite(rate) || rate < 4000.)
    return 0.;
  const int low = std::max(2, static_cast<int>(rate / 1600.)),
            high = std::min(window / 2, static_cast<int>(rate / 35.));
  for (int lag = 1; lag <= high; ++lag)
  {
    double acf = 0.;
    for (int i = 0; i < window - lag; ++i)
      acf += x[i] * x[i + lag];
    const double denominator = energy[window - lag - 1] + total - energy[lag - 1];
    nsdf[lag] = denominator > 1.e-12 ? 2. * acf / denominator : 0.;
  }
  std::array<int, window / 2> peaks{};
  int count = 0, lag = 1;
  while (lag < high && nsdf[lag] > 0.)
    ++lag; // reject the lobe attached to zero
  lag = std::max(lag, low);
  double highest = 0.;
  while (lag < high)
  {
    while (lag < high && nsdf[lag] <= 0.)
      ++lag;
    int peak = lag;
    while (lag < high && nsdf[lag] > 0.)
    {
      if (nsdf[lag] > nsdf[peak])
        peak = lag;
      ++lag;
    }
    if (peak > low && peak < high)
    {
      peaks[count++] = peak;
      highest = std::max(highest, nsdf[peak]);
    }
  }
  if (highest < .85)
    return 0.;
  for (int i = 0; i < count; ++i)
  {
    const int p = peaks[i];
    if (nsdf[p] < highest * .9)
      continue;
    const double denominator = nsdf[p - 1] - 2. * nsdf[p] + nsdf[p + 1];
    const double offset =
      std::abs(denominator) > 1.e-12 ? std::clamp(.5 * (nsdf[p - 1] - nsdf[p + 1]) / denominator, -1., 1.) : 0.;
    const double hz = rate / (p + offset);
    return hz >= 35. && hz <= 1600. ? hz : 0.;
  }
  return 0.;
}

// Smooth within a note in pitch space. Confirm large changes across two
// overlapping windows so a brief harmonic at the attack cannot take over.
class PitchTracker
{
public:
  double Update(double frequency, double seconds)
  {
    if (frequency <= 0.)
    {
      mCandidate = 0.;
      mSilence += seconds;
      if (mSilence >= .065) mFrequency = 0.;
      return mFrequency;
    }
    mSilence = 0.;
    if (mFrequency <= 0.) return mFrequency = frequency;

    const double change = std::log2(frequency / mFrequency);
    if (std::abs(change) > 45. / 1200.)
    {
      if (mCandidate > 0. && std::abs(std::log2(frequency / mCandidate)) <= 25. / 1200.)
      {
        mCandidate = 0.;
        return mFrequency = frequency;
      }
      mCandidate = frequency;
      return mFrequency;
    }
    mCandidate = 0.;
    const double weight = -std::expm1(-seconds / .06);
    return mFrequency *= std::exp2(weight * change);
  }

private:
  double mFrequency = 0., mCandidate = 0., mSilence = 0.;
};

class Tuner
{
public:
  Tuner()
  : mWorker([this] { Run(); })
  {
  }
  ~Tuner()
  {
    {
      std::lock_guard<std::mutex> lock(mWaitMutex);
      mStop.store(true);
    }
    mWake.notify_one();
    mWorker.join();
  }
  void Enable(bool enable)
  {
    // Also called at audio block boundaries by host automation. No locks or
    // notifications here; the sleeping worker checks the flag every 20 ms.
    if (mEnabled.exchange(enable) == enable) return;
    mGeneration.fetch_add(1);
    mFrequency.store(0.);
  }
  bool Enabled() const { return mEnabled.load(); }
  double Frequency() const { return Enabled() && mResultGeneration.load() == mGeneration.load() && Ticks() - mLastResult.load() < 700 ? mFrequency.load() : 0.; }
  auto ReadHistory() const { return mHistory.Read(Ticks(), mGeneration.load()); }
  static long long Now() { return Ticks(); }
  // Audio producer only. Rate changes and queue overruns invalidate partial windows.
  void Push(const double* left, const double* right, int frames, double rate)
  {
    if (!Enabled())
      return;
    const auto generation = mGeneration.load();
    const int decimation = std::max(1, static_cast<int>(std::ceil(rate / 12000.)));
    if (generation != mAudioGeneration || rate != mRate)
    {
      mAudioGeneration = generation;
      mRate = rate;
      mCoefficient = std::exp(-6.283185307179586 * 2200. / rate);
      mPhase = 0;
      mLP = {};
      ++mEpoch;
    }
    double lEnergy = 0., rEnergy = 0.;
    for (int i = 0; i < frames; ++i)
    {
      lEnergy += left[i] * left[i];
      if (right)
        rEnergy += right[i] * right[i];
    }
    const double* source = right && rEnergy > lEnergy ? right : left;
    const double coefficient = mCoefficient;
    auto write = mWrite.load(std::memory_order_relaxed);
    const auto startWrite = write;
    auto read = mRead.load(std::memory_order_acquire);
    constexpr size_t mask = capacity - 1;
    static_assert((capacity & mask) == 0);
    for (int i = 0; i < frames; ++i)
    {
      double x = std::isfinite(source[i]) ? source[i] : 0.;
      for (auto& lp : mLP)
      {
        lp = x + coefficient * (lp - x);
        if (std::abs(lp) < 1.e-30)
          lp = 0.;
        x = lp;
      }
      if (++mPhase < decimation)
        continue;
      mPhase = 0;
      const auto next = (write + 1) & mask;
      if (next == read)
      {
        read = mRead.load(std::memory_order_acquire);
        if (next == read)
        {
          ++mEpoch;
          continue;
        }
      }
      mQueue[write] = {x, rate / decimation, generation, mEpoch};
      write = next;
    }
    if (write != startWrite)
      mWrite.store(write, std::memory_order_release);
  }

private:
  struct Sample
  {
    double value = 0., rate = 12000.;
    unsigned generation = 0, epoch = 0;
  };
  static constexpr size_t capacity = 8192;
  static long long Ticks()
  {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
      .count();
  }
  void Run()
  {
    std::array<double, window> ring{}, ordered{};
    PitchTracker tracker;
    int index = 0, size = 0, hop = 0;
    unsigned generation = 0, epoch = 0;
    while (!mStop.load())
    {
      if (!Enabled())
      {
        std::unique_lock<std::mutex> lock(mWaitMutex);
        mWake.wait_for(lock, std::chrono::milliseconds(20), [this] { return mStop.load() || Enabled(); });
        if (mStop.load())
          break;
      }
      auto read = mRead.load(std::memory_order_relaxed);
      const auto startRead = read;
      const auto write = mWrite.load(std::memory_order_acquire);
      constexpr size_t mask = capacity - 1;
      while (read != write)
      {
        const auto sample = mQueue[read];
        read = (read + 1) & mask;
        if (!Enabled() || sample.generation != mGeneration.load())
        {
          size = hop = index = 0;
          continue;
        }
        if (sample.generation != generation || sample.epoch != epoch)
        {
          generation = sample.generation;
          epoch = sample.epoch;
          tracker = {};
          size = hop = index = 0;
        }
        ring[index] = sample.value;
        index = (index + 1) % window;
        size = std::min(window, size + 1);
        if (++hop >= hopSize && size == window)
        {
          hop = 0;
          for (int i = 0; i < window; ++i)
            ordered[i] = ring[(index + i) % window];
          const double frequency = tracker.Update(Detect(ordered.data(), sample.rate), hopSize / sample.rate);
          if (Enabled() && generation == mGeneration.load())
          {
            const auto now = Ticks();
            mHistory.Append(frequency, now, generation);
            mLastResult.store(now);
            mFrequency.store(frequency);
            mResultGeneration.store(generation);
          }
        }
      }
      if (read != startRead)
        mRead.store(read, std::memory_order_release);
      if (!Enabled())
        mFrequency.store(0.);
      std::this_thread::sleep_for(std::chrono::milliseconds(4));
    }
  }
  History mHistory;
  std::array<Sample, capacity> mQueue{};
  std::atomic<size_t> mRead{0}, mWrite{0};
  std::atomic<bool> mEnabled{false}, mStop{false};
  std::atomic<unsigned> mGeneration{0}, mResultGeneration{0};
  std::atomic<double> mFrequency{0.};
  std::atomic<long long> mLastResult{0};
  std::array<double, 4> mLP{};
  double mRate = 0., mCoefficient = 0.;
  unsigned mAudioGeneration = 0, mEpoch = 0;
  int mPhase = 0;
  std::mutex mWaitMutex;
  std::condition_variable mWake;
  std::thread mWorker;
};
} // namespace grid_tuner
