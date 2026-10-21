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
      mLowRatio[ch] = std::pow(mGain[ch], 1. / .65 - 1.);
      mHighRatio[ch] = std::pow(mGain[ch], 1. / .35 - 1.);
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
  void Reset(double rate)
  {
    mRate = rate; mPhase = 0.; mLow = {}; mFeedback = {};
    for (int i = 0; i < 4; ++i) mInput[i].Reset(rate, kInput[i] * 2. + .002);
    for (int i = 0; i < 4; ++i) mTank[i].Reset(rate, kTank[i] * 2. + .002);
    for (int ch = 0; ch < 2; ++ch) mLong[ch].Reset(static_cast<int>(rate * (kLong[ch] * 2. + .004)));
  }
  void Configure(double size, double decay, double tone, double motion)
  {
    mSize = size; mCoefficient = Coefficient(tone, mRate); mMotion = motion;
    const double loop = (kLong[0] + kLong[1] + kTank[0] + kTank[1] + kTank[2] + kTank[3]) * size;
    mGain = std::pow(.001, loop * .5 / decay);
  }
  Stereo Tick(Stereo in)
  {
    double input = .5 * (in[0] + in[1]);
    for (int i = 0; i < 4; ++i) input = mInput[i].Tick(input, mRate * kInput[i] * mSize, i < 2 ? .75 : .625);
    Stereo out{};
    // Read both old tank tails before writing either side.
    const Stereo old{mLong[0].Read(mRate * kLong[0] * mSize), mLong[1].Read(mRate * kLong[1] * mSize)};
    for (int ch = 0; ch < 2; ++ch)
    {
      double x = input + mGain * LowPass(old[1-ch], mLow[ch], mCoefficient);
      const double wobble = mMotion * .0005 * mRate * std::sin(mPhase + ch * 1.5707963267948966);
      x = mTank[ch*2].Tick(x, mRate * kTank[ch*2] * mSize + wobble, -.7);
      x = mTank[ch*2+1].Tick(x, mRate * kTank[ch*2+1] * mSize, .5);
      out[ch] = .65 * mLong[ch].Read(mRate * kLong[ch] * mSize * .31)
                - .4 * mLong[1-ch].Read(mRate * kLong[1-ch] * mSize * .71) + .15 * x;
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
  double mRate = 48000., mSize = 1., mGain = 0., mCoefficient = 0., mMotion = 0., mPhase = 0.;
};

// Lush stereo allpass-loop reverb. The early/late arrangement is adapted from
// JPverb; bounds, rates, decay calibration and filtering are nam-grid specific.
class CloudTank
{
public:
  void Reset(double rate)
  {
    mRate = rate; mPhase = 0.; mFeedback = {}; mLow = {};
    constexpr std::array<Stereo, 4> early{{{.0059,.0343},{.0301,.0100},{.0144,.0262},{.0236,.0189}}};
    for (int i = 0; i < 4; ++i) mEarly[i].Reset(rate, early[i][0], early[i][1]);
    for (int i = 0; i < 10; ++i)
      mLate[i].Reset(rate, .0017 + .0023 * (i % 5), .0161 - .0021 * (i % 5), (i % 2 ? -1. : 1.) * .7853981633974483);
    for (int ch = 0; ch < 2; ++ch) mLong[ch].Reset(rate, .16);
  }
  void Configure(double size, double decay, double tone, double motion)
  {
    mSize = size; mCoefficient = Coefficient(tone, mRate); mMotion = motion;
    // Approximate allpass group delay plus the long recirculating paths.
    mGain = std::pow(.001, .14 * size / decay);
  }
  Stereo Tick(Stereo input)
  {
    Stereo wet = input;
    for (auto& stage : mEarly) wet = stage.Tick(wet, mSize, .65);
    const Stereo early = wet;
    for (int ch = 0; ch < 2; ++ch) wet[ch] = .3 * wet[ch] + mFeedback[ch] * mGain;
    for (auto& stage : mLate) wet = stage.Tick(wet, mSize, .65);
    for (int ch = 0; ch < 2; ++ch)
    {
      const double t = (.037 + ch * .013) * mSize + .0012 * mMotion * std::sin(mPhase + ch * 1.5707963267948966);
      const double delayed = mLong[ch].Read(mRate * t); mLong[ch].Push(wet[ch]);
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
  std::array<MovingDelay, 2> mLong;
  Stereo mFeedback{}, mLow{};
  double mRate = 48000., mSize = 1., mGain = 0., mCoefficient = 0., mMotion = 0., mPhase = 0.;
};
} // namespace grid_reverb
