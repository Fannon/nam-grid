#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include "EffectParameters.h"
#include "ExactMemo.h"
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
  }
  void Target(const std::array<double, N>& values, int offset, bool synced = false)
  {
    for (size_t j = 0; j < N; ++j)
      mTarget[j] = synced && j == 0 ? values[j]
                                    : std::clamp(values[j], kEffectParameters[offset + j].minimum,
                                                 kEffectParameters[offset + j].maximum);
    if (mFirst)
    {
      mValue = mTarget;
      mFirst = false;
    }
  }
  void Target(const std::array<double, N>& values, const std::array<int, N>& ids)
  {
    for (size_t j = 0; j < N; ++j)
      mTarget[j] = std::clamp(values[j], kEffectParameters[ids[j]].minimum, kEffectParameters[ids[j]].maximum);
    if (mFirst) { mValue = mTarget; mFirst = false; }
  }
  const std::array<double, N>& Tick()
  {
    for (size_t j = 0; j < N; ++j)
      mValue[j] = mTarget[j] + mCoefficient * (mValue[j] - mTarget[j]);
    return mValue;
  }

private:
  std::array<double, N> mValue{}, mTarget{};
  double mCoefficient = 0.;
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
} // namespace grid_fx

class SlotDelay
{
public:
  void Reset(double rate)
  {
    mRate = rate;
    mLine.Reset(static_cast<int>(rate * 8.1));
    mControls.Reset(rate);
    mExtras.Reset(rate);
    mPing.Reset(rate);
    mLP = mLP2 = mDC = mEnv = mPhase = 0.; mDuckRelease.Reset(rate);
    mCounter = 0;
  }
  void Process(double* samples, int frames, SlotEffect effect, const UtilityControls& controls,
               SlotDelay* right = nullptr, double* rightSamples = nullptr, bool draining = false)
  {
    mDuckRelease.Target({controls.extra.duckRelease}, uDuckRelease);
    auto values = controls.delay;
    if (controls.sync)
      values[0] = grid_tempo::Seconds(controls.tempo, controls.division) * 1000.;
    mControls.Target(values, uDelayTime, controls.sync);
    mExtras.Target({controls.delayMotion, controls.delayOutput}, uDelayMotion);
    mPing.Target({right && controls.pingPong ? 1. : 0.}, uDelayPingPong);
    const double dcCoefficient = std::exp(-2. * grid_fx::pi * 70. / mRate);
    const double envelopeAttack = std::exp(-1. / (.003 * mRate));
    for (int i = 0; i < frames; ++i)
    {
      const auto& c = mControls.Tick();
      const auto& extra = mExtras.Tick();
      const double ping = mPing.Tick()[0];
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
        const double read = tank.mLine.Read(c[0] * .001 * mRate + flutter);
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
                        + (draining ? 0. : c[1] * .01) * feedback);
        const double peak = (1. - ping) * std::abs(x[ch]) + ping * std::max(std::abs(x[0]), std::abs(x[1]));
        tank.mEnv = grid_fx::Flush(peak + (peak > tank.mEnv ? envelopeAttack : envelopeRelease) * (tank.mEnv - peak));
        const double duck = effect == SlotEffect::DigitalDelay ? 1. / (1. + character * 12. * tank.mEnv) : 1.;
        (ch == 0 ? samples : rightSamples)[i] = ((1. - mix) * x[ch] + mix * wet[ch] * duck) * output;
      }
    }
  }

private:
  grid_fx::DelayLine mLine;
  grid_fx::SmoothControls<5> mControls;
  grid_fx::SmoothControls<2> mExtras;
  grid_fx::SmoothControls<1> mPing, mDuckRelease;
  grid_fx::Memo mDuckPole;
  grid_fx::Memo mOutput;
  double mRate = 48000., mLP = 0., mLP2 = 0., mDC = 0., mEnv = 0., mPhase = 0., mDamping = 0.;
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
    mSpread.Reset(rate); mExtension.Reset(rate); mRise = 0.;
    mPhase = mFeedback = mEnv = mTone = 0.;
    mAllpass = {}; mFirstStages = true; mStageWeights = {0., 1., 0., 0.};
    mCounter = 0;
  }
  void Process(double* samples, int frames, SlotEffect effect, const UtilityControls& controls, bool right = false)
  {
    auto values = controls.modulation;
    if (controls.sync)
      values[0] = 1. / grid_tempo::Seconds(controls.tempo, controls.division);
    mControls.Target(values, uModRate, controls.sync);
    mSpread.Target({right && effect != SlotEffect::Vibrato ? (effect == SlotEffect::Chorus ? controls.chorusSpread : controls.extra.modSpread) : 0.}, uChorusSpread);
    mExtension.Target({controls.extra.chorusDelay, controls.extra.flangerDelay, controls.extra.modOutput, controls.extra.vibratoRise},
      std::array<int, 4>{uChorusDelay, uFlangerDelay, uModOutput, uVibratoRise});
    const int stages = std::clamp(static_cast<int>(controls.extra.phaserStages), 0, 3);
    if (mFirstStages) { mStageWeights = {}; mStageWeights[stages] = 1.; mFirstStages = false; }
    const double envelopeAttack = std::exp(-1. / (.005 * mRate)), envelopeRelease = std::exp(-1. / (.15 * mRate));
    for (int i = 0; i < frames; ++i)
    {
      const auto& c = mControls.Tick();
      const auto& extension = mExtension.Tick();
      const double x = samples[i];
      if (mEnv < .001) mRise = 0.;
      else mRise = extension[3] <= 0. ? 1. : std::min(1., mRise + 1. / (extension[3] * .001 * mRate));
      const double depth = c[1] * .01 * (effect == SlotEffect::Vibrato && extension[3] > 0. ? mRise : 1.), character = c[3] * .01, mix = c[4] * .01;
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
        const double tone = mToneCoefficient(-2. * grid_fx::pi * (12000. - 9000. * character) / mRate,
                                             [](double x) { return std::exp(x); });
        mTone = grid_fx::Flush(wet + tone * (mTone - wet));
        wet += character * (mTone - wet);
        mLine.Push(x + (flange ? c[2] * .01 * (1. - 2. * character) * std::tanh(wet) : 0.));
      }
      samples[i] = effect == SlotEffect::Vibrato ? wet : (1. - mix) * x + mix * wet;
      if (effect == SlotEffect::Vibrato || effect == SlotEffect::Tremolo)
        samples[i] *= mOutput(extension[2] * .05, [](double x) { return std::pow(10., x); });
    }
  }

private:
  grid_fx::DelayLine mLine;
  grid_fx::SmoothControls<5> mControls;
  grid_fx::SmoothControls<1> mSpread;
  grid_fx::SmoothControls<4> mExtension;
  grid_fx::Memo mOutput;
  double mRise = 0.;
  std::array<double, 4> mStageWeights{0., 1., 0., 0.};
  bool mFirstStages = true;
  std::array<double, 12> mAllpass{};
  grid_fx::Memo mShape, mToneCoefficient;
  double mRate = 48000., mPhase = 0., mFeedback = 0., mEnv = 0., mTone = 0., mCoefficient = 0.;
  int mCounter = 0;
};

#include "dsp/airwindows/AirwindowsTape.h"
class SlotTape
{
public:
  void Reset(double rate)
  {
    mTape.Reset(rate);
    mControls.Reset(rate);
  }
  void Process(double* samples, int frames, const UtilityControls& controls)
  {
    mControls.Target(controls.tape, uTapeDrive);
    for (int start = 0; start < frames; start += 32)
    {
      const int count = std::min(32, frames - start);
      double dry[32], right[32];
      std::copy_n(samples + start, count, dry);
      const auto c = mControls.Tick();
      mTape.Set(c[0], c[1]);
      double* inputs[]{dry, dry};
      double* outputs[]{samples + start, right};
      mTape.Process(inputs, outputs, count);
      for (int i = 0; i < count; ++i)
      {
        const auto& smooth = i == 0 ? c : mControls.Tick();
        const double mix = smooth[2] * .01, output = std::pow(10., smooth[3] * .05);
        samples[start + i] = (dry[i] * (1. - mix) + samples[start + i] * mix) * output;
      }
    }
  }

private:
  AirwindowsTape mTape{};
  grid_fx::SmoothControls<4> mControls;
};
