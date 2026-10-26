#pragma once
#include "EffectParameters.h"
#include <vector>

// Balance preserves unity at center and attenuates the opposite side at extremes.
// Stereo is a centered mid/side amount; polarity and gain changes ramp through zero without a jump.
class StereoUtility
{
public:
  void Reset(double rate)
  {
    mStep = 1. / std::max(1., rate * .005);
    mFirst = true;
    mRate = rate; mDelay = mSkew = 0.; mIndex = 0; mMode = {1., 0., 0., 0.};
    for (auto& history : mHistory) history.assign(static_cast<size_t>(std::ceil(rate * .041)) + 2, 0.);
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
      mDelay = std::clamp(c.fineDelay, 0., 20.) * .001 * mRate;
      mSkew = std::clamp(c.extra.utilitySkew, -20., 20.) * .001 * mRate;
      mMode = {}; mMode[std::clamp(static_cast<int>(c.extra.utilityMode), 0, 3)] = 1.;
      mFirst = false;
    }
    // Slew 8 linear units in 5 ms (covering the largest gain/polarity change).
    // Smaller changes finish sooner; delay slews separately at .25 sample/sample.
    for (int i = 0; i < frames; ++i)
    {
      for (int j = 0; j < 3; ++j)
        mValue[j] += std::clamp(target[j] - mValue[j], -mStep * 8., mStep * 8.);
      const double l = left[i], r = right ? right[i] : l;
      const int mode = std::clamp(static_cast<int>(c.extra.utilityMode), 0, 3);
      for (int j = 0; j < 4; ++j) mMode[j] += std::clamp((mode == j ? 1. : 0.) - mMode[j], -mStep, mStep);
      mSkew += std::clamp(std::clamp(c.extra.utilitySkew, -20., 20.) * .001 * mRate - mSkew, -.25, .25);
      mDelay += std::clamp(std::clamp(c.fineDelay, 0., 20.) * .001 * mRate - mDelay, -.25, .25);
      mHistory[0][mIndex] = l; mHistory[1][mIndex] = r;
      const auto read = [&](int ch, double delay) {
        const size_t whole = static_cast<size_t>(delay);
        const double fraction = delay - whole;
        const size_t a = (mIndex + mHistory[ch].size() - whole) % mHistory[ch].size();
        const size_t b = (a + mHistory[ch].size() - 1) % mHistory[ch].size();
        return mHistory[ch][a] + fraction * (mHistory[ch][b] - mHistory[ch][a]);
      };
      const double originalL = read(0, mDelay + std::max(0., -mSkew));
      const double originalR = read(1, mDelay + std::max(0., mSkew));
      const double dl = (mMode[0] + mMode[2]) * originalL + (mMode[1] + mMode[3]) * originalR;
      const double dr = (mMode[0] + mMode[3]) * originalR + (mMode[1] + mMode[2]) * originalL;
      mIndex = (mIndex + 1) % mHistory[0].size();
      const double mid = .5 * (dl + dr), side = .5 * (dl - dr) * mValue[2];
      left[i] = (mid + side) * mValue[0];
      if (right)
        right[i] = (mid - side) * mValue[1];
    }
  }

private:
  std::array<double, 3> mValue{1., 1., 1.};
  double mStep = 1. / 240.;
  bool mFirst = true;
  std::array<std::vector<double>, 2> mHistory;
  size_t mIndex = 0;
  double mRate = 48000., mDelay = 0., mSkew = 0.;
  std::array<double, 4> mMode{1., 0., 0., 0.};
};
