#pragma once
#include "MinimumPhaseOversampling.h"

namespace grid_oversampling
{
// The linear identity path through the actual interpolator/decimator is a
// host-rate FIR. Collapse it during preparation rather than running a second
// oversampler per dry channel. It has the wet filters' phase, not a fixed delay.
template <int Factor, int BaseTaps = 32>
class LinearReference
{
public:
  static constexpr int taps = 2 * BaseTaps + 1, ring = taps > 128 ? 256 : 128;
  void Reset()
  {
    mHistory = {}; mIndex = 0;
    // Reset is called off audio, including the first shared kernel construction.
    static const auto kernel = [] {
      FIR<Factor, BaseTaps> filter; filter.Reset();
      std::array<double, taps> result{};
      for (int i = 0; i < taps; ++i)
        result[i] = filter.Tick(i == 0 ? 1. : 0., [](double x) { return x; });
      return result;
    }();
    mKernel = kernel;
  }
  double Tick(double input) { Push(input); return Filtered(); }
  // Keep history even at fully wet/dry endpoints, without paying for a dot product.
  void Push(double input)
  {
    mIndex = (mIndex - 1) & (ring - 1);
    mHistory[mIndex] = mHistory[mIndex + ring] = input;
  }
  double Filtered() const
  {
    double result = 0.;
#if (defined(_M_X64) || defined(__x86_64__)) && !defined(NAM_PEDAL_DRIVE_SCALAR)
    auto sum = _mm_setzero_pd();
    for (int i = 0; i < taps - 1; i += 2)
      sum = _mm_add_pd(sum, _mm_mul_pd(_mm_loadu_pd(mKernel.data() + i), _mm_loadu_pd(mHistory.data() + mIndex + i)));
    std::array<double, 2> lanes{}; _mm_storeu_pd(lanes.data(), sum);
    result = lanes[0] + lanes[1];
#elif defined(__aarch64__) && !defined(NAM_PEDAL_DRIVE_SCALAR)
    auto sum = vdupq_n_f64(0.);
    for (int i = 0; i < taps - 1; i += 2)
      sum = vaddq_f64(sum, vmulq_f64(vld1q_f64(mKernel.data() + i), vld1q_f64(mHistory.data() + mIndex + i)));
    result = vgetq_lane_f64(sum, 0) + vgetq_lane_f64(sum, 1);
#else
    for (int i = 0; i < taps - 1; ++i) result += mKernel[i] * mHistory[mIndex + i];
#endif
    return Flush(result + mKernel[taps - 1] * mHistory[mIndex + taps - 1]);
  }
private:
  std::array<double, taps> mKernel{};
  std::array<double, 2 * ring> mHistory{};
  int mIndex = 0;
};

// Exact dry at Mix zero and an aligned dry reference at nonzero Mix require a
// phase transition. Fade that bypass over 20 ms instead of switching samples.
// The transition can briefly colour the signal, as with bypassing a phase-shifting
// effect; settled partial Mix has no resampler-induced dry/wet cancellation.
class MixBypass
{
public:
  void Reset(double rate) { mStep = 1. / (.02 * std::max(1., rate)); mFirst = true; mValue = mTarget = 0.; }
  void Target(bool active)
  {
    mTarget = active ? 1. : 0.;
    if (mFirst) { mValue = mTarget; mFirst = false; }
  }
  void Tick() { mValue = mTarget > mValue ? std::min(mTarget, mValue + mStep) : std::max(mTarget, mValue - mStep); }
  bool IsDry() const { return mValue == 0.; }
  double Apply(double dry, double aligned) const
  { return mValue == 0. ? dry : mValue == 1. ? aligned : dry + mValue * (aligned - dry); }
private:
  double mStep = 1., mValue = 0., mTarget = 0.;
  bool mFirst = true;
};
} // namespace grid_oversampling
