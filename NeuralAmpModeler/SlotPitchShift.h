#pragma once
#include "SlotTimeEffects.h"

// Original NAM Grid MIT moving-read-head pitch shifter. A short overlap hides
// history jumps; matching preceding stereo waveforms at each jump reduces the
// periodic cancellations of fixed, half-window-spaced heads on sustained notes.
// The search uses only past samples, has a fixed bound and allocates in Reset.
namespace pitch_shift
{
class Grains
{
public:
  void Reset(double rate)
  {
    mRate = rate; for (auto& line : mLine) line.Reset(static_cast<int>(.08 * rate) + 8);
    mDelay = .025 * rate; mNext = mDelay; mFade = 1.;
  }
  std::array<double, 2> Tick(double l, double r, double ratio, bool stereo)
  {
    mLine[0].Push(l); if (stereo) mLine[1].Push(r);
    constexpr double fadeSeconds = .004;
    const double lower = 2. + fadeSeconds * mRate * std::max(1., ratio - 1.);
    if (mFade >= 1. && (mDelay < lower || mDelay > .046 * mRate))
    {
      mNext = Match(ratio > 1. ? .045 * mRate : .012 * mRate, stereo);
      mFade = 0.;
    }
    const double blend = mFade >= 1. ? 1. : .5 - .5 * std::cos(grid_fx::pi * mFade);
    std::array<double, 2> out{};
    for (int ch = 0; ch < (stereo ? 2 : 1); ++ch)
    {
      const double a = mLine[ch].Read(mDelay);
      out[ch] = mFade >= 1. ? a : a + blend * (mLine[ch].Read(mNext) - a);
    }
    const double step = 1. - ratio;
    mDelay += step;
    if (mFade < 1.)
    {
      mNext += step; mFade = std::min(1., mFade + 1. / (fadeSeconds * mRate));
      if (mFade >= 1.) mDelay = mNext;
    }
    return out;
  }
private:
  double Match(double target, bool stereo) const
  {
    constexpr int taps = 32;
    const int channels = stereo ? 2 : 1;
    const int spacing = std::max(1, static_cast<int>(.00025 * mRate));
    std::array<std::array<double, taps>, 2> reference{};
    double energy = 0.;
    for (int ch = 0; ch < channels; ++ch)
      for (int i = 0; i < taps; ++i)
      { const double x = reference[ch][i] = mLine[ch].Read(mDelay + i * spacing); energy += x * x; }
    if (energy < 1.e-12) return target;
    const auto error = [&](double delay) {
      double sum = 0.;
      for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < taps; ++i)
        { const double d = reference[ch][i] - mLine[ch].Read(delay + i * spacing); sum += d * d; }
      // Prefer a regular grain length when more than one period matches equally.
      return sum + energy * 1.e-8 * std::abs(delay - target) / mRate;
    };
    const int stride = std::max(1, static_cast<int>(mRate / 6000.));
    const int radius = static_cast<int>(.008 * mRate);
    double best = target, score = error(best);
    for (int d = -radius; d <= radius; d += stride)
    { const double e = error(target + d); if (e < score) { score = e; best = target + d; } }
    const double coarse = best;
    for (int d = -stride; d <= stride; ++d)
    { const double e = error(coarse + d); if (e < score) { score = e; best = coarse + d; } }
    const double a = error(best - 1.), b = error(best), c = error(best + 1.);
    const double curvature = a - 2. * b + c;
    if (curvature > 1.e-20) best += std::clamp(.5 * (a - c) / curvature, -1., 1.);
    return best;
  }
  std::array<grid_fx::DelayLine, 2> mLine;
  double mRate = 48000., mDelay = 1200., mNext = 1200., mFade = 1.;
};
} // namespace pitch_shift

// Fixed interval for both channels; no pitch/key tracking, mono sum or dry
// delay. Wet source filtering precedes resampling; output high cut is wet only.
class SlotPitchShift
{
public:
  void Reset(double rate)
  {
    mRate = std::max(1., rate); mControls.Reset(mRate); mGrains.Reset(mRate);
    mPre = {}; mPost = {}; mPitchBlend = 0.; mFirst = true;
    mCounter = 0; mLastCutoff = -1.; mLastTone = -1.;
  }
  void Process(double* l, double* r, int n, const UtilityControls& c)
  {
    mControls.Target(c.pitchShift, uPitchInterval);
    const double pitchTarget = std::abs(c.pitchShift[0] + .01 * c.pitchShift[1]) > 1.e-4 ? 1. : 0.;
    if (mFirst) { mPitchBlend = pitchTarget; mFirst = false; }
    for (int i = 0; i < n; ++i)
    {
      const auto& v = mControls.Tick();
      const double pitch = v[0] + .01 * v[1];
      const double ratio = mRatio(pitch / 12., [](double x) { return std::exp2(x); });
      mPitchBlend += std::clamp(pitchTarget - mPitchBlend, -1. / (.02 * mRate), 1. / (.02 * mRate));
      if (mCounter++ == 0)
      {
        const double cutoff = std::min(v[2], .4 * mRate / std::max(1., ratio));
        if (cutoff != mLastCutoff)
        {
          const double g = std::tan(grid_fx::pi * cutoff / mRate);
          for (int b = 0; b < 2; ++b)
          { const double k = b ? .7653668647301795 : 1.8477590650225735;
            mA1[b] = 1. / (1. + g * (g + k)); mA2[b] = g * mA1[b]; mA3[b] = g * mA2[b]; }
          mLastCutoff = cutoff;
        }
        if (v[2] != mLastTone)
        { mLow = std::exp(-2. * grid_fx::pi * std::min(v[2], .4 * mRate) / mRate); mLastTone = v[2]; }
      }
      mCounter %= 16;
      const double dryL = l[i], dryR = r ? r[i] : dryL;
      const double preL = Filter(dryL, 0), preR = r ? Filter(dryR, 1) : preL;
      auto wet = mGrains.Tick(preL, preR, ratio, r != nullptr);
      // Land on exact dry after the inaudible final part of a Mix fade.
      const double mix = c.pitchShift[4] <= 0. && v[4] < 1.e-10 ? 0. : v[4] * .01;
      const double output = mOutput(v[3] * .05, [](double db) { return std::pow(10., db); });
      const double toneBlend = std::clamp((16000. - v[2]) / 1000., 0., 1.);
      for (int ch = 0; ch < (r ? 2 : 1); ++ch)
      {
        const double dry = ch ? dryR : dryL, pre = ch ? preR : preL;
        for (auto& pole : mPost[ch])
        { pole = grid_fx::Flush(wet[ch] + mLow * (pole - wet[ch])); wet[ch] = pole; }
        const double unison = dry + toneBlend * (pre - dry);
        const double voice = unison + mPitchBlend * (wet[ch] - unison);
        (ch ? r[i] : l[i]) = mix == 0. || (mPitchBlend == 0. && toneBlend == 0.)
          ? dry * output : (dry + mix * (voice - dry)) * output;
      }
    }
  }
private:
  struct State { double a = 0., b = 0.; };
  // Andrew Simper's TPT state-variable low-pass equations; see notices.
  double Filter(double x, int ch)
  {
    for (int b = 0; b < 2; ++b)
    {
      auto& s = mPre[ch][b]; const double v3 = x - s.b;
      const double v1 = mA1[b] * s.a + mA2[b] * v3;
      const double v2 = s.b + mA2[b] * s.a + mA3[b] * v3;
      s.a = grid_fx::Flush(2. * v1 - s.a); s.b = grid_fx::Flush(2. * v2 - s.b); x = v2;
    }
    return x;
  }
  pitch_shift::Grains mGrains;
  grid_fx::SmoothControls<5> mControls;
  grid_fx::Memo mOutput, mRatio;
  std::array<std::array<State, 2>, 2> mPre{};
  std::array<std::array<double, 2>, 2> mPost{};
  std::array<double, 2> mA1{}, mA2{}, mA3{};
  double mRate = 48000., mLow = 0., mLastCutoff = -1., mLastTone = -1., mPitchBlend = 0.;
  int mCounter = 0; bool mFirst = true;
};
