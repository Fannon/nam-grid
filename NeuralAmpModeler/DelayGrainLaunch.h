#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace expanded_delay
{
class GrainLauncher
{
public:
  struct Grain
  {
    double age = 1., offset = 0., step = 0., phase = 0.;
    bool latched = false;
  };
  static double Frames(double length, double rate) { return std::max(8., std::round(length * rate * .001)); }
  Grain Launch(double rate, double position, double limit, double delay, double length, double speed, double scatter, bool reverse, bool glitch)
  {
    const double frames = Frames(length, rate);
    // Normal forward playback at zero scatter reads the ordinary delay tap.
    const double step = reverse ? 1. + speed : 1. - speed;
    const double earliest = reverse ? 0. : std::max(0., -step * frames);
    const double jitter = scatter * std::min(delay, .42 * rate);
    double age = std::max(2. + earliest, delay + (Random() - .5) * jitter - step * frames * .5);
    bool latched = false;
    if (glitch)
    {
      if (mLatch == 0 && Random() < .2 + .65 * scatter)
      {
        mCaptured = position - age;
        mLatch = 2 + static_cast<int>(5. * scatter);
      }
      if (mLatch > 0)
      {
        age = std::max(2. + earliest, position - mCaptured);
        latched = true;
        --mLatch;
      }
    }
    Grain grain{age, reverse || scatter > 0. ? (Random() - .5) * jitter * .24 : 0., step, 0., latched};
    // Keep the whole grain, both channels included, inside the line: long reverse or
    // slowed grains and drifting Glitch latches would otherwise read clamped audio.
    grain.age = std::min(grain.age, limit - 2. - std::max(0., step) * frames - std::abs(grain.offset));
    grain.offset = std::max(2. + earliest - grain.age, grain.offset);
    return grain;
  }
private:
  double Random()
  {
    mSeed ^= mSeed << 13; mSeed ^= mSeed >> 17; mSeed ^= mSeed << 5;
    return static_cast<double>(mSeed) * (1. / 4294967295.);
  }
  double mCaptured = 0.;
  uint32_t mSeed = 0xc283ee81u;
  int mLatch = 0;
};
}
