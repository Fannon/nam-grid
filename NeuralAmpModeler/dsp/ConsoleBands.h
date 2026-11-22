#pragma once
#include <algorithm>
#include <array>
#include <cmath>

// Console EQ's three bands. Bass is a one-pole low band at Bass freq, Treble a one-pole
// high band at Treble freq and Mid what lies between, so the bands always sum back to the
// input. Each band passes through the Airwindows density curve (Chris Johnson, CStrip2 and
// Density, MIT; see dsp/airwindows/outboard): boosts blend toward a sine and saturate on
// louder notes, cuts blend toward a starved curve. Unlike CStrip2, whose band filters sit
// far below their labels and whose curve roughly doubles every change on quiet signals,
// each knob's decibels are the quiet-signal change and each frequency is the band corner.
namespace console_eq
{
inline constexpr double halfPi = 1.5707963267948966;
// Upstream's density d: the band is scaled by 1 + d and blended by |d| toward the sine
// curve (d > 0) or the starved cosine curve (d < 0). Small signals see
// (1 + d)(1 - d + d pi/2) for boosts and (1 + d)^2 for cuts; this solves for the d
// whose quiet-signal gain is exactly `db`. A signed d crosses neutral continuously.
inline double DensityFor(double db)
{
  const double gain = std::pow(10., db / 20.);
  if (db < 0.) return std::sqrt(gain) - 1.;
  constexpr double k = halfPi - 1.;
  return (-halfPi + std::sqrt(halfPi * halfPi - 4. * k * (1. - gain))) / (2. * k);
}
inline double Shape(double x, double density)
{
  x *= 1. + density;
  if (density == 0.) return x;
  const double blend = std::abs(density);
  const double magnitude = std::abs(x), angle = std::min(magnitude * halfPi, halfPi);
  const double curve = density > 0. ? std::sin(angle) : 1. - std::cos(angle);
  const double y = magnitude * (1. - blend) + curve * blend;
  return x < 0. ? -y : y;
}
class Bands
{
public:
  void Reset() { mLow = {}; mTop = {}; mFirst = true; mSteps = 0; }
  // Gains in dB (-6 to +6), corners in Hz. The densities ramp linearly over `steps`
  // samples (the caller's control interval); the first settings apply immediately.
  void Set(double rate, double trebleDB, double midDB, double bassDB, double trebleHz, double bassHz, int steps)
  {
    // Bilinear (TPT) one-poles: the analogue first-order response, so the high band
    // reaches its full change near Nyquist instead of stopping short of it.
    const auto coefficient = [rate](double hz) {
      const double g = std::tan(3.141592653589793 * std::clamp(hz, 10., .45 * rate) / rate);
      return g / (1. + g);
    };
    const std::array<double, 3> target{DensityFor(trebleDB), DensityFor(midDB), DensityFor(bassDB)};
    if (mFirst || steps < 1) { mDensity = target; mStep = {}; mSteps = 0; mFirst = false; }
    else
    {
      for (int b = 0; b < 3; ++b) mStep[b] = (target[b] - mDensity[b]) / steps;
      mTarget = target;
      mSteps = steps;
    }
    mLowAmount = coefficient(bassHz);
    mTopAmount = coefficient(trebleHz);
  }
  // Advance the density ramps once per sample, before both channels' Tick.
  void Advance()
  {
    if (mSteps == 0) return;
    if (--mSteps == 0) mDensity = mTarget;
    else for (int b = 0; b < 3; ++b) mDensity[b] += mStep[b];
  }
  double Tick(double x, int channel)
  {
    const double bass = Low(x, mLow[channel], mLowAmount), top = Low(x, mTop[channel], mTopAmount);
    const double treble = x - top, mid = top - bass;
    return Shape(bass, mDensity[2]) + Shape(mid, mDensity[1]) + Shape(treble, mDensity[0]);
  }

private:
  static double Low(double x, double& state, double amount)
  {
    const double v = (x - state) * amount, low = v + state;
    state = low + v;
    if (std::abs(state) < 1.e-30) state = 0.;
    return low;
  }
  std::array<double, 3> mDensity{}, mTarget{}, mStep{}; // Treble, Mid, Bass.
  int mSteps = 0;
  bool mFirst = true;
  double mLowAmount = 0., mTopAmount = 0.;
  std::array<double, 2> mLow{}, mTop{};
};
} // namespace console_eq
