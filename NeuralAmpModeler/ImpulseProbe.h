#pragma once
#include "SlotOperations.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <mutex>
#include <thread>
#include <vector>

// Plays an impulse through a private copy of a built-in module on a worker thread, so
// the editor can show how the module spreads one pick attack over time: separate spikes
// are distinct echoes, a smooth fill is a diffuse wash. This is the module's measured
// wet response, not a sketch. Editor only; the audio thread never touches it.
class ImpulseProbe
{
public:
  static constexpr int bins = 240;
  static constexpr double rate = 24000.; // Effects scale with the rate; half the work of 48 kHz.
  struct Request
  {
    SlotEffect effect = SlotEffect::NAM;
    UtilityControls controls;
    double seconds = 1.;
    uint64_t key = 0;
  };
  struct Result
  {
    std::array<float, bins> peak{}; // Linear peak of each time bin, both channels.
    double seconds = 0.;
    uint64_t key = 0;
    bool valid = false;
  };

  ~ImpulseProbe()
  {
    mAbort.store(true, std::memory_order_relaxed);
    {
      std::lock_guard<std::mutex> lock(mMutex);
      mStop = true;
    }
    mWake.notify_one();
    if (mWorker.joinable()) mWorker.join();
  }
  // Asks for a measurement; a newer request replaces one that has not started.
  void Ask(const Request& request)
  {
    {
      std::lock_guard<std::mutex> lock(mMutex);
      if (request.key == mResult.key || (mPending && request.key == mRequest.key) || request.key == mRunning) return;
      mRequest = request;
      mPending = true;
      if (!mWorker.joinable()) mWorker = std::thread([this] { Run(); });
    }
    mWake.notify_one();
  }
  Result Latest() const
  {
    std::lock_guard<std::mutex> lock(mMutex);
    return mResult;
  }
  // The measurement itself. `abort` stops it early (the result is then invalid).
  static Result Measure(const Request& request, const std::atomic<bool>* abort = nullptr)
  {
    Result result;
    result.seconds = request.seconds;
    result.key = request.key;
    constexpr int block = 256;
    SlotConfig config;
    config.effect = request.effect;
    auto prepared = PrepareSlot(config, rate, block);
    if (!prepared.dsp) return result;
    NAMStage stage;
    stage.Publish(std::move(prepared.dsp));
    stage.ResetStopped(rate, block);
    const int frames = std::max(bins, static_cast<int>(request.seconds * rate));
    std::array<double, block> l{}, r{};
    for (int position = 0; position < frames; position += block)
    {
      if (abort && abort->load(std::memory_order_relaxed)) return result;
      const int n = std::min(block, frames - position);
      l.fill(0.); r.fill(0.);
      if (position == 0) l[0] = r[0] = 1.;
      stage.Process(l.data(), n, true, 0., 0., {}, {}, request.controls, r.data());
      for (int i = 0; i < n; ++i)
      {
        const int bin = std::min(bins - 1, static_cast<int>(static_cast<int64_t>(position + i) * bins / frames));
        const double level = std::max(std::abs(l[i]), std::abs(r[i]));
        if (std::isfinite(level)) result.peak[bin] = std::max(result.peak[bin], static_cast<float>(level));
      }
    }
    result.valid = true;
    return result;
  }
  // A stable key for a set of controls, so an unchanged module is measured once.
  static uint64_t Key(SlotEffect effect, std::initializer_list<double> values)
  {
    uint64_t key = 1469598103934665603ull ^ static_cast<uint64_t>(effect);
    for (const double value : values)
    {
      uint64_t bits = 0;
      std::memcpy(&bits, &value, sizeof(bits));
      key = (key ^ bits) * 1099511628211ull;
    }
    return key;
  }

private:
  void Run()
  {
    std::unique_lock<std::mutex> lock(mMutex);
    while (true)
    {
      mWake.wait(lock, [this] { return mStop || mPending; });
      if (mStop) return;
      const auto request = mRequest;
      mPending = false;
      mRunning = request.key;
      lock.unlock();
      const auto measured = Measure(request, &mAbort);
      lock.lock();
      mRunning = 0;
      if (measured.valid) mResult = measured;
    }
  }
  mutable std::mutex mMutex;
  std::condition_variable mWake;
  std::thread mWorker;
  Request mRequest;
  Result mResult;
  uint64_t mRunning = 0;
  bool mPending = false, mStop = false;
  std::atomic<bool> mAbort{false};
};
