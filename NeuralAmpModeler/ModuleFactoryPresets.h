#pragma once
#include "ParameterLayout.h"
#include <initializer_list>
#include <span>

namespace module_presets
{
struct Value
{
  int control; // Slot-control index, in SlotValues order.
  double value;
};
struct Preset
{
  const char* name;
  std::initializer_list<Value> values;
};
constexpr int U(int p) { return parameter_layout::standardControls + p; }
using parameter_layout::kSlotCompThreshold;
using parameter_layout::kSlotCompRelease;
using parameter_layout::kSlotCompMakeup;

// Each module offers a few established, clearly different starting points rather than every
// variation; a module whose insertion default already is its usual starting point may have
// none. Settings follow published pedal-manual starting points, documented player settings and
// professional plugin presets, translated to these controls and checked on a guitar DI. Names
// describe the result. Values use NAM Grid units; unlisted controls use insertion defaults.
// These arrays and their order are NOT persistent identities; only applied values are saved.

// Pedal Comp's Sustain range is calibrated against a Ross-type pedal. Pickup stays as the
// player set it unless a preset is for a particular instrument.
inline const Preset pedalComp[]{
  {"Light always-on",
   {{U(uPedalCompSustain), 30.}, {U(uPedalCompAttack), 0.}, {U(uPedalCompFocus), 20.}, {U(uPedalCompBlend), 50.}}},
  // Fast attack on the pick and a fully compressed signal: the country "cluck".
  {"Snappy clean picking",
   {{U(uPedalCompSustain), 60.}, {U(uPedalCompAttack), 0.}, {U(uPedalCompFocus), -30.}, {U(uPedalCompBlend), 100.}}},
  {"Long lead notes",
   {{U(uPedalCompSustain), 75.}, {U(uPedalCompAttack), 1.}, {U(uPedalCompFocus), -20.}, {U(uPedalCompBlend), 60.}}},
  {"Acoustic strumming",
   {{U(uPedalCompPickup), 2.}, {U(uPedalCompSustain), 35.}, {U(uPedalCompAttack), 1.}, {U(uPedalCompFocus), 30.},
    {U(uPedalCompBlend), 85.}}},
};
// Thresholds assume typical notes near -18 dBFS on these peak detectors (a DAW peak meter
// around -9 dBFS). On a guitar DI, typical notes come down by about 3-6 dB with makeup near
// bypass loudness.
inline const Preset smoothComp[]{
  {"Clean guitar leveling",
   {{kSlotCompThreshold, -22.}, {kSlotCompRelease, 150.}, {kSlotCompMakeup, 4.5}, {U(uCompAttack), 8.},
    {U(uSmoothRatio), 3.5}}},
  {"Even bass notes",
   {{kSlotCompThreshold, -22.}, {kSlotCompRelease, 200.}, {kSlotCompMakeup, 4.5}, {U(uCompAttack), 15.},
    {U(uSmoothRatio), 4.}}},
};
inline const Preset punchComp[]{
  {"Tight picking",
   {{kSlotCompThreshold, -22.}, {kSlotCompRelease, 40.}, {kSlotCompMakeup, 4.}, {U(uPunchAttack), 2.},
    {U(uPunchRatio), 3.6}, {U(uCompMix), 100.}}},
  // Heavy compression blended under the dry signal ("New York" compression).
  {"Parallel punch",
   {{kSlotCompThreshold, -28.}, {kSlotCompRelease, 100.}, {kSlotCompMakeup, 6.}, {U(uPunchAttack), 20.},
    {U(uPunchRatio), 6.}, {U(uCompMix), 50.}}},
};
inline const Preset butterComp[]{
  {"Thick parallel", {{U(uButterCompress), 100.}, {U(uButterOutput), 0.}, {U(uButterMix), 50.}}},
};
inline const Preset limiter[]{
  {"Louder, controlled",
   {{U(uLimiterInput), 6.}, {U(uLimiterCeiling), -1.}, {U(uLimiterRelease), 100.}, {U(uLimiterClip), 30.}}},
};

inline const Preset toneEQ[]{
  // The "smile" curve: fuller lows and highs around scooped low mids.
  {"Scooped mids",
   {{parameter_layout::kSlotEQBass, 3.}, {parameter_layout::kSlotEQMid, -4.}, {U(uToneMidFrequency), 500.},
    {parameter_layout::kSlotEQTreble, 2.}}},
  {"Mid-forward lead", {{parameter_layout::kSlotEQBass, -1.5}, {parameter_layout::kSlotEQMid, 3.}, {U(uEQOutput), -2.}}},
  {"Dark jazz clean", {{parameter_layout::kSlotEQBass, 1.5}, {parameter_layout::kSlotEQTreble, -5.}}},
};
inline const Preset cutEQ[]{
  {"Tight high gain", {{parameter_layout::kSlotEQHighPass, 110.}, {parameter_layout::kSlotEQLowPass, 7000.}}},
  {"Bass cleanup", {{parameter_layout::kSlotEQHighPass, 35.}, {parameter_layout::kSlotEQLowPass, 12000.}}},
};
inline const Preset parametricEQ[]{
  {"Low-mid mud cut", {{U(uFrequency), 300.}, {U(uGain), -3.}, {U(uQ), .8}, {parameter_layout::kSlotEQHighPass, 60.}}},
  {"Harshness cut", {{U(uFrequency), 3500.}, {U(uGain), -3.}, {U(uQ), 1.5}}},
};
inline const Preset filter[]{
  {"Touch wah", {{U(uFilterCutoff), 500.}, {U(uFilterResonance), 2.}, {U(uFilterEnvelope), 2.}}},
};
// A parked ("cocked") wah used as a fixed tone filter.
inline const Preset wah[]{
  {"Cocked wah, throaty", {{U(uWahPosition), 30.}, {U(uWahResonance), 2.5}, {U(uWahRange), 40.}, {U(uWahOutput), 3.}}},
  {"Cocked wah, lead bite",
   {{U(uWahPosition), 70.}, {U(uWahResonance), 3.5}, {U(uWahRange), 60.}, {U(uWahMix), 70.}, {U(uWahOutput), 3.}}},
};

inline const Preset boost[]{
  {"Tight amp push", {{U(uBoostGain), 6.}, {U(uBoostTilt), 3.}, {parameter_layout::kSlotEQHighPass, 90.}}},
  {"Warm push", {{U(uBoostGain), 6.}, {U(uBoostTilt), -2.}}},
};
inline const Preset tube[]{
  {"Warm tube edge", {{U(uTubeDrive), 5.}, {U(uTubeBias), 20.}, {U(uTubeHighCut), 10000.}, {U(uTubeOutput), -3.}}},
};
inline const Preset distortion[]{
  {"Hard rhythm", {{U(uDistStyle), 1.}, {U(uDistDrive), 15.}, {U(uDistTone), 4500.}, {U(uDistOutput), -11.}}},
  // Rectification adds the octave-up character of octave fuzz pedals.
  {"Octave-up fuzz", {{U(uDistStyle), 5.}, {U(uDistDrive), 12.}, {U(uDistTone), 5000.}, {U(uDistOutput), -4.}}},
};
// High-gain trims leave typical notes near bypass loudness on a guitar DI.
inline const Preset pedalDrive[]{
  // Drive low, Level high: a mid-focused push that tightens a driven amp.
  {"Screamer boost", {{U(uPedalStyle), 0.}, {U(uPedalDrive), 1.}, {U(uPedalTone), 6000.}, {U(uPedalOutput), 5.}}},
  {"Blues breakup", {{U(uPedalStyle), 7.}, {U(uPedalDrive), 6.}, {U(uPedalOutput), -3.}}},
  {"Rodent distortion", {{U(uPedalStyle), 1.}, {U(uPedalDrive), 15.}, {U(uPedalTone), 4500.}, {U(uPedalOutput), -17.}}},
  {"Sustaining fuzz", {{U(uPedalStyle), 4.}, {U(uPedalDrive), 18.}, {U(uPedalTone), 3500.}, {U(uPedalOutput), -21.}}},
  {"Bass drive, clean lows",
   {{U(uPedalStyle), 2.}, {U(uPedalDrive), 15.}, {U(uPedalLowKeep), 100.}, {U(uPedalSplit), 180.}, {U(uPedalOutput), -16.}}},
};

// Delay levels: one loud repeat for slapback, two to four repeats for rhythmic and lead echoes.
inline const Preset digitalDelay[]{
  {"Slapback",
   {{U(uDelayTime), 110.}, {U(uDelayFeedback), 0.}, {U(uDelayTone), 8000.}, {U(uDelayCharacter), 0.},
    {U(uDelayMix), 35.}}},
  {"Dotted eighth",
   {{U(uTempoSync), 1.}, {U(uTempoDivision), 4.}, {U(uDelayFeedback), 35.}, {U(uDelayTone), 6000.},
    {U(uDelayCharacter), 0.}, {U(uDelayMix), 30.}}},
  // Long lead repeats that duck under played notes and bloom in the gaps.
  {"Ducked lead",
   {{U(uDelayTime), 480.}, {U(uDelayFeedback), 40.}, {U(uDelayTone), 7000.}, {U(uDelayCharacter), 60.},
    {U(uDelayMix), 30.}}},
};
inline const Preset analogDelay[]{
  {"Warm lead repeats", {{U(uDelayTime), 420.}, {U(uDelayFeedback), 35.}, {U(uDelayTone), 3200.}, {U(uDelayCharacter), 40.}, {U(uDelayMix), 30.}}},
  {"Dark modulated wash", {{U(uDelayTime), 600.}, {U(uDelayFeedback), 55.}, {U(uDelayTone), 2400.}, {U(uDelayCharacter), 50.}, {U(uDelayMotion), 40.}, {U(uDelayMix), 30.}}},
};
inline const Preset tapeDelay[]{
  {"Tape slapback", {{U(uDelayTime), 120.}, {U(uDelayFeedback), 5.}, {U(uDelayTone), 5500.}, {U(uDelayCharacter), 25.}, {U(uDelayMotion), 8.}, {U(uDelayMix), 35.}}},
  {"Worn tape repeats", {{U(uDelayTime), 430.}, {U(uDelayFeedback), 38.}, {U(uDelayTone), 3000.}, {U(uDelayCharacter), 55.}, {U(uDelayMotion), 35.}, {U(uDelayMix), 30.}}},
};
inline const Preset multiHead[]{
  {"Rolling three heads", {{U(uTapeHeads), 2.}, {U(uDelayTime), 600.}, {U(uDelayFeedback), 28.}, {U(uDelayMix), 28.}}},
};
inline const Preset reverseDelay[]{
  {"Reverse half notes", {{U(uTempoSync), 1.}, {U(uTempoDivision), 7.}, {U(uReverseFeedback), 25.}, {U(uDelayTone), 4500.}, {U(uDelayMix), 35.}}},
};
inline const Preset dualDelay[]{
  {"Quarter and dotted eighth", {{U(uTempoSync), 1.}, {U(uTempoDivision), 5.}, {U(uDualRatio), .75}, {U(uDelayFeedback), 28.}, {U(uDelayMix), 30.}}},
};
inline const Preset diffusedDelay[]{
  {"Echo into cloud", {{U(uDelayTime), 650.}, {U(uDelayFeedback), 60.}, {U(uDelayDiffusion), 85.}, {U(uDelayTone), 4500.}, {U(uDelayMotion), 25.}, {U(uDelayMix), 32.}}},
};

inline const Preset room[]{
  {"Live room", {{U(uReverbDecay), 1.2}, {U(uReverbSize), 100.}, {U(uReverbPredelay), 12.}, {U(uReverbMix), 18.}}},
};
inline const Preset plate[]{
  {"Short bright plate", {{U(uReverbDecay), 1.1}, {U(uReverbTone), 8000.}, {U(uReverbPredelay), 10.}, {U(uReverbMix), 14.}}},
  {"Singing lead plate", {{U(uReverbDecay), 2.8}, {U(uReverbPredelay), 35.}, {U(uReverbHighPass), 180.}, {U(uReverbDuck), 25.}, {U(uReverbMix), 22.}}},
};
inline const Preset hall[]{
  {"Big dark hall", {{U(uReverbDecay), 6.}, {U(uReverbSize), 160.}, {U(uReverbTone), 3500.}, {U(uReverbPredelay), 40.}, {U(uReverbHighPass), 180.}, {U(uReverbMix), 28.}}},
};
inline const Preset cloud[]{
  {"Long ducked cloud", {{U(uReverbDecay), 12.}, {U(uReverbSize), 180.}, {U(uReverbPredelay), 50.}, {U(uReverbHighPass), 220.}, {U(uReverbDuck), 55.}, {U(uReverbMix), 35.}}},
};
inline const Preset spring[]{
  {"Small amp spring", {{U(uSpringDecay), 1.2}, {U(uSpringDwell), 1.5}, {U(uSpringDrip), 35.}, {U(uSpringMix), 14.}}},
  // The Dispersive tank's chirp provides the surf "drip"; Dwell drives the tank harder.
  {"Surf drip", {{U(uSpringTank), 1.}, {U(uSpringDecay), 2.5}, {U(uSpringDwell), 7.}, {U(uSpringDrip), 90.}, {U(uReverbTone), 6500.}, {U(uSpringMix), 35.}}},
};
inline const Preset shimmer[]{
  {"Octave halo", {{U(uShimmerDecay), 6.}, {U(uShimmerPitch), 12.}, {U(uShimmerAmount), 25.}, {U(uShimmerMix), 30.}}},
};
inline const Preset space[]{
  {"Slow dark space", {{U(uSpaceReplace), 20.}, {U(uSpaceBrightness), 30.}, {U(uSpaceDetune), 20.}, {U(uSpaceSize), 75.}, {U(uSpaceMix), 28.}}},
};
// Cosmic Echo recipes follow the documented style starting points.
inline const Preset cosmic[]{
  {"Clustered echoes", {{U(uCosmicStyle), 6.}, {U(uCosmicTime), 350.}, {U(uCosmicWarp), 15.}, {U(uCosmicDensity), 10.}, {U(uCosmicTone), 6500.}, {U(uCosmicMotion), 5.}, {U(uCosmicMix), 20.}}},
  {"Slow swell", {{U(uCosmicStyle), 1.}, {U(uCosmicTime), 400.}, {U(uCosmicMix), 25.}}},
  {"Stereo echo groups", {{U(uCosmicStyle), 7.}, {U(uCosmicTime), 500.}, {U(uCosmicWarp), 0.}, {U(uCosmicDensity), 15.}, {U(uCosmicTone), 6500.}, {U(uCosmicMotion), 5.}, {U(uCosmicMix), 25.}}},
  {"Chord bloom", {{U(uCosmicStyle), 8.}, {U(uCosmicTime), 700.}, {U(uCosmicWarp), 75.}, {U(uCosmicFeedback), 90.}, {U(uCosmicDensity), 90.}, {U(uCosmicTone), 5000.}, {U(uCosmicMotion), 25.}, {U(uCosmicMix), 30.}}},
};

inline const Preset chorus[]{
  // Slow and shallow: felt more than heard on clean rhythm parts.
  {"Subtle clean width", {{U(uModRate), .6}, {U(uModDepth), 20.}, {U(uChorusSpread), 80.}, {U(uModMix), 25.}}},
  {"Lush 80s chorus", {{U(uChorusVoices), 1.}, {U(uModRate), .8}, {U(uModDepth), 55.}, {U(uModMix), 50.}}},
};
inline const Preset juno[]{
  {"Wide two voices", {{U(uJunoMode), 1.}, {U(uJunoWidth), 100.}, {U(uJunoMix), 45.}}},
};
inline const Preset flanger[]{
  {"Slow jet sweep", {{U(uModRate), .12}, {U(uModDepth), 75.}, {U(uModFeedback), 55.}, {U(uModMix), 45.}}},
  {"Light comb motion", {{U(uModRate), .4}, {U(uModDepth), 30.}, {U(uModFeedback), 15.}, {U(uFlangerDelay), 5.}, {U(uModMix), 30.}}},
};
inline const Preset phaser[]{
  // Four stages, no feedback and a slow rate: the subtle vintage script-pedal sweep.
  {"Slow vintage phase", {{U(uPhaserStages), 0.}, {U(uModRate), .3}, {U(uModDepth), 55.}, {U(uModFeedback), 0.}, {U(uModMix), 50.}}},
  {"Deep stereo phase", {{U(uPhaserStages), 3.}, {U(uModRate), .18}, {U(uModDepth), 75.}, {U(uModFeedback), 35.}, {U(uModSpread), 70.}, {U(uModMix), 50.}}},
};
inline const Preset tremolo[]{
  {"Amp tremolo", {{U(uModRate), 4.5}, {U(uModDepth), 45.}}},
  {"Hard eighth-note chop", {{U(uTempoSync), 1.}, {U(uTempoDivision), 3.}, {U(uModDepth), 100.}, {U(uModCharacter), 100.}}},
};
inline const Preset vibrato[]{
  {"Seasick warble", {{U(uModRate), .8}, {U(uModDepth), 45.}}},
};
inline const Preset vibe[]{
  {"Vibrato mode swirl", {{U(uVibeMode), 1.}, {U(uModRate), 4.5}, {U(uModDepth), 40.}, {U(uModCharacter), 50.}}},
};
// Leslie speeds: about 0.8 Hz chorale and 6.7 Hz tremolo for the horn.
inline const Preset rotary[]{
  {"Slow chorale", {{U(uRotarySpeed), 0.}, {U(uRotarySlow), .8}, {U(uRotaryDrive), 1.}, {U(uRotarySpread), 60.}}},
  {"Fast tremolo", {{U(uRotarySpeed), 1.}, {U(uRotaryFast), 6.7}, {U(uRotaryDrive), 3.}, {U(uRotaryAccel), 1.5}}},
};

inline const Preset octaver[]{
  {"Octave-down blend", {{U(uOctDry), 100.}, {U(uOctSub), 60.}, {U(uOctSub2), 0.}, {U(uOctTone), 800.}, {U(uOctOutput), -1.5}}},
  {"Synth bass", {{U(uOctDry), 0.}, {U(uOctSub), 100.}, {U(uOctSub2), 0.}, {U(uOctTone), 600.}, {U(uOctOutput), -4.}}},
};
inline const Preset doubler[]{
  {"Wide loose double", {{U(uDoubleDetune), 10.}, {U(uDoubleDelay), 22.}, {U(uDoubleHumanize), 40.}, {U(uDoubleHighCut), 7000.}, {U(uDoubleMix), 32.}}},
};
inline const Preset stutter[]{
  {"Sixteenth-note chops", {{U(uTempoSync), 1.}, {U(uTempoDivision), 1.}, {U(uStutterDuty), 40.}, {U(uStutterFade), 3.}, {U(uStutterMix), 75.}}},
};
inline const Preset crusher[]{
  {"Broken small sampler", {{U(uCrushBits), 6.}, {U(uCrushRate), 6000.}, {U(uCrushTone), 3500.}, {U(uCrushMix), 65.}, {U(uCrushOutput), -2.}}},
};
// The LoFi Tape Machine guide's starting points.
inline const Preset lofi[]{
  {"Subtle drift", {{U(uLoFiSpeed), 15.}, {U(uLoFiWow), 10.}, {U(uLoFiFlutter), 5.}, {U(uLoFiWear), 10.}}},
  {"Worn cassette", {{U(uLoFiSpeed), 3.75}, {U(uLoFiWow), 35.}, {U(uLoFiFlutter), 25.}, {U(uLoFiNoise), 20.}, {U(uLoFiWear), 50.}}},
  {"Damaged tape", {{U(uLoFiSpeed), 1.875}, {U(uLoFiWow), 70.}, {U(uLoFiFlutter), 60.}, {U(uLoFiNoise), 35.}, {U(uLoFiWear), 90.}}},
};
inline const Preset swell[]{
  {"Slow chord bloom", {{U(uSwellRise), 800.}, {U(uSwellRelease), 300.}, {U(uSwellCurve), 1.5}, {U(uSwellHold), 100.}}},
};

inline std::span<const Preset> For(SlotEffect effect)
{
  switch (effect)
  {
    case SlotEffect::PedalComp: return pedalComp;
    case SlotEffect::SmoothComp: return smoothComp;
    case SlotEffect::PunchComp: return punchComp;
    case SlotEffect::ButterComp: return butterComp;
    case SlotEffect::Limiter: return limiter;
    case SlotEffect::DigitalDelay: return digitalDelay;
    case SlotEffect::ToneEQ: return toneEQ;
    case SlotEffect::CutEQ: return cutEQ;
    case SlotEffect::ParametricEQ: return parametricEQ;
    case SlotEffect::Boost: return boost;
    case SlotEffect::SynthFilter: return filter;
    case SlotEffect::AutoSwell: return swell;
    case SlotEffect::AnalogDelay: return analogDelay;
    case SlotEffect::TapeDelay: return tapeDelay;
    case SlotEffect::MultiHeadDelay: return multiHead;
    case SlotEffect::ReverseDelay: return reverseDelay;
    case SlotEffect::DualDelay: return dualDelay;
    case SlotEffect::DiffusedDelay: return diffusedDelay;
    case SlotEffect::RoomReverb: return room;
    case SlotEffect::PlateReverb: return plate;
    case SlotEffect::CloudReverb: return cloud;
    case SlotEffect::HallReverb: return hall;
    case SlotEffect::SpringReverb: return spring;
    case SlotEffect::ShimmerReverb: return shimmer;
    case SlotEffect::GalacticSpace: return space;
    case SlotEffect::CosmicEcho: return cosmic;
    case SlotEffect::Chorus: return chorus;
    case SlotEffect::JunoChorus: return juno;
    case SlotEffect::Flanger: return flanger;
    case SlotEffect::Phaser: return phaser;
    case SlotEffect::Tremolo: return tremolo;
    case SlotEffect::Vibrato: return vibrato;
    case SlotEffect::UniVibe: return vibe;
    case SlotEffect::Rotary: return rotary;
    case SlotEffect::Wah: return wah;
    case SlotEffect::Octaver: return octaver;
    case SlotEffect::Doubler: return doubler;
    case SlotEffect::Stutter: return stutter;
    case SlotEffect::Bitcrusher: return crusher;
    case SlotEffect::TubeBoost: return tube;
    case SlotEffect::Distortion: return distortion;
    case SlotEffect::PedalDrive: return pedalDrive;
    case SlotEffect::LoFiTape: return lofi;
    default: return {};
  }
}
} // namespace module_presets
