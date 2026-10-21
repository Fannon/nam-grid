#pragma once
#include <algorithm>
#include <cmath>

// Algorithm references: AudioDSPTools (MIT, Steven Atkinson) and LEAF's
// tCompressor (MIT, Jeff Snyder). Independent mono implementation; no LEAF runtime.
// See docs/EFFECTS.md and installer/ThirdPartyNotices.txt for provenance/notices.
enum class SlotEffect
{
  NAM = 0,
  NaturalGate,
  TightGate,
  SmoothComp,
  PunchComp,
  ToneEQ,
  CutEQ,
  CabinetIR,
  ParametricEQ,
  Boost,
  SynthFilter,
  AutoSwell,
  DigitalDelay,
  AnalogDelay,
  TapeDelay,
  RoomReverb,
  PlateReverb,
  CloudReverb,
  Chorus,
  Flanger,
  Phaser,
  Tremolo,
  Vibrato,
  Tape,
  TubeBoost,
  Utility,
  SpringReverb,
  Wah,
  Rotary,
  UniVibe,
  Limiter,
  Octaver,
  Doubler,
  ReverseDelay,
  DualDelay,
  Freeze,
  Stutter,
  Bitcrusher,
  RingMod,
  ShimmerReverb,
  JunoChorus,
  FrequencyShifter,
  Distortion,
  LoFiTape,
  NoisePrint = 64 // Retain the prototype's ID for named-state migration.
};
inline const char* SlotEffectName(SlotEffect effect)
{
  switch (effect)
  {
    case SlotEffect::NAM: return "NAM";
    case SlotEffect::NaturalGate:
    case SlotEffect::TightGate: return "Noise Gate";
    case SlotEffect::SmoothComp: return "Smooth Compressor";
    case SlotEffect::PunchComp: return "Punch Compressor";
    case SlotEffect::ToneEQ: return "Guitar EQ";
    case SlotEffect::CutEQ: return "Cut EQ";
    case SlotEffect::CabinetIR: return "Cabinet IR";
    case SlotEffect::ParametricEQ: return "Parametric EQ";
    case SlotEffect::Boost: return "Boost";
    case SlotEffect::SynthFilter: return "Filter";
    case SlotEffect::AutoSwell: return "Auto Swell";
    case SlotEffect::DigitalDelay: return "Digital Delay";
    case SlotEffect::AnalogDelay: return "Analog Echo";
    case SlotEffect::TapeDelay: return "Tape Echo";
    case SlotEffect::RoomReverb: return "Room";
    case SlotEffect::PlateReverb: return "Dense Plate";
    case SlotEffect::CloudReverb: return "Cloud";
    case SlotEffect::Chorus: return "Chorus";
    case SlotEffect::Flanger: return "Flanger";
    case SlotEffect::Phaser: return "Phaser";
    case SlotEffect::Tremolo: return "Auto Tremolo";
    case SlotEffect::Vibrato: return "Vibrato";
    case SlotEffect::Tape: return "Airwindows Tape";
    case SlotEffect::TubeBoost: return "Tube Boost";
    case SlotEffect::Utility: return "Utility";
    case SlotEffect::SpringReverb: return "Spring Reverb";
    case SlotEffect::Wah: return "Wah";
    case SlotEffect::Rotary: return "Rotary";
    case SlotEffect::UniVibe: return "Uni-Vibe";
    case SlotEffect::Limiter: return "Peak Limiter";
    case SlotEffect::Octaver: return "Mono Octaver";
    case SlotEffect::Doubler: return "Stereo Doubler";
    case SlotEffect::ReverseDelay: return "Reverse Delay";
    case SlotEffect::DualDelay: return "Dual Delay";
    case SlotEffect::Freeze: return "Freeze / Hold";
    case SlotEffect::Stutter: return "Stutter";
    case SlotEffect::Bitcrusher: return "Bitcrusher";
    case SlotEffect::RingMod: return "Ring Modulator";
    case SlotEffect::ShimmerReverb: return "Shimmer";
    case SlotEffect::JunoChorus: return "Juno Chorus";
    case SlotEffect::FrequencyShifter: return "Frequency Shifter";
    case SlotEffect::Distortion: return "Distortion";
    case SlotEffect::LoFiTape: return "LoFi Tape Machine";
    case SlotEffect::NoisePrint: return "Noise Gate (Smart)";
  }
  return "Unknown";
}
inline bool IsValidSlotEffect(SlotEffect effect)
{ return (effect >= SlotEffect::NAM && effect <= SlotEffect::LoFiTape) || effect == SlotEffect::NoisePrint; }
struct DynamicsControls
{
  double gateThreshold = -55., gateRelease = 120., gateRange = 60.;
  double compThreshold = -18., compRelease = 120., compMakeup = 0.;
  double compAttack = 8., compMix = 100., punchAttack = 25.;
  int gateMode = -1; // Unspecified retains the legacy DSP entry point; hosts supply Natural=0 / Tight=1.
};

class SlotDynamics
{
public:
  void Reset(double rate)
  {
    mRate = std::max(1., rate);
    mSmooth = Coefficient(5.);
    mEnvelope = mReduction = mHold = 0.;
    mOpen = false;
    mFirst = true;
  }
  // Positive gain reduction in dB; quadratic soft knee joins both straight lines.
  static double CompressionDB(double level, double threshold, double ratio, double knee)
  {
    const double over = level - threshold, slope = 1. - 1. / ratio;
    if (over <= -knee * .5)
      return 0.;
    if (over >= knee * .5)
      return slope * over;
    const double x = over + knee * .5;
    return slope * x * x / (2. * knee);
  }
  void Process(double* samples, int frames, SlotEffect effect, const DynamicsControls& controls)
  {
    mPeakReduction = 0.;
    const bool gate = effect == SlotEffect::NaturalGate || effect == SlotEffect::TightGate;
    const bool tight = controls.gateMode < 0 ? effect == SlotEffect::TightGate : controls.gateMode == 1;
    const bool punch = effect == SlotEffect::PunchComp;
    const double threshold =
      std::clamp(gate ? controls.gateThreshold : controls.compThreshold, gate ? -90. : -48., gate ? -10. : 0.);
    const double amount = std::clamp(gate ? controls.gateRange : controls.compMakeup, 0., gate ? 80. : 18.);
    const double release =
      std::clamp(gate ? controls.gateRelease : controls.compRelease, gate ? 20. : 40., gate ? 500. : 600.);
    const double close = Coefficient(release);
    const double attack = std::clamp(punch ? controls.punchAttack : controls.compAttack, .5, 50.);
    const double mix = std::clamp(controls.compMix * .01, 0., 1.);
    const double open = Coefficient(gate ? (tight ? .3 : 1.5) : attack);
    const double detectorAttack = Coefficient(.1), detectorRelease = Coefficient(gate ? 10. : 15.);
    const double hold = mRate * (tight ? .015 : .04);
    if (mFirst)
    {
      mThreshold = threshold;
      mAmount = amount;
      mReduction = gate ? amount : 0.;
      mMix = mix;
      mFirst = false;
    }
    for (int i = 0; i < frames; ++i)
    {
      mThreshold = Smooth(mThreshold, threshold, mSmooth);
      mAmount = Smooth(mAmount, amount, mSmooth);
      const double magnitude = std::abs(samples[i]);
      mEnvelope = Smooth(mEnvelope, magnitude, magnitude > mEnvelope ? detectorAttack : detectorRelease);
      const double level = 20. * std::log10(std::max(1.e-12, mEnvelope));
      double target;
      if (gate)
      {
        if (level >= mThreshold)
        {
          mOpen = true;
          mHold = hold;
        }
        else if (level < mThreshold - 3.)
        {
          mHold = std::max(0., mHold - 1.);
          if (mHold == 0.)
            mOpen = false;
        }
        // Natural expands gradually (2:1); Tight closes to the selected depth.
        target = mOpen ? 0. : tight ? mAmount : std::min(mAmount, std::max(0., mThreshold - level));
      }
      else
        target = CompressionDB(level, mThreshold, punch ? 4. : 2.5, punch ? 3. : 9.);
      // Gates open quickly / close gently; compressors attack reduction / release it.
      const double coefficient = gate ? (target < mReduction ? open : close) : (target > mReduction ? open : close);
      mReduction = Smooth(mReduction, target, coefficient);
      mPeakReduction = std::max(mPeakReduction, mReduction);
      mMix = Smooth(mMix, mix, mSmooth);
      const double gain = std::pow(10., ((gate ? 0. : mAmount) - mReduction) * .05);
      samples[i] *= gate ? gain : 1. + mMix * (gain - 1.);
    }
  }
  double PeakReduction() const { return mPeakReduction; }

private:
  double Coefficient(double ms) const { return std::exp(-1. / (.001 * ms * mRate)); }
  static double Smooth(double value, double target, double coefficient)
  {
    const double result = target + coefficient * (value - target);
    return std::abs(result) < 1.e-30 ? 0. : result;
  }
  double mRate = 48000., mSmooth = 0., mEnvelope = 0., mReduction = 0., mHold = 0.;
  double mThreshold = 0., mAmount = 0., mMix = 1.;
  double mPeakReduction = 0.;
  bool mOpen = false, mFirst = true;
};
