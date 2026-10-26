#pragma once
#include "SlotTimeEffects.h"

// Original NAM Grid DSP. All storage is prepared while processing is stopped.
// These are musical algorithms, not reproductions of commercial products.
namespace grid_roadmap
{
inline double Gain(double db) { return std::pow(10., db * .05); }
inline double Pole(double hz, double rate) { return std::exp(-2. * grid_fx::pi * std::min(hz, .45 * rate) / rate); }
// Identical expressions, cached on the complete pow/exp argument for per-sample use.
inline double Gain(grid_fx::Memo& memo, double db) { return memo(db * .05, [](double x) { return std::pow(10., x); }); }
inline double Pole(grid_fx::Memo& memo, double hz, double rate)
{ return memo(-2. * grid_fx::pi * std::min(hz, .45 * rate) / rate, [](double x) { return std::exp(x); }); }
inline double Low(double x, double& state, double coefficient)
{ return state = grid_fx::Flush(x + coefficient * (state - x)); }

class Limiter
{
public:
  void Reset(double rate) { mRate = rate; mGain = 1.; mReduction = 0.; mControls.Reset(rate); mClip.Reset(rate); mInput.Reset(rate); }
  // Clip then limit: a soft knee from 3 dB below the ceiling, flattening towards 3 dB above it,
  // shaves the fastest peaks so the limiter's gain moves less; the limiter still holds the ceiling.
  static double SoftClip(double x, double ceiling)
  {
    constexpr double knee = .70794578438, top = 1.41253754462; // -3 dB and +3 dB re the ceiling
    const double level = std::abs(x) / ceiling;
    if (level <= knee) return x;
    return std::copysign(ceiling * (knee + (top - knee) * std::tanh((level - knee) / (top - knee))), x);
  }
  void Process(double* l, double* r, int n, const UtilityControls& c)
  {
    mControls.Target(c.limiter, uLimiterCeiling);
    mInput.Target({c.extra.limiterInput}, uLimiterInput);
    mClip.Target({c.limiterClip}, uLimiterClip);
    double minimumGain = 1.;
    for (int i = 0; i < n; ++i)
    {
      const auto& v = mControls.Tick();
      const double input = Gain(mInputGain, mInput.Tick()[0]);
      l[i] *= input; if (r) r[i] *= input;
      const double clip = mClip.Tick()[0] * .01;
      const double ceiling = Gain(mCeiling, v[0]);
      if (clip > 0.) // Exactly the plain limiter at 0%.
      {
        l[i] += clip * (SoftClip(l[i], ceiling) - l[i]);
        if (r) r[i] += clip * (SoftClip(r[i], ceiling) - r[i]);
      }
      const double peak = std::max(std::abs(l[i]), r ? std::abs(r[i]) : 0.);
      const double target = peak > ceiling ? ceiling / peak : 1.;
      const double release = mRelease(-1. / (.001 * v[1] * mRate), [](double x) { return std::exp(x); });
      mGain = std::min(target, 1. + release * (mGain - 1.));
      l[i] *= mGain;
      if (r) r[i] *= mGain;
      minimumGain = std::min(minimumGain, mGain);
    }
    mReduction = -20. * std::log10(std::max(1.e-12, minimumGain));
  }
  double Reduction() const { return mReduction; }
private:
  grid_fx::SmoothControls<2> mControls;
  grid_fx::SmoothControls<1> mClip, mInput;
  grid_fx::Memo mInputGain;
  grid_fx::Memo mCeiling, mRelease;
  double mRate = 48000., mGain = 1., mReduction = 0.;
};

class Rotary
{
public:
  void Reset(double rate)
  {
    mRate = rate; mPhase = mDrumPhase = 0.; mSpeed = .8; mLow = {}; mBalance.Reset(rate);
    for (auto& line : mLine) line.Reset(static_cast<int>(rate * .012));
    mControls.Reset(rate);
  }
  void Process(double* l, double* r, int n, const UtilityControls& c)
  {
    mControls.Target(c.rotary, uRotarySlow);
    mBalance.Target({c.extra.rotaryBalance}, uRotaryBalance);
    const double crossover = Pole(800., mRate);
    for (int i = 0; i < n; ++i)
    {
      const auto& v = mControls.Tick();
      const double balance = mBalance.Tick()[0] * .01;
      const double speed = c.rotary[2] >= .5 ? v[1] : v[0];
      mSpeed = speed + mInertia(-1. / (v[3] * mRate), [](double x) { return std::exp(x); }) * (mSpeed - speed);
      mPhase = std::fmod(mPhase + mSpeed / mRate, 1.);
      mDrumPhase = std::fmod(mDrumPhase + .73 * mSpeed / mRate, 1.);
      const double dry[2]{l[i], r ? r[i] : l[i]}, drive = Gain(mDrive, v[4]), mix = v[6] * .01;
      double wet[2]{};
      for (int ch = 0; ch < 2; ++ch)
      {
        const double angle = 2. * grid_fx::pi * (mPhase + (ch ? .25 * v[5] * .01 : 0.));
        const double low = Low(dry[ch], mLow[ch], crossover);
        const double high = std::tanh((dry[ch] - low) * drive) / std::max(1., drive * .5);
        const double horn = mLine[ch].Read(mRate * (.004 + .0018 * std::sin(angle)));
        mLine[ch].Push(high);
        wet[ch] = (1. - std::max(0., balance)) * low * (.78 + .22 * std::sin(2. * grid_fx::pi * mDrumPhase + ch * v[5] * .01))
                  + (1. + std::min(0., balance)) * horn * (.72 + .28 * std::cos(angle));
      }
      l[i] = dry[0] + mix * (wet[0] - dry[0]);
      if (r) r[i] = dry[1] + mix * (wet[1] - dry[1]);
    }
  }
private:
  grid_fx::SmoothControls<7> mControls;
  grid_fx::SmoothControls<1> mBalance;
  std::array<grid_fx::DelayLine, 2> mLine;
  std::array<double, 2> mLow{};
  grid_fx::Memo mInertia, mDrive;
  double mRate = 48000., mPhase = 0., mDrumPhase = 0., mSpeed = .8;
};

class Vibe
{
public:
  void Reset(double rate) { mRate = rate; mPhase = mMode = 0.; mState = {}; mFirst = true; mControls.Reset(rate); mExtension.Reset(rate); }
  void Process(double* x, int n, const UtilityControls& c, bool right = false)
  {
    mExtension.Target({right ? c.extra.vibeSpread : 0., c.extra.modOutput}, std::array<int, 2>{uVibeSpread, uModOutput});
    auto values = c.modulation;
    if (c.sync) values[0] = 1. / grid_tempo::Seconds(c.tempo, c.division);
    mControls.Target(values, uModRate, c.sync);
    if (mFirst) { mMode = c.vibeMode >= .5 ? 1. : 0.; mFirst = false; }
    constexpr double centers[]{180., 430., 1050., 2900.};
    for (int i = 0; i < n; ++i)
    {
      const auto& v = mControls.Tick();
      mPhase = std::fmod(mPhase + v[0] / mRate, 1.);
      // Unequal optical stages and a skewed lamp response produce the deep,
      // asymmetric pulse, distinct from the evenly swept general-purpose phaser.
      const auto& extra = mExtension.Tick();
      const double phase = mPhase + .005 * extra[0];
      const double sine = std::sin(2. * grid_fx::pi * phase);
      const double lamp = (sine + .28 * std::sin(4. * grid_fx::pi * phase + .6)) * v[1] * .01;
      double wet = x[i];
      for (int j = 0; j < 4; ++j)
      {
        const double f = std::min(.4 * mRate, centers[j] * std::exp2(lamp * (1.2 + v[3] * .012)));
        const double t = std::tan(grid_fx::pi * f / mRate), a = (t - 1.) / (t + 1.);
        const double y = a * wet + mState[j];
        mState[j] = grid_fx::Flush(wet - a * y); wet = y;
      }
      mMode += std::clamp((c.vibeMode >= .5 ? 1. : 0.) - mMode, -1. / (.005 * mRate), 1. / (.005 * mRate));
      const double voiced = .5 * (x[i] + wet) + .5 * mMode * (wet - x[i]);
      x[i] += v[4] * .01 * (voiced - x[i]);
      x[i] *= Gain(mOutput, extra[1]);
    }
  }
private:
  grid_fx::SmoothControls<5> mControls;
  grid_fx::SmoothControls<2> mExtension;
  grid_fx::Memo mOutput;
  std::array<double, 4> mState{};
  double mRate = 48000., mPhase = 0., mMode = 0.;
  bool mFirst = true;
};

class Octaver
{
public:
  void Reset(double rate)
  { mRate = rate; mDetect = mEnv = mTone1 = mTone2 = 0.; mPositive = false; mOne = mTwo = 1.; mControls.Reset(rate); }
  void Process(double* l, double* r, int n, const UtilityControls& c)
  {
    mControls.Target(c.octaver, uOctDry);
    const double detector = Pole(std::clamp(c.extra.octTrackingFilter, 80., 1200.), mRate);
    const double attack = std::exp(-1. / (.002 * mRate)), release = std::exp(-1. / (.04 * mRate));
    for (int i = 0; i < n; ++i)
    {
      const auto& v = mControls.Tick();
      const double dryL = l[i], dryR = r ? r[i] : dryL, mono = .5 * (dryL + dryR);
      const double detect = Low(mono, mDetect, detector);
      const double peak = std::abs(mono);
      mEnv = grid_fx::Flush(peak + (peak > mEnv ? attack : release) * (mEnv - peak));
      const double hysteresis = std::max(1.e-6, mEnv * (c.extra.octSensitivity == 3. ? .03 : std::clamp(c.extra.octSensitivity, .1, 15.) * .01));
      if (!mPositive && detect > hysteresis)
      {
        mPositive = true; mOne = -mOne;
        if (mOne > 0.) mTwo = -mTwo;
      }
      else if (mPositive && detect < -hysteresis) mPositive = false;
      const double amplitude = mEnv > 1.e-5 ? mEnv : 0.;
      const double pole = Pole(mTone, v[3], mRate);
      const double sub = (Low(mOne * amplitude, mTone1, pole) * v[1]
                          + Low(mTwo * amplitude, mTone2, pole) * v[2]) * .01;
      const double gain = Gain(mGain, v[4]);
      l[i] = (dryL * v[0] * .01 + sub) * gain;
      if (r) r[i] = (dryR * v[0] * .01 + sub) * gain;
    }
  }
private:
  grid_fx::SmoothControls<5> mControls;
  grid_fx::Memo mTone, mGain;
  double mRate = 48000., mDetect = 0., mEnv = 0., mTone1 = 0., mTone2 = 0., mOne = 1., mTwo = 1.;
  bool mPositive = false;
};

class Doubler
{
public:
  void Reset(double rate)
  {
    mRate = rate; mPhase = {0., .5}; mControls.Reset(rate); mExtension.Reset(rate); mHumanPhase = 0.; mWetLow = {};
    for (auto& line : mLine) line.Reset(static_cast<int>(rate * .09));
  }
  void Process(double* l, double* r, int n, const UtilityControls& c)
  {
    mControls.Target(c.doubler, uDoubleDetune);
    mExtension.Target({c.extra.doubleHumanize, c.extra.doubleHighCut}, std::array<int, 2>{uDoubleHumanize, uDoubleHighCut});
    for (int i = 0; i < n; ++i)
    {
      const auto& v = mControls.Tick();
      const double dry[]{l[i], r ? r[i] : l[i]}; double wet[2]{};
      const auto& extra = mExtension.Tick();
      mHumanPhase += 1. / mRate;
      if (mHumanPhase >= 1000.) mHumanPhase -= 1000.;
      for (int ch = 0; ch < 2; ++ch)
      {
        const double ratio = mRatio[ch]((ch ? -v[0] : v[0]) / 1200., [](double x) { return std::exp2(x); });
        mPhase[ch] = std::fmod(mPhase[ch] + std::abs(1. - ratio) / (.04 * mRate), 1.);
        const double p = mPhase[ch], q = std::fmod(p + .5, 1.);
        const double a = .5 - .5 * std::cos(2. * grid_fx::pi * p);
        const auto read = [&](double phase) {
          return mLine[ch].Read(mRate * (v[1] * .001 + .04 * (ch ? phase : 1. - phase) + extra[0] * .00003 * (1. + std::sin(2. * grid_fx::pi * mHumanPhase * (ch ? .173 : .137)))));
        };
        wet[ch] = a * read(p) + (1. - a) * read(q);
        mLine[ch].Push(dry[ch]);
        mWetLow[ch] = Low(wet[ch], mWetLow[ch], Pole(mTone, extra[1], mRate));
        if (extra[1] < 20000.) wet[ch] = mWetLow[ch];
      }
      const double mid = .5 * (wet[0] + wet[1]), side = .5 * (wet[0] - wet[1]) * v[2] * .01;
      const double mix = v[3] * .01, gain = Gain(mGain, v[4]);
      l[i] = (dry[0] + mix * (mid + side - dry[0])) * gain;
      if (r) r[i] = (dry[1] + mix * (mid - side - dry[1])) * gain;
    }
  }
private:
  std::array<grid_fx::DelayLine, 2> mLine;
  grid_fx::SmoothControls<2> mExtension;
  grid_fx::Memo mTone;
  double mHumanPhase = 0.;
  std::array<double, 2> mWetLow{};
  std::array<double, 2> mPhase{};
  grid_fx::SmoothControls<5> mControls;
  std::array<grid_fx::Memo, 2> mRatio;
  grid_fx::Memo mGain;
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
    mFreeze.Reset(rate); mStutter.Reset(rate); mDelay.Reset(rate); mOutput.Reset(rate); mCaptureExtras.Reset(rate);
  }
  void Process(double* l, double* r, int n, SlotEffect effect, const UtilityControls& c, bool draining = false)
  {
    mFreeze.Target(c.freeze, uFreezeHold);
    mCaptureExtras.Target({c.extra.reverseFeedback, c.extra.reverseEnvelope, c.extra.freezeAttack, c.extra.freezeRelease},
      std::array<int, 4>{uReverseFeedback, uReverseEnvelope, uFreezeAttack, uFreezeRelease});
    auto stutter = c.stutter; auto delay = c.delay;
    if (c.sync)
    {
      stutter[0] = grid_tempo::Seconds(c.tempo, c.division) * 1000.;
      delay[0] = stutter[0];
    }
    mStutter.Target(stutter, uStutterTime, c.sync); mDelay.Target(delay, uDelayTime, c.sync);
    mOutput.Target({c.delayOutput}, uDelayOutput);
    for (int i = 0; i < n; ++i)
    {
      const auto& extra = mCaptureExtras.Tick();
      const auto& f = mFreeze.Tick(); const auto& s = mStutter.Tick(); const auto& d = mDelay.Tick();
      const bool freeze = effect == SlotEffect::Freeze, reverse = effect == SlotEffect::ReverseDelay;
      const bool hold = freeze && c.freeze[0] >= .5;
      const size_t length = static_cast<size_t>(std::clamp((freeze ? f[1] : reverse ? d[0] : s[0]) * .001 * mRate,
                                                        16., static_cast<double>(mHistory[0].size() - 2)));
      const double dry[]{l[i], r ? r[i] : l[i]};
      // A restored Hold waits for a fresh window rather than latching silence.
      if (!hold || !mHeld)
      {
        for (int ch = 0; ch < 2; ++ch) mHistory[ch][mWrite] = dry[ch] + (reverse && !draining ? extra[0] * .01 * mLastWet[ch] : 0.);
        if (++mWrite == mHistory[0].size()) mWrite = 0; // Avoid per-sample integer division.
        mAvailable = std::min(mHistory[0].size(), mAvailable + 1);
      }
      const bool refreshDue = !freeze && (mLength == 0 || (!(effect == SlotEffect::Stutter && c.extra.stutterHold >= .5) && ++mRefresh >= length * (reverse ? 1 : 4)));
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
      mWetGain += std::clamp(targetGain - mWetGain, -1. / ((freeze ? extra[3] * .001 : .005) * mRate), 1. / ((freeze ? extra[2] * .001 : .005) * mRate));
      const size_t fade = std::min(std::max(size_t{1}, mLength / 4),
                                  static_cast<size_t>(mRate * .001 * (freeze ? f[2] : reverse ? extra[1] : s[2])));
      const auto read = [&](int ch, size_t p) {
        // Same indices as modulo: callers pass p < mLength, and mStart, p < size.
        if (const size_t period = std::max(size_t{1}, mLength); p >= period) p %= period;
        if (reverse) p = mLength - 1 - p;
        const size_t index = mStart + p, size = mCaptured[ch].size();
        return mCaptured[ch][index >= size ? index - size : index];
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
        wet[ch] = Low(wet[ch], mLow[ch], Pole(mTone, freeze ? f[3] : reverse ? d[2] : 18000., mRate));
        if (mTransition > 0)
        {
          const double a = 1. - static_cast<double>(mTransition) / std::max(1, static_cast<int>(.005 * mRate));
          wet[ch] = mTransitionFrom[ch] + a * (wet[ch] - mTransitionFrom[ch]);
        }
        mLastWet[ch] = wet[ch];
      }
      if (mTransition > 0) --mTransition;
      // Freeze overlaps source-window ends, so its repeat period is length-fade.
      // Stutter's gate fades through zero; Reverse changes banks with mTransition.
      // Their musical window period stays length rather than skipping the overlap.
      if (mLength && ++mPosition >= mLength)
        mPosition = freeze ? std::min(fade, mLength - 1) : 0;
      const double mix = (freeze ? f[4] : reverse ? d[4] : s[3]) * .01;
      const double output = mOutput.Tick()[0];
      const double gain = reverse ? Gain(mGain, output) : 1.;
      l[i] = (freeze ? dry[0] + mix * wet[0] * mWetGain : dry[0] + mix * (wet[0] * mWetGain - dry[0])) * gain;
      if (r) r[i] = (freeze ? dry[1] + mix * wet[1] * mWetGain : dry[1] + mix * (wet[1] * mWetGain - dry[1])) * gain;
    }
  }
private:
  std::array<std::vector<double>, 2> mHistory;
  std::array<std::vector<double>, 2> mCaptured;
  grid_fx::SmoothControls<5> mFreeze, mDelay;
  grid_fx::SmoothControls<4> mStutter, mCaptureExtras;
  grid_fx::SmoothControls<1> mOutput;
  grid_fx::Memo mTone, mGain;
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
    mRate = rate; mLow = {}; mControls.Reset(rate); mExtras.Reset(rate); mPing.Reset(rate); mOutput.Reset(rate);
    for (auto& line : mLine) line.Reset(static_cast<int>(16.1 * rate));
  }
  void Process(double* l, double* r, int n, const UtilityControls& c, bool draining = false)
  {
    auto values = c.delay;
    if (c.sync) values[0] = grid_tempo::Seconds(c.tempo, c.division) * 1000.;
    mControls.Target(values, uDelayTime, c.sync); mExtras.Target(c.dual, uDualRatio);
    mPing.Target({c.pingPong ? 1. : 0.}, uDelayPingPong);
    mOutput.Target({c.delayOutput}, uDelayOutput);
    for (int i = 0; i < n; ++i)
    {
      const auto& v = mControls.Tick(); const auto& extra = mExtras.Tick();
      const double ping = mPing.Tick()[0], output = mOutput.Tick()[0];
      const double dry[]{l[i], r ? r[i] : l[i]}; double wet[2]{};
      for (int ch = 0; ch < 2; ++ch)
      {
        wet[ch] = Low(mLine[ch].Read(v[0] * .001 * mRate * (ch ? extra[0] : 1.)), mLow[ch], Pole(mTone, v[2], mRate));
      }
      // Read both taps before either write so cross-feedback is sample accurate.
      const double send[]{(1. - ping) * dry[0] + ping * .5 * (dry[0] + dry[1]), (1. - ping) * dry[1]};
      for (int ch = 0; ch < 2; ++ch)
        mLine[ch].Push(send[ch] + (draining ? 0. : v[1] * .01) * ((1. - ping) * wet[ch] + ping * wet[1 - ch]));
      const double balance = extra[1] * .01, mix = v[4] * .01, gain = Gain(mGain, output);
      const double a = wet[0] * std::min(1., 2. * (1. - balance)), b = wet[1] * std::min(1., 2. * balance);
      l[i] = (dry[0] + mix * ((r ? a : .5 * (a + b)) - dry[0])) * gain;
      if (r) r[i] = (dry[1] + mix * (b - dry[1])) * gain;
    }
  }
private:
  grid_fx::SmoothControls<5> mControls;
  grid_fx::SmoothControls<2> mExtras;
  grid_fx::SmoothControls<1> mPing, mOutput;
  std::array<grid_fx::DelayLine, 2> mLine;
  std::array<double, 2> mLow{};
  grid_fx::Memo mTone, mGain;
  double mRate = 48000.;
};

class Texture
{
public:
  void Reset(double rate) { mRate = rate; mPhase = mHeld = mLow = 0.; mRingSpread.Reset(rate); mCarrierWeights = {1., 0., 0.}; mFirstCarrier = true; mCrush.Reset(rate); mRing.Reset(rate); }
  void Process(double* x, int n, SlotEffect effect, const UtilityControls& c, bool right = false)
  {
    mRingSpread.Target({right ? c.extra.ringSpread : 0.}, uRingSpread);
    const int shape = std::clamp(static_cast<int>(c.extra.ringShape), 0, 2);
    if (mFirstCarrier) { mCarrierWeights = {}; mCarrierWeights[shape] = 1.; mFirstCarrier = false; }
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
        wet = Low(mHeld, mLow, Pole(mTone, crush[2], mRate)); mix = crush[3] * .01;
      }
      else
      {
        mPhase = std::fmod(mPhase + ring[0] / mRate, 1.);
        const double angle = 2. * grid_fx::pi * (mPhase + .005 * mRingSpread.Tick()[0]);
        const double sine = std::sin(angle);
        const std::array<double, 3> carrier{sine, 2. / grid_fx::pi * std::asin(sine), std::tanh(4. * sine) / std::tanh(4.)};
        double wave = 0.;
        for (int j = 0; j < 3; ++j)
        {
          mCarrierWeights[j] += std::clamp((shape == j ? 1. : 0.) - mCarrierWeights[j], -1. / (.005 * mRate), 1. / (.005 * mRate));
          wave += mCarrierWeights[j] * carrier[j];
        }
        wet = Low(x[i] * (1. - ring[1] * .01 + ring[1] * .01 * wave),
                  mLow, Pole(mTone, ring[2], mRate)); mix = ring[3] * .01;
      }
      x[i] = (x[i] + mix * (wet - x[i])) * (crusher ? Gain(mGain, crush[4]) : 1.);
    }
  }
private:
  grid_fx::SmoothControls<5> mCrush;
  grid_fx::SmoothControls<4> mRing;
  grid_fx::SmoothControls<1> mRingSpread;
  std::array<double, 3> mCarrierWeights{1., 0., 0.};
  bool mFirstCarrier = true;
  grid_fx::Memo mTone, mGain;
  double mRate = 48000., mPhase = 0., mHeld = 0., mLow = 0.;
};
} // namespace grid_roadmap
