#pragma once
#include "EffectParameters.h"

// Balance preserves unity at center and attenuates the opposite side at extremes.
// Stereo is a centered mid/side amount; polarity and gain changes ramp through zero without a jump.
class StereoUtility
{
public:
  void Reset(double rate)
  {
    mStep = 1. / std::max(1., rate * .005);
    mFirst = true;
  }
  void Process(double* left, double* right, int frames, const UtilityControls& c)
  {
    const double gain = std::pow(10., std::clamp(c.level, -60., 12.) * .05);
    const double balance = std::clamp(c.balance * .01, -1., 1.);
    const std::array<double, 3> target{gain * (1. - std::max(0., balance)) * (c.invertL ? -1. : 1.),
                                       gain * (1. + std::min(0., balance)) * (c.invertR ? -1. : 1.),
                                       1. + std::clamp(c.stereo * .01, -1., 1.)};
    if (mFirst)
    {
      mValue = target;
      mFirst = false;
    }
    // A fixed-time ramp is independent of host block size.
    for (int i = 0; i < frames; ++i)
    {
      for (int j = 0; j < 3; ++j)
        mValue[j] += std::clamp(target[j] - mValue[j], -mStep * 8., mStep * 8.);
      const double l = left[i], r = right ? right[i] : l;
      const double mid = .5 * (l + r), side = .5 * (l - r) * mValue[2];
      left[i] = (mid + side) * mValue[0];
      if (right)
        right[i] = (mid - side) * mValue[1];
    }
  }

private:
  std::array<double, 3> mValue{1., 1., 1.};
  double mStep = 1. / 240.;
  bool mFirst = true;
};
