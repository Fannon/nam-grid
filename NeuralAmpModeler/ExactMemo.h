#pragma once
#include <cstdint>
#include <cstring>

namespace grid_fx
{
// Exact cache for a pure per-sample function. Smoothed controls settle and
// envelopes/gains rest at fixed values, so steady state skips exp/pow/log/tan
// without changing output. Pass the function's complete argument (including any
// sample-rate term) so a cached value can never outlive its inputs.
class Memo
{
public:
  template <class Function>
  double operator()(double argument, Function&& function)
  {
    // Bitwise identity: signed zeros/NaN payloads are distinct, so the result is always exact.
    uint64_t bits;
    std::memcpy(&bits, &argument, sizeof bits);
    if (!mValid || bits != mBits)
    {
      mBits = bits;
      mValid = true;
      mValue = function(argument);
    }
    return mValue;
  }

private:
  uint64_t mBits = 0;
  double mValue = 0.;
  bool mValid = false;
};
} // namespace grid_fx
