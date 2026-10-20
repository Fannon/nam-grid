#pragma once
#include "SlotTimeEffects.h"

// Original nam-grid DSP. All storage is prepared while processing is stopped.
// These are musical algorithms, not reproductions of commercial products.
namespace grid_roadmap
{
inline double Gain(double db) { return std::pow(10., db * .05); }
inline double Pole(double hz, double rate) { return std::exp(-2. * grid_fx::pi * std::min(hz, .45 * rate) / rate); }
inline double Low(double x, double& state, double coefficient)
{ return state = grid_fx::Flush(x + coefficient * (state - x)); }

class Limiter
{
public:
  void Reset(double rate) { mRate = rate; mGain = 1.; mReduction = 0.; mControls.Reset(rate); }
  void Process(double* l, double* r, int n, const UtilityControls& c)
  {
    mControls.Target(c.limiter, uLimiterCeiling);
    mReduction = 0.;
    for (int i = 0; i < n; ++i)
    {
      const auto& v = mControls.Tick();
      const double ceiling = Gain(v[0]), peak = std::max(std::abs(l[i]), r ? std::abs(r[i]) : 0.);
      const double target = peak > ceiling ? ceiling / peak : 1.;
      const double release = std::exp(-1. / (.001 * v[1] * mRate));
      mGain = std::min(target, 1. + release * (mGain - 1.));
      l[i] *= mGain;
      if (r) r[i] *= mGain;
      mReduction = std::max(mReduction, -20. * std::log10(std::max(1.e-12, mGain)));
    }
  }
  double Reduction() const { return mReduction; }
private:
  grid_fx::SmoothControls<2> mControls;
  double mRate = 48000., mGain = 1., mReduction = 0.;
};

class Rotary
{
public:
  void Reset(double rate)
  {
    mRate = rate; mPhase = mDrumPhase = 0.; mSpeed = .8; mLow = {};
    for (auto& line : mLine) line.Reset(static_cast<int>(rate * .012));
    mControls.Reset(rate);
  }
  void Process(double* l, double* r, int n, const UtilityControls& c)
  {
    mControls.Target(c.rotary, uRotarySlow);
    for (int i = 0; i < n; ++i)
    {
      const auto& v = mControls.Tick();
      const double speed = c.rotary[2] >= .5 ? v[1] : v[0];
      mSpeed = speed + std::exp(-1. / (v[3] * mRate)) * (mSpeed - speed);
      mPhase = std::fmod(mPhase + mSpeed / mRate, 1.);
      mDrumPhase = std::fmod(mDrumPhase + .73 * mSpeed / mRate, 1.);
      const double dry[2]{l[i], r ? r[i] : l[i]}, drive = Gain(v[4]), mix = v[6] * .01;
      double wet[2]{};
      for (int ch = 0; ch < 2; ++ch)
      {
        const double angle = 2. * grid_fx::pi * (mPhase + (ch ? .25 * v[5] * .01 : 0.));
        const double low = Low(dry[ch], mLow[ch], Pole(800., mRate));
        const double high = std::tanh((dry[ch] - low) * drive) / std::max(1., drive * .5);
        const double horn = mLine[ch].Read(mRate * (.004 + .0018 * std::sin(angle)));
        mLine[ch].Push(high);
        wet[ch] = low * (.78 + .22 * std::sin(2. * grid_fx::pi * mDrumPhase + ch * v[5] * .01))
                  + horn * (.72 + .28 * std::cos(angle));
      }
      l[i] = dry[0] + mix * (wet[0] - dry[0]);
      if (r) r[i] = dry[1] + mix * (wet[1] - dry[1]);
    }
  }
private:
  grid_fx::SmoothControls<7> mControls;
  std::array<grid_fx::DelayLine, 2> mLine;
  std::array<double, 2> mLow{};
  double mRate = 48000., mPhase = 0., mDrumPhase = 0., mSpeed = .8;
};

class Vibe
{
public:
  void Reset(double rate) { mRate = rate; mPhase = 0.; mState = {}; mControls.Reset(rate); }
  void Process(double* x, int n, const UtilityControls& c)
  {
    auto values = c.modulation;
    if (c.sync) values[0] = 1. / grid_tempo::Seconds(c.tempo, c.division);
    mControls.Target(values, uModRate, c.sync);
    constexpr double centers[]{180., 430., 1050., 2900.};
    for (int i = 0; i < n; ++i)
    {
      const auto& v = mControls.Tick();
      mPhase = std::fmod(mPhase + v[0] / mRate, 1.);
      // Unequal optical stages and a skewed lamp response produce the deep,
      // asymmetric pulse, distinct from the evenly swept general-purpose phaser.
      const double sine = std::sin(2. * grid_fx::pi * mPhase);
      const double lamp = (sine + .28 * std::sin(4. * grid_fx::pi * mPhase + .6)) * v[1] * .01;
      double wet = x[i];
      for (int j = 0; j < 4; ++j)
      {
        const double f = std::min(.4 * mRate, centers[j] * std::exp2(lamp * (1.2 + v[3] * .012)));
        const double t = std::tan(grid_fx::pi * f / mRate), a = (t - 1.) / (t + 1.);
        const double y = a * wet + mState[j];
        mState[j] = grid_fx::Flush(wet - a * y); wet = y;
      }
      const double voiced = c.vibeMode >= .5 ? wet : .5 * (x[i] + wet);
      x[i] += v[4] * .01 * (voiced - x[i]);
    }
  }
private:
  grid_fx::SmoothControls<5> mControls;
  std::array<double, 4> mState{};
  double mRate = 48000., mPhase = 0.;
};

class Octaver
{
public:
  void Reset(double rate)
  { mRate = rate; mDetect = mEnv = mTone1 = mTone2 = 0.; mPositive = false; mOne = mTwo = 1.; mControls.Reset(rate); }
  void Process(double* l, double* r, int n, const UtilityControls& c)
  {
    mControls.Target(c.octaver, uOctDry);
    for (int i = 0; i < n; ++i)
    {
      const auto& v = mControls.Tick();
      const double dryL = l[i], dryR = r ? r[i] : dryL, mono = .5 * (dryL + dryR);
      const double detect = Low(mono, mDetect, Pole(450., mRate));
      const double peak = std::abs(mono);
      mEnv = peak + std::exp(-1. / ((peak > mEnv ? .002 : .04) * mRate)) * (mEnv - peak);
      const double hysteresis = std::max(1.e-6, mEnv * .03);
      if (!mPositive && detect > hysteresis)
      {
        mPositive = true; mOne = -mOne;
        if (mOne > 0.) mTwo = -mTwo;
      }
      else if (mPositive && detect < -hysteresis) mPositive = false;
      const double amplitude = mEnv > 1.e-5 ? mEnv : 0.;
      const double pole = Pole(v[3], mRate);
      const double sub = (Low(mOne * amplitude, mTone1, pole) * v[1]
                          + Low(mTwo * amplitude, mTone2, pole) * v[2]) * .01;
      const double gain = Gain(v[4]);
      l[i] = (dryL * v[0] * .01 + sub) * gain;
      if (r) r[i] = (dryR * v[0] * .01 + sub) * gain;
    }
  }
private:
  grid_fx::SmoothControls<5> mControls;
  double mRate = 48000., mDetect = 0., mEnv = 0., mTone1 = 0., mTone2 = 0., mOne = 1., mTwo = 1.;
  bool mPositive = false;
};

class Doubler
{
public:
  void Reset(double rate)
  {
    mRate = rate; mPhase = {0., .5}; mControls.Reset(rate);
    for (auto& line : mLine) line.Reset(static_cast<int>(rate * .09));
  }
  void Process(double* l, double* r, int n, const UtilityControls& c)
  {
    mControls.Target(c.doubler, uDoubleDetune);
    for (int i = 0; i < n; ++i)
    {
      const auto& v = mControls.Tick();
      const double dry[]{l[i], r ? r[i] : l[i]}; double wet[2]{};
      for (int ch = 0; ch < 2; ++ch)
      {
        const double ratio = std::exp2((ch ? -v[0] : v[0]) / 1200.);
        mPhase[ch] = std::fmod(mPhase[ch] + std::abs(1. - ratio) / (.04 * mRate), 1.);
        const double p = mPhase[ch], q = std::fmod(p + .5, 1.);
        const double a = .5 - .5 * std::cos(2. * grid_fx::pi * p);
        const auto read = [&](double phase) {
          return mLine[ch].Read(mRate * (v[1] * .001 + .04 * (ch ? phase : 1. - phase)));
        };
        wet[ch] = a * read(p) + (1. - a) * read(q);
        mLine[ch].Push(dry[ch]);
      }
      const double mid = .5 * (wet[0] + wet[1]), side = .5 * (wet[0] - wet[1]) * v[2] * .01;
      const double mix = v[3] * .01, gain = Gain(v[4]);
      l[i] = (dry[0] + mix * (mid + side - dry[0])) * gain;
      if (r) r[i] = (dry[1] + mix * (mid - side - dry[1])) * gain;
    }
  }
private:
  std::array<grid_fx::DelayLine, 2> mLine;
  std::array<double, 2> mPhase{};
  grid_fx::SmoothControls<5> mControls;
  double mRate = 48000.;
};

// Stereo history shared by reverse/hold/stutter. Reads are bounded, and overlap
// windows remove hard capture/repeat seams. A capture is transient audio, not state.
class Capture
{
public:
  void Reset(double rate, double maxSeconds = 8.1)
  {
    mRate = rate; mWrite = mStart = mPosition = mLength = mRefresh = mAvailable = 0; mHeld = false;
    mLow = {}; mWetGain = 0.; mTransition = 0; mLastWet = mTransitionFrom = {};
    for (auto& b : mHistory) b.assign(static_cast<size_t>(rate * maxSeconds) + 4, 0.);
    for (auto& b : mCaptured) b.assign(static_cast<size_t>(rate * maxSeconds) + 4, 0.);
    mFreeze.Reset(rate); mStutter.Reset(rate); mDelay.Reset(rate);
  }
  void Process(double* l, double* r, int n, SlotEffect effect, const UtilityControls& c)
  {
    mFreeze.Target(c.freeze, uFreezeHold);
    auto stutter = c.stutter; auto delay = c.delay;
    if (c.sync)
    {
      stutter[0] = grid_tempo::Seconds(c.tempo, c.division) * 1000.;
      delay[0] = stutter[0];
    }
    mStutter.Target(stutter, uStutterTime, c.sync); mDelay.Target(delay, uDelayTime, c.sync);
    for (int i = 0; i < n; ++i)
    {
      const auto& f = mFreeze.Tick(); const auto& s = mStutter.Tick(); const auto& d = mDelay.Tick();
      const bool freeze = effect == SlotEffect::Freeze, reverse = effect == SlotEffect::ReverseDelay;
      const bool hold = freeze && c.freeze[0] >= .5;
      const size_t length = static_cast<size_t>(std::clamp((freeze ? f[1] : reverse ? d[0] : s[0]) * .001 * mRate,
                                                        16., static_cast<double>(mHistory[0].size() - 2)));
      const double dry[]{l[i], r ? r[i] : l[i]};
      // A restored Hold waits for a fresh window rather than latching silence.
      if (!hold || !mHeld)
      {
        for (int ch = 0; ch < 2; ++ch) mHistory[ch][mWrite] = dry[ch];
        mWrite = (mWrite + 1) % mHistory[0].size();
        mAvailable = std::min(mHistory[0].size(), mAvailable + 1);
      }
      const bool refreshDue = !freeze && (mLength == 0 || ++mRefresh >= length * (reverse ? 1 : 4));
      if (mAvailable >= length && ((hold && !mHeld) || refreshDue))
      {
        // Swap complete banks rather than copying a potentially multi-second
        // window on the audio thread. The captured bank stays immutable.
        for (int ch = 0; ch < 2; ++ch) mHistory[ch].swap(mCaptured[ch]);
        mLength = length; mStart = (mWrite + mHistory[0].size() - mLength) % mHistory[0].size();
        mPosition = mRefresh = mAvailable = 0;
        mTransitionFrom = mLastWet; mTransition = std::max(1, static_cast<int>(.005 * mRate));
        if (hold) mHeld = true;
      }
      if (!hold) mHeld = false;
      const double targetGain = freeze && !mHeld ? 0. : 1.;
      mWetGain += std::clamp(targetGain - mWetGain, -1. / (.005 * mRate), 1. / (.005 * mRate));
      const size_t fade = std::min(std::max(size_t{1}, mLength / 4),
                                  static_cast<size_t>(mRate * .001 * (freeze ? f[2] : reverse ? 5. : s[2])));
      const auto read = [&](int ch, size_t p) {
        p %= std::max(size_t{1}, mLength);
        if (reverse) p = mLength - 1 - p;
        return mCaptured[ch][(mStart + p) % mCaptured[ch].size()];
      };
      double wet[2]{};
      for (int ch = 0; ch < 2; ++ch)
      {
        if (mLength)
        {
          wet[ch] = read(ch, mPosition);
          // Fade to the next loop's beginning before wrapping.
          if (mPosition >= mLength - fade)
          {
            const double a = static_cast<double>(mPosition - (mLength - fade)) / fade;
            wet[ch] = wet[ch] * (1. - a) + read(ch, mPosition - (mLength - fade)) * a;
          }
          if (!freeze && !reverse)
          {
            const double gateEnd = mLength * s[1] * .01;
            const double gate = std::clamp(std::min(static_cast<double>(mPosition) / fade,
                                                   (gateEnd - mPosition) / fade), 0., 1.);
            wet[ch] *= gate;
          }
        }
        wet[ch] = Low(wet[ch], mLow[ch], Pole(freeze ? f[3] : reverse ? d[2] : 18000., mRate));
        if (mTransition > 0)
        {
          const double a = 1. - static_cast<double>(mTransition) / std::max(1, static_cast<int>(.005 * mRate));
          wet[ch] = mTransitionFrom[ch] + a * (wet[ch] - mTransitionFrom[ch]);
        }
        mLastWet[ch] = wet[ch];
      }
      if (mTransition > 0) --mTransition;
      if (mLength && ++mPosition >= mLength) mPosition = std::min(fade, mLength - 1);
      const double mix = (freeze ? f[4] : reverse ? d[4] : s[3]) * .01;
      const double gain = reverse ? Gain(std::clamp(c.delayOutput, -18., 6.)) : 1.;
      l[i] = (freeze ? dry[0] + mix * wet[0] * mWetGain : dry[0] + mix * (wet[0] * mWetGain - dry[0])) * gain;
      if (r) r[i] = (freeze ? dry[1] + mix * wet[1] * mWetGain : dry[1] + mix * (wet[1] * mWetGain - dry[1])) * gain;
    }
  }
private:
  std::array<std::vector<double>, 2> mHistory;
  std::array<std::vector<double>, 2> mCaptured;
  grid_fx::SmoothControls<5> mFreeze, mDelay;
  grid_fx::SmoothControls<4> mStutter;
  std::array<double, 2> mLow{};
  size_t mWrite = 0, mStart = 0, mPosition = 0, mLength = 0, mRefresh = 0, mAvailable = 0;
  double mRate = 48000., mWetGain = 0.;
  bool mHeld = false;
  int mTransition = 0;
  std::array<double, 2> mLastWet{}, mTransitionFrom{};
};

class DualDelay
{
public:
  void Reset(double rate)
  {
    mRate = rate; mLow = {}; mControls.Reset(rate); mExtras.Reset(rate);
    for (auto& line : mLine) line.Reset(static_cast<int>(16.1 * rate));
  }
  void Process(double* l, double* r, int n, const UtilityControls& c)
  {
    auto values = c.delay;
    if (c.sync) values[0] = grid_tempo::Seconds(c.tempo, c.division) * 1000.;
    mControls.Target(values, uDelayTime, c.sync); mExtras.Target(c.dual, uDualRatio);
    for (int i = 0; i < n; ++i)
    {
      const auto& v = mControls.Tick(); const auto& extra = mExtras.Tick();
      const double dry[]{l[i], r ? r[i] : l[i]}; double wet[2]{};
      for (int ch = 0; ch < 2; ++ch)
      {
        wet[ch] = Low(mLine[ch].Read(v[0] * .001 * mRate * (ch ? extra[0] : 1.)), mLow[ch], Pole(v[2], mRate));
        mLine[ch].Push(dry[ch] + v[1] * .01 * wet[ch]);
      }
      const double balance = extra[1] * .01, mix = v[4] * .01, gain = Gain(std::clamp(c.delayOutput, -18., 6.));
      const double a = wet[0] * std::min(1., 2. * (1. - balance)), b = wet[1] * std::min(1., 2. * balance);
      l[i] = (dry[0] + mix * ((r ? a : .5 * (a + b)) - dry[0])) * gain;
      if (r) r[i] = (dry[1] + mix * (b - dry[1])) * gain;
    }
  }
private:
  grid_fx::SmoothControls<5> mControls;
  grid_fx::SmoothControls<2> mExtras;
  std::array<grid_fx::DelayLine, 2> mLine;
  std::array<double, 2> mLow{};
  double mRate = 48000.;
};

class Texture
{
public:
  void Reset(double rate) { mRate = rate; mPhase = mHeld = mLow = 0.; mCrush.Reset(rate); mRing.Reset(rate); }
  void Process(double* x, int n, SlotEffect effect, const UtilityControls& c)
  {
    mCrush.Target(c.crusher, uCrushBits); mRing.Target(c.ring, uRingFrequency);
    for (int i = 0; i < n; ++i)
    {
      const auto& crush = mCrush.Tick(); const auto& ring = mRing.Tick();
      const bool crusher = effect == SlotEffect::Bitcrusher;
      double wet, mix;
      if (crusher)
      {
        mPhase += std::min(crush[1], mRate) / mRate;
        if (mPhase >= 1.)
        {
          mPhase -= 1.; const double levels = std::exp2(std::round(crush[0]) - 1.);
          mHeld = std::round(std::clamp(x[i], -1., 1.) * levels) / levels;
        }
        wet = Low(mHeld, mLow, Pole(crush[2], mRate)); mix = crush[3] * .01;
      }
      else
      {
        mPhase = std::fmod(mPhase + ring[0] / mRate, 1.);
        wet = Low(x[i] * (1. - ring[1] * .01 + ring[1] * .01 * std::sin(2. * grid_fx::pi * mPhase)),
                  mLow, Pole(ring[2], mRate)); mix = ring[3] * .01;
      }
      x[i] = (x[i] + mix * (wet - x[i])) * (crusher ? Gain(crush[4]) : 1.);
    }
  }
private:
  grid_fx::SmoothControls<5> mCrush;
  grid_fx::SmoothControls<4> mRing;
  double mRate = 48000., mPhase = 0., mHeld = 0., mLow = 0.;
};
} // namespace grid_roadmap
