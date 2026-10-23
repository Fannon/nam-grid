#pragma once
#include "../NeuralAmpModeler/RigTransition.h"

inline void TestRigTransition()
{
  using namespace std::chrono_literals;
  // Without a processing audio thread a hold is withdrawn, so the caller edits directly.
  RigTransition stopped;
  assert(!stopped.Hold(5ms) && stopped.Idle());

  for (const auto mode : {RigTransition::Mode::Dip, RigTransition::Mode::Crossfade})
  {
    RigTransition transition;
    std::atomic<bool> run{true};
    std::atomic<int> holdingBlocks{0}, runningBlocks{0};
    double minPower = 2., maxPower = 0., previousOut = 1., previousIn = 0.;
    bool overlap = false, monotonic = true;
    std::thread audio([&] {
      while (run)
      {
        const auto phase = transition.Begin();
        if (phase == RigTransition::kHolding) ++holdingBlocks;
        if (phase == RigTransition::kRunning)
        {
          ++runningBlocks;
          allocations = deletions = 0;
          tracking = true;
          for (int i = 0; i < 64; ++i)
          {
            const double out = transition.OutgoingGain(), in = transition.IncomingGain();
            monotonic &= out <= previousOut + 1e-12 && in >= previousIn - 1e-12;
            previousOut = out; previousIn = in;
            overlap |= out > 0. && in > 0.;
            minPower = std::min(minPower, out * out + in * in);
            maxPower = std::max(maxPower, out * out + in * in);
            transition.Advance();
          }
          tracking = false;
          assert(allocations == 0 && deletions == 0);
          if (transition.Done()) transition.Finish();
        }
        std::this_thread::sleep_for(100us);
      }
    });
    assert(transition.Hold(1s));
    // Audio keeps acknowledging the hold until the next rig is ready.
    std::this_thread::sleep_for(5ms);
    assert(holdingBlocks > 0 && !transition.Idle());
    transition.Start(1, mode, 48000.);
    assert(transition.WaitIdle(2s));
    run = false;
    audio.join();
    assert(transition.Target() == 1 && previousOut < 1e-3 && previousIn > .999 && monotonic);
    assert(transition.OutgoingGain() == 0. && transition.IncomingGain() == 1.); // Exact once complete.
    if (mode == RigTransition::Mode::Crossfade)
      assert(overlap && std::abs(minPower - 1.) < 1e-9 && std::abs(maxPower - 1.) < 1e-9); // Equal power.
    else
      assert(!overlap && minPower == 0.); // Old out, then new in: one grid at a time.
    const int expected = static_cast<int>(48000. * (mode == RigTransition::Mode::Dip ? 2 * RigTransition::kDipSeconds
                                                                                      : RigTransition::kCrossfadeSeconds));
    assert(std::abs(runningBlocks * 64 - expected) <= 64);
  }
  // A rejected rig releases the hold without switching.
  RigTransition rejected;
  std::atomic<bool> run{true};
  std::thread audio([&] { while (run) { rejected.Begin(); std::this_thread::sleep_for(100us); } });
  assert(rejected.Hold(1s));
  rejected.Release();
  assert(rejected.Idle());
  run = false;
  audio.join();
  std::cout << "Rig transitions: hold handshake, timeout fallback, dip and equal-power crossfade passed\n";
}
