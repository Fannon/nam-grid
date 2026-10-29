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

// Settings follow published pedal-manual starting points and professional plugin factory
// presets, translated to these controls; Pedal Comp's Sustain range is calibrated against a
// Ross-type pedal. Names describe the result. Pickup stays as the player set it unless a
// preset is for a particular instrument.
inline const Preset pedalComp[]{
  {"Light always-on",
   {{U(uPedalCompSustain), 30.}, {U(uPedalCompAttack), 0.}, {U(uPedalCompFocus), 20.}, {U(uPedalCompBlend), 50.}}},
  {"Snappy clean picking",
   {{U(uPedalCompSustain), 60.}, {U(uPedalCompAttack), 0.}, {U(uPedalCompFocus), -30.}, {U(uPedalCompBlend), 100.}}},
  {"Even sustain, some dry",
   {{U(uPedalCompSustain), 60.}, {U(uPedalCompAttack), 0.}, {U(uPedalCompBlend), 70.}}},
  {"Heavy squash, natural attack",
   {{U(uPedalCompSustain), 70.}, {U(uPedalCompAttack), 0.}, {U(uPedalCompBlend), 40.}}},
  {"Gentle leveling",
   {{U(uPedalCompSustain), 10.}, {U(uPedalCompAttack), 1.}, {U(uPedalCompBlend), 100.}}},
  {"Long lead notes",
   {{U(uPedalCompSustain), 75.}, {U(uPedalCompAttack), 1.}, {U(uPedalCompFocus), -20.}, {U(uPedalCompBlend), 60.}}},
  {"Acoustic strumming",
   {{U(uPedalCompPickup), 2.}, {U(uPedalCompSustain), 35.}, {U(uPedalCompAttack), 1.}, {U(uPedalCompFocus), 30.},
    {U(uPedalCompBlend), 85.}}},
  {"Acoustic fingerpicking",
   {{U(uPedalCompPickup), 2.}, {U(uPedalCompSustain), 25.}, {U(uPedalCompAttack), 1.}, {U(uPedalCompFocus), 15.},
    {U(uPedalCompBlend), 70.}}},
};
// Thresholds assume typical notes near -18 dBFS on these peak detectors (a DAW peak meter
// around -9 dBFS). Measured on a guitar DI and after a clean capture, typical notes come down
// by about 0.5-6 dB (Firm control: about 8 dB), with makeup near bypass loudness.
inline const Preset smoothComp[]{
  {"Clean guitar leveling",
   {{kSlotCompThreshold, -22.}, {kSlotCompRelease, 150.}, {kSlotCompMakeup, 4.5}, {U(uCompAttack), 8.},
    {U(uSmoothRatio), 3.5}}},
  {"Barely there",
   {{kSlotCompThreshold, -16.}, {kSlotCompRelease, 200.}, {kSlotCompMakeup, 1.}, {U(uCompAttack), 30.},
    {U(uSmoothRatio), 1.5}}},
  {"Long gentle sustain",
   {{kSlotCompThreshold, -30.}, {kSlotCompRelease, 150.}, {kSlotCompMakeup, 4.}, {U(uCompAttack), 30.},
    {U(uSmoothRatio), 1.6}}},
  {"Acoustic strumming leveler",
   {{kSlotCompThreshold, -22.}, {kSlotCompRelease, 120.}, {kSlotCompMakeup, 3.}, {U(uCompAttack), 25.},
    {U(uSmoothRatio), 3.}, {U(uCompDetectorCut), 120.}}},
  {"Even bass notes",
   {{kSlotCompThreshold, -22.}, {kSlotCompRelease, 200.}, {kSlotCompMakeup, 4.5}, {U(uCompAttack), 15.},
    {U(uSmoothRatio), 4.}}},
};
inline const Preset punchComp[]{
  {"Tight picking",
   {{kSlotCompThreshold, -22.}, {kSlotCompRelease, 40.}, {kSlotCompMakeup, 4.}, {U(uPunchAttack), 2.},
    {U(uPunchRatio), 3.6}, {U(uCompMix), 100.}}},
  {"Upfront, keeps dynamics",
   {{kSlotCompThreshold, -20.}, {kSlotCompRelease, 350.}, {kSlotCompMakeup, 3.5}, {U(uPunchAttack), 10.},
    {U(uCompMix), 80.}}},
  {"Firm control after an amp",
   {{kSlotCompThreshold, -22.}, {kSlotCompRelease, 40.}, {kSlotCompMakeup, 6.}, {U(uPunchAttack), .5},
    {U(uPunchRatio), 8.}, {U(uCompMix), 100.}}},
  {"Parallel punch",
   {{kSlotCompThreshold, -28.}, {kSlotCompRelease, 100.}, {kSlotCompMakeup, 6.}, {U(uPunchAttack), 20.},
    {U(uPunchRatio), 6.}, {U(uCompMix), 50.}}},
};
inline const Preset butterComp[]{
  {"Subtle glue", {{U(uButterCompress), 50.}, {U(uButterOutput), 0.}}},
  {"Dense and even", {{U(uButterCompress), 85.}, {U(uButterOutput), 0.}}},
  {"Thick parallel", {{U(uButterCompress), 100.}, {U(uButterOutput), 0.}, {U(uButterMix), 50.}}},
};
inline const Preset limiter[]{
  {"Transparent safety", {{U(uLimiterCeiling), -.5}, {U(uLimiterRelease), 150.}}},
  {"Louder, controlled",
   {{U(uLimiterInput), 6.}, {U(uLimiterCeiling), -1.}, {U(uLimiterRelease), 100.}, {U(uLimiterClip), 30.}}},
};
inline const Preset digitalDelay[]{
  {"Slapback",
   {{U(uDelayTime), 90.}, {U(uDelayFeedback), 0.}, {U(uDelayTone), 8000.}, {U(uDelayCharacter), 0.},
    {U(uDelayMix), 25.}}},
  {"Dotted eighth",
   {{U(uTempoSync), 1.}, {U(uTempoDivision), 4.}, {U(uDelayFeedback), 35.}, {U(uDelayTone), 6000.},
    {U(uDelayCharacter), 30.}, {U(uDelayMix), 22.}}},
  {"Ducked lead",
   {{U(uDelayTime), 420.}, {U(uDelayFeedback), 40.}, {U(uDelayTone), 7000.}, {U(uDelayCharacter), 60.},
    {U(uDelayMix), 30.}}},
};

// Additional recipes are original NAM Grid starting points, informed by the
// sources in docs/presets.md. Values use NAM Grid units, not another plugin's
// normalized knob positions. Unlisted controls use insertion defaults. These
// arrays and their order are NOT persistent identities; only applied values save.
inline const Preset toneEQ[]{
  {"Warm clean", {{parameter_layout::kSlotEQBass, 1.5}, {parameter_layout::kSlotEQTreble, -2.}}},
  {"Mid-forward lead", {{parameter_layout::kSlotEQBass, -1.5}, {parameter_layout::kSlotEQMid, 3.}, {U(uEQOutput), -2.}}},
  {"Open rhythm", {{parameter_layout::kSlotEQMid, -2.}, {parameter_layout::kSlotEQTreble, 1.5}}},
};
inline const Preset cutEQ[]{
  {"Guitar rumble and fizz", {{parameter_layout::kSlotEQHighPass, 80.}, {parameter_layout::kSlotEQLowPass, 8000.}}},
  {"Bass cleanup", {{parameter_layout::kSlotEQHighPass, 35.}, {parameter_layout::kSlotEQLowPass, 12000.}}},
  {"Smooth bright cabinet", {{parameter_layout::kSlotEQHighPass, 70.}, {parameter_layout::kSlotEQLowPass, 6000.}, {parameter_layout::kSlotEQPresence, -2.}}},
};
inline const Preset parametricEQ[]{
  {"Broad low-mid cleanup", {{U(uFrequency), 300.}, {U(uGain), -3.}, {U(uQ), .8}, {parameter_layout::kSlotEQHighPass, 60.}}},
  {"Upper-mid presence", {{U(uFrequency), 1800.}, {U(uGain), 2.}, {U(uQ), .7}}},
  {"Warm low shelf", {{U(uParamLowMode), 1.}, {parameter_layout::kSlotEQHighPass, 120.}, {U(uParamShelfGain), 2.}}},
};
inline const Preset boost[]{
  {"Flat push", {{U(uBoostGain), 6.}}},
  {"Tight amp push", {{U(uBoostGain), 6.}, {U(uBoostTilt), 3.}, {parameter_layout::kSlotEQHighPass, 90.}}},
  {"Warm push", {{U(uBoostGain), 3.}, {U(uBoostTilt), -2.}}},
};
inline const Preset filter[]{
  {"Gentle darkening", {{U(uFilterCutoff), 3500.}, {U(uFilterResonance), .71}}},
  {"Touch wah", {{U(uFilterCutoff), 500.}, {U(uFilterResonance), 2.}, {U(uFilterEnvelope), 2.}}},
  {"Vocal band", {{U(uFilterMode), 2.}, {U(uFilterCutoff), 900.}, {U(uFilterResonance), 1.4}, {U(uFilterEnvelope), 1.}}},
};
inline const Preset swell[]{
  {"Soft pick attack", {{U(uSwellRise), 100.}, {U(uSwellRelease), 100.}, {U(uSwellMix), 80.}}},
  {"Slow chord bloom", {{U(uSwellRise), 800.}, {U(uSwellRelease), 300.}, {U(uSwellCurve), 1.5}, {U(uSwellHold), 100.}}},
};
inline const Preset analogDelay[]{
  {"Dark slap", {{U(uDelayTime), 110.}, {U(uDelayFeedback), 8.}, {U(uDelayTone), 2800.}, {U(uDelayCharacter), 45.}, {U(uDelayMotion), 5.}, {U(uDelayMix), 20.}}},
  {"Warm lead repeats", {{U(uDelayTime), 380.}, {U(uDelayFeedback), 32.}, {U(uDelayTone), 3200.}, {U(uDelayCharacter), 40.}, {U(uDelayMix), 24.}}},
  {"Wavering quarter notes", {{U(uTempoSync), 1.}, {U(uTempoDivision), 5.}, {U(uDelayFeedback), 45.}, {U(uDelayTone), 2600.}, {U(uDelayMotion), 35.}, {U(uDelayMix), 28.}}},
};
inline const Preset tapeDelay[]{
  {"Bright short tape", {{U(uDelayTime), 150.}, {U(uDelayFeedback), 15.}, {U(uDelayTone), 7500.}, {U(uDelayCharacter), 15.}, {U(uDelayMotion), 5.}, {U(uDelayMix), 20.}}},
  {"Worn tape lead", {{U(uDelayTime), 430.}, {U(uDelayFeedback), 38.}, {U(uDelayTone), 3000.}, {U(uDelayCharacter), 55.}, {U(uDelayMotion), 35.}, {U(uDelayMix), 25.}}},
  {"Dotted tape", {{U(uTempoSync), 1.}, {U(uTempoDivision), 4.}, {U(uDelayFeedback), 32.}, {U(uDelayMotion), 12.}, {U(uDelayMix), 24.}}},
};
inline const Preset multiHead[]{
  {"Rolling three heads", {{U(uTapeHeads), 2.}, {U(uDelayTime), 600.}, {U(uDelayFeedback), 28.}, {U(uDelayMix), 24.}}},
  {"Stereo cascade", {{U(uTapeHeads), 7.}, {U(uTempoSync), 1.}, {U(uTempoDivision), 7.}, {U(uDelayFeedback), 35.}, {U(uDelayMix), 28.}}},
  {"Soft all-head wash", {{U(uTapeHeads), 3.}, {U(uDelayTime), 850.}, {U(uDelayFeedback), 55.}, {U(uDelayTone), 3000.}, {U(uDelayCharacter), 40.}, {U(uDelayMotion), 25.}, {U(uDelayMix), 30.}}},
};
inline const Preset reverseDelay[]{
  {"Backwards accents", {{U(uDelayTime), 350.}, {U(uReverseFeedback), 0.}, {U(uDelayMix), 25.}}},
  {"Reverse half notes", {{U(uTempoSync), 1.}, {U(uTempoDivision), 7.}, {U(uReverseFeedback), 25.}, {U(uDelayTone), 4500.}, {U(uDelayMix), 35.}}},
};
inline const Preset dualDelay[]{
  {"Quarter and dotted eighth", {{U(uTempoSync), 1.}, {U(uTempoDivision), 5.}, {U(uDualRatio), .75}, {U(uDelayFeedback), 28.}, {U(uDelayMix), 24.}}},
  {"Uneven ping-pong", {{U(uDelayTime), 350.}, {U(uDualRatio), 1.5}, {U(uDelayPingPong), 1.}, {U(uDelayFeedback), 35.}, {U(uDelayMix), 28.}}},
};
inline const Preset diffusedDelay[]{
  {"Soft echo clusters", {{U(uDelayTime), 320.}, {U(uDelayFeedback), 30.}, {U(uDelayDiffusion), 40.}, {U(uDelayMix), 22.}}},
  {"Echo into cloud", {{U(uDelayTime), 650.}, {U(uDelayFeedback), 60.}, {U(uDelayDiffusion), 85.}, {U(uDelayTone), 4500.}, {U(uDelayMotion), 25.}, {U(uDelayMix), 32.}}},
};
inline const Preset room[]{
  {"Small amp room", {{U(uReverbDecay), .45}, {U(uReverbSize), 55.}, {U(uReverbPredelay), 3.}, {U(uReverbMix), 10.}}},
  {"Live room", {{U(uReverbDecay), 1.2}, {U(uReverbSize), 100.}, {U(uReverbPredelay), 12.}, {U(uReverbMix), 18.}}},
};
inline const Preset plate[]{
  {"Short bright plate", {{U(uReverbDecay), 1.1}, {U(uReverbTone), 8000.}, {U(uReverbPredelay), 10.}, {U(uReverbMix), 14.}}},
  {"Singing lead plate", {{U(uReverbDecay), 2.8}, {U(uReverbPredelay), 35.}, {U(uReverbHighPass), 180.}, {U(uReverbDuck), 25.}, {U(uReverbMix), 22.}}},
};
inline const Preset cloud[]{
  {"Soft chord bed", {{U(uReverbDecay), 5.}, {U(uReverbSize), 130.}, {U(uReverbTone), 4500.}, {U(uReverbMix), 25.}}},
  {"Long ducked cloud", {{U(uReverbDecay), 12.}, {U(uReverbSize), 180.}, {U(uReverbPredelay), 50.}, {U(uReverbHighPass), 220.}, {U(uReverbDuck), 55.}, {U(uReverbMix), 35.}}},
};
inline const Preset hall[]{
  {"Clear concert hall", {{U(uReverbDecay), 2.2}, {U(uReverbSize), 100.}, {U(uReverbTone), 7500.}, {U(uReverbPredelay), 25.}, {U(uReverbMix), 18.}}},
  {"Dark spacious hall", {{U(uReverbDecay), 6.}, {U(uReverbSize), 160.}, {U(uReverbTone), 3500.}, {U(uReverbPredelay), 40.}, {U(uReverbHighPass), 180.}, {U(uReverbMix), 28.}}},
};
inline const Preset spring[]{
  {"Small amp spring", {{U(uSpringDecay), 1.2}, {U(uSpringDwell), 1.5}, {U(uSpringDrip), 35.}, {U(uSpringMix), 14.}}},
  {"Splashy surf", {{U(uSpringDecay), 2.5}, {U(uSpringDwell), 5.}, {U(uSpringDrip), 85.}, {U(uReverbTone), 6500.}, {U(uSpringMix), 30.}}},
};
inline const Preset shimmer[]{
  {"Octave halo", {{U(uShimmerDecay), 5.}, {U(uShimmerPitch), 12.}, {U(uShimmerAmount), 20.}, {U(uShimmerMix), 22.}}},
  {"Falling dark pad", {{U(uShimmerDecay), 8.}, {U(uShimmerPitch), -12.}, {U(uShimmerTone), 3500.}, {U(uShimmerAmount), 30.}, {U(uShimmerMix), 30.}}},
};
inline const Preset airPlate[]{
  {"Compact plate", {{U(uAirPlateSustain), 30.}, {U(uAirPlatePredelay), 8.}, {U(uAirPlateMix), 12.}}},
  {"Broad dark plate", {{U(uAirPlateSustain), 70.}, {U(uAirPlateResolution), 75.}, {U(uAirPlatePredelay), 30.}, {U(uAirPlateLowCut), 180.}, {U(uAirPlateMix), 24.}}},
};
inline const Preset airHall[]{
  {"Close guitar hall", {{U(uAirHallSustain), 30.}, {U(uAirHallEarly), 70.}, {U(uAirHallMix), 15.}}},
  {"Distant dark hall", {{U(uAirHallSustain), 75.}, {U(uAirHallTone), 30.}, {U(uAirHallEarly), 25.}, {U(uAirHallPosition), 75.}, {U(uAirHallMix), 25.}}},
};
inline const Preset space[]{
  {"Slow dark space", {{U(uSpaceReplace), 20.}, {U(uSpaceBrightness), 30.}, {U(uSpaceDetune), 20.}, {U(uSpaceSize), 75.}, {U(uSpaceMix), 28.}}},
  {"Moving galaxy", {{U(uSpaceReplace), 35.}, {U(uSpaceDetune), 60.}, {U(uSpaceSize), 80.}, {U(uSpaceResolution), 75.}, {U(uSpaceMix), 32.}}},
};
inline const Preset cosmic[]{
  {"Smooth Gemini", {{U(uCosmicStyle), 0.}, {U(uCosmicTime), 180.}, {U(uCosmicWarp), 65.}, {U(uCosmicFeedback), 55.}, {U(uCosmicDensity), 100.}, {U(uCosmicMix), 22.}}},
  {"Slow Sagittarius bloom", {{U(uCosmicStyle), 1.}, {U(uCosmicTime), 400.}, {U(uCosmicFeedback), 65.}, {U(uCosmicMix), 25.}}},
  {"Dark Virgo echoes", {{U(uCosmicStyle), 2.}, {U(uCosmicTime), 450.}, {U(uCosmicWarp), 20.}, {U(uCosmicDensity), 25.}, {U(uCosmicTone), 3000.}, {U(uCosmicMix), 25.}}},
  {"Centaurus swell", {{U(uCosmicStyle), 3.}, {U(uCosmicTime), 350.}, {U(uCosmicFeedback), 60.}, {U(uCosmicMix), 25.}}},
  {"Long evolving tank", {{U(uCosmicStyle), 4.}, {U(uCosmicTime), 500.}, {U(uCosmicFeedback), 70.}, {U(uCosmicTone), 4500.}, {U(uCosmicMix), 25.}}},
  {"Clear Lyra echoes", {{U(uCosmicStyle), 5.}, {U(uCosmicTime), 350.}, {U(uCosmicWarp), 10.}, {U(uCosmicDensity), 20.}, {U(uCosmicFeedback), 45.}, {U(uCosmicMix), 22.}}},
  {"Aurora guitar hall", {{U(uCosmicStyle), 6.}, {U(uCosmicTime), 225.}, {U(uCosmicMix), 20.}}},
  {"Pulsar rhythm", {{U(uCosmicStyle), 7.}, {U(uCosmicTime), 400.}, {U(uCosmicWarp), 15.}, {U(uCosmicDensity), 25.}, {U(uCosmicFeedback), 50.}, {U(uCosmicMix), 24.}}},
  {"Binary chord bloom", {{U(uCosmicStyle), 8.}, {U(uCosmicTime), 600.}, {U(uCosmicDensity), 80.}, {U(uCosmicFeedback), 65.}, {U(uCosmicMix), 28.}}},
  {"Quick Photon plate", {{U(uCosmicStyle), 9.}, {U(uCosmicTime), 100.}, {U(uCosmicWarp), 55.}, {U(uCosmicDensity), 95.}, {U(uCosmicFeedback), 55.}, {U(uCosmicMotion), 10.}, {U(uCosmicMix), 18.}}},
};
inline const Preset chorus[]{
  {"Subtle clean width", {{U(uModRate), .6}, {U(uModDepth), 20.}, {U(uChorusSpread), 80.}, {U(uModMix), 25.}}},
  {"Deep slow chorus", {{U(uModRate), .35}, {U(uModDepth), 60.}, {U(uChorusDelay), 22.}, {U(uModMix), 45.}}},
  {"Fast shimmer", {{U(uModRate), 2.8}, {U(uModDepth), 25.}, {U(uChorusDelay), 14.}, {U(uModMix), 35.}}},
};
inline const Preset juno[]{
  {"Soft slow voice", {{U(uJunoMode), 0.}, {U(uJunoBrightness), 65.}, {U(uJunoMix), 30.}}},
  {"Wide two voices", {{U(uJunoMode), 1.}, {U(uJunoWidth), 100.}, {U(uJunoMix), 45.}}},
  {"Centered vintage motion", {{U(uJunoMode), 3.}, {U(uJunoBrightness), 70.}, {U(uJunoMix), 35.}}},
};
inline const Preset flanger[]{
  {"Slow jet sweep", {{U(uModRate), .12}, {U(uModDepth), 75.}, {U(uModFeedback), 55.}, {U(uModMix), 45.}}},
  {"Light comb motion", {{U(uModRate), .4}, {U(uModDepth), 30.}, {U(uModFeedback), 15.}, {U(uFlangerDelay), 5.}, {U(uModMix), 30.}}},
  {"Parked comb", {{U(uModDepth), 0.}, {U(uFlangerDelay), 7.}, {U(uModFeedback), 35.}, {U(uModMix), 40.}}},
};
inline const Preset phaser[]{
  {"Gentle four-stage sweep", {{U(uPhaserStages), 0.}, {U(uModRate), .4}, {U(uModDepth), 55.}, {U(uModFeedback), 5.}, {U(uModMix), 45.}}},
  {"Deep stereo phase", {{U(uPhaserStages), 3.}, {U(uModRate), .18}, {U(uModDepth), 75.}, {U(uModFeedback), 35.}, {U(uModSpread), 70.}, {U(uModMix), 50.}}},
};
inline const Preset tremolo[]{
  {"Gentle amp pulse", {{U(uModRate), 3.5}, {U(uModDepth), 35.}, {U(uModCharacter), 10.}}},
  {"Hard eighth-note chop", {{U(uTempoSync), 1.}, {U(uTempoDivision), 3.}, {U(uModDepth), 100.}, {U(uModCharacter), 100.}}},
  {"Slow stereo sway", {{U(uModRate), .7}, {U(uModDepth), 65.}, {U(uModSpread), 100.}}},
};
inline const Preset vibrato[]{
  {"Gentle pitch movement", {{U(uModRate), 4.}, {U(uModDepth), 8.}, {U(uVibratoRise), 300.}}},
  {"Slow seasick bend", {{U(uModRate), .8}, {U(uModDepth), 45.}, {U(uVibratoRise), 0.}}},
};
inline const Preset vibe[]{
  {"Slow chewy chorus", {{U(uModRate), .9}, {U(uModDepth), 70.}, {U(uModCharacter), 40.}, {U(uModMix), 50.}}},
  {"Fast pitch swirl", {{U(uVibeMode), 1.}, {U(uModRate), 4.5}, {U(uModDepth), 40.}, {U(uModCharacter), 50.}}},
};
inline const Preset rotary[]{
  {"Slow cabinet", {{U(uRotarySpeed), 0.}, {U(uRotarySlow), .7}, {U(uRotaryDrive), 1.}, {U(uRotarySpread), 60.}}},
  {"Fast bright rotor", {{U(uRotarySpeed), 1.}, {U(uRotaryFast), 6.5}, {U(uRotaryDrive), 4.}, {U(uRotaryBalance), 25.}, {U(uRotaryAccel), 1.5}}},
};
inline const Preset wah[]{
  {"Parked low vowel", {{U(uWahPosition), 30.}, {U(uWahResonance), 2.5}, {U(uWahRange), 40.}}},
  {"Parked lead bite", {{U(uWahPosition), 70.}, {U(uWahResonance), 3.5}, {U(uWahRange), 60.}, {U(uWahMix), 70.}}},
};
inline const Preset octaver[]{
  {"Subtle low octave", {{U(uOctDry), 100.}, {U(uOctSub), 30.}, {U(uOctSub2), 0.}, {U(uOctTone), 800.}, {U(uOctOutput), -1.5}}},
  {"Sub synth", {{U(uOctDry), 20.}, {U(uOctSub), 80.}, {U(uOctSub2), 20.}, {U(uOctTone), 450.}, {U(uOctOutput), -3.}}},
};
inline const Preset doubler[]{
  {"Tight double", {{U(uDoubleDetune), 4.}, {U(uDoubleDelay), 10.}, {U(uDoubleHumanize), 15.}, {U(uDoubleMix), 22.}}},
  {"Wide loose double", {{U(uDoubleDetune), 10.}, {U(uDoubleDelay), 22.}, {U(uDoubleHumanize), 40.}, {U(uDoubleHighCut), 7000.}, {U(uDoubleMix), 32.}}},
};
inline const Preset ring[]{
  {"Low metallic blend", {{U(uRingFrequency), 55.}, {U(uRingTone), 4000.}, {U(uRingMix), 25.}}},
  {"Bell sidebands", {{U(uRingFrequency), 440.}, {U(uRingShape), 0.}, {U(uRingTone), 8000.}, {U(uRingMix), 45.}}},
};
inline const Preset shifter[]{
  {"Slow stereo rotation", {{U(uShiftFrequency), 2.}, {U(uShiftFeedback), 5.}, {U(uShiftDelay), 10.}, {U(uShiftStereo), 100.}, {U(uShiftMix), 30.}}},
  {"Falling metallic echoes", {{U(uShiftFrequency), -80.}, {U(uShiftFeedback), 30.}, {U(uShiftDelay), 180.}, {U(uShiftTone), 4500.}, {U(uShiftMix), 25.}}},
};
inline const Preset freeze[]{
  {"Soft held chord", {{U(uFreezeWindow), 800.}, {U(uFreezeFade), 80.}, {U(uFreezeAttack), 500.}, {U(uFreezeRelease), 1200.}, {U(uFreezeTone), 4000.}, {U(uFreezeMix), 30.}}},
  {"Short tonal layer", {{U(uFreezeWindow), 100.}, {U(uFreezeFade), 15.}, {U(uFreezeAttack), 50.}, {U(uFreezeRelease), 300.}, {U(uFreezeMix), 20.}}},
};
inline const Preset stutter[]{
  {"Eighth-note slices", {{U(uTempoSync), 1.}, {U(uTempoDivision), 3.}, {U(uStutterDuty), 70.}, {U(uStutterFade), 5.}, {U(uStutterMix), 50.}}},
  {"Sixteenth-note chops", {{U(uTempoSync), 1.}, {U(uTempoDivision), 1.}, {U(uStutterDuty), 40.}, {U(uStutterFade), 3.}, {U(uStutterMix), 75.}}},
};
inline const Preset crusher[]{
  {"Soft digital grain", {{U(uCrushBits), 12.}, {U(uCrushRate), 24000.}, {U(uCrushTone), 8000.}, {U(uCrushMix), 35.}}},
  {"Broken small sampler", {{U(uCrushBits), 6.}, {U(uCrushRate), 6000.}, {U(uCrushTone), 3500.}, {U(uCrushMix), 65.}, {U(uCrushOutput), -2.}}},
};
inline const Preset tape[]{
  {"Light tape rounding", {{U(uTapeDrive), 2.}, {U(uTapeBump), 15.}, {U(uTapeOutput), -1.}}},
  {"Thick parallel tape", {{U(uTapeDrive), 8.}, {U(uTapeBump), 45.}, {U(uTapeMix), 50.}, {U(uTapeOutput), -2.}}},
};
inline const Preset tube[]{
  {"Warm tube edge", {{U(uTubeDrive), 5.}, {U(uTubeBias), 20.}, {U(uTubeHighCut), 10000.}, {U(uTubeOutput), -3.}}},
  {"Tight tube push", {{U(uTubeDrive), 9.}, {U(uBoostTilt), 2.}, {parameter_layout::kSlotEQHighPass, 90.}, {U(uTubeOutput), -5.}}},
};
// High-gain trims account for the extra sustained energy on the audition pluck.
// Input level and playing still determine perceived loudness.
inline const Preset distortion[]{
  {"Rounded crunch", {{U(uDistStyle), 0.}, {U(uDistDrive), 8.}, {U(uDistTone), 6000.}, {U(uDistOutput), -5.}}},
  {"Hard rhythm", {{U(uDistStyle), 1.}, {U(uDistDrive), 15.}, {U(uDistTone), 4500.}, {U(uDistOutput), -11.}}},
  {"Dense fuzz sustain", {{U(uDistStyle), 2.}, {U(uDistDrive), 22.}, {U(uDistTone), 3500.}, {U(uDistOutput), -22.}}},
};
inline const Preset pedalDrive[]{
  {"Tight Screamer push", {{U(uPedalStyle), 0.}, {U(uPedalDrive), 3.}, {U(uPedalTone), 6500.}, {U(uPedalOutput), -1.}}},
  {"Rodent rhythm", {{U(uPedalStyle), 1.}, {U(uPedalDrive), 15.}, {U(uPedalTone), 4500.}, {U(uPedalOutput), -15.}}},
  {"Bass edge, clean lows", {{U(uPedalStyle), 2.}, {U(uPedalDrive), 15.}, {U(uPedalLowKeep), 100.}, {U(uPedalSplit), 180.}, {U(uPedalOutput), -13.}}},
  {"Parallel breakup", {{U(uPedalStyle), 3.}, {U(uPedalDrive), 6.}, {U(uPedalOutput), -3.}}},
  {"Sustaining fuzz", {{U(uPedalStyle), 4.}, {U(uPedalDrive), 18.}, {U(uPedalTone), 3500.}, {U(uPedalOutput), -21.}}},
  {"Cascaded edge", {{U(uPedalStyle), 5.}, {U(uPedalDrive), 3.}, {U(uPedalOutput), -3.}}},
  {"Treble push", {{U(uPedalStyle), 6.}, {U(uPedalDrive), 3.}, {U(uPedalTone), 7000.}, {U(uPedalOutput), -3.}}},
  {"Open blues breakup", {{U(uPedalStyle), 7.}, {U(uPedalDrive), 6.}, {U(uPedalOutput), -3.}}},
  {"Transparent edge", {{U(uPedalStyle), 8.}, {U(uPedalDrive), 6.}, {U(uPedalOutput), -3.}}},
  {"Vintage woolly fuzz", {{U(uPedalStyle), 9.}, {U(uPedalDrive), 12.}, {U(uPedalTone), 4500.}, {U(uPedalOutput), -15.}}},
};
inline const Preset lofi[]{
  {"Clean slow reel", {{U(uLoFiSpeed), 7.5}, {U(uLoFiWow), 15.}, {U(uLoFiFlutter), 10.}, {U(uLoFiMix), 100.}}},
  {"Worn cassette", {{U(uLoFiSpeed), 1.875}, {U(uLoFiDrive), 4.}, {U(uLoFiWow), 55.}, {U(uLoFiFlutter), 35.}, {U(uLoFiWear), 20.}, {U(uLoFiOutput), -2.}}},
  {"Unsteady parallel reel", {{U(uLoFiSpeed), 3.75}, {U(uLoFiWow), 75.}, {U(uLoFiFlutter), 45.}, {U(uLoFiMix), 45.}}},
};
inline const Preset strip[]{
  {"Light channel glue", {{U(uStripCompress), 25.}, {U(uStripSaturate), 10.}, {U(uStripOutput), 0.}}},
  {"Thick parallel channel", {{U(uStripCompress), 65.}, {U(uStripSpeed), 40.}, {U(uStripSaturate), 35.}, {U(uStripMix), 50.}, {U(uStripOutput), -1.}}},
};
inline const Preset colour[]{
  {"Light desk colour", {{U(uColourInput), 3.}, {U(uColourOutput), -3.}}},
  {"Driven desk blend", {{U(uColourInput), 9.}, {U(uColourOutput), -9.}, {U(uColourMix), 50.}}},
};
inline const Preset consoleEQ[]{
  {"Warm broad contour", {{U(uConsoleBass), 1.5}, {U(uConsoleTreble), -1.5}, {U(uEQOutput), -1.}}},
  {"Forward guitar", {{U(uConsoleMid), 2.}, {U(uConsoleBass), -1.}, {U(uEQOutput), -1.5}}},
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
    case SlotEffect::AirPlate: return airPlate;
    case SlotEffect::AirHall: return airHall;
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
    case SlotEffect::RingMod: return ring;
    case SlotEffect::FrequencyShifter: return shifter;
    case SlotEffect::Freeze: return freeze;
    case SlotEffect::Stutter: return stutter;
    case SlotEffect::Bitcrusher: return crusher;
    case SlotEffect::Tape: return tape;
    case SlotEffect::TubeBoost: return tube;
    case SlotEffect::Distortion: return distortion;
    case SlotEffect::PedalDrive: return pedalDrive;
    case SlotEffect::LoFiTape: return lofi;
    case SlotEffect::OutboardChannel: return strip;
    case SlotEffect::ConsoleColour: return colour;
    case SlotEffect::ConsoleEQ: return consoleEQ;
    default: return {};
  }
}
} // namespace module_presets
