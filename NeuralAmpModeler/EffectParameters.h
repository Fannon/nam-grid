#pragma once
#include <array>
#include <span>
#include <utility>
#include "EffectModes.h"
#include "TempoSync.h"
#include "SlotDynamics.h"
#include "NoisePrint.h"

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
  uUtilityStereo,
  uUtilityInvertL,
  uUtilityInvertR,
  uDelayPingPong,
  uChorusSpread,
  uReverbStereo,
  uSpringDecay,
  uSpringDwell,
  uSpringDrip,
  uSpringMix,
  uSpringOutput,
  uWahPosition,
  uWahResonance,
  uWahRange,
  uWahMix,
  uWahOutput,
  uUtilityDelay,
  uRotarySlow, uRotaryFast, uRotarySpeed, uRotaryAccel, uRotaryDrive, uRotarySpread, uRotaryMix,
  uVibeMode,
  uLimiterCeiling, uLimiterRelease,
  uOctDry, uOctSub, uOctSub2, uOctTone, uOctOutput,
  uDoubleDetune, uDoubleDelay, uDoubleSpread, uDoubleMix, uDoubleOutput,
  uFreezeHold, uFreezeWindow, uFreezeFade, uFreezeTone, uFreezeMix,
  uStutterTime, uStutterDuty, uStutterFade, uStutterMix,
  uCrushBits, uCrushRate, uCrushTone, uCrushMix, uCrushOutput,
  uRingFrequency, uRingDepth, uRingTone, uRingMix,
  uDualRatio, uDualBalance,
  uEQMode,
  uJunoMode, uJunoMix, uJunoBrightness,
  uShimmerMix, uShimmerDecay, uShimmerTone, uShimmerPitch, uShimmerAmount, uShimmerMod, uShimmerFreeze,
  uShiftFrequency, uShiftFeedback, uShiftDelay, uShiftSide, uShiftTone, uShiftStereo, uShiftMix,
  uDistStyle, uDistDrive, uDistTone, uDistBias, uDistMix, uDistOutput,
  uNoiseLearn,
  uNoiseSensitivity,
  uNoiseReduction,
  uNoiseAttack,
  uNoiseRelease,
  uNoiseMix,
  uNoiseGateThreshold,
  uGateMode,
  uFilterMode,
  uLoFiSpeed, uLoFiWow, uLoFiFlutter, uLoFiNoise, uLoFiWear, uLoFiMix, uLoFiOutput,
  uTapeHeads, uDelayDiffusion,
  uAirPlateInput, uAirPlateSustain, uAirPlateResolution, uAirPlatePredelay, uAirPlateMix, uAirPlateLowCut, uAirPlateOutput,
  uAirHallSustain, uAirHallResolution, uAirHallTone, uAirHallEarly, uAirHallPosition, uAirHallMix, uAirHallOutput,
  uSpaceReplace, uSpaceBrightness, uSpaceDetune, uSpaceSize, uSpaceResolution, uSpaceMix, uSpaceOutput,
  uCosmicTime, uCosmicWarp, uCosmicFeedback, uCosmicDensity, uCosmicTone, uCosmicMotion, uCosmicMix, uCosmicStyle,
  kUtilityControls
};
inline constexpr std::span<const char* const> EffectChoices(int control)
{
  switch (control)
  {
    case uTempoDivision: return grid_tempo::names;
    case uJunoMode: return effect_modes::junoNames;
    case uDistStyle: return effect_modes::distortionNames;
    case uGateMode: return effect_modes::gateNames;
    case uFilterMode: return effect_modes::filterNames;
    case uTapeHeads: return effect_modes::tapeHeadNames;
    case uCosmicStyle: return effect_modes::cosmicNames;
    default: return {};
  }
}
struct EffectParameter
{
  const char* name;
  const char* label;
  double initial, minimum, maximum, step;
  const char* unit;
  int position;
  bool exponential = false;
  double power = 1.;
};
// Read presets/scenes from the preceding audition builds after clarifying
// frequency-cutoff names. The controls and values themselves are unchanged.
inline constexpr std::array<std::pair<int, const char*>, 8> kPreviousCutoffNames{{
  {uDelayTone,"Delay Tone"}, {uOctTone,"Octaver Tone"}, {uFreezeTone,"Freeze Tone"},
  {uCrushTone,"Crusher Tone"}, {uRingTone,"Ring Tone"}, {uShiftTone,"Shifter Tone"},
  {uDistTone,"Distortion Tone"}, {uCosmicTone,"Cosmic Tone"}}};
inline constexpr std::array<EffectParameter, kUtilityControls> kEffectParameters{
  {{"Parametric Frequency", "Frequency", 800, 20, 20000, 1, "Hz", 0, true},
   {"Parametric Gain", "Gain", 0, -12, 12, .1, "dB", 1},
   {"Parametric Q", "Q", .7, .1, 20, .01, "", 2},
   {"Boost Gain", "Gain", 6, 0, 18, .1, "dB", 0},
   {"Boost Tilt", "Tilt", 0, -6, 6, .1, "dB", 1},
   {"Filter Cutoff", "Cutoff", 2500, 40, 12000, 1, "Hz", 1, true},
   {"Filter Resonance", "Resonance", .7, .5, 4, .01, "", 2},
   {"Filter Envelope", "Envelope", 0, 0, 4, .01, "oct", 3},
   {"Swell Rise", "Rise", 250, 10, 2000, 1, "ms", 0, true},
   {"Swell Threshold", "Threshold", -42, -72, -12, .1, "dB", 1},
   {"Swell Release", "Release", 120, 20, 800, 1, "ms", 2, true},
   {"Delay Time", "Time", 350, 5, 4000, 1, "ms", 0, false, 2.},
   {"Delay Feedback", "Feedback", 35, 0, 82, .1, "%", 1},
   {"Delay High Cut", "High cut", 5000, 800, 12000, 1, "Hz", 2, true},
   {"Delay Character", "Character", 20, 0, 100, .1, "%", 3},
   {"Delay Mix", "Mix", 25, 0, 100, .1, "%", 5},
   {"Reverb Decay", "Decay", 2, .15, 30, .01, "s", 0, true},
   {"Reverb Size", "Size", 100, 25, 200, .1, "%", 1},
   {"Reverb Tone", "Damping", 5500, 800, 12000, 1, "Hz", 2, true},
   {"Reverb Predelay", "Pre-delay", 15, 0, 200, .1, "ms", 3},
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
   {"Tube Asymmetry", "Asymmetry", 25, 0, 100, .1, "%", 3},
   {"Tube Mix", "Mix", 100, 0, 100, .1, "%", 4},
   {"Tube Output", "Output", 0, -18, 18, .1, "dB", 5},
   {"Punch Attack", "Attack", 25, .5, 50, .1, "ms", 3, true},
   {"Tempo Sync", "Sync", 0, 0, 1, 1, "", 5},
   {"Tempo Division", "Division", 5, 0, 9, 1, "", 0},
   {"Utility Output", "Output", 0, -60, 12, .1, "dB", 0},
   {"Utility Balance", "L/R balance", 0, -100, 100, .1, "%", 1},
   {"Utility Width", "Width", 0, -100, 100, .1, "%", 2},
   {"Utility Invert L", "Invert L", 0, 0, 1, 1, "", 3},
   {"Utility Invert R", "Invert R", 0, 0, 1, 1, "", 4},
   {"Delay Ping Pong", "Ping-pong", 0, 0, 1, 1, "", 4},
   {"Chorus Spread", "Spread", 75, 0, 100, .1, "%", 5},
   {"Reverb Stereo", "Stereo wet", 1, 0, 1, 1, "", 4},
   {"Spring Decay", "Decay", 1.8, .3, 4., .01, "s", 0, true},
   {"Spring Dwell", "Dwell", 3, 0, 12, .1, "dB", 1},
   {"Spring Drip", "Drip", 60, 0, 100, .1, "%", 3},
   {"Spring Mix", "Mix", 20, 0, 100, .1, "%", 5},
   {"Spring Output", "Output", 0, -12, 12, .1, "dB", 6},
   {"Wah Position", "Position", 50, 0, 100, .1, "%", 0},
   {"Wah Resonance", "Resonance", 3, 1, 6, .01, "Q", 1},
   {"Wah Register", "Register", 50, 0, 100, .1, "%", 2},
   {"Wah Mix", "Mix", 100, 0, 100, .1, "%", 3},
   {"Wah Output", "Output", 0, -12, 12, .1, "dB", 4},
   {"Utility Delay", "Fine delay", 0, 0, 20, .01, "ms", 5},
   {"Rotary Slow", "Slow speed", .8, .1, 2, .01, "Hz", 0},
   {"Rotary Fast", "Fast speed", 6, 2, 10, .01, "Hz", 1},
   {"Rotary Speed", "Fast", 0, 0, 1, 1, "", 2},
   {"Rotary Ramp Time", "Ramp time", 1, .1, 5, .01, "s", 3},
   {"Rotary Drive", "Drive", 3, 0, 18, .1, "dB", 4},
   {"Rotary Spread", "Spread", 80, 0, 100, .1, "%", 5},
   {"Rotary Mix", "Mix", 75, 0, 100, .1, "%", 6},
   {"Vibe Vibrato", "Vibrato mode", 0, 0, 1, 1, "", 5},
   {"Limiter Ceiling", "Ceiling", -1, -24, 0, .1, "dB", 0},
   {"Limiter Release", "Release", 75, 10, 500, 1, "ms", 1, true},
   {"Octaver Dry", "Dry", 100, 0, 100, .1, "%", 0},
   {"Octaver 1 Oct Down", "1 oct down", 50, 0, 100, .1, "%", 1},
   {"Octaver 2 Oct Down", "2 oct down", 0, 0, 100, .1, "%", 2},
   {"Octaver High Cut", "High cut", 1000, 80, 5000, 1, "Hz", 3, true},
   {"Octaver Output", "Output", 0, -24, 6, .1, "dB", 4},
   {"Doubler Detune", "Detune", 9, 1, 25, .1, "cents", 0},
   {"Doubler Delay", "Delay", 12, 1, 30, .1, "ms", 1},
   {"Doubler Spread", "Spread", 100, 0, 100, .1, "%", 2},
   {"Doubler Mix", "Mix", 35, 0, 100, .1, "%", 3},
   {"Doubler Output", "Output", 0, -24, 6, .1, "dB", 4},
   {"Freeze Hold", "Hold", 0, 0, 1, 1, "", 0},
   {"Freeze Window", "Capture", 500, 50, 2000, 1, "ms", 1, true},
   {"Freeze Crossfade", "Crossfade", 20, 1, 100, .1, "ms", 2},
   {"Freeze High Cut", "High cut", 6000, 200, 16000, 1, "Hz", 3, true},
   {"Freeze Level", "Held level", 35, 0, 100, .1, "%", 4},
   {"Stutter Time", "Window", 125, 10, 1000, 1, "ms", 0, true},
   {"Stutter Gate Length", "Gate length", 75, 5, 100, .1, "%", 1},
   {"Stutter Edge Fade", "Edge fade", 5, .1, 20, .1, "ms", 2},
   {"Stutter Mix", "Mix", 50, 0, 100, .1, "%", 3},
   {"Crusher Bits", "Bits", 8, 2, 16, 1, "bits", 0},
   {"Crusher Rate", "Sample rate", 12000, 100, 48000, 1, "Hz", 1, true},
   {"Crusher High Cut", "High cut", 8000, 200, 20000, 1, "Hz", 2, true},
   {"Crusher Mix", "Mix", 100, 0, 100, .1, "%", 3},
   {"Crusher Output", "Output", 0, -24, 6, .1, "dB", 4},
   {"Ring Frequency", "Frequency", 440, 1, 5000, .1, "Hz", 0, true},
   {"Ring Depth", "Depth", 100, 0, 100, .1, "%", 1},
   {"Ring High Cut", "High cut", 12000, 200, 20000, 1, "Hz", 2, true},
   {"Ring Mix", "Mix", 50, 0, 100, .1, "%", 3},
   {"Dual Delay Ratio", "Time ratio", 1.5, .25, 2, .01, "x", 3},
   {"Dual Delay Balance", "Tap balance", 50, 0, 100, .1, "%", 4},
   {"Parametric Notch", "Notch mode", 0, 0, 1, 1, "", 5},
   {"Juno Mode", "Mode", 1, 0, 2, 1, "", 0},
   {"Juno Mix", "Mix", 50, 0, 100, .1, "%", 1},
   {"Juno Brightness", "Brightness", 100, 0, 100, .1, "%", 2},
   {"Shimmer Mix", "Mix", 35, 0, 100, .1, "%", 0},
   {"Shimmer Decay", "Decay", 3.5, .5, 20, .01, "s", 1, true},
   {"Shimmer Damping", "Damping", 5000, 800, 12000, 1, "Hz", 2, true},
   {"Shimmer Pitch", "Pitch", 12, -24, 24, 1, "st", 3},
   {"Shimmer Amount", "Amount", 35, 0, 100, .1, "%", 4},
   {"Shimmer Mod", "Modulation", 25, 0, 100, .1, "%", 5},
   {"Shimmer Freeze", "Freeze", 0, 0, 1, 1, "", 6},
   {"Shifter Frequency", "Shift", 0, -2000, 2000, 1, "Hz", 0},
   {"Shifter Feedback", "Feedback", 35, 0, 100, .1, "%", 1},
   {"Shifter Delay", "Delay", 120, .05, 1000, .01, "ms", 2, true},
   {"Shifter Sideband", "Sideband", 100, -100, 100, .1, "%", 3},
   {"Shifter High Cut", "High cut", 8000, 200, 12000, 1, "Hz", 4, true},
   {"Shifter Spread", "Spread", 0, 0, 100, .1, "%", 5},
   {"Shifter Mix", "Mix", 50, 0, 100, .1, "%", 6},
   {"Distortion Style", "Style", 0, 0, effect_modes::distortionCount - 1, 1, "", 0},
   {"Distortion Drive", "Drive", 12, 0, 36, .1, "dB", 1},
   {"Distortion High Cut", "High cut", 8000, 200, 12000, 1, "Hz", 2, true},
   {"Distortion Bias", "Bias", 0, -100, 100, .1, "%", 3},
   {"Distortion Mix", "Mix", 100, 0, 100, .1, "%", 4},
   {"Distortion Output", "Output", -6, -24, 6, .1, "dB", 5},
   {"Noise Learn", "Learn noise", 0, 0, 1, 1, "", 0},
   {"Noise Sensitivity", "Sensitivity", 1.5, -6, 12, .1, "dB", 1},
   {"Noise Reduction", "Reduction", 24, 0, 48, .1, "dB", 2},
   {"Noise Attack", "Attack", 2, .2, 20, .1, "ms", 3, true},
   {"Noise Release", "Release", 120, 20, 500, 1, "ms", 4, true},
   {"Noise Mix", "Mix", 100, 0, 100, .1, "%", 6},
   {"Noise Gate Threshold", "Gate threshold", -100, -100, 0, .1, "dB", 5},
   {"Gate Mode", "Mode", 0, 0, 1, 1, "", 3},
   {"Filter Mode", "Mode", 0, 0, 2, 1, "", 0},
   {"LoFi Speed", "Speed", 7.5, 1.875, 30, .001, "ips", 0, true},
   {"LoFi Wow", "Wow", 25, 0, 100, .1, "%", 1},
   {"LoFi Flutter", "Flutter", 20, 0, 100, .1, "%", 2},
   {"LoFi Noise", "Noise", 20, 0, 100, .1, "%", 3},
   {"LoFi Wear", "Wear", 25, 0, 100, .1, "%", 4},
   {"LoFi Mix", "Mix", 100, 0, 100, .1, "%", 5},
   {"LoFi Output", "Output", 0, -24, 6, .1, "dB", 6},
   {"Tape Heads", "Heads", 1, 0, 7, 1, "", 3},
   {"Delay Diffusion", "Diffusion", 50, 0, 100, .1, "%", 3},
   {"Air Plate Input", "Input", 0, -18, 0, .1, "dB", 0},
   {"Air Plate Sustain", "Sustain", 50, 0, 100, .1, "%", 1},
   {"Air Plate Resolution", "Resolution", 100, 25, 100, .1, "%", 2},
   {"Air Plate Pre-delay", "Pre-delay", 15, 0, 200, .1, "ms", 3},
   {"Air Plate Mix", "Mix", 18, 0, 100, .1, "%", 4},
   {"Air Plate Low Cut", "Low cut", 100, 0, 500, 1, "Hz", 5},
   {"Air Plate Output", "Output", 0, -18, 6, .1, "dB", 6},
   {"Air Hall Sustain", "Sustain", 50, 0, 95, .1, "%", 0},
   {"Air Hall Resolution", "Resolution", 100, 25, 100, .1, "%", 1},
   {"Air Hall Brightness", "Brightness", 50, 0, 100, .1, "%", 2},
   {"Air Hall Early Level", "Early level", 50, 0, 100, .1, "%", 3},
   {"Air Hall Position", "Position", 50, 0, 100, 1, "%", 4},
   {"Air Hall Mix", "Mix", 20, 0, 100, .1, "%", 5},
   {"Air Hall Output", "Output", 0, -18, 6, .1, "dB", 6},
   {"Space Replace", "Replace", 50, 5, 100, .1, "%", 0},
   {"Space Brightness", "Brightness", 50, 5, 100, .1, "%", 1},
   {"Space Detune", "Detune", 35, 0, 100, .1, "%", 2},
   {"Space Size", "Size", 60, 5, 100, .1, "%", 3},
   {"Space Resolution", "Resolution", 100, 25, 100, .1, "%", 4},
   {"Space Mix", "Mix", 25, 0, 100, .1, "%", 5},
   {"Space Output", "Output", 0, -18, 6, .1, "dB", 6},
   {"Cosmic Time", "Time", 500, 20, 8000, 1, "ms", 0, false, 2.},
   {"Cosmic Warp", "Warp", 65, 0, 100, .1, "%", 1},
   {"Cosmic Feedback", "Feedback", 65, 0, 98, .1, "%", 2},
   {"Cosmic Density", "Density", 80, 0, 100, .1, "%", 3},
   {"Cosmic High Cut", "High cut", 6000, 200, 16000, 1, "Hz", 4, true},
   {"Cosmic Motion", "Motion", 25, 0, 100, .1, "%", 5},
   {"Cosmic Mix", "Mix", 25, 0, 100, .1, "%", 6},
   {"Cosmic Style", "Style", 0, 0, effect_modes::cosmicCount-1, 1, "", 0}}};
static_assert([] {
  for (int i = 0; i < kUtilityControls; ++i)
  {
    const auto& p = kEffectParameters[i];
    if (!p.name || !p.label || p.minimum > p.initial || p.initial > p.maximum
        || p.step <= 0. || p.power <= 0. || p.position < 0 || p.position >= 7) return false;
    const auto choices = EffectChoices(i);
    if (!choices.empty() && (p.minimum != 0. || p.maximum != choices.size() - 1 || p.step != 1.)) return false;
  }
  return true;
}(), "Every utility parameter needs a valid descriptor and one of the seven editor positions");
inline bool IsDelay(SlotEffect e)
{
  switch (e)
  {
    case SlotEffect::DigitalDelay: case SlotEffect::AnalogDelay: case SlotEffect::TapeDelay:
    case SlotEffect::ReverseDelay: case SlotEffect::DualDelay: case SlotEffect::MultiHeadDelay: case SlotEffect::DiffusedDelay: return true;
    default: return false;
  }
}
inline bool IsReverb(SlotEffect e)
{
  switch (e)
  {
    case SlotEffect::RoomReverb: case SlotEffect::PlateReverb:
    case SlotEffect::CloudReverb: case SlotEffect::HallReverb: case SlotEffect::SpringReverb:
    case SlotEffect::AirPlate: case SlotEffect::AirHall: case SlotEffect::GalacticSpace: return true;
    default: return false;
  }
}
inline bool IsModulation(SlotEffect e)
{
  switch (e)
  {
    case SlotEffect::Chorus: case SlotEffect::Flanger: case SlotEffect::Phaser:
    case SlotEffect::Tremolo: case SlotEffect::Vibrato: return true;
    default: return false;
  }
}
// These processors own both channel histories or a linked detector. The other
// built-ins use separate Prepared channel instances; enum order is immaterial.
inline bool UsesCoupledStereo(SlotEffect e)
{
  switch (e)
  {
    case SlotEffect::Rotary: case SlotEffect::Limiter: case SlotEffect::Octaver:
    case SlotEffect::Doubler: case SlotEffect::ReverseDelay: case SlotEffect::DualDelay:
    case SlotEffect::Freeze: case SlotEffect::Stutter: case SlotEffect::ShimmerReverb:
    case SlotEffect::JunoChorus:
    case SlotEffect::FrequencyShifter:
    case SlotEffect::NoisePrint: case SlotEffect::LoFiTape:
    case SlotEffect::RoomReverb: case SlotEffect::PlateReverb: case SlotEffect::CloudReverb: case SlotEffect::HallReverb:
    case SlotEffect::AirPlate: case SlotEffect::AirHall: case SlotEffect::GalacticSpace:
    case SlotEffect::MultiHeadDelay: case SlotEffect::DiffusedDelay: case SlotEffect::CosmicEcho: return true;
    default: return false;
  }
}
inline bool UsesUtility(SlotEffect e, int p)
{
  if (e == SlotEffect::CosmicEcho) return p >= uCosmicTime && p <= uCosmicStyle;
  if (e == SlotEffect::AirPlate) return p >= uAirPlateInput && p <= uAirPlateOutput;
  if (e == SlotEffect::AirHall) return p >= uAirHallSustain && p <= uAirHallOutput;
  if (e == SlotEffect::GalacticSpace) return p >= uSpaceReplace && p <= uSpaceOutput;
  if (e == SlotEffect::MultiHeadDelay)
    return (p >= uDelayTime && p <= uDelayMix) || p == uDelayMotion || p == uTapeHeads || p == uDelayOutput;
  if (e == SlotEffect::DiffusedDelay)
    return (p >= uDelayTime && p <= uDelayTone) || p == uDelayDiffusion || p == uDelayMotion || p == uDelayMix || p == uDelayOutput;
  if (e == SlotEffect::LoFiTape) return p >= uLoFiSpeed && p <= uLoFiOutput;
  if (e == SlotEffect::NaturalGate || e == SlotEffect::TightGate) return p == uGateMode;
  if (e == SlotEffect::Distortion) return p >= uDistStyle && p <= uDistOutput;
  if (e == SlotEffect::Rotary) return p >= uRotarySlow && p <= uRotaryMix;
  if (e == SlotEffect::UniVibe) return p == uModRate || p == uModDepth || p == uModCharacter || p == uModMix || p == uVibeMode;
  if (e == SlotEffect::Limiter) return p >= uLimiterCeiling && p <= uLimiterRelease;
  if (e == SlotEffect::Octaver) return p >= uOctDry && p <= uOctOutput;
  if (e == SlotEffect::Doubler) return p >= uDoubleDetune && p <= uDoubleOutput;
  if (e == SlotEffect::Freeze) return p >= uFreezeHold && p <= uFreezeMix;
  if (e == SlotEffect::Stutter) return p >= uStutterTime && p <= uStutterMix;
  if (e == SlotEffect::Bitcrusher) return p >= uCrushBits && p <= uCrushOutput;
  if (e == SlotEffect::RingMod) return p >= uRingFrequency && p <= uRingMix;
  if (e == SlotEffect::JunoChorus) return p >= uJunoMode && p <= uJunoBrightness;
  if (e == SlotEffect::ShimmerReverb) return p >= uShimmerMix && p <= uShimmerFreeze;
  if (e == SlotEffect::FrequencyShifter) return p >= uShiftFrequency && p <= uShiftMix;
  if (e == SlotEffect::ReverseDelay) return p == uDelayTime || p == uDelayTone || p == uDelayMix || p == uDelayOutput;
  if (e == SlotEffect::DualDelay) return p == uDelayTime || p == uDelayFeedback || p == uDelayTone || p == uDelayMix || p == uDelayOutput || p == uDualRatio || p == uDualBalance;
  if (e == SlotEffect::NoisePrint)
    return p >= uNoiseLearn && p <= uNoiseGateThreshold;
  if (e == SlotEffect::SpringReverb)
    return (p >= uSpringDecay && p <= uSpringOutput) || p == uReverbTone;
  if (e == SlotEffect::Wah)
    return p >= uWahPosition && p <= uWahOutput;
  if (e == SlotEffect::Chorus && p == uChorusSpread)
    return true;
  if (e == SlotEffect::Utility)
    return (p >= uUtilityLevel && p <= uUtilityInvertR) || p == uUtilityDelay;
  if (e == SlotEffect::ParametricEQ)
    return (p >= uFrequency && p <= uQ) || p == uEQMode;
  if (e == SlotEffect::Boost)
    return p == uBoostGain || p == uBoostTilt;
  if (e == SlotEffect::TubeBoost)
    return p == uBoostTilt || (p >= uTubeDrive && p <= uTubeOutput);
  if (e == SlotEffect::SmoothComp)
    return p == uCompAttack || p == uCompMix;
  if (e == SlotEffect::PunchComp)
    return p == uPunchAttack || p == uCompMix;
  if (e == SlotEffect::SynthFilter)
    return (p >= uFilterCutoff && p <= uFilterEnvelope) || p == uFilterMode;
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
  int filterMode = 0;
  double rise = 250., threshold = -42., release = 120.;
  std::array<double, 5> delay{350., 35., 5000., 20., 25.};
  std::array<double, 5> reverb{2., 100., 5500., 15., 20.};
  std::array<double, 5> modulation{.8, 50., 20., 0., 50.};
  std::array<double, 4> tape{0., 50., 100., 0.};
  double delayMotion = 20., delayOutput = 0., reverbMotion = 25., reverbHighPass = 80.;
  std::array<double, 4> tube{6., 25., 100., 0.};
  bool sync = false, invertL = false, invertR = false, pingPong = false, stereoReverb = true;
  double chorusSpread = 75.;
  std::array<double, 5> spring{1.8, 3., 60., 20., 0.};
  std::array<double, 5> wah{50., 3., 50., 100., 0.};
  int division = 5;
  double tempo = 120., level = 0., balance = 0., stereo = 0.;
  double fineDelay = 0., vibeMode = 0., eqMode = 0.;
  std::array<double, 7> rotary{.8, 6., 0., 1., 3., 80., 75.};
  std::array<double, 2> limiter{-1., 75.};
  std::array<double, 5> octaver{100., 50., 0., 1000., 0.}, doubler{9., 12., 100., 35., 0.};
  std::array<double, 5> freeze{0., 500., 20., 6000., 35.};
  std::array<double, 4> stutter{125., 75., 5., 50.}, ring{440., 100., 12000., 50.};
  std::array<double, 5> crusher{8., 12000., 8000., 100., 0.};
  std::array<double, 2> dual{1.5, 50.};
  std::array<double, 3> juno{1., 50., 100.};
  std::array<double, 7> shimmer{35., 3.5, 5000., 12., 35., 25., 0.};
  std::array<double, 7> shift{0., 35., 120., 100., 8000., 0., 50.};
  std::array<double, 6> distortion{0., 12., 8000., 0., 100., -6.};
  noise_print::Controls noise;
  std::array<double, 7> lofi{7.5, 25., 20., 20., 25., 100., 0.};
  int tapeHeads = 1;
  double delayDiffusion = 50.;
  std::array<double, 7> airPlate{0., 50., 100., 15., 18., 100., 0.};
  std::array<double, 7> airHall{50., 100., 50., 50., 50., 20., 0.};
  std::array<double, 7> space{50., 50., 35., 60., 100., 25., 0.};
  std::array<double, 7> cosmic{500., 65., 65., 80., 6000., 25., 25.};
  int cosmicStyle = 0;
};
