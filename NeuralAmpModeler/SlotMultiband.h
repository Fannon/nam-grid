#pragma once
#include "SlotTimeEffects.h"

// Original MIT NAM Grid design: three-band upward and downward compression with a
// parallel Depth blend, the "over the top" multiband workflow without lookahead.
// Linkwitz-Riley 24 dB/octave crossovers split the bands; the low band passes the high
// split's allpass, so the unprocessed bands sum to a flat-magnitude allpass of the input.
// Depth blends with that same sum rather than the raw input: both paths share one phase
// response, so partial Depth never comb filters. Each band has a stereo-linked power detector
// whose attack and release scale with Time. Threshold is the mid band's; the low and
// high bands sit 3 and 10 dB lower, following a guitar's typical spectrum, so each
// band engages on similar playing. Above its threshold a band is compressed downward
// (Downward 0-100% maps to 1:1-20:1 exponentially); more than 12 dB below it, upward
// (1:1-4:1, at most +18 dB, faded out below -60 dBFS so silence and hiss stay put).
// TPT state-variable filters: Andrew Simper's equations.
class SlotMultiband
{
public:
  static constexpr int kBands = 3;
  void Reset(double rate)
  {
    mRate = std::max(1., rate); mControls.Reset(mRate); mShape.Reset(mRate);
    mFilters = {}; mPower = {}; mGain = {1., 1., 1.}; mStep = {}; mGainDB = {}; mLevelDB = {-120., -120., -120.};
    mCounter = 0;
  }
  void Process(double* l, double* r, int n, const UtilityControls& c)
  {
    mControls.Target(c.multiband, uMultibandDepth);
    mShape.Target(c.multibandShape, uMultibandLowSplit);
    mPeakGain = {}; mPeakLevel = {-120., -120., -120.};
    for (int i = 0; i < n; ++i)
    {
      const auto& v = mControls.Tick();
      const auto& s = mShape.Tick();
      if (mCounter % kUpdate == 0) Update(v, s);
      ++mCounter;
      const double dryL = l[i], dryR = r ? r[i] : dryL;
      std::array<double, kBands> bandL, bandR;
      Split(dryL, mFilters[0], bandL);
      if (r) Split(dryR, mFilters[1], bandR); else bandR = bandL;
      const double depth = mDepth = v[0] * .01, output = mOutputGain(v[7] * .05, [](double x) { return std::pow(10., x); });
      double wetL = 0., wetR = 0.;
      for (int b = 0; b < kBands; ++b)
      {
        const double power = std::max(bandL[b] * bandL[b], bandR[b] * bandR[b]);
        mPower[b] = grid_fx::Flush(power + (power > mPower[b] ? mAttack[b] : mRelease[b]) * (mPower[b] - power));
        mGain[b] += mStep[b];
        const double blend = 1. + depth * (mGain[b] - 1.);
        wetL += blend * bandL[b]; wetR += blend * bandR[b];
      }
      l[i] = wetL * output;
      if (r) r[i] = wetR * output;
    }
  }
  // Largest dynamic gain change of each band in this block (dB, before the band's
  // output gain and Depth) and its detected level, for the display.
  const std::array<double, kBands>& BandGain() const { return mPeakGain; }
  const std::array<double, kBands>& BandLevel() const { return mPeakLevel; }
  double Reduction() const
  {
    double cut = 0.;
    for (double g : mPeakGain) cut = std::max(cut, -g);
    return cut * mDepth;
  }
  // Static gain of one band at a detected level; shared with the display and tests.
  static double DynamicGain(double levelDB, double threshold, double downward, double upward)
  {
    const double ratioDown = std::pow(20., downward * .01), ratioUp = std::pow(4., upward * .01);
    if (levelDB > threshold) return -(levelDB - threshold) * (1. - 1. / ratioDown);
    const double floor = threshold - kUpwardGap;
    if (levelDB >= floor) return 0.;
    const double guard = std::clamp((levelDB + 75.) / 15., 0., 1.);
    return std::min(kUpwardLimit, (floor - levelDB) * (1. - 1. / ratioUp)) * guard;
  }
  static constexpr double kUpwardGap = 12., kUpwardLimit = 18.;
  // Threshold offsets of the low, mid and high bands relative to Threshold.
  static constexpr std::array<double, kBands> kOffset{-3., 0., -10.};
private:
  static constexpr int kUpdate = 8;
  struct Filter { double a = 0., b = 0.; };
  struct Outputs { double low, band, high; };
  // Per channel: low split LP x2 and HP x2, high split LP x2 and HP x2, high split allpass.
  using Filters = std::array<Filter, 9>;
  // Detector attack and release in ms at Time 100%, low to high band.
  static constexpr std::array<double, kBands> kAttack{48., 22., 13.5}, kRelease{280., 280., 130.};
  void Update(const std::array<double, 8>& v, const std::array<double, 3>& s)
  {
    const double low = std::clamp(s[0], 20., .4 * mRate);
    const double high = std::clamp(std::max(s[1], low), 20., .4 * mRate);
    for (int f = 0; f < 2; ++f)
    {
      const double g = std::tan(grid_fx::pi * (f ? high : low) / mRate);
      mA1[f] = 1. / (1. + g * (g + kSqrt2)); mA2[f] = g * mA1[f]; mA3[f] = g * mA2[f];
    }
    const double time = v[3] * .01;
    for (int b = 0; b < kBands; ++b)
    {
      mAttack[b] = std::exp(-1. / (.001 * kAttack[b] * time * mRate));
      mRelease[b] = std::exp(-1. / (.001 * kRelease[b] * time * mRate));
      const double level = 10. * std::log10(mPower[b] + 1.e-30);
      const double dynamic = DynamicGain(level, s[2] + kOffset[b], v[1], v[2]);
      mGainDB[b] = dynamic;
      mPeakGain[b] = std::abs(dynamic) > std::abs(mPeakGain[b]) ? dynamic : mPeakGain[b];
      mPeakLevel[b] = std::max(mPeakLevel[b], level);
      // Linear ramp to the new band gain over the next update interval.
      const double target = std::pow(10., (dynamic + v[4 + b]) * .05);
      mStep[b] = (target - mGain[b]) / kUpdate;
    }
  }
  // Linkwitz-Riley: two cascaded Butterworth sections per output. LP + HP of one split
  // is a second-order allpass, which the low band also passes at the high split.
  void Split(double x, Filters& s, std::array<double, kBands>& bands) const
  {
    const double low = Svf(Svf(x, s[0], 0).low, s[1], 0).low;
    const double upper = Svf(Svf(x, s[2], 0).high, s[3], 0).high;
    bands[1] = Svf(Svf(upper, s[4], 1).low, s[5], 1).low;
    bands[2] = Svf(Svf(upper, s[6], 1).high, s[7], 1).high;
    const auto aligned = Svf(low, s[8], 1);
    bands[0] = low - 2. * kSqrt2 * aligned.band;
  }
  Outputs Svf(double x, Filter& f, int which) const
  {
    const double v3 = x - f.b;
    const double v1 = mA1[which] * f.a + mA2[which] * v3;
    const double v2 = f.b + mA2[which] * f.a + mA3[which] * v3;
    f.a = grid_fx::Flush(2. * v1 - f.a); f.b = grid_fx::Flush(2. * v2 - f.b);
    return {v2, v1, x - kSqrt2 * v1 - v2};
  }
  static constexpr double kSqrt2 = 1.4142135623730951;
  grid_fx::SmoothControls<8> mControls;
  grid_fx::SmoothControls<3> mShape;
  grid_fx::Memo mOutputGain;
  std::array<Filters, 2> mFilters{};
  std::array<double, 2> mA1{1., 1.}, mA2{}, mA3{};
  std::array<double, kBands> mPower{}, mGain{1., 1., 1.}, mStep{}, mGainDB{}, mLevelDB{}, mAttack{}, mRelease{};
  std::array<double, kBands> mPeakGain{}, mPeakLevel{};
  double mRate = 48000., mDepth = 0.;
  int64_t mCounter = 0;
};
