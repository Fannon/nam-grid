#pragma once
#include <array>
#include <cstddef>

namespace airwindows_outboard
{
// NAM Grid adaptation: sample-clock interpolation between CStrip2 coefficient
// updates. Initial/unchanged settings use the original values without arithmetic.
template <std::size_t N>
class CoefficientRamp
{
public:
  void Target(const std::array<double, N>& target)
  {
    if (mFirst) { mCurrent = mTarget = target; mFirst = false; return; }
    if (target == mTarget) return;
    mTarget = target;
    for (std::size_t i = 0; i < N; ++i) mStep[i] = (mTarget[i] - mCurrent[i]) / 32.;
    mRemaining = 32;
  }
  bool Tick()
  {
    if (!mRemaining) return false;
    if (--mRemaining == 0) mCurrent = mTarget;
    else for (std::size_t i = 0; i < N; ++i) mCurrent[i] += mStep[i];
    return true;
  }
  const std::array<double, N>& Values() const { return mCurrent; }
private:
  std::array<double, N> mCurrent{}, mTarget{}, mStep{};
  int mRemaining = 0;
  bool mFirst = true;
};
} // namespace airwindows_outboard
