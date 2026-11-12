#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// The optional final gate of Smart Gate and Noise Gate (AI). It detects the cleaned
// signal after the module's Mix, so leftover raw noise cannot hold it open: a 5 ms peak
// detector, 3 dB hysteresis, a 20 ms hold, 0.5 ms opening and Gate release closing.
// The threshold's minimum (-100 dB) is Off; an enabled gate starts closed after a reset.
namespace final_gate
{
struct Settings
{
  bool enabled = false;
  double level = 0., detectorRelease = 0., openStep = 1., closeStep = 1.;
  uint64_t holdSamples = 0;
};
// `allowed` disables the gate while its module cannot judge noise, e.g. during Learn.
inline Settings Prepare(double thresholdDB, double releaseMs, double rate, bool allowed = true)
{
  const double threshold = std::clamp(thresholdDB, -100., 0.);
  return {allowed && threshold > -100., std::pow(10., threshold * .05), std::exp(-1. / (.005 * rate)),
          1. / (.0005 * rate), 1. / (.001 * std::clamp(releaseMs, 1., 500.) * rate),
          static_cast<uint64_t>(.020 * rate)};
}

class Gate
{
public:
  void Reset()
  {
    mEnvelope = 0.;
    mGain = 1.;
    mHold = 0;
    mOpen = false;
    mFirst = true;
  }
  // Advances one sample from the larger |L|, |R| peak of the signal the returned gain
  // then scales. Detection precedes the gate's own gain, so a closed gate can reopen.
  double Tick(double peak, const Settings& s)
  {
    if (mFirst)
    {
      // Avoids a noise burst on recall/reset when the gate is enabled.
      mGain = s.enabled ? 0. : 1.;
      mFirst = false;
    }
    mEnvelope = std::max(peak, Flush(mEnvelope * s.detectorRelease));
    if (!s.enabled || mEnvelope >= s.level)
    {
      mOpen = true;
      mHold = s.holdSamples;
    }
    else if (mOpen)
    {
      if (mEnvelope >= s.level * .7079457843841379) // 3 dB hysteresis.
        mHold = s.holdSamples;
      else if (mHold)
        --mHold;
      else
        mOpen = false;
    }
    const double target = mOpen ? 1. : 0.;
    mGain += std::clamp(target - mGain, -s.closeStep, s.openStep);
    return mGain;
  }

private:
  static double Flush(double x) { return std::abs(x) < 1.e-30 ? 0. : x; }
  double mEnvelope = 0., mGain = 1.;
  uint64_t mHold = 0;
  bool mOpen = false, mFirst = true;
};
} // namespace final_gate
