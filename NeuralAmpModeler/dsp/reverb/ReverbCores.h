#pragma once
#include "ReverbPrimitives.h"

// Rate-scaled double-precision adaptations of the Signalsmith Basics FDN,
// Emilie Gillet's Clouds plate topology, and Julian Parker's stereo diffusers.
// See README.md and LICENSE.txt in this directory for revisions and notices.
namespace grid_reverb
{
// Four multichannel diffusion banks around an eight-line Householder FDN.
// The structure follows Signalsmith; stereo mapping, normalized matrices,
// band-decay calibration and smooth delay lifecycle are adapted for nam-grid.
class DiffusionFDN
{
public:
  // Decay multipliers below 200 Hz and above the Damping frequency. Shared with
  // the editor's per-band decay guide.
  static constexpr double kLowDecayRatio = .65, kHighDecayRatio = .35;
  void Reset(double rate, bool hall)
  {
    mRate = rate; mHall = hall; mLoopSeconds = hall ? .11 : .055;
    constexpr std::array<double, 5> bankScale{.125, 1., .5, .25, .25};
    uint32_t seed = 0x6dd09ee5u;
    for (int bank = 0; bank < 5; ++bank)
      for (int ch = 0; ch < 8; ++ch)
      {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        const double jitter = double(seed & 65535) / 65536.;
        mDiffusionTime[bank][ch] = mLoopSeconds * bankScale[bank] * (.5 + ch + jitter) / 8.;
        mDiffusion[bank][ch].Reset(rate, mDiffusionTime[bank][ch] * 2. + .005);
      }
    for (int ch = 0; ch < 8; ++ch)
    {
      mLoopTime[ch] = mLoopSeconds * std::exp2((ch % 2 ? -1. : 1.) * (.0625 + .125 * (ch / 2)));
      mLoop[ch].Reset(rate, mLoopTime[ch] * 2. + .004);
    }
    mBass = {}; mTreble = {}; mPhase = 0.;
  }
  void Configure(double size, double decay, double tone, double motion)
  {
    mSize = size; mMotion = motion * .0007 * mRate;
    mLowCoefficient = Coefficient(200., mRate); mHighCoefficient = Coefficient(tone, mRate);
    for (int ch = 0; ch < 8; ++ch)
    {
      mGain[ch] = std::pow(.001, mLoopTime[ch] * size / decay);
      mLowRatio[ch] = std::pow(mGain[ch], 1. / kLowDecayRatio - 1.);
      mHighRatio[ch] = std::pow(mGain[ch], 1. / kHighDecayRatio - 1.);
    }
  }
  Stereo Tick(Stereo in)
  {
    Field x{};
    for (int ch = 0; ch < 8; ++ch) x[ch] = in[ch % 2] * (ch & 2 ? -.5 : .5);
    x = Diffuse(x, 0); x = Diffuse(x, 1);
    Field feedback{}; double sum = 0.;
    for (int ch = 0; ch < 8; ++ch)
    {
      const double motion = ch < 3 ? mMotion * (.5 + .5 * std::sin(mPhase + ch * 2.0943951023931953)) : 0.;
      feedback[ch] = mLoop[ch].Read(mRate * mLoopTime[ch] * mSize + motion); sum += feedback[ch];
    }
    // Orthogonal Householder, followed by contractive low/high shelves.
    for (int ch = 0; ch < 8; ++ch)
    {
      double y = feedback[ch] - .25 * sum;
      y += (mLowRatio[ch] - 1.) * LowPass(y, mBass[ch], mLowCoefficient);
      y = mHighRatio[ch] * y + (1. - mHighRatio[ch]) * LowPass(y, mTreble[ch], mHighCoefficient);
      feedback[ch] = y;
      mLoop[(ch + 3) % 8].Push(.35 * x[ch] + y * mGain[ch]);
    }
    const auto early = Diffuse(x, 4);
    for (int ch = 0; ch < 8; ++ch) x[ch] = feedback[ch] + early[ch] * (mHall ? .12 : .35);
    x = Diffuse(x, 2); x = Diffuse(x, 3);
    Stereo out{};
    for (int ch = 0; ch < 8; ++ch) out[ch % 2] += x[ch] * (ch & 2 ? -1. : 1.);
    mPhase += .23 * 2. * grid_fx::pi / mRate;
    if (mPhase >= 2. * grid_fx::pi) mPhase -= 2. * grid_fx::pi;
    return out;
  }
  bool Clipped() const
  {
    for (const auto& line : mLoop) if (line.Clipped()) return true;
    for (const auto& bank : mDiffusion) for (const auto& line : bank) if (line.Clipped()) return true;
    return false;
  }
private:
  Field Diffuse(Field x, int bank)
  {
    for (int ch = 0; ch < 8; ++ch)
    {
      const double delayed = mDiffusion[bank][ch].Read(mRate * mDiffusionTime[bank][ch] * mSize);
      mDiffusion[bank][ch].Push(x[ch]); x[ch] = delayed;
    }
    Hadamard(x); return x;
  }
  std::array<std::array<MovingDelay, 8>, 5> mDiffusion;
  std::array<std::array<double, 8>, 5> mDiffusionTime{};
  std::array<MovingDelay, 8> mLoop;
  Field mLoopTime{}, mGain{}, mLowRatio{}, mHighRatio{}, mBass{}, mTreble{};
  double mRate = 48000., mLoopSeconds = .055, mSize = 1., mMotion = 0., mPhase = 0.;
  double mLowCoefficient = 0., mHighCoefficient = 0.;
  bool mHall = false;
};

// Griesinger/Dattorro two-branch tank, informed by the MIT Clouds implementation.
// Floating delay storage replaces Clouds' 12-bit, 32 kHz effects engine.
class PlateTank
{
public:
  // Nominal travel time through one branch, used for the damping display guide.
  static constexpr double PassSeconds(double size)
  { return (kLong[0] + kLong[1] + kTank[0] + kTank[1] + kTank[2] + kTank[3]) * size * .5; }
  void Reset(double rate)
  {
    mRate = rate; mPhase = 0.; mLow = {}; mFeedback = {}; mLoss = -1.;
    for (int i = 0; i < 4; ++i) mInput[i].Reset(rate, kInput[i] * 2. + .002);
    for (int i = 0; i < 4; ++i) mTank[i].Reset(rate, kTank[i] * 2. + .002);
    for (int ch = 0; ch < 2; ++ch) mLong[ch].Reset(static_cast<int>(rate * (kLong[ch] * 2. + .004)));
  }
  void Configure(double size, double decay, double tone, double motion)
  {
    mSize = size; mCoefficient = Coefficient(tone, mRate); mMotion = motion;
    // A loop-only gain leaves the long tank allpasses ringing beyond Decay.
    // Distribute the same exponential loss over every stored delay instead:
    // a path lasting t seconds loses 60*t/decay dB, even inside an allpass.
    const double loss = size / decay;
    if (loss != mLoss)
    {
      mLoss = loss;
      for (int i = 0; i < 4; ++i)
      {
        mInputGain[i] = std::pow(.001, kInput[i] * loss);
        mTankGain[i] = std::pow(.001, kTank[i] * loss);
      }
      for (int ch = 0; ch < 2; ++ch)
      {
        mLongGain[ch] = std::pow(.001, kLong[ch] * loss);
        mTapGain[ch][0] = std::pow(.001, kLong[ch] * loss * .31);
        mTapGain[ch][1] = std::pow(.001, kLong[ch] * loss * .71);
      }
    }
  }
  Stereo Tick(Stereo in)
  {
    double input = .5 * (in[0] + in[1]);
    for (int i = 0; i < 4; ++i)
      input = mInput[i].Tick(input, mRate * kInput[i] * mSize, i < 2 ? .75 : .625, mInputGain[i]);
    Stereo out{};
    // Read both old tank tails before writing either side.
    const Stereo old{mLongGain[0] * mLong[0].Read(mRate * kLong[0] * mSize),
                     mLongGain[1] * mLong[1].Read(mRate * kLong[1] * mSize)};
    for (int ch = 0; ch < 2; ++ch)
    {
      double x = input + LowPass(old[1-ch], mLow[ch], mCoefficient);
      const double wobble = mMotion * .0005 * mRate * std::sin(mPhase + ch * 1.5707963267948966);
      x = mTank[ch*2].Tick(x, mRate * kTank[ch*2] * mSize + wobble, -.7, mTankGain[ch*2]);
      x = mTank[ch*2+1].Tick(x, mRate * kTank[ch*2+1] * mSize, .5, mTankGain[ch*2+1]);
      out[ch] = .65 * mTapGain[ch][0] * mLong[ch].Read(mRate * kLong[ch] * mSize * .31)
                - .4 * mTapGain[1-ch][1] * mLong[1-ch].Read(mRate * kLong[1-ch] * mSize * .71) + .15 * x;
      mLong[ch].Push(x);
    }
    mPhase += 2. * grid_fx::pi * .3 / mRate;
    if (mPhase >= 2. * grid_fx::pi) mPhase -= 2. * grid_fx::pi;
    return out;
  }
  bool Clipped() const
  { for (const auto& a : mInput) if (a.Clipped()) return true; for (const auto& a : mTank) if (a.Clipped()) return true; return false; }
private:
  inline static constexpr std::array<double, 4> kInput{113./32000., 162./32000., 241./32000., 399./32000.};
  inline static constexpr std::array<double, 4> kTank{1653./32000., 2038./32000., 1913./32000., 1663./32000.};
  inline static constexpr Stereo kLong{3411./32000., 4782./32000.};
  std::array<Allpass, 4> mInput, mTank;
  std::array<grid_fx::DelayLine, 2> mLong;
  Stereo mLow{}, mFeedback{};
  std::array<double, 4> mTankGain{}, mInputGain{};
  Stereo mLongGain{};
  std::array<Stereo, 2> mTapGain{};
  double mRate = 48000., mSize = 1., mCoefficient = 0., mMotion = 0., mPhase = 0., mLoss = -1.;
};

// Lush stereo allpass-loop reverb. The early/late arrangement is adapted from
// JPverb; bounds, rates, decay calibration and filtering are NAM Grid specific.
class CloudTank
{
public:
  // Nominal loop travel time, used for the damping display guide.
  static constexpr double PassSeconds(double size) { return .14 * size; }
  void Reset(double rate)
  {
    mRate = rate; mPhase = 0.; mFeedback = {}; mLow = {}; mConfigured = {-1., -1.};
    constexpr std::array<Stereo, 4> early{{{.0059,.0343},{.0301,.0100},{.0144,.0262},{.0236,.0189}}};
    for (int i = 0; i < 4; ++i) mEarly[i].Reset(rate, early[i][0], early[i][1]);
    for (int i = 0; i < 10; ++i)
      mLate[i].Reset(rate, .0017 + .0023 * (i % 5), .0161 - .0021 * (i % 5), (i % 2 ? -1. : 1.) * .7853981633974483);
    for (int ch = 0; ch < 2; ++ch) mLong[ch].Reset(rate, .16);
  }
  void Configure(double size, double decay, double tone, double motion)
  {
    mSize = size; mCoefficient = Coefficient(tone, mRate); mMotion = motion;
    // A loop-only gain let the late allpasses ring past short Decay settings, while
    // interpolated reads low-passed every pass and cut long tails short. Every delay
    // instead loses 60 dB per Decay for the time a sample spends in it, so the whole
    // tank decays at the set rate however energy circulates.
    if (size == mConfigured[0] && decay == mConfigured[1]) return;
    mConfigured = {size, decay};
    const auto loss = [&](double samples) { return std::pow(.001, samples / (mRate * decay)); };
    const auto prepare = [&](const StereoAllpass& stage, Stereo& samples, Stereo& gain) {
      samples = stage.Samples(size);
      gain = {loss(samples[0]), loss(samples[1])};
    };
    for (int i = 0; i < 4; ++i) prepare(mEarly[i], mEarlySamples[i], mEarlyGain[i]);
    for (int i = 0; i < 10; ++i) prepare(mLate[i], mLateSamples[i], mLateGain[i]);
    for (int ch = 0; ch < 2; ++ch) mLongGain[ch] = loss(mRate * LongSeconds(ch, size));
  }
  Stereo Tick(Stereo input)
  {
    Stereo wet = input;
    for (int i = 0; i < 4; ++i) wet = mEarly[i].Tick(wet, mEarlySamples[i], mEarlyGain[i], .65);
    const Stereo early = wet;
    for (int ch = 0; ch < 2; ++ch) wet[ch] = .3 * wet[ch] + mFeedback[ch];
    for (int i = 0; i < 10; ++i) wet = mLate[i].Tick(wet, mLateSamples[i], mLateGain[i], .65);
    for (int ch = 0; ch < 2; ++ch)
    {
      // Motion moves whole-sample taps; MovingDelay crossfades between them.
      const double t = LongSeconds(ch, mSize) + .0012 * mMotion * std::sin(mPhase + ch * 1.5707963267948966);
      const double delayed = mLongGain[ch] * mLong[ch].Read(std::round(mRate * t)); mLong[ch].Push(wet[ch]);
      mFeedback[ch] = LowPass(delayed, mLow[ch], mCoefficient);
      wet[ch] = .65 * mFeedback[ch] + .15 * early[ch];
    }
    mPhase += 2. * grid_fx::pi * .19 / mRate;
    if (mPhase >= 2. * grid_fx::pi) mPhase -= 2. * grid_fx::pi;
    return wet;
  }
  bool Clipped() const
  {
    for (const auto& a : mEarly) if (a.Clipped()) return true;
    for (const auto& a : mLate) if (a.Clipped()) return true;
    return mLong[0].Clipped() || mLong[1].Clipped();
  }
private:
  std::array<StereoAllpass, 4> mEarly;
  std::array<StereoAllpass, 10> mLate;
  static constexpr double LongSeconds(int ch, double size) { return (.037 + ch * .013) * size; }
  std::array<MovingDelay, 2> mLong;
  std::array<Stereo, 4> mEarlySamples{}, mEarlyGain{};
  std::array<Stereo, 10> mLateSamples{}, mLateGain{};
  Stereo mFeedback{}, mLow{}, mLongGain{}, mConfigured{-1., -1.};
  double mRate = 48000., mSize = 1., mCoefficient = 0., mMotion = 0., mPhase = 0.;
};
} // namespace grid_reverb
