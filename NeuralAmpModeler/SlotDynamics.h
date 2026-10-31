#pragma once
#include <algorithm>
#include <cmath>

// Algorithm references: AudioDSPTools (MIT, Steven Atkinson) and LEAF's
// tCompressor (MIT, Jeff Snyder). Independent mono implementation; no LEAF runtime.
// See docs/module-implementation.md and ThirdPartyNotices.txt for provenance/notices.
enum class SlotEffect
{
  // Persistent IDs saved in projects and presets: append only. Never renumber,
  // reorder or reuse an ID; retire a module by leaving its ID unused.
  NAM = 0,
  NoiseGate = 1,
  SmoothComp = 2,
  PunchComp = 3,
  ToneEQ = 4,
  CutEQ = 5,
  CabinetIR = 6,
  ParametricEQ = 7,
  Boost = 8,
  SynthFilter = 9,
  AutoSwell = 10,
  DigitalDelay = 11,
  AnalogDelay = 12,
  TapeDelay = 13,
  RoomReverb = 14,
  PlateReverb = 15,
  CloudReverb = 16,
  Chorus = 17,
  Flanger = 18,
  Phaser = 19,
  Tremolo = 20,
  Vibrato = 21,
  Tape = 22,
  TubeBoost = 23,
  Utility = 24,
  SpringReverb = 25,
  Wah = 26,
  Rotary = 27,
  UniVibe = 28,
  Limiter = 29,
  Octaver = 30,
  Doubler = 31,
  ReverseDelay = 32,
  DualDelay = 33,
  Freeze = 34,
  Stutter = 35,
  Bitcrusher = 36,
  RingMod = 37,
  ShimmerReverb = 38,
  JunoChorus = 39,
  FrequencyShifter = 40,
  Distortion = 41,
  LoFiTape = 42,
  NoisePrint = 43,
  HallReverb = 44,
  MultiHeadDelay = 45,
  DiffusedDelay = 46,
  AirPlate = 47,
  AirHall = 48,
  GalacticSpace = 49,
  CosmicEcho = 50,
  OutboardChannel = 51,
  ConsoleColour = 52,
  ConsoleEQ = 53,
  ButterComp = 54,
  DigitalBlack = 55,
  PedalDrive = 56,
  PedalComp = 57,
  DynamicEQ = 58,
  TransientShaper = 59,
  HarmonicTremolo = 60,
  Last = HarmonicTremolo,
};
inline const char* SlotEffectName(SlotEffect effect)
{
  switch (effect)
  {
    case SlotEffect::DynamicEQ: return "Dynamic EQ";
    case SlotEffect::TransientShaper: return "Transient Shaper";
    case SlotEffect::HarmonicTremolo: return "Harmonic Tremolo";
    case SlotEffect::NAM: return "NAM";
    case SlotEffect::NoiseGate: return "Noise Gate";
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
    case SlotEffect::HallReverb: return "Hall";
    case SlotEffect::MultiHeadDelay: return "Multi-head Tape";
    case SlotEffect::DiffusedDelay: return "Diffused Delay";
    case SlotEffect::AirPlate: return "Airwindows Plate";
    case SlotEffect::AirHall: return "Airwindows Guitar Hall";
    case SlotEffect::GalacticSpace: return "Galactic Space";
    case SlotEffect::CosmicEcho: return "Cosmic Echo";
    case SlotEffect::OutboardChannel: return "Outboard Channel";
    case SlotEffect::ConsoleColour: return "Console Colour";
    case SlotEffect::ConsoleEQ: return "Console EQ";
    case SlotEffect::ButterComp: return "Butter Comp";
    case SlotEffect::DigitalBlack: return "Digital Black Gate";
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
    case SlotEffect::PedalDrive: return "Pedal Drive";
    case SlotEffect::PedalComp: return "Pedal Comp";
    case SlotEffect::LoFiTape: return "LoFi Tape Machine";
    case SlotEffect::NoisePrint: return "Noise Gate (Smart)";
  }
  return "Unknown";
}
inline bool IsValidSlotEffect(SlotEffect effect)
{ return effect >= SlotEffect::NAM && effect <= SlotEffect::Last; }
struct DynamicsControls
{
  double gateThreshold = -55., gateRelease = 120., gateRange = 60.;
  double compThreshold = -18., compRelease = 120., compMakeup = 0.;
  double compAttack = 8., compMix = 100., punchAttack = 25.;
  double gateAttack = 0;
  double gateHold = 0;
  double gateHysteresis = 3;
  double smoothRatio = 2.5;
  double punchRatio = 4;
  double detectorCut = 0;
  int gateMode = 0; // Natural = 0, Tight = 1
};

class SlotDynamics
{
public:
  void Reset(double rate)
  {
    mRate = std::max(1., rate);
    mSmooth = Coefficient(5.);
    mEnvelope = mReduction = mHold = mDetectorLow = 0.;
    mMinimumMonitorGain = 1.;
    mMakeupDB = 0.; mMakeupGain = 1.;
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
  void Process(double* samples, int frames, SlotEffect effect, const DynamicsControls& controls, double* beforeMakeup = nullptr)
  {
    mMinimumMonitorGain = 1.;
    const bool gate = effect == SlotEffect::NoiseGate;
    const bool tight = controls.gateMode == 1;
    const bool punch = effect == SlotEffect::PunchComp;
    const double threshold =
      std::clamp(gate ? controls.gateThreshold : controls.compThreshold, gate ? -90. : -48., gate ? -10. : 0.);
    const double amount = std::clamp(gate ? controls.gateRange : controls.compMakeup, 0., gate ? 80. : 18.);
    const double release =
      std::clamp(gate ? controls.gateRelease : controls.compRelease, gate ? 20. : 40., gate ? 500. : 600.);
    const double close = Coefficient(release);
    const double attack = std::clamp(punch ? controls.punchAttack : controls.compAttack, .5, 50.);
    const double mix = std::clamp(controls.compMix * .01, 0., 1.);
    const double open = Coefficient(gate ? (controls.gateAttack > 0. ? controls.gateAttack : tight ? .3 : 1.5) : attack);
    const double detectorAttack = Coefficient(.1), detectorRelease = Coefficient(gate ? 10. : 15.);
    const double hold = mRate * (controls.gateHold > 0. ? controls.gateHold * .001 : tight ? .015 : .04);
    const double detectorPole = std::exp(-6.283185307179586 * std::clamp(controls.detectorCut, 0., 500.) / mRate);
    const double ratio = std::clamp(punch ? controls.punchRatio : controls.smoothRatio, 1., 20.);
    const double knee = punch ? 3. : 9.;
    if (mFirst)
    {
      mRatio = ratio; mHysteresis = controls.gateHysteresis;
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
      mRatio = Smooth(mRatio, ratio, mSmooth);
      mHysteresis = Smooth(mHysteresis, std::clamp(controls.gateHysteresis, 0., 12.), mSmooth);
      mDetectorLow = Smooth(mDetectorLow, samples[i], detectorPole);
      const double magnitude = std::abs(!gate && controls.detectorCut > 0. ? samples[i] - mDetectorLow : samples[i]);
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
        else if (level < mThreshold - mHysteresis)
        {
          mHold = std::max(0., mHold - 1.);
          if (mHold == 0.)
            mOpen = false;
        }
        // Natural expands gradually (2:1); Tight closes to the selected depth.
        target = mOpen ? 0. : tight ? mAmount : std::min(mAmount, std::max(0., mThreshold - level));
      }
      else
        target = CompressionDB(level, mThreshold, mRatio, knee);
      // Gates open quickly / close gently; compressors attack reduction / release it.
      const double coefficient = gate ? (target < mReduction ? open : close) : (target > mReduction ? open : close);
      mReduction = Smooth(mReduction, target, coefficient);
      mMix = Smooth(mMix, mix, mSmooth);
      double gain, reduced;
      if (gate)
        reduced = gain = std::pow(10., (0. - mReduction) * .05);
      else
      {
        constexpr double dbToNatural = 0.1151292546497022842; // ln(10) / 20
        // Cache the smoothed makeup, not the target: its existing 5 ms dB ramp
        // must keep following automation sample by sample. Settled makeup needs
        // no conversion, and audio/metering share one reduction exponential.
        if (mAmount != mMakeupDB)
        {
          mMakeupDB = mAmount;
          mMakeupGain = std::exp(mMakeupDB * dbToNatural);
        }
        const double reductionGain = std::exp(-mReduction * dbToNatural);
        gain = mMakeupGain * reductionGain;
        reduced = 1. + mMix * (reductionGain - 1.);
      }
      mMinimumMonitorGain = std::min(mMinimumMonitorGain,reduced);
      if (beforeMakeup) beforeMakeup[i]=samples[i]*reduced;
      samples[i] *= gate ? gain : 1. + mMix * (gain - 1.);
    }
  }
  double PeakReduction() const { return -20.*std::log10(std::max(1.e-12,mMinimumMonitorGain)); }

private:
  double Coefficient(double ms) const { return std::exp(-1. / (.001 * ms * mRate)); }
  static double Smooth(double value, double target, double coefficient)
  {
    const double result = target + coefficient * (value - target);
    return std::abs(result) < 1.e-30 ? 0. : result;
  }
  double mRate = 48000., mSmooth = 0., mEnvelope = 0., mReduction = 0., mHold = 0.;
  double mThreshold = 0., mAmount = 0., mMix = 1.;
  double mDetectorLow = 0., mRatio = 2.5, mHysteresis = 3.;
  double mMinimumMonitorGain = 1.;
  double mMakeupDB = 0., mMakeupGain = 1.;
  bool mOpen = false, mFirst = true;
};
