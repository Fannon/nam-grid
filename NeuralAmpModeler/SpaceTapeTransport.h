#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace expanded_delay
{
// All heads and both channels follow one transport. Time is the last head's delay.
class TapeTransport
{
public:
  void Reset(double rate)
  {
    *this = {};
    mRate = rate;
    // RE-201 full-range step measurements, Downing & Terjesen (2016), section 3.2.
    mSlowDown = -std::expm1(-1. / (1.89 * rate));
    mSpeedUp = -std::expm1(-1. / (1.07 * rate));
    mNextDrift = Random();
  }

  void Tick(double targetMs, double motion)
  {
    if (mTimeMs < 0.) mTimeMs = targetMs;
    const double error = targetMs - mTimeMs;
    double delta = error * (error > 0. ? mSlowDown : mSpeedUp);
    // One millisecond of added delay per millisecond would stop playback.
    // Bend large slowdown steps smoothly toward that limit across the extended Time range.
    const double half = 500. / mRate;
    if (delta > half) delta = 2. * half - half * half / delta;
    mTimeMs += delta;

    const double wowHz = WowHz(mTimeMs);
    AdvancePhase(mWow, wowHz / mRate);
    AdvancePhase(mFlutter, wowHz * kFlutterRatio / mRate);
    if (AdvancePhase(mDriftPhase, kDriftHz / mRate))
    {
      mDrift = mNextDrift;
      mNextDrift = Random();
    }
    const double u = mDriftPhase;
    const double drift = mDrift + (mNextDrift - mDrift) * u * u * (3. - 2. * u);
    const double flutter = .8 * std::sin(kTau * mFlutter) + .2 * std::sin(2. * kTau * mFlutter);
    const double offset = kWow * std::sin(kTau * mWow) + kDrift * drift + kFlutter * flutter;
    const double tap = mTimeMs * .001 * mRate + motion * mRate * offset;
    // Modulation automation adds its own velocity. Keep even the combined trajectory forward.
    mTap = mTap < 0. ? tap : std::min(tap, mTap + .99);
  }

  double TimeMs() const { return mTimeMs; }
  double Tap() const { return mTap; }
  static double MotionCents(double ms, double motion)
  {
    // The cubic drift can travel two units per segment, with peak normalized slope 1.5.
    const double slope = motion * (kTau * WowHz(ms) * (kWow + kFlutter * kFlutterRatio * 1.2)
                                   + 3. * kDriftHz * kDrift);
    return -1200. * std::log2(1. - slope);
  }

private:
  static constexpr double kTau = 2. * std::numbers::pi, kFlutterRatio = 6.7 / .43;
  static constexpr double kWow = .0023, kDrift = .0014, kFlutter = .00038, kDriftHz = .7;
  static double WowHz(double ms) { return .43 * std::clamp(std::sqrt(420. / ms), .55, 2.5); }
  static bool AdvancePhase(double& phase, double step)
  {
    phase += step;
    if (phase < 1.) return false;
    phase -= 1.;
    return true;
  }
  double Random()
  {
    mSeed = 1664525u * mSeed + 1013904223u;
    return static_cast<double>(mSeed) * (2. / 4294967295.) - 1.;
  }
  uint32_t mSeed = 0x51ce7a9du;
  double mRate = 48000., mTimeMs = -1., mTap = -1., mSlowDown = 0., mSpeedUp = 0.;
  double mWow = 0., mFlutter = 0., mDriftPhase = 0., mDrift = 0., mNextDrift = 0.;
};
}
