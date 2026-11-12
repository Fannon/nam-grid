#pragma once
#include "MinimumPhaseOversampling.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

// Original MIT DSP for NAM Grid. Distortion-pedal voices with a shared two-stage
// structure: input coupling, optional pre-clip emphasis, two clipping stages with
// interstage filtering and a level-following bias shift, then each style's own
// post-clip voicing. Design references (no circuit-model code copied): published
// BOSS DS-1, MT-2 and HM-2 manuals and circuit analyses (ElectroSmash, Electric
// Druid, PedalPCB forum), the MXR Distortion+ and DOD 250 schematics, and Marshall
// Guv'nor/Shredmaster descriptions. Voices were voiced against measured responses
// of NAM pedal captures; they are inspired voices, not calibrated emulations.
// See docs/modules/distortion.md and ThirdPartyNotices.txt.
namespace grid_pedal_distortion
{
inline constexpr double pi = 3.14159265358979323846;
inline double Flush(double x) { return std::abs(x) < 1.e-30 ? 0. : x; }
using Oversampler4x = grid_oversampling::FIR<4>;

// Clipping curves for first-order antiderivative antialiasing (Parker, Zavalishin and
// Le Bivic, DAFx 2016). Every curve has unity slope at zero and a cheap closed-form
// antiderivative that is zero at zero: these run eight times per host sample, where
// tanh's log-cosh antiderivative cost about a third of the whole module.
enum class Shape
{
  Soft,      // x / sqrt(1 + x^2): smooth op-amp/transistor saturation
  Silicon,   // half Soft, half hard clip: a firm knee like silicon diodes to ground
  Germanium, // x / (1 + |x|): low threshold, very gradual knee
  Rail       // hard clip: op-amp rail or a second hard diode stage
};
inline double HardIntegral(double x)
{ const double m = std::abs(x); return m <= 1. ? .5 * x * x : m - .5; }
inline double Curve(Shape shape, double x)
{
  switch (shape)
  {
    case Shape::Silicon: return .5 * (x / std::sqrt(1. + x * x) + std::clamp(x, -1., 1.));
    case Shape::Germanium: return x / (1. + std::abs(x));
    case Shape::Rail: return std::clamp(x, -1., 1.);
    default: return x / std::sqrt(1. + x * x);
  }
}
inline double CurveIntegral(Shape shape, double x)
{
  switch (shape)
  {
    case Shape::Silicon: return .5 * (std::sqrt(1. + x * x) - 1. + HardIntegral(x));
    case Shape::Germanium: { const double m = std::abs(x); return m - std::log1p(m); }
    case Shape::Rail: return HardIntegral(x);
    default: return std::sqrt(1. + x * x) - 1.;
  }
}
// Averages a curve over the line between adjacent oversampled inputs. The previous
// antiderivative is always kept, so each changing sample costs one evaluation; nearly
// equal inputs use the curve at their midpoint instead of a cancelling difference. The
// first sample starts at its own input, so a biased stage is silent from a reset, and
// an unchanged input (silence) reuses its value without evaluating anything.
struct Clipper
{
  double previous = 0., integral = 0., held = 0.;
  bool primed = false, holding = false;
  double Tick(Shape shape, double x)
  {
    if (x == previous && primed)
    {
      if (!holding) { held = Curve(shape, x); holding = true; }
      return held;
    }
    holding = false;
    const double current = CurveIntegral(shape, x);
    if (!primed) { primed = true; previous = x; integral = current; }
    const double delta = x - previous;
    const double scale = std::max(1., std::max(std::abs(x), std::abs(previous)));
    const double y = std::abs(delta) > 1.e-5 * scale ? (current - integral) / delta : Curve(shape, .5 * (x + previous));
    previous = x;
    integral = current;
    return y;
  }
};

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
  Shape first; double firstBias, firstDrive, firstTrack;
  double interLP, interHP;
  Shape second; double secondBias, secondTrack;
                             // Track: bias added per unit of that stage's overdrive (see Core)
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
  {20., 56., .45, 190., false, {}, Shape::Soft, .35, 1., .3, 15000., 0., Shape::Silicon, .12, 1., 12000.,
   {{{600., .7, -5.}, {}, {}}}, 0., .7, .0766},
  // Raw Dist: op-amp gain whose bass corner and bandwidth move with Gain, into germanium diodes.
  {12., 62., 0., 30., false, {}, Shape::Soft, 0., 0., 0., 0., 0., Shape::Germanium, .15, .7, 16000.,
   {{}}, 0., .7, .0933},
  // British Crunch: two preamp-like stages, interstage coupling and a darker amp-style top.
  {24., 54., .5, 70., false, {700., .8, 3.}, Shape::Soft, .18, 1., .1, 2200., 120., Shape::Soft, .06, .1, 0.,
   {{{650., .8, 2.}, {110., .7, 5.}, {}}}, 4500., .55, .0434},
  // Hot Lead: tighter, higher-gain cascade with a mid hump and smooth top.
  {26., 60., .45, 220., true, {800., .8, 4.}, Shape::Soft, .05, 1., 0., 3000., 150., Shape::Silicon, 0., 0., 0.,
   {{{900., .9, 2.}, {}, {}}}, 6500., .7, .0499},
  // Metal Stack: pre-clip mid hump, two hard stages, then a double-peaked mid scoop.
  {20., 58., .4, 106., false, {1000., .5, 9.}, Shape::Soft, 0., 1., 0., 7200., 48., Shape::Rail, 0., 0., 10000.,
   {{{105., .9, 10.}, {4900., 1.6, 7.}, {1200., .6, -5.}}}, 0., .7, .0365},
  // Chainsaw: dense asymmetric drive, then the low and stagger-tuned upper-mid peaks at maximum.
  {22., 52., .5, 60., false, {}, Shape::Soft, .2, 1., .1, 7000., 100., Shape::Silicon, .1, .15, 9000.,
   {{{87., 1.2, 14.}, {960., 1.6, 12.}, {1300., 1.6, 12.}}}, 7000., .7, .0281},
}};

// The oversampled half of a style: everything up to and including the clipping.
// Configure runs in preparation; SetTight and SetGain run when those controls move.
// Tick allocates nothing.
class Core
{
public:
  void Configure(int style, double rate)
  {
    mStyle = std::clamp(style, 0, styleCount - 1);
    mRate = rate;
    const auto& d = designs[mStyle];
    mRaw = mStyle == static_cast<int>(Style::RawDist);
    mInterLP = Pole(d.interLP); mInterHP = Pole(d.interHP);
    mClipLP = Pole(d.clipLP);
    mDC = Pole(5.);
    mTrackPole = std::exp(-1. / (.01 * rate));
    mEmphasis.Peak(d.emphasis.hz, d.emphasis.q, d.emphasis.db, rate);
    mFirstOffset = Curve(d.first, d.firstBias);
    mSecondOffset = Curve(d.second, d.secondBias);
    const double gain = mGain;
    mTight = 2.; mGain = -1.; // force both updates
    SetTight(0.);
    SetGain(gain);
  }
  // Tight scales the style's bass cut before clipping by up to 2.5 times either way.
  void SetTight(double tight)
  {
    tight = std::clamp(tight, -1., 1.);
    if (tight == mTight) return;
    mTight = tight;
    mTightFactor = std::pow(2., 1.3 * tight);
    mInputPole = Pole(designs[mStyle].inputHP * mTightFactor);
    UpdateRaw();
  }
  // gain is the linear Gain from the selected style's range (GainFor).
  void SetGain(double gain)
  {
    gain = std::max(gain, 1.e-6);
    if (gain == mGain) return;
    mGain = gain;
    const auto& d = designs[mStyle];
    const double first = std::pow(gain, d.firstShare);
    mFirstGain = d.firstDrive * first;
    mSecondGain = gain / first;
    UpdateRaw();
  }
  double Tick(double x)
  {
    const auto& d = designs[mStyle];
    // Input coupling (and Tight): one or two first-order high-pass sections.
    x -= mInput1 = Flush(x + mInputPole * (mInput1 - x));
    if (d.inputSecondOrder) x -= mInput2 = Flush(x + mInputPole * (mInput2 - x));
    if (d.emphasis.db != 0.) x = mEmphasis.Tick(x);
    double drive; // the second clipping stage's input
    if (mRaw)
    {
      // Non-inverting op-amp: bass below the gain leg's corner sees unity gain, and
      // the op-amp's bandwidth falls as its gain rises. Its rails sit far above the diodes.
      mGainLow = Flush(x + mGainHP * (mGainLow - x));
      const double opamp = x + (mGain - 1.) * (x - mGainLow);
      mBandwidth = Flush(opamp + mGBW * (mBandwidth - opamp));
      drive = .9 * mBandwidth;
    }
    else
    {
      const double input = mFirstGain * x;
      double s = mFirst.Tick(d.first, input + d.firstBias + d.firstTrack * Follow(mFirstTrack, input)) - mFirstOffset;
      if (mInterLP > 0.) s = mInter1 = Flush(s + mInterLP * (mInter1 - s));
      if (mInterHP > 0.) s -= mInter2 = Flush(s + mInterHP * (mInter2 - s));
      drive = mSecondGain * s;
    }
    double y = mSecond.Tick(d.second, drive + d.secondBias + d.secondTrack * Follow(mSecondTrack, drive)) - mSecondOffset;
    if (mClipLP > 0.) y = mClip = Flush(y + mClipLP * (mClip - y));
    // Remove the signal-dependent DC that asymmetric clipping creates.
    const double blocked = mBlock = Flush(y - mPrevious + mDC * mBlock);
    mPrevious = y;
    return blocked;
  }

private:
  // Level-following bias: the operating point moves with how far a stage is overdriven,
  // like a coupling capacitor charged by one-sided conduction. The duty cycle then stays
  // uneven however hard the stage is driven, which keeps even harmonics in loud notes
  // where a fixed bias vanishes into a symmetric square wave.
  double Follow(double& state, double input) const
  {
    const double excess = std::max(input - 1., 0.);
    return state = Flush(excess + mTrackPole * (state - excess));
  }
  double Pole(double hz) const { return hz <= 0. ? 0. : std::exp(-2. * pi * std::min(hz, .45 * mRate) / mRate); }
  void UpdateRaw()
  {
    if (!mRaw) return;
    // The gain leg's capacitor puts the bass corner at 3.4 Hz per unit of gain, so
    // low Gain stays full-range and high Gain thins out, up to the 720 Hz of
    // the original at maximum. Tight moves this corner too.
    mGainHP = Pole(std::clamp(3.4 * (mGain - 1.), 20., 720.) * mTightFactor);
    mGBW = Pole(std::max(3.e6 / mGain, 5000.));
  }
  int mStyle = 0;
  bool mRaw = false;
  double mRate = 192000., mTight = 0., mTightFactor = 1., mGain = 1.;
  double mInputPole = 0., mInterLP = 0., mInterHP = 0., mClipLP = 0., mDC = 0., mTrackPole = 0.;
  double mGBW = 0., mGainHP = 0., mFirstGain = 1., mSecondGain = 1., mFirstOffset = 0., mSecondOffset = 0.;
  double mInput1 = 0., mInput2 = 0., mInter1 = 0., mInter2 = 0., mClip = 0., mBlock = 0., mPrevious = 0.;
  double mGainLow = 0., mBandwidth = 0., mFirstTrack = 0., mSecondTrack = 0.;
  Biquad mEmphasis;
  Clipper mFirst, mSecond;
};

// The linear half of a style after clipping: voicing peaks, low-pass and output trim.
// Being linear, it runs after decimation at the host rate.
class Voicing
{
public:
  void Configure(int style, double rate)
  {
    const auto& d = designs[std::clamp(style, 0, styleCount - 1)];
    // Only the configured peaks run; the table lists them first.
    mCount = 0;
    for (const auto& band : d.post)
      if (band.db != 0.) mPeaks[mCount++].Peak(band.hz, band.q, band.db, rate);
    mHasLowPass = d.postLP > 0.;
    if (mHasLowPass) mLowPass.Pass(d.postLP, d.postLPQ, rate, false);
    mOutput = d.output;
  }
  double Tick(double x)
  {
    for (int i = 0; i < mCount; ++i) x = mPeaks[i].Tick(x);
    if (mHasLowPass) x = mLowPass.Tick(x);
    return x * mOutput;
  }

private:
  int mCount = 0;
  bool mHasLowPass = false;
  double mOutput = 1.;
  std::array<Biquad, 3> mPeaks{};
  Biquad mLowPass;
};

// A whole style at one rate, for the editor's preview. The audio path runs Core
// oversampled and Voicing at the host rate.
class Voice
{
public:
  void Configure(int style, double rate, double tight)
  {
    mCore = {}; mVoicing = {};
    mCore.Configure(style, rate);
    mCore.SetTight(tight);
    mVoicing.Configure(style, rate);
  }
  double Tick(double x, double gain)
  {
    mCore.SetGain(gain);
    return mVoicing.Tick(mCore.Tick(x));
  }

private:
  Core mCore;
  Voicing mVoicing;
};

inline double GainFor(int style, double percent)
{
  const auto& d = designs[std::clamp(style, 0, styleCount - 1)];
  return std::pow(10., (d.gainLow + (d.gainHigh - d.gainLow) * std::clamp(percent, 0., 100.) * .01) / 20.);
}
} // namespace grid_pedal_distortion
