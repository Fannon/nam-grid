#pragma once
#include <array>
#include "SlotDynamics.h"

// Shared within a family, independent for every grid slot. Descriptors also drive
// the editor, so parameter ranges, defaults and the eight control positions agree.
enum UtilityParam
{
  uFrequency,
  uGain,
  uQ,
  uBoostGain,
  uBoostTilt,
  uFilterCutoff,
  uFilterResonance,
  uFilterEnvelope,
  uSwellRise,
  uSwellThreshold,
  uSwellRelease,
  uDelayTime,
  uDelayFeedback,
  uDelayTone,
  uDelayCharacter,
  uDelayMix,
  uReverbDecay,
  uReverbSize,
  uReverbTone,
  uReverbPredelay,
  uReverbMix,
  uModRate,
  uModDepth,
  uModFeedback,
  uModCharacter,
  uModMix,
  uTapeDrive,
  uTapeBump,
  uTapeMix,
  uTapeOutput,
  uCompAttack,
  uCompMix,
  uDelayMotion,
  uDelayOutput,
  uReverbMotion,
  uReverbHighPass,
  uTubeDrive,
  uTubeBias,
  uTubeMix,
  uTubeOutput,
  uPunchAttack,
  uTempoSync,
  uTempoDivision,
  uUtilityLevel,
  uUtilityBalance,
  uUtilityWidth,
  uUtilityInvertL,
  uUtilityInvertR,
  kUtilityControls
};
struct EffectParameter
{
  const char* name;
  const char* label;
  double initial, minimum, maximum, step;
  const char* unit;
  int position;
  bool exponential = false;
};
inline constexpr std::array<EffectParameter, kUtilityControls> kEffectParameters{
  {{"Parametric Frequency", "Frequency", 800, 80, 8000, 1, "Hz", 0, true},
   {"Parametric Gain", "Gain", 0, -12, 12, .1, "dB", 1},
   {"Parametric Q", "Q", .7, .3, 4, .01, "", 2},
   {"Boost Gain", "Gain", 6, 0, 18, .1, "dB", 0},
   {"Boost Tilt", "Tilt", 0, -6, 6, .1, "dB", 1},
   {"Filter Cutoff", "Cutoff", 2500, 40, 12000, 1, "Hz", 0, true},
   {"Filter Resonance", "Resonance", .7, .5, 4, .01, "", 1},
   {"Filter Envelope", "Envelope", 0, 0, 4, .01, "oct", 2},
   {"Swell Rise", "Rise", 250, 10, 1200, 1, "ms", 0, true},
   {"Swell Threshold", "Threshold", -42, -72, -12, .1, "dB", 1},
   {"Swell Release", "Release", 120, 20, 800, 1, "ms", 2, true},
   {"Delay Time", "Time", 350, 20, 1500, 1, "ms", 0, true},
   {"Delay Feedback", "Feedback", 35, 0, 82, .1, "%", 1},
   {"Delay Tone", "Tone", 5000, 800, 12000, 1, "Hz", 2, true},
   {"Delay Character", "Character", 20, 0, 100, .1, "%", 3},
   {"Delay Mix", "Mix", 25, 0, 100, .1, "%", 5},
   {"Reverb Decay", "Decay", 2, .25, 10, .01, "s", 0, true},
   {"Reverb Size", "Size", 100, 50, 150, .1, "%", 1},
   {"Reverb Tone", "Damping", 5500, 800, 12000, 1, "Hz", 2, true},
   {"Reverb Predelay", "Pre-delay", 15, 0, 100, .1, "ms", 3},
   {"Reverb Mix", "Mix", 20, 0, 100, .1, "%", 6},
   {"Mod Rate", "Rate", .8, .05, 10, .01, "Hz", 0, true},
   {"Mod Depth", "Depth", 50, 0, 100, .1, "%", 1},
   {"Mod Feedback", "Feedback", 20, 0, 75, .1, "%", 2},
   {"Mod Character", "Character", 0, 0, 100, .1, "%", 3},
   {"Mod Mix", "Mix", 50, 0, 100, .1, "%", 4},
   {"Tape Drive", "Drive", 0, -12, 12, .1, "dB", 0},
   {"Tape Bump", "Bump", 50, 0, 100, .1, "%", 1},
   {"Tape Mix", "Mix", 100, 0, 100, .1, "%", 2},
   {"Tape Output", "Output", 0, -18, 6, .1, "dB", 3},
   {"Comp Attack", "Attack", 8, .5, 50, .1, "ms", 3, true},
   {"Comp Mix", "Mix", 100, 0, 100, .1, "%", 4},
   {"Delay Modulation", "Modulation", 20, 0, 100, .1, "%", 4},
   {"Delay Output", "Output", 0, -18, 6, .1, "dB", 6},
   {"Reverb Modulation", "Modulation", 25, 0, 100, .1, "%", 4},
   {"Reverb High Pass", "Low cut", 80, 0, 500, 1, "Hz", 5},
   {"Tube Drive", "Drive", 6, 0, 24, .1, "dB", 0},
   {"Tube Bias", "Bias", 25, 0, 100, .1, "%", 3},
   {"Tube Mix", "Mix", 100, 0, 100, .1, "%", 4},
   {"Tube Output", "Output", 0, -18, 18, .1, "dB", 5},
   {"Punch Attack", "Attack", 25, .5, 50, .1, "ms", 3, true},
   {"Tempo Sync", "Sync", 0, 0, 1, 1, "", 5},
   {"Tempo Division", "Division", 4, 0, 8, 1, "", 0},
   {"Utility Level", "Level", 0, -60, 12, .1, "dB", 0},
   {"Utility Balance", "L/R balance", 0, -100, 100, .1, "%", 1},
   {"Utility Width", "Width", 100, 0, 200, .1, "%", 2},
   {"Utility Invert L", "Invert L", 0, 0, 1, 1, "", 3},
   {"Utility Invert R", "Invert R", 0, 0, 1, 1, "", 4}}};
inline bool IsDelay(SlotEffect e)
{ return e >= SlotEffect::DigitalDelay && e <= SlotEffect::TapeDelay; }
inline bool IsReverb(SlotEffect e)
{ return e >= SlotEffect::RoomReverb && e <= SlotEffect::CloudReverb; }
inline bool IsModulation(SlotEffect e)
{ return e >= SlotEffect::Chorus && e <= SlotEffect::Vibrato; }
inline bool UsesUtility(SlotEffect e, int p)
{
  if (e == SlotEffect::Utility)
    return p >= uUtilityLevel && p <= uUtilityInvertR;
  if (e == SlotEffect::ParametricEQ)
    return p >= uFrequency && p <= uQ;
  if (e == SlotEffect::Boost)
    return p == uBoostGain || p == uBoostTilt;
  if (e == SlotEffect::TubeBoost)
    return p == uBoostTilt || (p >= uTubeDrive && p <= uTubeOutput);
  if (e == SlotEffect::SmoothComp)
    return p == uCompAttack || p == uCompMix;
  if (e == SlotEffect::PunchComp)
    return p == uPunchAttack || p == uCompMix;
  if (e == SlotEffect::SynthFilter)
    return p >= uFilterCutoff && p <= uFilterEnvelope;
  if (e == SlotEffect::AutoSwell)
    return p >= uSwellRise && p <= uSwellRelease;
  if (IsDelay(e))
    return (p >= uDelayTime && p <= uDelayMix) || p == uDelayOutput
           || (p == uDelayMotion && e != SlotEffect::DigitalDelay);
  if (IsReverb(e))
    return (p >= uReverbDecay && p <= uReverbMix) || p == uReverbMotion || p == uReverbHighPass;
  if (IsModulation(e))
    return p >= uModRate && p <= uModMix && (p != uModMix || e != SlotEffect::Vibrato)
           && (p != uModFeedback || e == SlotEffect::Flanger || e == SlotEffect::Phaser);
  if (e == SlotEffect::Tape)
    return p >= uTapeDrive && p <= uTapeOutput;
  return false;
}
struct UtilityControls
{
  double frequency = 800., gain = 0., q = .7, boost = 6., tilt = 0.;
  double cutoff = 2500., resonance = .7, envelope = 0.;
  double rise = 250., threshold = -42., release = 120.;
  std::array<double, 5> delay{350., 35., 5000., 20., 25.};
  std::array<double, 5> reverb{2., 100., 5500., 15., 20.};
  std::array<double, 5> modulation{.8, 50., 20., 0., 50.};
  std::array<double, 4> tape{0., 50., 100., 0.};
  double delayMotion = 20., delayOutput = 0., reverbMotion = 25., reverbHighPass = 80.;
  std::array<double, 4> tube{6., 25., 100., 0.};
  bool sync = false, invertL = false, invertR = false;
  int division = 4;
  double tempo = 120., level = 0., balance = 0., width = 100.;
};
