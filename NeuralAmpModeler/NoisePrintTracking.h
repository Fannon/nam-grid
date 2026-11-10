#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace noise_print
{
// Original MIT NAM Grid control layer. Follow only the level of the committed
// print; input evidence never changes the print itself. Uncertain/tonal input
// freezes upward adaptation. This is not a guitar classifier.
class Tracking
{
public:
  static constexpr int bands = 96;
  void Reset(double rate)
  {
    mRate = std::max(1., rate);
    mHop = std::max(1, static_cast<int>(std::lround(mRate / 250.)));
    const double seconds = mHop / mRate;
    mSmooth = std::exp(-seconds / .04);
    mSlow = std::exp(-seconds / .15);
    mRise = 1. - std::exp(-1. / (1.0 * mRate));
    mFall = 1. - std::exp(-1. / (.25 * mRate));
    mOff = 1. - std::exp(-1. / (.02 * mRate));
    mSlopeEvery = std::max(1, static_cast<int>(std::lround(.05 / seconds)));
    mHoldTicks = std::max(1, static_cast<int>(std::lround(.25 / seconds)));
    mStableTicks = std::max(1, static_cast<int>(std::lround(.25 / seconds)));
    Clear();
  }
  void Clear()
  {
    mRatio = {};
    mPrevious = {};
    mScale = mTarget = mSlowLevel = 1.;
    mCounter = mStable = mHold = mSlopeCount = 0;
    mEnabled = mReady = mPreviousReady = false;
  }
  void Reference(const std::array<double, bands>& power, bool valid)
  {
    mInverse = {};
    mUse = {};
    mUsable = 0;
    double total = 0., peak = 0.;
    for (double p : power) { total += p; peak = std::max(peak, p); }
    // A nearly pure tone is ambiguous with a sustained note at the same pitch.
    // Keep the fixed print rather than learn an upward level from that input.
    mBroadReference = valid && total > 1.e-18 && peak < total * .12;
    if (mBroadReference)
      for (int k = 0; k < bands; ++k)
      {
        const double hz = 5. * std::exp(std::log(4000.) * k / (bands - 1));
        if (hz >= 80. && hz <= std::min(8000., .4 * mRate) && power[k] > total * 1.e-5)
        {
          mUse[k] = true;
          mInverse[k] = 1. / std::max(power[k], 1.e-30);
          ++mUsable;
        }
      }
    Clear();
  }
  double Tick(const std::array<double, bands>& power, bool enabled)
  {
    if (!enabled || !mBroadReference || mUsable < 12)
    {
      if (mEnabled)
      {
        mCounter = mStable = mHold = mSlopeCount = 0;
        mReady = mPreviousReady = false;
      }
      mEnabled = false;
      mTarget = 1.;
      mScale += mOff * (1. - mScale);
      if (std::abs(mScale - 1.) < 1.e-7) mScale = 1.;
      return mScale;
    }
    if (!mEnabled)
    {
      mCounter = mStable = mHold = mSlopeCount = 0;
      mReady = mPreviousReady = false;
      mEnabled = true;
    }
    if (++mCounter >= mHop)
    {
      mCounter = 0;
      Observe(power);
    }
    const double step = mTarget > mScale ? mRise : mFall;
    mScale += step * (mTarget - mScale);
    if (std::abs(mScale - 1.) < 1.e-7 && mTarget == 1.) mScale = 1.;
    return mScale;
  }
  double Scale() const { return mScale; }
  bool Active() const { return mEnabled; }

private:
  void Observe(const std::array<double, bands>& power)
  {
    std::array<double, bands> sorted{};
    int count = 0;
    for (int k = 0; k < bands; ++k)
      if (mUse[k])
      {
        const double ratio = std::clamp(power[k] * mInverse[k], 0., 1.e12);
        mRatio[k] = mReady ? ratio + mSmooth * (mRatio[k] - ratio) : ratio;
        sorted[count++] = mRatio[k];
      }
    std::sort(sorted.begin(), sorted.begin() + count);
    const double low = std::max(1.e-12, sorted[count / 4]);
    const double middle = std::max(1.e-12, sorted[count / 2]);
    const double high = sorted[9 * count / 10];
    const double peak = sorted[count - 1];
    if (!mReady) mSlowLevel = middle;
    // Sudden changes get time to reveal their spectrum before permitting a rise.
    if (middle > mSlowLevel * 1.5) mHold = mHoldTicks;
    mSlowLevel = middle + mSlow * (mSlowLevel - middle);
    bool decay = false;
    if (++mSlopeCount >= mSlopeEvery)
    {
      mSlopeCount = 0;
      int falling = 0;
      for (int k = 0; k < bands; ++k)
        if (mUse[k])
        {
          if (mPreviousReady && mRatio[k] < mPrevious[k] * .98) ++falling;
          mPrevious[k] = mRatio[k];
        }
      decay = mPreviousReady && falling * 10 >= count * 7;
      mPreviousReady = true;
      if (decay) mHold = mHoldTicks;
    }
    const bool shape = high < low * 2.5 && peak < low * 4.;
    if (mHold > 0) --mHold;
    const bool confident = shape && !decay && mHold == 0;
    mStable = confident ? std::min(mStable + 1, mStableTicks) : 0;
    const double candidate = std::clamp(low, 1., 4.); // At most +6dB; never below the committed print.
    if (mStable >= mStableTicks && candidate > 1.25)
      mTarget = candidate;
    else
      mTarget = std::min(mScale, candidate); // Freeze pending rises; a fall towards the print is safe.
    mReady = true;
  }
  std::array<double, bands> mInverse{}, mRatio{}, mPrevious{};
  std::array<bool, bands> mUse{};
  double mRate = 48000., mScale = 1., mTarget = 1., mSlowLevel = 1.;
  double mSmooth = 0., mSlow = 0., mRise = 0., mFall = 0., mOff = 0.;
  int mHop = 192, mCounter = 0, mStable = 0, mHold = 0, mSlopeEvery = 12, mSlopeCount = 0;
  int mHoldTicks = 63, mStableTicks = 63, mUsable = 0;
  bool mEnabled = false, mReady = false, mPreviousReady = false, mBroadReference = false;
};
} // namespace noise_print
