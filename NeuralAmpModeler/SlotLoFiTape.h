#pragma once
#include <cstdint>
#include "SlotTimeEffects.h"
#include "dsp/PedalDrive.h"

struct LoFiTapeHistory
{
  // 2.56 seconds of measured read-head pitch motion, in cents, at 100 Hz.
  static constexpr int points = 256;
  std::array<float, points> wow{}, flutter{};
  int next = 0;
  uint64_t sequence = 0;
};

// Original NAM Grid transport/degradation with the attributed Airwindows Tape
// Spiral shaper and the shared Pedal Drive 4x antialiasing stage. The complete
// Airwindows tape core is not used: head bump/softening remain a separate module.
// A shared transport and dropout envelope preserve stereo placement; audio and
// hiss have independent channel histories. All memory is allocated in Reset.
class SlotLoFiTape
{
public:
  void Reset(double rate)
  {
    mRate = std::max(1., rate);
    for (auto& line : mLine) line.Reset(static_cast<int>(std::ceil(mRate * .04)) + 8);
    mControls.Reset(mRate); mDriveControl.Reset(mRate);
    for (auto& shaper : mSaturator) shaper.Reset();
    mLow = {}; mLow2 = {}; mDC = {}; mNoiseLow = {};
    mWowPhase = mFlutterPhase = mDrift = mDriftTarget = 0.;
    mDriftFrames = 0;
    mDropPhase = 1.; mDropStep = mDropDepth = 0.;
    mDropWait = static_cast<int>(mRate * .3);
    mTransportSeed = 0x6d2b79f5u;
    mNoiseSeed = {0x1234567u, 0x9abcdefu};
    mDriftCoefficient = std::exp(-1. / (.4 * mRate));
    mDCCoefficient = std::exp(-2. * grid_fx::pi * 18. / mRate);
    mNoiseCoefficient = std::exp(-2. * grid_fx::pi * std::min(1200., .2 * mRate) / mRate);
    mCounter = 0;
    mHistory = {}; mHistoryClock = mPreviousWow = mPreviousFlutter = 0.;
  }

  const LoFiTapeHistory& History() const { return mHistory; } // audio thread only

  void Process(double* left, double* right, int frames, const UtilityControls& controls, bool draining = false)
  {
    mControls.Target(controls.lofi, uLoFiSpeed);
    mDriveControl.Target({controls.extra.lofiDrive}, uLoFiDrive);
    for (int i = 0; i < frames; ++i)
    {
      const auto& v = mControls.Tick();
      const double wear = v[4] * .01;
      const double driveDB = mDriveControl.Tick()[0];
      const double drive = mDriveGain(driveDB * .05, [](double x) { return std::pow(10., x); });
      const double saturationMix = std::min(1., driveDB / 3.);
      if (mCounter == 0)
      {
        // Speed changes bandwidth and transport rates, never the average pitch.
        const double speed = v[0] / 7.5;
        const double transport = std::sqrt(speed);
        mWowIncrement = .43 * transport / mRate;
        mFlutterIncrement = 6.7 * transport / mRate;
        const double cutoff = std::min(.45 * mRate, std::clamp(7200. * std::pow(speed, .7)
                                                     * (1. - .7 * wear), 900., 18000.));
        mLowCoefficient = std::exp(-2. * grid_fx::pi * cutoff / mRate);
        mNoiseGain = .03 * std::pow(v[3] * .01, 2.);
        mOutput = std::pow(10., v[6] / 20.);
      }
      mCounter = (mCounter + 1) % 16;
      if (--mDriftFrames <= 0)
      {
        mDriftTarget = Random(mTransportSeed);
        mDriftFrames = std::max(1, static_cast<int>(mRate * (.15 + .35 * (.5 + .5 * Random(mTransportSeed)))));
      }
      mDrift = mDriftTarget + mDriftCoefficient * (mDrift - mDriftTarget);
      mWowPhase += mWowIncrement; mWowPhase -= std::floor(mWowPhase);
      mFlutterPhase += mFlutterIncrement; mFlutterPhase -= std::floor(mFlutterPhase);
      const double wow = std::sin(2. * grid_fx::pi * mWowPhase);
      const double flutter = .8 * std::sin(2. * grid_fx::pi * mFlutterPhase)
                             + .2 * std::sin(4. * grid_fx::pi * mFlutterPhase);
      // 10 ms carrier delay keeps the modulated read head causal at all settings.
      const double wowDelay = .005 * v[1] * .01 * (.85 * wow + .15 * mDrift);
      const double flutterDelay = .00035 * v[2] * .01 * flutter;
      const double delay = mRate * (.01 + wowDelay + flutterDelay);
      const double wowVelocity = 1. - mRate * (wowDelay - mPreviousWow);
      const double flutterVelocity = 1. - mRate * (flutterDelay - mPreviousFlutter);
      mPreviousWow = wowDelay; mPreviousFlutter = flutterDelay;
      mHistoryClock += 100. / mRate;
      if (mHistoryClock >= 1.)
      {
        mHistoryClock -= 1.;
        mHistory.wow[mHistory.next] = static_cast<float>(1200. * std::log2(std::clamp(wowVelocity, .25, 4.)));
        mHistory.flutter[mHistory.next] = static_cast<float>(1200. * std::log2(std::clamp(flutterVelocity, .25, 4.)));
        mHistory.next = (mHistory.next + 1) % LoFiTapeHistory::points;
        ++mHistory.sequence;
      }
      if (mDropPhase >= 1. && --mDropWait <= 0)
      {
        const double random = .5 + .5 * Random(mTransportSeed);
        mDropPhase = 0.;
        mDropStep = 1. / (mRate * (.012 + .12 * random));
        mDropDepth = .9 * (.4 + .6 * random);
        mDropWait = std::max(1, static_cast<int>(mRate * (.08 + .65 * (.5 + .5 * Random(mTransportSeed)))));
      }
      const double contact = mDropPhase < 1. ? .5 - .5 * std::cos(2. * grid_fx::pi * mDropPhase) : 0.;
      mDropPhase = std::min(1., mDropPhase + mDropStep);
      const double dropout = 1. - wear * wear * mDropDepth * contact;
      const double mix = v[5] * .01;
      for (int ch = 0; ch < 2; ++ch)
      {
        double* samples = ch ? right : left;
        const double dry = samples ? samples[i] : 0.;
        // Advance both channels even in mono, keeping transport/noise deterministic
        // when hosts change channel layouts. Draining must not regenerate hiss.
        const double noise = Random(mNoiseSeed[ch]);
        mNoiseLow[ch] = grid_fx::Flush(noise + mNoiseCoefficient * (mNoiseLow[ch] - noise));
        const double saturated = driveDB == 0. ? dry : mSaturator[ch].Tick(dry, [&](double x) { return AirwindowsTape::Spiral(x * drive) / drive; });
        const double input = saturationMix == 0. ? dry : dry + saturationMix * (saturated - dry);
        mDC[ch] = grid_fx::Flush(input + mDCCoefficient * (mDC[ch] - input));
        mLine[ch].Push(input - mDC[ch]);
        const double signal = mLine[ch].Read(delay) * dropout
                              + (draining ? 0. : mNoiseGain * (noise - mNoiseLow[ch]));
        mLow[ch] = grid_fx::Flush(signal + mLowCoefficient * (mLow[ch] - signal));
        mLow2[ch] = grid_fx::Flush(mLow[ch] + mLowCoefficient * (mLow2[ch] - mLow[ch]));
        if (samples) samples[i] = (dry + mix * (mLow2[ch] - dry)) * mOutput;
      }
    }
  }

private:
  static double Random(uint32_t& state)
  {
    state ^= state << 13; state ^= state >> 17; state ^= state << 5;
    return static_cast<double>(state) * (2. / 4294967295.) - 1.;
  }
  std::array<grid_fx::DelayLine, 2> mLine;
  grid_fx::SmoothControls<7> mControls;
  grid_fx::SmoothControls<1> mDriveControl;
  grid_fx::Memo mDriveGain;
  std::array<grid_pedal::Oversampler4x, 2> mSaturator;
  std::array<double, 2> mLow{}, mLow2{}, mDC{}, mNoiseLow{};
  std::array<uint32_t, 2> mNoiseSeed{};
  uint32_t mTransportSeed = 1;
  double mRate = 48000., mWowPhase = 0., mFlutterPhase = 0.;
  double mDrift = 0., mDriftTarget = 0., mDriftCoefficient = 0., mDCCoefficient = 0., mNoiseCoefficient = 0.;
  double mDropPhase = 1., mDropStep = 0., mDropDepth = 0.;
  double mWowIncrement = 0., mFlutterIncrement = 0.;
  double mLowCoefficient = 0., mNoiseGain = 0., mOutput = 1.;
  LoFiTapeHistory mHistory;
  double mHistoryClock = 0., mPreviousWow = 0., mPreviousFlutter = 0.;
  int mDriftFrames = 0, mDropWait = 0, mCounter = 0;
};
