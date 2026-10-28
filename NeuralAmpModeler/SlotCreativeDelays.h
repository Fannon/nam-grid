#pragma once
#include "dsp/reverb/ReverbPrimitives.h"

// Multi-head tape echo: four physical read positions, shared tape motion, and
// feedback from the last head only. Adding heads cannot increase loop gain.
class SlotMultiHeadDelay
{
public:
  void Reset(double rate)
  {
    mMixLaw.Reset(rate);
    mRate = rate; mControls.Reset(rate); mExtras.Reset(rate); mHeads = {};
    for (auto& line : mLine) line.Reset(static_cast<int>(rate * 8.1));
    mLow = {}; mDC = {}; mPhase = 0.; mCounter = 0; mFirst = true; mCrossFeedback = 0.; mDrain.Reset(rate);
  }
  void Process(double* left, double* right, int frames, const UtilityControls& c, bool draining = false)
  {
    auto values = c.delay;
    if (c.sync) values[0] = grid_tempo::Seconds(c.tempo, c.division) * 1000.;
    mControls.Target(values, uDelayTime, c.sync); mExtras.Target({c.delayMotion, c.delayOutput}, uDelayMotion);
    const auto& target = effect_modes::TapeHeads(c.tapeHeads);
    if (mFirst) { mHeads = target.gains; mCrossFeedback = target.crossFeedback ? 1. : 0.; mFirst = false; }
    for (int i = 0; i < frames; ++i)
    {
      mMixLaw.Tick(c.constantDry);
      const auto& v = mControls.Tick(); const auto& extra = mExtras.Tick();
      const double live = mDrain.Tick(draining);
      if (mCounter == 0)
      {
        mDamping = grid_reverb::Coefficient(v[2], mRate); mDCcoefficient = grid_reverb::Coefficient(60., mRate);
        mSmooth = std::exp(-1. / (.02 * mRate));
      }
      mCounter = (mCounter + 1) % 32;
      mOutput = mOutputGain(extra[1] / 20., [](double x) { return std::pow(10., x); });
      for (int ch = 0; ch < 2; ++ch)
        for (int head = 0; head < 4; ++head)
          mHeads[ch][head] = target.gains[ch][head] + mSmooth * (mHeads[ch][head] - target.gains[ch][head]);
      const double cross = target.crossFeedback ? 1. : 0.;
      mCrossFeedback = cross + mSmooth * (mCrossFeedback-cross);
      const grid_reverb::Stereo dry{left[i], right ? right[i] : left[i]};
      grid_reverb::Stereo wet{}, feedback{};
      const double modulation = extra[0] * .01 * .001 * (std::sin(mPhase) + .15 * std::sin(7. * mPhase));
      for (int ch = 0; ch < 2; ++ch)
      {
        for (int head = 0; head < 4; ++head)
        {
          const double seconds = (v[0] * .001 + modulation) * (head + 1) * .25;
          const double tap = mLine[ch].Read(std::max(1., mRate * seconds));
          const double filtered = grid_reverb::LowPass(tap, mLow[ch][head], mDamping);
          const double drive = 1. + v[3] * .03;
          const double saturated = std::tanh(filtered * drive) / drive;
          wet[ch] += saturated * mHeads[ch][head];
          if (head == 3) feedback[ch] = saturated;
        }
      }
      for (int ch = 0; ch < 2; ++ch)
      {
        const double send = dry[ch] - grid_reverb::LowPass(dry[ch], mDC[ch], mDCcoefficient);
        const double routed = feedback[ch] + mCrossFeedback * (feedback[1-ch]-feedback[ch]);
        mLine[ch].Push(send + v[1] * .01 * live * routed);
      }
      const double mix = v[4] * .01;
      const double monoWet = right ? wet[0] : .5 * (wet[0]+wet[1]);
      left[i] = mMixLaw.Apply((dry[0] + mix * (monoWet - dry[0])) * mOutput, dry[0], mix, mOutput);
      if (right) right[i] = mMixLaw.Apply((dry[1] + mix * (wet[1] - dry[1])) * mOutput, dry[1], mix, mOutput);
      mPhase += 2. * grid_fx::pi * .67 / mRate;
      if (mPhase >= 2. * grid_fx::pi) mPhase -= 2. * grid_fx::pi;
    }
  }
private:
  grid_fx::MixLaw mMixLaw;
  std::array<grid_fx::DelayLine, 2> mLine;
  std::array<std::array<double, 4>, 2> mLow{};
  grid_reverb::Stereo mDC{};
  std::array<std::array<double, 4>, 2> mHeads{};
  grid_fx::SmoothControls<5> mControls;
  grid_fx::SmoothControls<2> mExtras;
  grid_fx::DrainRamp mDrain;
  double mRate = 48000., mPhase = 0., mDamping = 0., mDCcoefficient = 0., mOutput = 1., mSmooth = 0., mCrossFeedback = 0.;
  grid_fx::Memo mOutputGain;
  int mCounter = 0; bool mFirst = true;
};

// Diffused delay inspired by Greyhole: a repeat feeds a rotating allpass chain.
// At Diffusion=0 the chain is bypassed in the feedback/output path; at 100% the
// same repeat blooms into a stereo cluster. Feedback remains contractive.
class SlotDiffusedDelay
{
public:
  void Reset(double rate)
  {
    mMixLaw.Reset(rate);
    mRate = rate; mControls.Reset(rate); mExtras.Reset(rate); mDiffusion.Reset(rate);
    for (auto& line : mLine) line.Reset(rate, 8.11);
    for (int i = 0; i < 12; ++i)
      mDiffusers[i].Reset(rate, .0011 + (i % 4) * .00073 + (i / 4) * .0013,
                         .0017 + (3 - i % 4) * .00091 + (i / 4) * .0017,
                         (i / 4 == 1 ? -1. : 1.) * 1.5707963267948966);
    mLow = {}; mPhase = 0.; mCounter = 0; mDrain.Reset(rate);
  }
  void Process(double* left, double* right, int frames, const UtilityControls& c, bool draining = false)
  {
    auto values = c.delay;
    if (c.sync) values[0] = grid_tempo::Seconds(c.tempo, c.division) * 1000.;
    mControls.Target(values, uDelayTime, c.sync); mExtras.Target({c.delayMotion, c.delayOutput}, uDelayMotion);
    mDiffusion.Target({c.delayDiffusion}, uDelayDiffusion);
    for (int i = 0; i < frames; ++i)
    {
      mMixLaw.Tick(c.constantDry);
      const auto& v = mControls.Tick(); const auto& extra = mExtras.Tick();
      const double diffusion = mDiffusion.Tick()[0] * .01, live = mDrain.Tick(draining);
      if (mCounter == 0) mCoefficient = grid_reverb::Coefficient(v[2], mRate);
      mCounter = (mCounter + 1) % 32;
      mOutput = mOutputGain(extra[1] / 20., [](double x) { return std::pow(10., x); });
      const grid_reverb::Stereo dry{left[i], right ? right[i] : left[i]};
      grid_reverb::Stereo repeat{};
      for (int ch = 0; ch < 2; ++ch)
      {
        const double movement = extra[0] * .01 * .0007 * std::sin(mPhase + ch * 1.5707963267948966);
        repeat[ch] = grid_reverb::LowPass(mLine[ch].Read(std::max(1., (v[0] * .001 + movement) * mRate)), mLow[ch], mCoefficient);
      }
      auto blurred = repeat;
      for (auto& diffuser : mDiffusers) blurred = diffuser.Tick(blurred, 1., .65);
      for (int ch = 0; ch < 2; ++ch)
      {
        const double wet = repeat[ch] + diffusion * (blurred[ch] - repeat[ch]);
        mLine[ch].Push(dry[ch] + v[1] * .01 * live * wet);
        const double output = mMixLaw.Apply((dry[ch] + v[4] * .01 * (wet - dry[ch])) * mOutput, dry[ch], v[4] * .01, mOutput);
        if (ch == 0) left[i] = output; else if (right) right[i] = output;
      }
      mPhase += 2. * grid_fx::pi * .31 / mRate;
      if (mPhase >= 2. * grid_fx::pi) mPhase -= 2. * grid_fx::pi;
    }
  }
private:
  grid_fx::MixLaw mMixLaw;
  std::array<grid_reverb::MovingDelay, 2> mLine;
  std::array<grid_reverb::StereoAllpass, 12> mDiffusers;
  grid_fx::SmoothControls<5> mControls;
  grid_fx::SmoothControls<2> mExtras;
  grid_fx::SmoothControls<1> mDiffusion;
  grid_reverb::Stereo mLow{};
  grid_fx::DrainRamp mDrain;
  double mRate = 48000., mCoefficient = 0., mOutput = 1., mPhase = 0.;
  grid_fx::Memo mOutputGain;
  int mCounter = 0;
};
