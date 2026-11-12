#pragma once
#include "Antiderivative.h"
#include "MinimumPhaseOversampling.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

// Original MIT DSP for NAM Grid. Distortion-pedal voices with a shared two-stage
// structure: input coupling, optional pre-clip emphasis, two clipping stages with
// interstage filtering, then each style's own post-clip voicing. Design references
// (no circuit-model code copied): published BOSS DS-1, MT-2 and HM-2 manuals and
// circuit analyses (ElectroSmash, Electric Druid, PedalPCB forum), the MXR Distortion+
// and DOD 250 schematics, and Marshall Guv'nor/Shredmaster descriptions. Voices were
// voiced against measured responses of NAM pedal captures; they are inspired voices,
// not calibrated emulations. See docs/modules/distortion.md and ThirdPartyNotices.txt.
namespace grid_pedal_distortion
{
inline constexpr double pi = 3.14159265358979323846;
inline double Flush(double x) { return std::abs(x) < 1.e-30 ? 0. : x; }
using Oversampler4x = grid_oversampling::FIR<4>;

// Clipping curves with closed-form antiderivatives for first-order ADAA.
enum class Shape
{
  Soft,      // tanh: smooth op-amp/tube-like saturation
  Silicon,   // half tanh, half hard clip: a firm knee like silicon diodes to ground
  Germanium, // x / (1 + |x|): low threshold, very gradual knee
  Rail       // hard clip: op-amp rail or a second hard diode stage
};
inline double Curve(Shape shape, double x)
{
  switch (shape)
  {
    case Shape::Silicon: return .5 * (std::tanh(x) + std::clamp(x, -1., 1.));
    case Shape::Germanium: return x / (1. + std::abs(x));
    case Shape::Rail: return std::clamp(x, -1., 1.);
    default: return std::tanh(x);
  }
}
inline double HardIntegral(double x)
{ const double m = std::abs(x); return m <= 1. ? .5 * x * x : m - .5; }
inline double CurveIntegral(Shape shape, double x)
{
  switch (shape)
  {
    case Shape::Silicon: return .5 * (grid_adaa::LogCosh(x) + HardIntegral(x));
    case Shape::Germanium: { const double m = std::abs(x); return m - std::log1p(m); }
    case Shape::Rail: return HardIntegral(x);
    default: return grid_adaa::LogCosh(x);
  }
}

// A peaking, shelving or pass biquad (RBJ Audio EQ Cookbook equations, public
// formulas; implementation original). Transposed direct form II.
struct Biquad
{
  double b0 = 1., b1 = 0., b2 = 0., a1 = 0., a2 = 0., z1 = 0., z2 = 0.;
  double Tick(double x)
  {
    const double y = b0 * x + z1;
    z1 = Flush(b1 * x - a1 * y + z2);
    z2 = Flush(b2 * x - a2 * y);
    return y;
  }
  void Clear() { z1 = z2 = 0.; }
  std::complex<double> Response(double hz, double rate) const
  {
    const auto z1c = std::polar(1., -2. * pi * hz / rate), z2c = z1c * z1c;
    return (b0 + b1 * z1c + b2 * z2c) / (1. + a1 * z1c + a2 * z2c);
  }
  void Set(double nb0, double nb1, double nb2, double a0, double na1, double na2)
  { b0 = nb0 / a0; b1 = nb1 / a0; b2 = nb2 / a0; a1 = na1 / a0; a2 = na2 / a0; }
  void Identity() { b0 = 1.; b1 = b2 = a1 = a2 = 0.; }
  void Peak(double hz, double q, double db, double rate)
  {
    if (db == 0.) { Identity(); return; }
    const double A = std::pow(10., db / 40.), w = 2. * pi * std::min(hz, .45 * rate) / rate;
    const double alpha = std::sin(w) / (2. * q), c = std::cos(w);
    Set(1. + alpha * A, -2. * c, 1. - alpha * A, 1. + alpha / A, -2. * c, 1. - alpha / A);
  }
  void Shelf(double hz, double db, double rate, bool high)
  {
    if (db == 0.) { Identity(); return; }
    const double A = std::pow(10., db / 40.), w = 2. * pi * std::min(hz, .45 * rate) / rate;
    const double c = std::cos(w), alpha = std::sin(w) / 2. * std::sqrt(2.), root = 2. * std::sqrt(A) * alpha;
    if (high)
      Set(A * ((A + 1.) + (A - 1.) * c + root), -2. * A * ((A - 1.) + (A + 1.) * c), A * ((A + 1.) + (A - 1.) * c - root),
          (A + 1.) - (A - 1.) * c + root, 2. * ((A - 1.) - (A + 1.) * c), (A + 1.) - (A - 1.) * c - root);
    else
      Set(A * ((A + 1.) - (A - 1.) * c + root), 2. * A * ((A - 1.) - (A + 1.) * c), A * ((A + 1.) - (A - 1.) * c - root),
          (A + 1.) + (A - 1.) * c + root, -2. * ((A - 1.) + (A + 1.) * c), (A + 1.) + (A - 1.) * c - root);
  }
  void Pass(double hz, double q, double rate, bool high)
  {
    const double w = 2. * pi * std::min(hz, .45 * rate) / rate, alpha = std::sin(w) / (2. * q), c = std::cos(w);
    if (high) Set((1. + c) / 2., -(1. + c), (1. + c) / 2., 1. + alpha, -2. * c, 1. - alpha);
    else Set((1. - c) / 2., 1. - c, (1. - c) / 2., 1. + alpha, -2. * c, 1. - alpha);
  }
};

struct Band { double hz = 0., q = .7, db = 0.; };
// Every voice uses the same stages; the table sets their tuning. Gain splits its
// decibels between the two clipping stages. Frequencies in Hz; zero disables a filter.
struct Design
{
  double gainLow, gainHigh;  // Gain 0-100% maps linearly in dB onto this range
  double firstShare;         // share of the gain (in dB) applied before the first stage
  double inputHP; bool inputSecondOrder;
  Band emphasis;             // pre-clip peak
  Shape first; double firstBias, firstDrive;
  double interLP, interHP;
  Shape second; double secondBias;
  double clipLP;             // one-pole after clipping
  std::array<Band, 3> post;  // post-clip voicing peaks
  double postLP, postLPQ;    // second-order low-pass, 0 = none
  double output;             // linear output trim at the end of the voice
};

enum class Style
{
  ClassicDist = 0,
  RawDist = 1,
  BritishCrunch = 2,
  HotLead = 3,
  MetalStack = 4,
  Chainsaw = 5,
  Count
};
inline constexpr int styleCount = static_cast<int>(Style::Count);

// Tuning per style, informed by capture measurements in tmp/ and the published analyses.
inline constexpr std::array<Design, styleCount> designs{{
  // Classic Dist: soft-asymmetric transistor boost into silicon diodes to ground and a scooped tone.
  {20., 56., .45, 190., false, {}, Shape::Soft, .35, 1., 15000., 0., Shape::Silicon, .12, 12000.,
   {{{600., .7, -5.}, {}, {}}}, 0., .7, .073},
  // Raw Dist: bass-shy op-amp gain with bandwidth that falls as gain rises, into germanium diodes.
  {12., 62., 0., 30., false, {}, Shape::Soft, 0., 0., 0., 0., Shape::Germanium, .3, 16000.,
   {{}}, 0., .7, .138},
  // British Crunch: two preamp-like stages, interstage coupling and a darker amp-style top.
  {24., 54., .5, 70., false, {700., .8, 3.}, Shape::Soft, .18, 1., 2200., 120., Shape::Soft, .06, 0.,
   {{{650., .8, 2.}, {110., .7, 5.}, {}}}, 4500., .55, .041},
  // Hot Lead: tighter, higher-gain cascade with a mid hump and smooth top.
  {26., 60., .45, 220., true, {800., .8, 4.}, Shape::Soft, .05, 1., 3000., 150., Shape::Silicon, 0., 0.,
   {{{900., .9, 2.}, {}, {}}}, 6500., .7, .0487},
  // Metal Stack: pre-clip mid hump, two hard stages, then a double-peaked mid scoop.
  {20., 58., .4, 106., false, {1000., .5, 9.}, Shape::Soft, 0., 1., 7200., 48., Shape::Rail, 0., 10000.,
   {{{105., .9, 10.}, {4900., 1.6, 7.}, {1200., .6, -5.}}}, 0., .7, .0365},
  // Chainsaw: dense asymmetric drive, then the low and stagger-tuned upper-mid peaks at maximum.
  {22., 52., .5, 60., false, {}, Shape::Soft, .2, 1., 7000., 100., Shape::Silicon, .1, 9000.,
   {{{87., 1.2, 14.}, {960., 1.6, 12.}, {1300., 1.6, 12.}}}, 7000., .7, .0269},
}};

// One style's processing chain at the oversampled rate. Configure is called on
// preparation and whenever the Tight control moves; Tick allocates nothing.
class Voice
{
public:
  void Configure(int style, double rate, double tight)
  {
    mStyle = std::clamp(style, 0, styleCount - 1);
    const auto& d = designs[mStyle];
    const double factor = std::pow(2., 1.3 * std::clamp(tight, -1., 1.));
    const auto pole = [&](double hz) { return hz <= 0. ? 0. : std::exp(-2. * pi * std::min(hz, .45 * rate) / rate); };
    mInputPole = pole(d.inputHP * factor);
    mInterLP = pole(d.interLP); mInterHP = pole(d.interHP);
    mClipLP = pole(d.clipLP);
    mDC = pole(5.);
    mEmphasis.Peak(d.emphasis.hz, d.emphasis.q, d.emphasis.db, rate);
    // Only the configured peaks run; the table lists them first.
    mPostCount = 0;
    for (const auto& band : d.post)
      if (band.db != 0.) mPost[mPostCount++].Peak(band.hz, band.q, band.db, rate);
    if (d.postLP > 0.) mLowPass.Pass(d.postLP, d.postLPQ, rate, false); else mLowPass.Identity();
    mRate = rate;
    // Gain-dependent coefficients are recalculated on the next Tick.
    mBandwidthGain = mSplitGain = -1.;
  }
  // gain is the linear Gain from the selected style's range.
  double Tick(double x, double gain)
  {
    const auto& d = designs[mStyle];
    // Input coupling (and Tight): one or two first-order high-pass sections.
    x -= mInput1 = Flush(x + mInputPole * (mInput1 - x));
    if (d.inputSecondOrder) x -= mInput2 = Flush(x + mInputPole * (mInput2 - x));
    if (d.emphasis.db != 0.) x = mEmphasis.Tick(x);
    double y;
    if (mStyle == static_cast<int>(Style::RawDist))
    {
      // Gain applies above a 720 Hz corner (the gain leg's capacitor); bass sees unity.
      if (gain != mBandwidthGain)
      {
        mBandwidthGain = gain;
        mGBW = std::exp(-2. * pi * std::min(std::max(3.e6 / gain, 5000.), .45 * mRate) / mRate);
        mGainHP = std::exp(-2. * pi * 720. / mRate);
      }
      mGainLow = Flush(x + mGainHP * (mGainLow - x));
      const double opamp = x + (gain - 1.) * (x - mGainLow);
      mBandwidth = Flush(opamp + mGBW * (mBandwidth - opamp));
      // The op-amp swings beyond the diodes; its rails only matter far above them.
      y = mSecond.Tick(mBandwidth * .9 + d.secondBias, [](double v) { return Curve(Shape::Germanium, v); },
                       [](double v) { return CurveIntegral(Shape::Germanium, v); });
    }
    else
    {
      if (gain != mSplitGain)
      {
        // Split the gain's decibels between the stages only when Gain moves.
        mSplitGain = gain;
        mFirstGain = d.firstDrive * std::pow(std::max(gain, 1.e-6), d.firstShare);
        mSecondGain = gain / std::pow(std::max(gain, 1.e-6), d.firstShare);
      }
      const double firstIn = mFirstGain * x + d.firstBias;
      double s = mFirst.Tick(firstIn, [&](double v) { return Curve(d.first, v); },
                             [&](double v) { return CurveIntegral(d.first, v); });
      s -= Curve(d.first, d.firstBias);
      if (mInterLP > 0.) s = mInter1 = Flush(s + mInterLP * (mInter1 - s));
      if (mInterHP > 0.) s -= mInter2 = Flush(s + mInterHP * (mInter2 - s));
      const double secondIn = mSecondGain * s + d.secondBias;
      y = mSecond.Tick(secondIn, [&](double v) { return Curve(d.second, v); },
                       [&](double v) { return CurveIntegral(d.second, v); });
      y -= Curve(d.second, d.secondBias);
    }
    if (mClipLP > 0.) y = mClip = Flush(y + mClipLP * (mClip - y));
    // Remove the signal-dependent DC that asymmetric clipping creates.
    const double blocked = mBlock = Flush(y - mPrevious + mDC * mBlock);
    mPrevious = y;
    double v = blocked;
    for (int i = 0; i < mPostCount; ++i) v = mPost[i].Tick(v);
    if (d.postLP > 0.) v = mLowPass.Tick(v);
    return v * d.output;
  }
  int Style() const { return mStyle; }

private:
  int mStyle = 0, mPostCount = 0;
  double mRate = 192000.;
  double mInputPole = 0., mInterLP = 0., mInterHP = 0., mClipLP = 0., mDC = 0.;
  double mInput1 = 0., mInput2 = 0., mInter1 = 0., mInter2 = 0., mClip = 0., mBlock = 0., mPrevious = 0.;
  double mBandwidthGain = -1., mGBW = 0., mGainHP = 0., mGainLow = 0., mBandwidth = 0.;
  double mSplitGain = -1., mFirstGain = 1., mSecondGain = 1.;
  Biquad mEmphasis, mLowPass;
  std::array<Biquad, 3> mPost{};
  grid_adaa::Average mFirst, mSecond;
};

inline double GainFor(int style, double percent)
{
  const auto& d = designs[std::clamp(style, 0, styleCount - 1)];
  return std::pow(10., (d.gainLow + (d.gainHigh - d.gainLow) * std::clamp(percent, 0., 100.) * .01) / 20.);
}
} // namespace grid_pedal_distortion
