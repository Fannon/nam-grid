#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>
#include "EffectParameters.h"
#include "ExactMemo.h"
#include "dsp/OversampledMix.h"
#include "TempoSync.h"

// Original MIT implementations for nam-grid. No third-party plugin SDK or DSP
// runtime. Fractional delay memory is sized exclusively in Reset, off audio.
namespace grid_fx
{
constexpr double pi = 3.14159265358979323846;
inline double Flush(double x)
{ return std::abs(x) < 1.e-30 ? 0. : x; }
class DelayLine
{
public:
  void Reset(int capacity)
  {
    mData.assign(std::max(4, capacity + 3), 0.);
    mIndex = 0;
    mMaxDelay = static_cast<double>(mData.size() - 2);
  }
  // Silence the line without reallocating (realtime-safe).
  void Clear() { std::fill(mData.begin(), mData.end(), 0.); mIndex = 0; }
  // Same result as std::clamp (NaN included). MSVC keeps value ternaries in
  // registers; reference-returning std::clamp costs measurably in reverb loops.
  double Read(double delay) const { return ReadInRange(delay < 1. ? 1. : mMaxDelay < delay ? mMaxDelay : delay); }
  // Precondition: 1 <= delay <= MaxDelay(), e.g. already clamped by the caller.
  double ReadInRange(double delay) const
  {
    // Split the integer offset before wrapping: adding the ring size to a tiny
    // negative fractional position can round up to size(), an out-of-bounds read.
    // Signed conversions are single x64 instructions (unsigned ones are not on MSVC);
    // the clamped value is positive and exactly representable either way.
    const auto integer = static_cast<int64_t>(delay);
    const auto whole = static_cast<size_t>(integer);
    const double fraction = delay - static_cast<double>(integer);
    const auto a = mIndex >= whole ? mIndex - whole : mData.size() - (whole - mIndex);
    const auto b = a == 0 ? mData.size() - 1 : a - 1;
    return mData[a] + fraction * (mData[b] - mData[a]);
  }
  void Push(double x)
  {
    mData[mIndex] = Flush(x);
    if (++mIndex == mData.size())
      mIndex = 0;
  }
  double MaxDelay() const { return mMaxDelay; }

private:
  std::vector<double> mData;
  size_t mIndex = 0;
  double mMaxDelay = 0.; // Cached: size_t -> double is not a single x64 conversion.
};
template <size_t N>
class SmoothControls
{
public:
  void Reset(double rate)
  {
    mCoefficient = std::exp(-1. / (.02 * rate));
    mFirst = true;
    mMoving = true;
  }
  void Target(const std::array<double, N>& values, int offset, bool synced = false)
  {
    for (size_t j = 0; j < N; ++j)
    {
      const double target = synced && j == 0 ? values[j]
        : std::clamp(values[j], kEffectParameters[offset + j].minimum, kEffectParameters[offset + j].maximum);
      if constexpr (N > 3) mMoving |= target != mTarget[j];
      mTarget[j] = target;
    }
    if (mFirst)
    {
      mValue = mTarget;
      mFirst = false;
    }
  }
  void Target(const std::array<double, N>& values, const std::array<int, N>& ids)
  {
    for (size_t j = 0; j < N; ++j)
    {
      const double target = std::clamp(values[j], kEffectParameters[ids[j]].minimum, kEffectParameters[ids[j]].maximum);
      if constexpr (N > 3) mMoving |= target != mTarget[j];
      mTarget[j] = target;
    }
    if (mFirst) { mValue = mTarget; mFirst = false; }
  }
  const std::array<double, N>& Tick()
  {
    // Small banks use the short vectorizable recurrence directly. Larger
    // banks amortize the fixed-point check across their additional controls.
    if constexpr (N > 3) if (!mMoving) return mValue;
    bool moving = false;
    for (size_t j = 0; j < N; ++j)
    {
      const double next = mTarget[j] + mCoefficient * (mValue[j] - mTarget[j]);
      if constexpr (N > 3) moving |= next != mValue[j];
      mValue[j] = next;
    }
    if constexpr (N > 3) mMoving = moving;
    return mValue;
  }
  double Value(size_t j) const { return mValue[j]; }
  // Moves one smoothed value without gliding, e.g. to a tap that is crossfaded in.
  void Snap(size_t j, double value)
  {
    mValue[j] = value;
    mMoving = true;
  }

private:
  std::array<double, N> mValue{}, mTarget{};
  double mCoefficient = 0.;
  bool mFirst = true;
  bool mMoving = true;
};
class MixLaw
{
public:
  void Reset(double rate) { mStep = 1. / std::max(1., .02 * rate); mValue = 0.; mFirst = true; }
  void Tick(bool constantDry)
  {
    const double target = constantDry ? 1. : 0.;
    if (mFirst) { mValue = target; mFirst = false; }
    else if (mValue < target) mValue = std::min(target, mValue + mStep);
    else if (mValue > target) mValue = std::max(target, mValue - mStep);
  }
  double Apply(double linear, double dry, double mix, double output = 1.) const
  {
    // Preserve the released expression and its rounding exactly in Linear mode.
    return mValue == 0. || mix == 0. ? linear : linear + (mValue * mix * dry) * output;
  }
private:
  double mValue = 0., mStep = 1.;
  bool mFirst = true;
};

class WetDucker
{
public:
  void Reset(double rate) { mRate = rate; mEnv = 0.; mAttack = std::exp(-1. / (.003 * rate)); mRelease = std::exp(-1. / (.15 * rate)); }
  double Tick(double left, double right, double amount)
  {
    const double peak = std::max(std::abs(left), std::abs(right));
    mEnv = Flush(peak + (peak > mEnv ? mAttack : mRelease) * (mEnv - peak));
    return 1. / (1. + amount * .12 * mEnv);
  }
private:
  double mRate = 48000., mEnv = 0., mAttack = 0., mRelease = 0.;
};
// Feedback scale that falls to zero over 5 ms once a bypassed slot starts draining and
// returns over 5 ms. Cutting circulating repeats at once writes a step into the delay
// line, which re-enabling within one repeat would play back as a click.
class DrainRamp
{
public:
  void Reset(double rate) { mStep = 1. / std::max(1., .005 * rate); mValue = 1.; }
  double Tick(bool draining)
  { return mValue = draining ? std::max(0., mValue - mStep) : std::min(1., mValue + mStep); }
private:
  double mStep = 1. / 240., mValue = 1.;
};
} // namespace grid_fx

class SlotDelay
{
public:
  void Reset(double rate)
  {
    mMixLaw.Reset(rate);
    mRate = rate;
    mLine.Reset(static_cast<int>(rate * 8.1));
    mControls.Reset(rate);
    mExtras.Reset(rate);
    mPing.Reset(rate);
    mLP = mLP2 = mDC = mEnv = mPhase = 0.; mDuckRelease.Reset(rate); mDrain.Reset(rate);
    mCounter = 0;
    mFade = 1.;
    mFadeStep = 1. / std::max(1., kFadeSeconds * rate);
    mJump = mHasTime = false;
  }
  void Process(double* samples, int frames, SlotEffect effect, const UtilityControls& controls,
               SlotDelay* right = nullptr, double* rightSamples = nullptr, bool draining = false)
  {
    mDuckRelease.Target({controls.extra.duckRelease}, uDuckRelease);
    auto values = controls.delay;
    if (controls.sync)
      values[0] = grid_tempo::Seconds(controls.tempo, controls.division) * 1000.;
    mControls.Target(values, uDelayTime, controls.sync);
    // Crossfade (Digital Delay): a Time step above kJumpMs in one control update, e.g. a host
    // jump, a division change or a knob drag, reads the new tap at once and fades to it instead
    // of gliding (Doppler). Smaller steps, such as modulation quanta and gradual tempo changes,
    // still glide. A step during a fade holds the new tap and fades again once this one ends.
    const auto& range = kEffectParameters[uDelayTime];
    const double time = controls.sync ? values[0] : std::clamp(values[0], range.minimum, range.maximum);
    const bool crossfade = effect == SlotEffect::DigitalDelay && controls.extra.delayTimeChange >= .5;
    if (crossfade && !mJump && mHasTime && std::abs(time - mTime) > kJumpMs)
    {
      mJump = true;
      mHold = mControls.Value(0);
    }
    mJump = mJump && crossfade;
    mTime = time;
    mHasTime = true;
    mExtras.Target({controls.delayMotion, controls.delayOutput}, uDelayMotion);
    mPing.Target({right && controls.pingPong ? 1. : 0.}, uDelayPingPong);
    const double dcCoefficient = std::exp(-2. * grid_fx::pi * 70. / mRate);
    const double envelopeAttack = std::exp(-1. / (.003 * mRate));
    for (int i = 0; i < frames; ++i)
    {
      mMixLaw.Tick(controls.constantDry);
      if (mJump && mFade >= 1.) [[unlikely]]
      {
        // Before Tick: the outgoing tap keeps the position it had, without a step.
        mFrom = mControls.Value(0);
        mControls.Snap(0, mTime);
        mFade = 0.;
        mJump = false;
      }
      const auto& c = mControls.Tick();
      double fade = 1.;
      if (mFade < 1.) [[unlikely]]
      {
        if (mJump) mControls.Snap(0, mHold);
        mFade = std::min(1., mFade + mFadeStep);
        fade = mFade * mFade * (3. - 2. * mFade); // Equal-gain smoothstep, flat at both ends.
      }
      const auto& extra = mExtras.Tick();
      const double ping = mPing.Tick()[0], live = mDrain.Tick(draining);
      const double envelopeRelease = mDuckPole(-1. / (.001 * mDuckRelease.Tick()[0] * mRate), [](double x) { return std::exp(x); });
      const double character = c[3] * .01, mix = c[4] * .01;
      if (mCounter++ % 32 == 0)
      {
        const double cutoff = effect == SlotEffect::AnalogDelay ? c[2] * (1. - .6 * character) : c[2];
        mDamping = std::exp(-2. * grid_fx::pi * std::min(cutoff, .4 * mRate) / mRate);
      }
      mCounter %= 32;
      mPhase += .67 / mRate;
      if (mPhase >= 1.)
        mPhase -= 1.;
      const double flutter =
        effect == SlotEffect::DigitalDelay
          ? 0.
          : extra[0] * .01 * mRate * (effect == SlotEffect::TapeDelay ? .0015 : .0008)
              * (std::sin(2. * grid_fx::pi * mPhase)
                 + (effect == SlotEffect::TapeDelay ? .15 * std::sin(2. * grid_fx::pi * mPhase * 7.) : 0.));
      const double x[2]{samples[i], right ? rightSamples[i] : 0.};
      double wet[2]{};
      SlotDelay* channels[2]{this, right};
      const int count = right ? 2 : 1;
      // Read both tanks before either write: cross-feedback has no block delay.
      for (int ch = 0; ch < count; ++ch)
      {
        auto& tank = *channels[ch];
        double read = tank.mLine.Read(c[0] * .001 * mRate + flutter);
        // The feedback path takes the same blend, so repeats switch without a chirp too.
        if (fade < 1.) [[unlikely]]
        {
          const double old = tank.mLine.Read(mFrom * .001 * mRate + flutter);
          read = old + fade * (read - old);
        }
        tank.mLP = grid_fx::Flush(read + mDamping * (tank.mLP - read));
        tank.mLP2 = grid_fx::Flush(tank.mLP + mDamping * (tank.mLP2 - tank.mLP));
        wet[ch] = effect == SlotEffect::AnalogDelay ? tank.mLP2 : tank.mLP;
        if (effect != SlotEffect::DigitalDelay && character > 0.)
        {
          const double drive = 1. + 3. * character;
          wet[ch] = std::tanh(wet[ch] * drive) / drive;
        }
      }
      const double send[2]{(1. - ping) * x[0] + ping * .5 * (x[0] + x[1]), (1. - ping) * x[1]};
      const double output = mOutput(extra[1] * .05, [](double x) { return std::pow(10., x); });
      for (int ch = 0; ch < count; ++ch)
      {
        auto& tank = *channels[ch];
        tank.mDC = grid_fx::Flush(send[ch] + dcCoefficient * (tank.mDC - send[ch]));
        const double feedback = (1. - ping) * wet[ch] + ping * wet[1 - ch];
        tank.mLine.Push((effect == SlotEffect::DigitalDelay ? send[ch] : send[ch] - tank.mDC)
                        + c[1] * .01 * live * feedback);
        const double peak = (1. - ping) * std::abs(x[ch]) + ping * std::max(std::abs(x[0]), std::abs(x[1]));
        tank.mEnv = grid_fx::Flush(peak + (peak > tank.mEnv ? envelopeAttack : envelopeRelease) * (tank.mEnv - peak));
        const double duck = effect == SlotEffect::DigitalDelay ? 1. / (1. + character * 12. * tank.mEnv) : 1.;
        (ch == 0 ? samples : rightSamples)[i] = mMixLaw.Apply(((1. - mix) * x[ch] + mix * wet[ch] * duck) * output, x[ch], mix, output);
      }
    }
  }

private:
  static constexpr double kJumpMs = 1., kFadeSeconds = .03;
  grid_fx::MixLaw mMixLaw;
  grid_fx::DelayLine mLine;
  grid_fx::SmoothControls<5> mControls;
  grid_fx::SmoothControls<2> mExtras;
  grid_fx::SmoothControls<1> mPing, mDuckRelease;
  grid_fx::DrainRamp mDrain;
  grid_fx::Memo mDuckPole;
  grid_fx::Memo mOutput;
  double mRate = 48000., mLP = 0., mLP2 = 0., mDC = 0., mEnv = 0., mPhase = 0., mDamping = 0.;
  // Time-change crossfade: progress (1 when idle), outgoing tap and pending jump, in ms.
  double mFade = 1., mFadeStep = 1., mFrom = 0., mTime = 0., mHold = 0.;
  bool mJump = false, mHasTime = false;
  int mCounter = 0;
};

class SlotModulation
{
public:
  void Reset(double rate)
  {
    mRate = rate;
    mLine.Reset(static_cast<int>(.06 * rate));
    mControls.Reset(rate);
    mSpread.Reset(rate); mExtension.Reset(rate); mCrossover.Reset(rate); mRise = 0.; mSplit = {};
    mPhase = mFeedback = mEnv = mTone = 0.;
    mAllpass = {}; mFirstStages = true; mStageWeights = {0., 1., 0., 0.};
    mCounter = 0; mFirstVoices = true; mVoices = 0.;
  }
  void Process(double* samples, int frames, SlotEffect effect, const UtilityControls& controls, bool right = false)
  {
    auto values = controls.modulation;
    if (controls.sync)
      values[0] = 1. / grid_tempo::Seconds(controls.tempo, controls.division);
    mControls.Target(values, uModRate, controls.sync);
    mSpread.Target({right && effect != SlotEffect::Vibrato ? (effect == SlotEffect::Chorus ? controls.chorusSpread : controls.extra.modSpread) : 0.}, uChorusSpread);
    mExtension.Target({controls.extra.chorusDelay, controls.extra.flangerDelay, controls.extra.vibratoRise},
      std::array<int, 3>{uChorusDelay, uFlangerDelay, uVibratoRise});
    if (effect == SlotEffect::HarmonicTremolo) mCrossover.Target({controls.extra.harmonicCrossover}, uHarmonicCrossover);
    const int stages = std::clamp(static_cast<int>(controls.extra.phaserStages), 0, 3);
    if (mFirstStages) { mStageWeights = {}; mStageWeights[stages] = 1.; mFirstStages = false; }
    const double voices = effect == SlotEffect::Chorus && controls.extra.chorusVoices >= .5 ? 1. : 0.;
    if (mFirstVoices) { mVoices = voices; mFirstVoices = false; }
    const double envelopeAttack = std::exp(-1. / (.005 * mRate)), envelopeRelease = std::exp(-1. / (.15 * mRate));
    for (int i = 0; i < frames; ++i)
    {
      const auto& c = mControls.Tick();
      const auto& extension = mExtension.Tick();
      const double x = samples[i];
      if (mEnv < .001) mRise = 0.;
      else mRise = extension[2] <= 0. ? 1. : std::min(1., mRise + 1. / (extension[2] * .001 * mRate));
      const double depth = c[1] * .01 * (effect == SlotEffect::Vibrato && extension[2] > 0. ? mRise : 1.), character = c[3] * .01, mix = c[4] * .01;
      const double peak = std::abs(x);
      mEnv = grid_fx::Flush(peak + (peak > mEnv ? envelopeAttack : envelopeRelease) * (mEnv - peak));
      mPhase += c[0] * (effect == SlotEffect::Tremolo && !controls.sync ? 1. + character * std::min(3., mEnv * 6.) : 1.)
                / mRate;
      if (mPhase >= 1.)
        mPhase -= 1.;
      const double phase = mPhase + .005 * mSpread.Tick()[0];
      const double lfo = std::sin(2. * grid_fx::pi * phase);
      double wet = x;
      if (effect == SlotEffect::Tremolo)
      {
        const double shape = 1. + 5. * character;
        wet = x * (1. - .5 * depth * (1. + std::tanh(shape * lfo) / mShape(shape, [](double s) { return std::tanh(s); })));
      }
      else if (effect == SlotEffect::HarmonicTremolo)
      {
        const double crossover = mCrossover.Tick()[0];
        if (mCounter++ % 16 == 0)
        {
          mSplitG = std::tan(grid_fx::pi * std::min(crossover, .4 * mRate) / mRate);
          mSplitA = 1. / (1. + mSplitG * (mSplitG + 2.));
        }
        mCounter %= 16;
        // Critically damped (Q 0.5) TPT low-pass; the treble band is the exact remainder, so
        // the bands always sum to the input and Depth 0 is the input itself.
        const double band = mSplitA * (mSplit[0] + mSplitG * (x - mSplit[1]));
        const double low = mSplit[1] + mSplitG * band;
        mSplit[0] = grid_fx::Flush(2. * band - mSplit[0]);
        mSplit[1] = grid_fx::Flush(2. * low - mSplit[1]);
        const double shape = 1. + 5. * character;
        const double swing = std::tanh(shape * lfo) / mShape(shape, [](double s) { return std::tanh(s); });
        // Bass dips while treble returns, then the reverse: two gains in opposite phase.
        wet = x - depth * (.5 * (1. + swing) * low + .5 * (1. - swing) * (x - low));
      }
      else if (effect == SlotEffect::Phaser)
      {
        if (mCounter++ % 16 == 0)
        {
          const double f = std::min(.35 * mRate, 650. * std::exp2(lfo * depth * 2.2 + character));
          const double t = std::tan(grid_fx::pi * f / mRate);
          mCoefficient = (t - 1.) / (t + 1.);
        }
        mCounter %= 16;
        wet = x + c[2] * .01 * mFeedback;
        // Stop inactive stages on the sample their fade ends, independent of host block size.
        const int stageLimit = std::max(std::array<int, 4>{4, 6, 8, 12}[stages], mStageWeights[3] > 0. ? 12 : mStageWeights[2] > 0. ? 8 : mStageWeights[1] > 0. ? 6 : 4);
        std::array<double, 4> taps{};
        for (int stage = 0; stage < stageLimit; ++stage)
        {
          auto& state = mAllpass[stage];
          const double y = mCoefficient * wet + state;
          state = grid_fx::Flush(wet - mCoefficient * y);
          wet = y;
          if (stage == 3) taps[0] = wet;
          if (stage == 5) taps[1] = wet;
          if (stage == 7) taps[2] = wet;
          if (stage == 11) taps[3] = wet;
        }
        wet = 0.;
        for (int j = 0; j < 4; ++j)
        {
          mStageWeights[j] += std::clamp((j == stages ? 1. : 0.) - mStageWeights[j], -1. / (.005 * mRate), 1. / (.005 * mRate));
          wet += mStageWeights[j] * taps[j];
        }
        mFeedback = std::tanh(wet);
      }
      else
      {
        const bool flange = effect == SlotEffect::Flanger;
        const bool vibrato = effect == SlotEffect::Vibrato;
        const double centre = flange ? extension[1] * .001 : vibrato ? .008 : extension[0] * .001;
        const double excursion = flange ? .0025 : vibrato ? .006 : .006;
        const double wobble = (1. - .25 * character) * lfo + .25 * character * std::sin(4. * grid_fx::pi * phase + .7);
        wet = mLine.Read(mRate * (centre + excursion * depth * wobble));
        // Chorus 3 voices: two more taps a third of a cycle apart, at their own depth and
        // delay. Their delays differ by milliseconds, so the voices add by power: 1/sqrt(3)
        // keeps the wet level of one voice. Switching modes fades over 5 ms.
        mVoices += std::clamp(voices - mVoices, -1. / (.005 * mRate), 1. / (.005 * mRate));
        if (mVoices > 0.)
        {
          double sum = wet;
          for (const auto& voice : kChorusVoices)
          {
            const double p = phase + voice.phase;
            const double motion = (1. - .25 * character) * std::sin(2. * grid_fx::pi * p) + .25 * character * std::sin(4. * grid_fx::pi * p + .7);
            sum += mLine.Read(mRate * (centre + voice.delay + excursion * depth * voice.depth * motion));
          }
          wet += mVoices * (sum * .5773502691896258 - wet);
        }
        const double tone = mToneCoefficient(-2. * grid_fx::pi * (12000. - 9000. * character) / mRate,
                                             [](double x) { return std::exp(x); });
        mTone = grid_fx::Flush(wet + tone * (mTone - wet));
        wet += character * (mTone - wet);
        mLine.Push(x + (flange ? c[2] * .01 * (1. - 2. * character) * std::tanh(wet) : 0.));
      }
      samples[i] = effect == SlotEffect::Vibrato ? wet : (1. - mix) * x + mix * wet;
    }
  }

private:
  struct ChorusVoice
  {
    double phase, depth, delay; // cycle offset, share of Depth, added base delay in seconds
  };
  static constexpr ChorusVoice kChorusVoices[]{{1. / 3., .85, .0023}, {2. / 3., .7, .0011}};
  grid_fx::DelayLine mLine;
  grid_fx::SmoothControls<5> mControls;
  grid_fx::SmoothControls<1> mSpread;
  grid_fx::SmoothControls<3> mExtension;
  grid_fx::SmoothControls<1> mCrossover;
  std::array<double, 2> mSplit{};
  double mSplitG = 0., mSplitA = 1.;
  double mRise = 0.;
  std::array<double, 4> mStageWeights{0., 1., 0., 0.};
  bool mFirstStages = true;
  std::array<double, 12> mAllpass{};
  grid_fx::Memo mShape, mToneCoefficient;
  double mRate = 48000., mPhase = 0., mFeedback = 0., mEnv = 0., mTone = 0., mCoefficient = 0., mVoices = 0.;
  int mCounter = 0;
  bool mFirstVoices = true;
};

#include "dsp/airwindows/AirwindowsTape.h"
class SlotTape
{
public:
  static int FactorForRate(double rate) { return rate < 88200. ? 2 : 1; }
  // The explicit factor is an offline comparison hook for the aliasing tests,
  // not a saved control; production always uses FactorForRate.
  void Reset(double rate, int factor = 0)
  {
    mFactor = factor == 1 || factor == 2 ? factor : FactorForRate(rate);
    mTape.Reset(rate * mFactor);
    mSetDrive = mSetBump = std::numeric_limits<double>::quiet_NaN(); // The first chunk always sets.
    mControls.Reset(rate); mCounter = 0;
    if (mFactor == 2) { mOversample2.Reset(); mDryReference.Reset(); }
    mMixBypass.Reset(rate);
  }
  void Process(double* samples, int frames, const UtilityControls& controls)
  {
    mControls.Target(controls.tape, uTapeDrive);
    mMixBypass.Target(controls.tape[2] > 0.);
    for (int offset = 0; offset < frames;)
    {
      const int count = std::min(32 - mCounter, frames - offset);
      std::array<double, 32> dry{}, mix{}, gain{};
      for (int i = 0; i < count; ++i)
      {
        const auto& c = mControls.Tick();
        // The tape ramps its gains across each call towards the chunk's last smoothed
        // value, so partial chunks at host-block edges follow the same control curve.
        if (i == count - 1 && (c[0] != mSetDrive || c[1] != mSetBump))
        {
          mTape.Set(c[0], c[1]);
          mSetDrive = c[0]; mSetBump = c[1];
        }
        dry[i] = samples[offset + i]; mix[i] = c[2] * .01;
        gain[i] = mOutput(c[3] * .05, [](double x) { return std::pow(10., x); });
      }
      if (mFactor == 2) ProcessHigh(samples + offset, count);
      else mTape.ProcessMono(samples + offset, count);
      for (int i = 0; i < count; ++i)
      {
        const double wet = samples[offset + i];
        double blended = dry[i] + mix[i] * (wet - dry[i]);
        if (mFactor == 2)
        {
          mDryReference.Push(dry[i]);
          mMixBypass.Tick();
          if (mix[i] != 1. && !mMixBypass.IsDry())
          {
            const double alignedDry = mDryReference.Filtered();
            blended = alignedDry + mix[i] * (wet - alignedDry);
          }
          blended = mMixBypass.Apply(dry[i], blended);
        }
        samples[offset + i] = blended * gain[i];
      }
      offset += count; mCounter = (mCounter + count) % 32;
    }
  }
private:
  void ProcessHigh(double* samples, int count)
  {
    constexpr int Factor = 2;
    std::array<double, 32 * Factor> high{};
    for (int i = 0; i < count; ++i)
    {
      const auto up = mOversample2.Upsample(samples[i]);
      std::copy(up.begin(), up.end(), high.begin() + i * Factor);
    }
    mTape.ProcessMono(high.data(), count * Factor);
    for (int i = 0; i < count; ++i)
    {
      std::array<double, Factor> down{};
      std::copy_n(high.data() + i * Factor, Factor, down.data());
      samples[i] = mOversample2.Downsample(down);
    }
  }
  AirwindowsTape mTape{};
  grid_oversampling::FIR<2, 32> mOversample2;
  grid_oversampling::LinearReference<2> mDryReference;
  grid_oversampling::MixBypass mMixBypass;
  grid_fx::SmoothControls<4> mControls;
  grid_fx::Memo mOutput;
  double mSetDrive = 0., mSetBump = 0.;
  int mFactor = 2, mCounter = 0;
};
