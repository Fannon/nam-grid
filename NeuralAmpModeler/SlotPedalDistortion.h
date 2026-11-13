#pragma once
#include "SlotTimeEffects.h"
#include "dsp/PedalDistortion.h"

// Distortion-pedal voices with a shared post-clip EQ, independent per channel in
// NAMStage. Each style clips at four times the host rate through minimum-phase
// filters, so the pedal adds no reported latency and dry Mix passes through
// unshifted; its linear voicing and the EQ run at the host rate. Style changes
// crossfade over 20 ms.
class SlotPedalDistortion
{
public:
  static constexpr int latency = grid_pedal_distortion::Oversampler4x::latency;
  static constexpr double bassHz = 100., trebleHz = 3000., midQ = .9;
  void Reset(double rate)
  {
    mRate = std::max(1., rate);
    mControls.Reset(mRate);
    mOversampler.Reset();
    mWeights = {};
    mTight = 0.;
    for (int i = 0; i < grid_pedal_distortion::styleCount; ++i)
      Start(i);
    mBass = mMid = mTreble = {};
    mCached.fill(-1.e30);
    mCounter = 0;
    mFirst = true;
  }
  // Small-signal response of the shared Bass, Mid and Treble EQ, for the editor display.
  static std::complex<double> EQResponse(double hz, double rate, double bass, double mid, double midHz, double treble)
  {
    grid_pedal_distortion::Biquad b, m, t;
    Design(b, m, t, rate, bass, mid, midHz, treble);
    return b.Response(hz, rate) * m.Response(hz, rate) * t.Response(hz, rate);
  }
  void Process(double* samples, int frames, const UtilityControls& controls)
  {
    if (frames <= 0)
      return;
    namespace pd = grid_pedal_distortion;
    mControls.Target(controls.pedalDistortion, uPDistStyle);
    const int style = static_cast<int>(std::clamp(controls.pedalDistortion[0], 0., double(pd::styleCount - 1)));
    if (mFirst)
    {
      mWeights[style] = 1.;
      mFirst = false;
    }
    else if (mWeights[style] == 0.)
      Start(style); // A style returning from silence starts with clean histories.
    for (int i = 0; i < frames; ++i)
    {
      const auto& c = mControls.Tick();
      const bool settled = mWeights[style] == 1.;
      // Gains follow every smoothed sample; filters keep a 16-sample schedule.
      // Fading styles follow Gain through their own ranges.
      if (!settled || c[1] != mCached[1])
      {
        for (int j = 0; j < pd::styleCount; ++j)
          if (j == style || mWeights[j] > 0.)
            mCores[j].SetGain(pd::GainFor(j, c[1]));
        mCached[1] = c[1];
      }
      if (c[7] != mCached[7])
      {
        mLevel = std::pow(10., c[7] * .05);
        mCached[7] = c[7];
      }
      if (mCounter == 0)
      {
        if (c[2] != mCached[2])
        {
          mTight = c[2] * .01;
          for (int j = 0; j < pd::styleCount; ++j)
            if (j == style || mWeights[j] > 0.)
              mCores[j].SetTight(mTight);
          mCached[2] = c[2];
        }
        if (c[3] != mCached[3] || c[4] != mCached[4] || c[5] != mCached[5] || c[6] != mCached[6])
        {
          Design(mBass, mMid, mTreble, mRate, c[3], c[4], c[5], c[6]);
          mCached[3] = c[3]; mCached[4] = c[4]; mCached[5] = c[5]; mCached[6] = c[6];
        }
      }
      mCounter = (mCounter + 1) % 16;
      if (!settled)
      {
        double others = 0.;
        for (int j = 0; j < pd::styleCount; ++j)
          if (j != style)
          {
            mWeights[j] = std::max(0., mWeights[j] - 1. / (.02 * mRate));
            others += mWeights[j];
          }
        mWeights[style] = 1. - others;
      }
      const double dry = samples[i];
      const double clipped = mOversampler.Tick(dry, [&](double x) {
        if (settled)
          return mCores[style].Tick(x);
        double sum = 0.;
        for (int j = 0; j < pd::styleCount; ++j)
          if (mWeights[j] > 0.)
            sum += mWeights[j] * mCores[j].Tick(x);
        return sum;
      });
      // One decimator serves every fading style, so their voicings share its output.
      double wet = 0.;
      if (settled)
        wet = mVoicings[style].Tick(clipped);
      else
        for (int j = 0; j < pd::styleCount; ++j)
          if (mWeights[j] > 0.)
            wet += mWeights[j] * mVoicings[j].Tick(clipped);
      wet = mTreble.Tick(mMid.Tick(mBass.Tick(wet)));
      samples[i] = (dry + c[8] * .01 * (wet - dry)) * mLevel;
    }
  }

private:
  void Start(int style)
  {
    mCores[style] = {};
    mCores[style].Configure(style, 4. * mRate);
    mCores[style].SetTight(mTight);
    mVoicings[style] = {};
    mVoicings[style].Configure(style, mRate);
  }
  static void Design(grid_pedal_distortion::Biquad& bass, grid_pedal_distortion::Biquad& mid,
                     grid_pedal_distortion::Biquad& treble, double rate, double bassDb, double midDb, double midHz,
                     double trebleDb)
  {
    bass.Shelf(bassHz, bassDb, rate, false);
    mid.Peak(midHz, midQ, midDb, rate);
    treble.Shelf(trebleHz, trebleDb, rate, true);
  }
  grid_fx::SmoothControls<9> mControls;
  grid_pedal_distortion::Oversampler4x mOversampler;
  std::array<grid_pedal_distortion::Core, grid_pedal_distortion::styleCount> mCores{};
  std::array<grid_pedal_distortion::Voicing, grid_pedal_distortion::styleCount> mVoicings{};
  std::array<double, grid_pedal_distortion::styleCount> mWeights{};
  std::array<double, 9> mCached{};
  grid_pedal_distortion::Biquad mBass, mMid, mTreble;
  double mRate = 48000., mLevel = 1., mTight = 0.;
  int mCounter = 0;
  bool mFirst = true;
};
