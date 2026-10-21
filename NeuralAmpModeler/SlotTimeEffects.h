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

// Eight-line orthogonal feedback delay network. Room uses sparse early taps;
// Plate adds four input allpasses; Cloud adds slow, decorrelated delay motion.
// These are original voiced algorithms, not claims of physical plate/spring models.
class SlotReverb
{
public:
  static constexpr std::array<double, 4> kDiffusionSeconds{.0043, .0067, .0091, .0137};
  static constexpr double kMaximumSize = 1.5, kRightScale = 1.037;
  // All diffusers share capacity; expose the available read range for diagnostics.
  double DiffuserMaxDelay() const { return mDiffusers.back().MaxDelay(); }
  void Reset(double rate)
  {
    mRate = rate;
    mControls.Reset(rate);
    mExtras.Reset(rate);
    mStereo.Reset(rate);
    mStereoMix.Reset(rate);
    mInputLow = mHighPassMix = 0.;
    mDamp = {};
    mPhase = 0.;
    mCounter = 0;
    for (auto& line : mLines)
      line.Reset(static_cast<int>(.18 * rate));
    for (auto& line : mDiffusers)
      line.Reset(static_cast<int>(std::ceil(kDiffusionSeconds.back() * kMaximumSize * kRightScale * rate)));
    mPredelay.Reset(static_cast<int>(.11 * rate));
    mEarly.Reset(static_cast<int>(.08 * rate));
  }
  void Process(double* samples, int frames, SlotEffect effect, const UtilityControls& controls, bool right = false)
  {
    mControls.Target(controls.reverb, uReverbDecay);
    mExtras.Target({controls.reverbMotion, controls.reverbHighPass}, uReverbMotion);
    constexpr std::array<double, 8> lengths{.0297, .0371, .0411, .0437, .0531, .0617, .0719, .0797};
    const bool room = effect == SlotEffect::RoomReverb, cloud = effect == SlotEffect::CloudReverb;
    for (int i = 0; i < frames; ++i)
    {
      const auto& c = mControls.Tick();
      const auto& extra = mExtras.Tick();
      const double size = c[1] * .01 * (room ? .58 : 1.) * (right ? kRightScale : 1.), mix = c[4] * .01;
      if (mCounter++ % 32 == 0)
      {
        mHighPassCoefficient = std::exp(-2. * grid_fx::pi * std::max(1., extra[1]) / mRate);
        mDamping = std::exp(-2. * grid_fx::pi * std::min(c[2], .4 * mRate) / mRate);
        for (int j = 0; j < 8; ++j)
          mFeedback[j] = std::pow(.001, lengths[j] * size / c[0]);
      }
      mCounter %= 32;
      const double x = samples[i];
      mInputLow = grid_fx::Flush(x + mHighPassCoefficient * (mInputLow - x));
      const double hpTarget = extra[1] > .01 ? 1. : 0.;
      mHighPassMix += std::clamp(hpTarget - mHighPassMix, -1. / (.02 * mRate), 1. / (.02 * mRate));
      const double filtered = x - mHighPassMix * mInputLow;
      // Fractional read needs one sample; explicit zero keeps zero pre-delay exact.
      double input = c[3] < .001 ? filtered : mPredelay.Read(std::max(1., c[3] * .001 * mRate));
      mPredelay.Push(filtered);
      const double early = .45 * mEarly.Read(mRate * .011 * size) - .3 * mEarly.Read(mRate * .023 * size)
                           + .2 * mEarly.Read(mRate * .037 * size);
      mEarly.Push(input);
      if (!room)
        for (int j = 0; j < 4; ++j)
        {
          const double delayed = mDiffusers[j].Read(mRate * kDiffusionSeconds[j] * size);
          const double y = delayed - .6 * input;
          mDiffusers[j].Push(input + .6 * y);
          input = y;
        }
      std::array<double, 8> taps{};
      double wet = 0.;
      for (int j = 0; j < 8; ++j)
      {
        const double motion = extra[0] == 0. ? 0. : extra[0] * .01
                              * (cloud  ? .0018
                                 : room ? .0002
                                        : .0006)
                              * std::sin(2. * grid_fx::pi * mPhase * (1. + .13 * j) + j);
        const double tap = mLines[j].Read(mRate * (lengths[j] * size + motion));
        mDamp[j] = grid_fx::Flush(tap + mDamping * (mDamp[j] - tap));
        taps[j] = mDamp[j] * mFeedback[j];
        wet += (j % 2 ? -1. : 1.) * tap * .125;
      }
      // Normalized Hadamard: energy preserving before per-line decay/damping.
      for (int span = 1; span < 8; span *= 2)
        for (int base = 0; base < 8; base += span * 2)
          for (int j = 0; j < span; ++j)
          {
            const double a = taps[base + j], b = taps[base + j + span];
            taps[base + j] = a + b;
            taps[base + j + span] = a - b;
          }
      for (int j = 0; j < 8; ++j)
        mLines[j].Push(taps[j] * .3535533905932738 + input * (j % 3 == 0 ? -.2 : .2));
      mPhase += .17 / mRate;
      // Do not wrap: non-integer related rates must stay continuous at the seam.
      if (mPhase > 1.e6)
        mPhase = 0.;
      wet = wet * (cloud ? 2. : 1.5) + early * (room ? .7 : .15);
      samples[i] = (1. - mix) * x + mix * wet;
    }
  }


  void ProcessStereo(double* left, double* right, const double* dryLeft, const double* dryRight, int frames,
                     SlotEffect effect, const UtilityControls& controls, SlotReverb& other)
  {
    // Contractive input matrix retains stereo information and excites both tanks
    // from either side. The networks use different delay lengths at every rate.
    for (int i = 0; i < frames; ++i)
    {
      left[i] = .75 * dryLeft[i] + .25 * dryRight[i];
      right[i] = .25 * dryLeft[i] + .75 * dryRight[i];
    }
    auto wet = controls;
    wet.reverb[4] = 100.;
    Process(left, frames, effect, wet);
    other.Process(right, frames, effect, wet, true);
    mStereo.Target({controls.stereoReverb ? 1. : 0.}, uReverbStereo);
    // Mix remains independently smoothed although the tank itself renders wet-only.
    mStereoMix.Target(controls.reverb, uReverbDecay);
    for (int i = 0; i < frames; ++i)
    {
      const double width = mStereo.Tick()[0], mix = mStereoMix.Tick()[4] * .01;
      const double mid = .5 * (left[i] + right[i]), side = .5 * (left[i] - right[i]) * width;
      left[i] = (1. - mix) * dryLeft[i] + mix * (mid + side);
      right[i] = (1. - mix) * dryRight[i] + mix * (mid - side);
    }
  }

private:
  std::array<grid_fx::DelayLine, 8> mLines;
  std::array<grid_fx::DelayLine, 4> mDiffusers;
  grid_fx::DelayLine mPredelay, mEarly;
  grid_fx::SmoothControls<5> mControls, mStereoMix;
  grid_fx::SmoothControls<1> mStereo;
  grid_fx::SmoothControls<2> mExtras;
  std::array<double, 8> mDamp{}, mFeedback{};
  double mRate = 48000., mDamping = 0., mPhase = 0.;
  double mInputLow = 0., mHighPassMix = 0., mHighPassCoefficient = 0.;
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
