#pragma once
#include <algorithm>
#include <cmath>

// Continuous 300 ms mean-square average. Emission windows do not reset the RMS
// integrator, so host block lengths and UI packet delivery cannot change levels.
struct LevelAnalysis
{
  void Reset(double rate)
  {
    rate = std::isfinite(rate) && rate > 0. ? rate : 48000.;
    mCoefficient = -std::expm1(-1. / (.3 * rate));
    mWindow = std::max(1, static_cast<int>(std::lround(rate / 30.)));
    mPosition = 0; mPower = mPeak = peak = rms = 0.;
  }
  bool Push(double sample)
  {
    sample = std::isfinite(sample) ? std::clamp(sample, -1.e6, 1.e6) : 0.;
    mPower += mCoefficient * (sample * sample - mPower);
    mPeak = std::max(mPeak, std::abs(sample));
    if (++mPosition < mWindow) return false;
    mPosition = 0; peak = mPeak; mPeak = 0.;
    if (mPower < 1.e-16) mPower = 0.;
    rms = std::sqrt(std::max(0., mPower));
    return true;
  }
  double peak = 0., rms = 0.;
private:
  double mCoefficient = -std::expm1(-1. / (.3 * 48000.)), mPower = 0., mPeak = 0.;
  int mWindow = 1600, mPosition = 0;
};
