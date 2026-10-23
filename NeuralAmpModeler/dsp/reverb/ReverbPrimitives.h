#pragma once
#include "../../SlotTimeEffects.h"
#include <cassert>

namespace grid_reverb
{
using Stereo = std::array<double, 2>;
using Field = std::array<double, 8>;

// Two fixed read positions, coalesced over a 40 ms crossfade. Size automation
// changes reflection spacing without sweeping a read head through old audio.
// Linear interpolation/crossfading avoids gain overshoot inside feedback loops.
class MovingDelay
{
public:
  void Reset(double rate, double maximumSeconds)
  {
    mLine.Reset(static_cast<int>(std::ceil(rate * maximumSeconds)) + 8);
    mStep = 1. / std::max(1., .04 * rate); mFade = 1.; mFirst = true; mClipped = false;
  }
  double Read(double samples)
  {
    const double maximum = mLine.MaxDelay();
    mClipped |= samples > maximum || samples < 1.;
    samples = samples < 1. ? 1. : maximum < samples ? maximum : samples; // std::clamp, without its reference cost
    if (mFirst) { mFrom = mTo = samples; mFirst = false; }
    if (mFade >= 1. && std::abs(samples - mTo) >= .25)
    { mFrom = mTo; mTo = samples; mFade = 0.; }
    // Both positions were clamped above; skip DelayLine's identical second clamp.
    if (mFade >= 1.) return mLine.ReadInRange(mTo);
    const double from = mLine.ReadInRange(mFrom), to = mLine.ReadInRange(mTo);
    const double value = from + mFade * (to - from);
    mFade = std::min(1., mFade + mStep);
    return value;
  }
  void Push(double value) { mLine.Push(value); }
  bool Clipped() const { return mClipped; }
private:
  grid_fx::DelayLine mLine;
  double mFrom = 1., mTo = 1., mFade = 1., mStep = 1.;
  bool mFirst = true, mClipped = false;
};
class Allpass
{
public:
  void Reset(double rate, double maximumSeconds) { mDelay.Reset(rate, maximumSeconds); }
  double Tick(double input, double delaySamples, double gain)
  {
    const double out = mDelay.Read(delaySamples) - gain * input;
    mDelay.Push(input + gain * out); return out;
  }
  bool Clipped() const { return mDelay.Clipped(); }
private:
  MovingDelay mDelay;
};

// Orthogonal two-channel allpass, following the stereo diffuser construction
// used by Julian Parker's MIT JPverb/Greyhole algorithms. It includes rotation
// in the delayed path instead of treating left/right as unrelated mono tanks.
class StereoAllpass
{
public:
  void Reset(double rate, double secondsL, double secondsR, double angle = .7853981633974483)
  {
    mSeconds = {secondsL, secondsR}; mRate = rate;
    mCos = std::cos(angle); mSin = std::sin(angle);
    for (int ch = 0; ch < 2; ++ch) mDelay[ch].Reset(rate, mSeconds[ch] * 2. + .005);
  }
  Stereo Tick(Stereo input, double size, double gain)
  {
    const double a = mDelay[0].Read(std::max(1., mRate * mSeconds[0] * size));
    const double b = mDelay[1].Read(std::max(1., mRate * mSeconds[1] * size));
    Stereo rotated{mCos * a - mSin * b, mSin * a + mCos * b};
    for (int ch = 0; ch < 2; ++ch)
    {
      const double out = rotated[ch] - gain * input[ch];
      mDelay[ch].Push(input[ch] + gain * out); input[ch] = out;
    }
    return input;
  }
  bool Clipped() const { return mDelay[0].Clipped() || mDelay[1].Clipped(); }
  double TotalSeconds() const { return mSeconds[0] + mSeconds[1]; }
private:
  std::array<MovingDelay, 2> mDelay;
  Stereo mSeconds{};
  double mRate = 48000., mCos = 0., mSin = 0.;
};
inline void Hadamard(Field& x)
{
  for (int span = 1; span < 8; span *= 2)
    for (int base = 0; base < 8; base += span * 2)
      for (int j = 0; j < span; ++j)
      {
        const double a = x[base + j], b = x[base + span + j];
        x[base + j] = a + b; x[base + span + j] = a - b;
      }
  for (auto& value : x) value *= .3535533905932738;
}
inline double LowPass(double input, double& state, double coefficient)
{ return state = grid_fx::Flush(input + coefficient * (state - input)); }
inline double Coefficient(double cutoff, double rate)
{ return std::exp(-2. * grid_fx::pi * std::clamp(cutoff, 1., .4 * rate) / rate); }
} // namespace grid_reverb
