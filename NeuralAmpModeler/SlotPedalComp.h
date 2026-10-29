#pragma once
#include "SlotDynamics.h"
#include "SlotTimeEffects.h"

// Pedal Comp: an original NAM Grid guitar compressor. It takes the musical ideas shared by
// sustain pedals (one Sustain knob with automatic level, a fast or slow attack, a release
// that follows the playing and a dry Blend) without modelling any circuit. Body / Pick tilts
// what the detector hears, which no EQ before or after the module can do.
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
// Typical detected note level for the reference pickup, in dBFS: about what a DAW peak
// meter shows as -9 dBFS on normal playing. Automatic makeup keeps such notes at bypass
// loudness. It reads the curve 2.5 dB higher because, on recorded playing, the releases
// hold some reduction between peaks; measured on a DI performance, typical notes then
// stay within about 0.5 dB of bypass at every Sustain and Attack setting.
inline constexpr double nominal = -18., makeupLift = 2.5;
inline constexpr double maximumMakeup = 24.;
struct Curve
{
  double threshold, ratio, knee, makeup;
};
// Sustain lowers the threshold and raises the ratio together. The range follows a
// Ross-type pedal's Sustain knob, measured from its recorded guitar DI and output: on
// played notes 0% barely touches the loudest peaks, 50% lowers notes 6 dB above a
// typical one by about 4 dB more than quiet ones, and 100% by about 7 dB.
inline Curve CurveFor(double sustainPercent, double knee)
{
  const double s = std::clamp(sustainPercent * .01, 0., 1.);
  Curve curve{nominal + 1.7 - 15.7 * std::pow(s, .4), 1.6 + 6.4 * s, knee, 0.};
  curve.makeup =
    std::min(maximumMakeup, SlotDynamics::CompressionDB(nominal + makeupLift, curve.threshold, curve.ratio, knee));
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
  static constexpr double fastReleaseMS = 150., slowReleaseMS = 250.;
  // Program-dependent release: a second state charges slowly toward part of the
  // reduction and lets go slowly. Short peaks recover quickly; held notes ease back.
  static constexpr double memoryShare = .4, memoryChargeMS = 300., memoryReleaseMS = 800.;
  static constexpr double peakReleaseMS = 20., rmsWindowMS = 15.;
  // Body / Pick: a first-order tilt in the detector around the pivot, up to this many dB
  // in each direction. Body hears note fundamentals, Pick hears the bright attack.
  static constexpr double focusPivotHz = 500., focusRangeDB = 6.;

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
    mPivot = std::exp(-2. * grid_fx::pi * focusPivotHz / mRate);
    mPeak = mPower = mFast = mMemory = 0.;
    mFocusLow = mFocusHigh = 1.;
    mPickup = -1;
    mCounter = 0;
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
        // Positive leans the detector toward Pick, negative toward Body.
        mFocusHigh = std::pow(10., v[2] * .01 * focusRangeDB / 20.);
        mFocusLow = 1. / mFocusHigh;
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
      const double blend = v[3] * .01;
      if (reduction > mMaxReduction)
      {
        mMaxReduction = reduction;
        mBlendAtMax = blend;
      }
      mMakeup = Approach(mMakeup, mCurve.makeup, mMakeupSmoothing);
      const double gain = mGain((mMakeup - reduction) * .05, [](double x) { return std::pow(10., x); });
      const double outL = blend == 0. ? dryL : dryL + blend * (dryL * gain - dryL);
      const double outR = blend == 0. ? dryR : dryR + blend * (dryR * gain - dryR);
      const double level = mLevel(v[4] * .05, [](double x) { return std::pow(10., x); });
      left[i] = outL * level;
      if (right) right[i] = outR * level;
    }
  }
  // Largest gain reduction of the last block as heard after Blend, before makeup and Level,
  // like the Smooth and Punch compressors' Mix.
  double Reduction() const
  { return -20. * std::log10(std::max(1.e-6, 1. + mBlendAtMax * (std::pow(10., -mMaxReduction * .05) - 1.))); }

private:
  struct Channel
  {
    double x1 = 0., y1 = 0., x2 = 0., y2 = 0., low = 0., body = 0.;
  };
  // Two one-pole high-passes (12 dB/octave) keep low strings and body boom from pumping
  // the reduction; Acoustic adds a one-pole low-pass above the piezo quack. The Body / Pick
  // tilt splits the result at the pivot and weights the two complementary bands.
  double Detect(Channel& s, double x) const
  {
    s.y1 = grid_fx::Flush(mLowCut * (s.y1 + x - s.x1));
    s.x1 = x;
    s.y2 = grid_fx::Flush(mLowCut * (s.y2 + s.y1 - s.x2));
    s.x2 = s.y1;
    s.low = grid_fx::Flush(s.y2 + mHighCut * (s.low - s.y2));
    s.body = grid_fx::Flush(s.low + mPivot * (s.body - s.low));
    return mFocusLow * s.body + mFocusHigh * (s.low - s.body);
  }
  static double Approach(double value, double target, double coefficient)
  { return grid_fx::Flush(target + coefficient * (value - target)); }

  grid_fx::SmoothControls<5> mControls;
  grid_fx::Memo mGain, mLevel;
  std::array<Channel, 2> mChannels{};
  grid_pedal_comp::Voice mVoice = grid_pedal_comp::voices[0];
  grid_pedal_comp::Curve mCurve{};
  std::array<double, 2> mAttack{}, mRelease{};
  double mRate = 48000., mMemoryCharge = 0., mMemoryRelease = 0., mPeakRelease = 0., mRMS = 0.;
  double mVoiceSmoothing = 0., mMakeupSmoothing = 0., mLowCut = 0., mHighCut = 0.;
  double mPivot = 0., mFocusLow = 1., mFocusHigh = 1.;
  double mPeak = 0., mPower = 0., mFast = 0., mMemory = 0., mMakeup = 0.;
  double mMaxReduction = 0., mBlendAtMax = 1.;
  int mPickup = -1, mCounter = 0;
  bool mFirst = true;
};
