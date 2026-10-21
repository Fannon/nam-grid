#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include "EffectParameters.h"
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
  }
  double Read(double delay) const
  {
    delay = std::clamp(delay, 1., MaxDelay());
    // Split the integer offset before wrapping: adding the ring size to a tiny
    // negative fractional position can round up to size(), an out-of-bounds read.
    const auto whole = static_cast<size_t>(delay);
    const double fraction = delay - whole;
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
  double MaxDelay() const { return static_cast<double>(mData.size() - 2); }

private:
  std::vector<double> mData;
  size_t mIndex = 0;
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
    mLP = mLP2 = mDC = mEnv = mPhase = 0.;
    mCounter = 0;
  }
  void Process(double* samples, int frames, SlotEffect effect, const UtilityControls& controls,
               SlotDelay* right = nullptr, double* rightSamples = nullptr, bool draining = false)
  {
    auto values = controls.delay;
    if (controls.sync)
      values[0] = grid_tempo::Seconds(controls.tempo, controls.division) * 1000.;
    mControls.Target(values, uDelayTime, controls.sync);
    mExtras.Target({controls.delayMotion, controls.delayOutput}, uDelayMotion);
    mPing.Target({right && controls.pingPong ? 1. : 0.}, uDelayPingPong);
    const double dcCoefficient = std::exp(-2. * grid_fx::pi * 70. / mRate);
    for (int i = 0; i < frames; ++i)
    {
      const auto& c = mControls.Tick();
      const auto& extra = mExtras.Tick();
      const double ping = mPing.Tick()[0];
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
      const double output = std::pow(10., extra[1] * .05);
      for (int ch = 0; ch < count; ++ch)
      {
        auto& tank = *channels[ch];
        tank.mDC = grid_fx::Flush(send[ch] + dcCoefficient * (tank.mDC - send[ch]));
        const double feedback = (1. - ping) * wet[ch] + ping * wet[1 - ch];
        tank.mLine.Push((effect == SlotEffect::DigitalDelay ? send[ch] : send[ch] - tank.mDC)
                        + (draining ? 0. : c[1] * .01) * feedback);
        const double peak = (1. - ping) * std::abs(x[ch]) + ping * std::max(std::abs(x[0]), std::abs(x[1]));
        tank.mEnv = grid_fx::Flush(peak + std::exp(-1. / ((peak > tank.mEnv ? .003 : .15) * mRate)) * (tank.mEnv - peak));
        const double duck = effect == SlotEffect::DigitalDelay ? 1. / (1. + character * 12. * tank.mEnv) : 1.;
        (ch == 0 ? samples : rightSamples)[i] = ((1. - mix) * x[ch] + mix * wet[ch] * duck) * output;
      }
    }
  }

private:
  grid_fx::DelayLine mLine;
  grid_fx::SmoothControls<5> mControls;
  grid_fx::SmoothControls<2> mExtras;
  grid_fx::SmoothControls<1> mPing;
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
    mSpread.Reset(rate);
    mPhase = mFeedback = mEnv = mTone = 0.;
    mAllpass = {};
    mCounter = 0;
  }
  void Process(double* samples, int frames, SlotEffect effect, const UtilityControls& controls, bool right = false)
  {
    auto values = controls.modulation;
    if (controls.sync)
      values[0] = 1. / grid_tempo::Seconds(controls.tempo, controls.division);
    mControls.Target(values, uModRate, controls.sync);
    mSpread.Target({effect == SlotEffect::Chorus && right ? controls.chorusSpread : 0.}, uChorusSpread);
    for (int i = 0; i < frames; ++i)
    {
      const auto& c = mControls.Tick();
      const double x = samples[i], depth = c[1] * .01, character = c[3] * .01, mix = c[4] * .01;
      const double peak = std::abs(x);
      mEnv = grid_fx::Flush(peak + std::exp(-1. / ((peak > mEnv ? .005 : .15) * mRate)) * (mEnv - peak));
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
        wet = x * (1. - .5 * depth * (1. + std::tanh(shape * lfo) / std::tanh(shape)));
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
        for (auto& state : mAllpass)
        {
          const double y = mCoefficient * wet + state;
          state = grid_fx::Flush(wet - mCoefficient * y);
          wet = y;
        }
        mFeedback = std::tanh(wet);
      }
      else
      {
        const bool flange = effect == SlotEffect::Flanger;
        const bool vibrato = effect == SlotEffect::Vibrato;
        const double centre = flange ? .003 : vibrato ? .008 : .018;
        const double excursion = flange ? .0025 : vibrato ? .006 : .006;
        const double wobble = (1. - .25 * character) * lfo + .25 * character * std::sin(4. * grid_fx::pi * phase + .7);
        wet = mLine.Read(mRate * (centre + excursion * depth * wobble));
        mTone =
          grid_fx::Flush(wet + std::exp(-2. * grid_fx::pi * (12000. - 9000. * character) / mRate) * (mTone - wet));
        wet += character * (mTone - wet);
        mLine.Push(x + (flange ? c[2] * .01 * (1. - 2. * character) * std::tanh(wet) : 0.));
      }
      samples[i] = effect == SlotEffect::Vibrato ? wet : (1. - mix) * x + mix * wet;
    }
  }

private:
  grid_fx::DelayLine mLine;
  grid_fx::SmoothControls<5> mControls;
  grid_fx::SmoothControls<1> mSpread;
  std::array<double, 6> mAllpass{};
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
