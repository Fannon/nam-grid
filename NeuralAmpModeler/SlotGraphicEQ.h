#pragma once
#include "SlotTimeEffects.h"
#include "GraphicEQVoicing.h"

// Original MIT NAM Grid implementation of a seven-band graphic EQ in the stompbox
// tradition: fixed bands, one boost/cut each and a Level. Voicing selects the band
// centres for guitar, bass or full-range sources. Each band adds (gain - 1) times a
// unity-peak TPT state-variable band-pass (Andrew Simper's equations) whose width
// follows the spacing to its neighbours, about one octave on the guitar voicing. The
// filters depend only on frequency and always run; a slider only moves the band's mix,
// ramped per sample, so sweeps never click and 0 dB bands return the input exactly.
// A voicing change glides the centres over about 30 ms.

class SlotGraphicEQ
{
public:
  static constexpr int kBands = graphic_eq::kBands;
  void Reset(double rate)
  {
    mRate = std::max(1., rate); mControls.Reset(mRate);
    mState = {}; mMix = mTarget = mStep = {}; mCounter = 0; mVoicing = -1;
    mGlide = std::exp(-1. / (.01 * mRate / kUpdate));
  }
  void Process(double* l, double* r, int n, const UtilityControls& c)
  {
    mControls.Target(c.graphicEQ, uGraphicBand1);
    const int voicing = graphic_eq::Voicing(c.graphicVoicing);
    if (mVoicing < 0) mCentres = graphic_eq::kCentres[voicing]; // Start on the saved voicing, no glide.
    mVoicing = voicing;
    for (int i = 0; i < n; ++i)
    {
      const auto& v = mControls.Tick();
      if (mCounter++ % kUpdate == 0) Update(v);
      mCounter %= kUpdate;
      double x = l[i], y = r ? r[i] : 0.;
      for (int b = 0; b < kBands; ++b)
      {
        mMix[b] += mStep[b];
        x += mMix[b] * Band(x, mState[0][b], b);
        if (r) y += mMix[b] * Band(y, mState[1][b], b);
      }
      const double level = mLevel(v[kBands] * .05, [](double d) { return std::pow(10., d); });
      l[i] = x * level;
      if (r) r[i] = y * level;
    }
  }
private:
  static constexpr int kUpdate = 16;
  struct State { double a = 0., b = 0.; };
  void Update(const std::array<double, kBands + 1>& v)
  {
    const auto& target = graphic_eq::kCentres[mVoicing];
    for (int b = 0; b < kBands; ++b)
    {
      // Glide in octaves, so a voicing change sweeps rather than jumps.
      mCentres[b] = std::exp2(std::log2(target[b]) + mGlide * (std::log2(mCentres[b]) - std::log2(target[b])));
      if (std::abs(mCentres[b] / target[b] - 1.) < 1.e-6) mCentres[b] = target[b];
      const double g = std::tan(grid_fx::pi * std::min(mCentres[b], .45 * mRate) / mRate);
      const double k = mK[b] = 1. / graphic_eq::Q(mVoicing, b);
      mA1[b] = 1. / (1. + g * (g + k)); mA2[b] = g * mA1[b]; mA3[b] = g * mA2[b];
      // Land exactly on the previous target, then ramp to this one over the interval.
      mMix[b] = mTarget[b];
      mTarget[b] = (std::pow(10., v[b] * .05) - 1.) * k;
      mStep[b] = (mTarget[b] - mMix[b]) / kUpdate;
    }
  }
  // Band-pass output v1 of the TPT state-variable filter; k v1 has unity gain at the centre.
  double Band(double x, State& s, int b) const
  {
    const double v3 = x - s.b;
    const double v1 = mA1[b] * s.a + mA2[b] * v3;
    const double v2 = s.b + mA2[b] * s.a + mA3[b] * v3;
    s.a = grid_fx::Flush(2. * v1 - s.a); s.b = grid_fx::Flush(2. * v2 - s.b);
    return v1;
  }
  grid_fx::SmoothControls<kBands + 1> mControls;
  grid_fx::Memo mLevel;
  std::array<std::array<State, kBands>, 2> mState{};
  std::array<double, kBands> mCentres{}, mA1{}, mA2{}, mA3{}, mK{}, mMix{}, mTarget{}, mStep{};
  double mRate = 48000., mGlide = 0.;
  int mCounter = 0, mVoicing = -1;
};
