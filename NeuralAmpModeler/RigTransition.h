#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>

// Hands a rig preset over between the plugin's two grids. The control thread first asks
// audio to hold the audible rig's controls, prepares the new rig in the other grid, then
// starts the switch. Audio either dips (old rig fades out, then the new one fades in, so
// only one grid runs at a time) or crossfades (both grids run briefly, equal power).
// One control producer, one audio consumer; no locks or allocation on audio.
class RigTransition
{
public:
  enum class Mode { Dip, Crossfade };
  enum Phase : int { kIdle, kHoldRequested, kHolding, kReady, kRunning };

  // Control thread ----------------------------------------------------------
  // Waits for an earlier switch, then asks audio to freeze the audible rig. Returns false
  // when audio does not answer in time (host not processing); then edit the audible grid.
  bool Hold(std::chrono::milliseconds timeout)
  {
    if (!WaitIdle(timeout))
      return false;
    mPhase.store(kHoldRequested, std::memory_order_release);
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (mPhase.load(std::memory_order_acquire) == kHoldRequested)
    {
      if (std::chrono::steady_clock::now() >= deadline)
      {
        int expected = kHoldRequested;
        // Withdraw the request unless audio acknowledged it at the last moment.
        if (mPhase.compare_exchange_strong(expected, kIdle, std::memory_order_acq_rel))
          return false;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
  }
  // Starts the switch to the prepared grid. Only valid while holding.
  void Start(int target, Mode mode, double sampleRate)
  {
    mTarget = target;
    mMode = mode;
    mFrames = std::max(1, static_cast<int>(sampleRate * (mode == Mode::Dip ? kDipSeconds : kCrossfadeSeconds)));
    mPhase.store(kReady, std::memory_order_release);
  }
  // Ends a hold without switching, e.g. when the new rig was rejected.
  void Release() { mPhase.store(kIdle, std::memory_order_release); }
  bool WaitIdle(std::chrono::milliseconds timeout) const
  {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!Idle())
    {
      if (std::chrono::steady_clock::now() >= deadline)
        return false;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
  }
  bool Idle() const { return mPhase.load(std::memory_order_acquire) == kIdle; }

  // Audio thread ------------------------------------------------------------
  // Call once per block before reading controls. Acknowledges a hold and starts a ready
  // switch; Started() then reports the first block of that switch.
  Phase Begin()
  {
    const int phase = mPhase.load(std::memory_order_acquire);
    mStarted = false;
    if (phase == kHoldRequested)
    {
      mPhase.store(kHolding, std::memory_order_release);
      return kHolding;
    }
    if (phase == kReady)
    {
      mPosition = 0;
      mStarted = true;
      mPhase.store(kRunning, std::memory_order_release);
      return kRunning;
    }
    return static_cast<Phase>(phase);
  }
  bool Started() const { return mStarted; }
  int Target() const { return mTarget; }
  Mode GetMode() const { return mMode; }
  int Length() const { return mMode == Mode::Dip ? 2 * mFrames : mFrames; }
  int Position() const { return mPosition; }
  // Whether the outgoing / incoming grid is audible anywhere in the next `frames` samples.
  bool OutgoingAudible() const { return mPosition < mFrames; }
  bool IncomingAudible(int frames) const { return mMode == Mode::Crossfade || mPosition + frames > mFrames; }
  // Gains at the current position; Advance() moves to the next sample.
  double OutgoingGain() const
  {
    if (mPosition >= mFrames)
      return 0.;
    const double x = static_cast<double>(mPosition) / mFrames;
    return mMode == Mode::Crossfade ? std::cos(kHalfPi * x) : .5 * (1. + std::cos(kPi * x));
  }
  double IncomingGain() const
  {
    const int position = mMode == Mode::Crossfade ? mPosition : mPosition - mFrames;
    if (position <= 0 || position >= mFrames)
      return position <= 0 ? 0. : 1.;
    const double x = static_cast<double>(position) / mFrames;
    return mMode == Mode::Crossfade ? std::sin(kHalfPi * x) : .5 * (1. - std::cos(kPi * x));
  }
  void Advance() { ++mPosition; }
  bool Done() const { return mPosition >= Length(); }
  // Call after the last use of the outgoing grid in the final block.
  void Finish() { mPhase.store(kIdle, std::memory_order_release); }

  // Host stopped (reset): any switch is complete.
  void FinishStopped() { mPhase.store(kIdle, std::memory_order_release); mStarted = false; }

  static constexpr double kDipSeconds = .012, kCrossfadeSeconds = .04;

private:
  static constexpr double kPi = 3.141592653589793, kHalfPi = kPi / 2.;
  std::atomic<int> mPhase{kIdle};
  // Written by control before the release store of kReady; read by audio afterwards.
  int mTarget = 0, mFrames = 1;
  Mode mMode = Mode::Dip;
  // Audio only.
  int mPosition = 0;
  bool mStarted = false;
};
