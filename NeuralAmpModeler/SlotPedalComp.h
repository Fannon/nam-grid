#pragma once
#include "SlotDynamics.h"
#include "SlotEQ.h"
#include "SlotTimeEffects.h"

// Pedal Comp: an original NAM Grid guitar compressor. It takes the musical ideas shared by
// sustain pedals (one Sustain knob with automatic level, a fast or slow attack, a release
// that follows the playing, a treble Tone and a dry Blend) without modelling any circuit.
// Feed-forward with a linked stereo detector: zero latency, no lookahead, no waveshaping.
namespace grid_pedal_comp
{
// What the detector hears for each Pickup choice. Sensitivity is added to the detected
// level; the audible signal never passes these filters.
struct Voice
{
  double sensitivity, knee, detectorLowCut, rms, detectorHighCut;
};
inline constexpr Voice voices[]{
  {5., 10., 70., 0., 0.},     // Single coil: about 5 dB quieter than a humbucker.
  {0., 10., 70., 0., 0.},     // Humbucker: the reference level.
  {5., 12., 150., 1., 5000.}, // Acoustic: averaged level, body boom and piezo quack ignored.
};
// Typical detected playing peak for the reference pickup, in dBFS. Automatic makeup
// returns a note at this level to its dry loudness, so bypass comparisons stay fair.
inline constexpr double nominal = -18.;
inline constexpr double maximumMakeup = 24.;
struct Curve
{
  double threshold, ratio, knee, makeup;
};
// Sustain lowers the threshold and raises the ratio together: 0% only touches hard
// peaks (1.5:1), 50% levels normal playing by about 6 dB, 100% squashes (10:1).
inline Curve CurveFor(double sustainPercent, double knee)
{
  const double s = std::clamp(sustainPercent * .01, 0., 1.);
  Curve curve{nominal + 6. - 30. * s, 1.5 + 8.5 * s * s, knee, 0.};
  curve.makeup = std::min(maximumMakeup, SlotDynamics::CompressionDB(nominal, curve.threshold, curve.ratio, knee));
  return curve;
}
// Settled steady-tone response (detected peak in, dB out) before Blend, Tone and Level.
inline double OutputDB(double inputDB, double sustainPercent, int pickup)
{
  const auto& voice = voices[std::clamp(pickup, 0, 2)];
  const auto curve = CurveFor(sustainPercent, voice.knee);
  return inputDB + curve.makeup
         - SlotDynamics::CompressionDB(inputDB + voice.sensitivity, curve.threshold, curve.ratio, curve.knee);
}
} // namespace grid_pedal_comp

class SlotPedalComp
{
public:
  static constexpr double fastAttackMS = 1.5, slowAttackMS = 20.;
  static constexpr double fastReleaseMS = 70., slowReleaseMS = 120.;
  // Program-dependent release: a second state charges slowly toward part of the
  // reduction and lets go slowly. Short peaks recover quickly; held notes ease back.
  static constexpr double memoryShare = .6, memoryChargeMS = 300., memoryReleaseMS = 1000.;
  static constexpr double peakReleaseMS = 20., rmsWindowMS = 15.;

  void Reset(double rate)
  {
    mRate = std::max(1., rate);
    mControls.Reset(mRate);
    const auto coefficient = [&](double ms) { return std::exp(-1. / (.001 * ms * mRate)); };
    mAttack[0] = coefficient(fastAttackMS);
    mAttack[1] = coefficient(slowAttackMS);
    mRelease[0] = coefficient(fastReleaseMS);
    mRelease[1] = coefficient(slowReleaseMS);
    mMemoryCharge = coefficient(memoryChargeMS);
    mMemoryRelease = coefficient(memoryReleaseMS);
    mPeakRelease = coefficient(peakReleaseMS);
    mRMS = coefficient(rmsWindowMS);
    mVoiceSmoothing = coefficient(20.);
    mMakeupSmoothing = coefficient(5.);
    for (auto& channel : mChannels) channel = {};
    for (auto& filter : mTone) filter.Reset();
    mPeak = mPower = mFast = mMemory = 0.;
    mPickup = -1;
    mCounter = 0;
    mToneDB = 0.;
    mToneActive = false; // Recompute the shelf for this rate.
    mFirst = true;
    mMaxReduction = 0.;
  }
  void Process(double* left, double* right, int frames, const UtilityControls& c)
  {
    using namespace grid_pedal_comp;
    mControls.Target(c.pedalComp, uPedalCompSustain);
    const int pickup = std::clamp(c.pedalCompPickup, 0, 2);
    const int slow = c.pedalComp[1] >= .5 ? 1 : 0;
    if (pickup != mPickup)
    {
      // Detector-only filters: the smoothed reduction hides the coefficient change.
      const auto& voice = voices[pickup];
      mLowCut = std::exp(-2. * grid_fx::pi * voice.detectorLowCut / mRate);
      mHighCut = voice.detectorHighCut > 0. ? std::exp(-2. * grid_fx::pi * voice.detectorHighCut / mRate) : 0.;
      if (mFirst) mVoice = voice;
      mPickup = pickup;
    }
    const auto& voice = voices[mPickup];
    mMaxReduction = 0.;
    for (int i = 0; i < frames; ++i)
    {
      const auto& v = mControls.Tick();
      // Sensitivity, knee and detector averaging glide to the new pickup over 20 ms.
      mVoice.sensitivity = voice.sensitivity + mVoiceSmoothing * (mVoice.sensitivity - voice.sensitivity);
      mVoice.knee = voice.knee + mVoiceSmoothing * (mVoice.knee - voice.knee);
      mVoice.rms = voice.rms + mVoiceSmoothing * (mVoice.rms - voice.rms);
      if (mCounter == 0)
      {
        mCurve = CurveFor(v[0], mVoice.knee);
        UpdateTone(v[2]);
      }
      mCounter = (mCounter + 1) % 16;
      if (mFirst)
      {
        mMakeup = mCurve.makeup;
        mFirst = false;
      }
      const double dryL = left[i], dryR = right ? right[i] : dryL;
      const double sideL = Detect(mChannels[0], dryL);
      const double sideR = right ? Detect(mChannels[1], dryR) : sideL;
      const double peak = std::max(std::abs(sideL), std::abs(sideR));
      mPeak = peak > mPeak ? peak : grid_fx::Flush(peak + mPeakRelease * (mPeak - peak));
      const double power = .5 * (sideL * sideL + sideR * sideR);
      mPower = grid_fx::Flush(power + mRMS * (mPower - power));
      // A sine's RMS is 3 dB below its peak; sqrt(2 power) keeps both detectors on one scale.
      const double detected = mPeak + mVoice.rms * (std::sqrt(2. * mPower) - mPeak);
      const double levelDB = 20. * std::log10(std::max(detected, 1.e-9)) + mVoice.sensitivity;
      const double target = SlotDynamics::CompressionDB(levelDB, mCurve.threshold, mCurve.ratio, mCurve.knee);
      mFast = Approach(mFast, target, target > mFast ? mAttack[slow] : mRelease[slow]);
      const double memory = memoryShare * target;
      mMemory = Approach(mMemory, memory, memory > mMemory ? mMemoryCharge : mMemoryRelease);
      const double reduction = std::max(mFast, mMemory);
      mMaxReduction = std::max(mMaxReduction, reduction);
      mMakeup = Approach(mMakeup, mCurve.makeup, mMakeupSmoothing);
      const double gain = mGain((mMakeup - reduction) * .05, [](double x) { return std::pow(10., x); });
      const double blend = v[3] * .01;
      double outL = blend == 0. ? dryL : dryL + blend * (dryL * gain - dryL);
      double outR = blend == 0. ? dryR : dryR + blend * (dryR * gain - dryR);
      if (mToneActive)
      {
        outL = mTone[0].Tick(outL);
        if (right) outR = mTone[1].Tick(outR);
      }
      const double level = mLevel(v[4] * .05, [](double x) { return std::pow(10., x); });
      left[i] = outL * level;
      if (right) right[i] = outR * level;
    }
  }
  // Largest compressor gain reduction of the last block, before makeup, Blend and Level.
  double Reduction() const { return mMaxReduction; }

private:
  struct Channel
  {
    double x1 = 0., y1 = 0., x2 = 0., y2 = 0., low = 0.;
  };
  // Two one-pole high-passes (12 dB/octave) keep low strings and body boom from pumping
  // the reduction; Acoustic adds a one-pole low-pass above the piezo quack.
  double Detect(Channel& s, double x) const
  {
    s.y1 = grid_fx::Flush(mLowCut * (s.y1 + x - s.x1));
    s.x1 = x;
    s.y2 = grid_fx::Flush(mLowCut * (s.y2 + s.y1 - s.x2));
    s.x2 = s.y1;
    s.low = grid_fx::Flush(s.y2 + mHighCut * (s.low - s.y2));
    return s.low;
  }
  void UpdateTone(double toneDB)
  {
    // A high shelf at 2.5 kHz after Blend: no phase difference between dry and compressed.
    if (std::abs(toneDB) < 1.e-3)
    {
      if (mToneActive) for (auto& filter : mTone) filter.Reset();
      mToneActive = false;
      return;
    }
    if (!mToneActive || toneDB != mToneDB)
      for (auto& filter : mTone)
        filter.SetParams(recursive_linear_filter::BiquadParams(mRate, 2500., .707, toneDB));
    mToneDB = toneDB;
    mToneActive = true;
  }
  static double Approach(double value, double target, double coefficient)
  { return grid_fx::Flush(target + coefficient * (value - target)); }

  grid_fx::SmoothControls<5> mControls;
  grid_fx::Memo mGain, mLevel;
  std::array<Channel, 2> mChannels{};
  std::array<ScalarFilter<recursive_linear_filter::HighShelf>, 2> mTone;
  grid_pedal_comp::Voice mVoice = grid_pedal_comp::voices[0];
  grid_pedal_comp::Curve mCurve{};
  std::array<double, 2> mAttack{}, mRelease{};
  double mRate = 48000., mMemoryCharge = 0., mMemoryRelease = 0., mPeakRelease = 0., mRMS = 0.;
  double mVoiceSmoothing = 0., mMakeupSmoothing = 0., mLowCut = 0., mHighCut = 0.;
  double mPeak = 0., mPower = 0., mFast = 0., mMemory = 0., mMakeup = 0., mToneDB = 0.;
  double mMaxReduction = 0.;
  int mPickup = -1, mCounter = 0;
  bool mFirst = true, mToneActive = false;
};
