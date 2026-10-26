#pragma once
#include "SlotTimeEffects.h"

// Original MIT implementations. Mathematical references/provenance are in
// docs/module-implementation.md; no third-party implementation is copied.
class SlotWah
{
public:
  void Reset(double rate)
  {
    mRate = rate;
    mControls.Reset(rate);
    mS1 = mS2 = 0.;
  }
  void Process(double* samples, int frames, const UtilityControls& controls)
  {
    mControls.Target(controls.wah, uWahPosition);
    for (int i = 0; i < frames; ++i)
    {
      const auto& c = mControls.Tick();
      // Heel/toe endpoints move together with Range; Position follows a log sweep.
      const double heel = 250. * mRange(c[2] * .01, [](double x) { return std::exp2(x); });
      const double hz = std::min(.35 * mRate, heel * mSweep(c[0] * .01, [](double x) { return std::pow(6.8, x); }));
      const double g = mWarp(grid_fx::pi * hz / mRate, [](double x) { return std::tan(x); }), k = 1. / c[1];
      const double x = samples[i];
      // Trapezoidal integration solves the two integrators' feedback together.
      const double band = (mS1 + g * (x - mS2)) / (1. + g * (g + k));
      const double low = mS2 + g * band;
      mS1 = grid_fx::Flush(2. * band - mS1);
      mS2 = grid_fx::Flush(2. * low - mS2);
      // A little broadband body avoids the thinness of an isolated band-pass.
      const double wet = .35 * x + 1.65 * k * band, mix = c[3] * .01;
      samples[i] = ((1. - mix) * x + mix * wet) * mOutput(c[4] * .05, [](double exponent) { return std::pow(10., exponent); });
    }
  }

private:
  grid_fx::SmoothControls<5> mControls;
  grid_fx::Memo mRange, mSweep, mWarp, mOutput;
  double mRate = 48000., mS1 = 0., mS2 = 0.;
};

// Three unequal waveguide-style tanks with stretched allpass dispersion in each
// feedback loop. Drip changes dispersion, not a generic reverb's modulation depth.
// This is a spring-inspired effect, not a calibrated model of a particular tank.
class SlotSpring
{
public:
  void Reset(double rate)
  {
    mRate = rate;
    mControls.Reset(rate);
    mTone.Reset(rate);
    mStereo.Reset(rate);
    mInputLow = {}; mPredelayControl.Reset(rate);
    for (auto& line : mPredelay) line.Reset(static_cast<int>(rate * .201));
    mDamp = {};
    mCounter = 0;
    for (int tank = 0; tank < 3; ++tank)
    {
      mTank[tank].Reset(static_cast<int>(.1 * rate));
      for (int section = 0; section < 16; ++section)
      {
        mDispersion[tank][section].Reset(static_cast<int>(.001 * rate));
        mStretch[tank][section] = std::max(1., rate * (.00037 + .000021 * section + .000013 * tank));
      }
    }
  }
  void Process(double* left, double* right, int frames, const UtilityControls& controls)
  {
    mControls.Target(controls.spring, uSpringDecay);
    mPredelayControl.Target({controls.extra.springPredelay}, uSpringPredelay);
    mTone.Target({controls.reverb[2]}, uReverbTone);
    mStereo.Target({controls.stereoReverb ? 1. : 0.}, uReverbStereo);
    constexpr std::array<double, 3> length{.0473, .0617, .0791};
    constexpr std::array<double, 3> pan{.85, .5, .15};
    const double hp = std::exp(-2. * grid_fx::pi * 130. / mRate);
    for (int i = 0; i < frames; ++i)
    {
      const auto& c = mControls.Tick();
      const double tone = mTone.Tick()[0], width = right ? mStereo.Tick()[0] : 0.;
      if (mCounter++ % 32 == 0)
      {
        mDamping = std::exp(-2. * grid_fx::pi * std::min(tone, .4 * mRate) / mRate);
        for (int tank = 0; tank < 3; ++tank)
          mFeedback[tank] = std::pow(.001, (length[tank] + .009) / c[0]);
      }
      mCounter %= 32;
      const double dry[2]{left[i], right ? right[i] : left[i]};
      double send[2]{};
      const double predelay = mPredelayControl.Tick()[0];
      const double drive = mDrive(c[1] * .05, [](double x) { return std::pow(10., x); });
      for (int ch = 0; ch < 2; ++ch)
      {
        mInputLow[ch] = grid_fx::Flush(dry[ch] + hp * (mInputLow[ch] - dry[ch]));
        const double excited = std::tanh(drive * (dry[ch] - mInputLow[ch]));
        send[ch] = predelay > 0. ? mPredelay[ch].Read(predelay * .001 * mRate) : excited;
        mPredelay[ch].Push(excited);
      }
      double wetLeft = 0., wetRight = 0.;
      for (int tank = 0; tank < 3; ++tank)
      {
        const double tap = mTank[tank].Read(mRate * length[tank]);
        mDamp[tank] = grid_fx::Flush(tap + mDamping * (mDamp[tank] - tap));
        double wave = .4 * (pan[tank] * send[0] + (1. - pan[tank]) * send[1]) + mFeedback[tank] * mDamp[tank];
        for (int section = 0; section < 16; ++section)
        {
          auto& allpass = mDispersion[tank][section];
          const double a = .15 + .5 * c[2] * .01;
          const double delayed = allpass.Read(mStretch[tank][section]);
          const double dispersed = delayed - a * wave;
          allpass.Push(wave + a * dispersed);
          wave = dispersed;
        }
        mTank[tank].Push(wave);
        wetLeft += pan[tank] * tap;
        wetRight += (1. - pan[tank]) * tap;
      }
      const double mid = .5 * (wetLeft + wetRight), side = .5 * (wetLeft - wetRight) * width;
      const double mix = c[3] * .01, output = mOutput(c[4] * .05, [](double x) { return std::pow(10., x); });
      left[i] = ((1. - mix) * dry[0] + mix * (mid + side)) * output;
      if (right)
        right[i] = ((1. - mix) * dry[1] + mix * (mid - side)) * output;
    }
  }

private:
  grid_fx::SmoothControls<5> mControls;
  grid_fx::SmoothControls<1> mTone, mStereo;
  grid_fx::SmoothControls<1> mPredelayControl;
  std::array<grid_fx::DelayLine, 2> mPredelay;
  std::array<grid_fx::DelayLine, 3> mTank;
  std::array<std::array<grid_fx::DelayLine, 16>, 3> mDispersion;
  std::array<std::array<double, 16>, 3> mStretch{};
  std::array<double, 2> mInputLow{};
  std::array<double, 3> mDamp{}, mFeedback{};
  grid_fx::Memo mDrive, mOutput;
  double mRate = 48000., mDamping = 0.;
  int mCounter = 0;
};
