#pragma once
#include "SlotTimeEffects.h"
#include <cstring>

template<size_t N>
void CheckSettledSmoothing(int offset, bool explicitIDs)
{
  std::array<int, N> ids{};
  for (size_t j = 0; j < N; ++j) ids[j] = offset + static_cast<int>(j);
  for (double rate : {44100., 48000., 96000.})
  {
    grid_fx::SmoothControls<N> controls;
    std::array<double, N> target{}, expected{};
    const double coefficient = std::exp(-1. / (.02 * rate));
    controls.Reset(rate);
    for (int i = 0; i < 120000; ++i)
    {
      if (i % 37 == 0)
      {
        for (size_t j = 0; j < N; ++j)
        {
          const auto& descriptor = kEffectParameters[ids[j]];
          const double fraction = i < 4000 ? .2 + .1 * std::sin(i * .005) : i < 12000 ? .8 : .37;
          target[j] = descriptor.minimum + fraction * (descriptor.maximum - descriptor.minimum);
        }
        if (explicitIDs) controls.Target(target, ids);
        else controls.Target(target, offset);
        if (i == 0) expected = target;
      }
      // Original recurrence, including its rounded fixed point: never snap a
      // settled value to the requested target or restart it on identical Target.
      for (size_t j = 0; j < N; ++j)
        expected[j] = target[j] + coefficient * (expected[j] - target[j]);
      const auto& actual = controls.Tick();
      assert(std::memcmp(actual.data(), expected.data(), sizeof(expected)) == 0);
    }
    controls.Reset(rate);
    controls.Target(target, ids);
    expected = target;
    for (size_t j = 0; j < N; ++j)
      expected[j] = target[j] + coefficient * (expected[j] - target[j]);
    allocations = deletions = 0; tracking = true;
    const auto& reset = controls.Tick();
    tracking = false;
    assert(allocations == 0 && deletions == 0);
    assert(std::memcmp(reset.data(), expected.data(), sizeof(expected)) == 0);
  }
}

inline void TestSettledSmoothing()
{
  CheckSettledSmoothing<3>(uModRate, false);
  CheckSettledSmoothing<9>(uPedalStyle, true);
  std::cout << "Settled smoothing: original recurrence, target changes, reset and allocations passed\n";
}
