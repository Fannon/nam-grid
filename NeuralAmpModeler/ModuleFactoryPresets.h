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

// Transient Shaper compares envelopes, so presets work at any input level. Output trims
// balance the shaping on guitar and bass. Gentle note sustain blends a modest tail
// lift under the dry attack; boosts still need headroom.
inline const Preset transientShaper[]{
  {"Defined picking", {{U(uTransientShaperAttack), 5.}, {U(uTransientShaperSustain), -2.}, {U(uTransientShaperRecovery), 150.},
    {U(uTransientShaperOutput), -.5}}},
  {"Soft acoustic", {{U(uTransientShaperAttack), -6.}, {U(uTransientShaperSustain), 1.}, {U(uTransientShaperRecovery), 150.}}},
  {"Tight palm mutes", {{U(uTransientShaperAttack), 2.}, {U(uTransientShaperSustain), -8.}, {U(uTransientShaperRecovery), 120.},
    {U(uTransientShaperSensitivity), 70.}, {U(uTransientShaperOutput), 1.5}}},
  {"Gentle note sustain", {{U(uTransientShaperAttack), 0.}, {U(uTransientShaperSustain), 4.}, {U(uTransientShaperRecovery), 220.},
    {U(uTransientShaperSensitivity), 45.}, {U(uTransientShaperMix), 60.}, {U(uTransientShaperOutput), -1.}}},
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
// Dynamic EQ thresholds are absolute, so each preset names where it sits. They were set so the
// band is cut on about a quarter to a third of played audio and rests between peaks: guitar DI
// peaking near -6 dBFS, the bundled clean capture's output, and a bass DI peaking near -6 dBFS.
inline const Preset dynamicEQ[]{
  {"Palm-mute control (after amp)", {{U(uDynamicEQFrequency), 140.}, {U(uDynamicEQQ), .9}, {U(uDynamicEQThreshold), -19.},
    {U(uDynamicEQRange), 6.}, {U(uDynamicEQAttack), 8.}, {U(uDynamicEQRelease), 140.}}},
  {"Tight into amp", {{U(uDynamicEQFrequency), 120.}, {U(uDynamicEQQ), .8}, {U(uDynamicEQThreshold), -27.},
    {U(uDynamicEQRange), 6.}, {U(uDynamicEQAttack), 6.}, {U(uDynamicEQRelease), 120.}}},
  {"Harsh pick tamer (after amp)", {{U(uDynamicEQFrequency), 3000.}, {U(uDynamicEQQ), 1.2}, {U(uDynamicEQThreshold), -30.},
    {U(uDynamicEQRange), 5.}, {U(uDynamicEQAttack), 1.}, {U(uDynamicEQRelease), 70.}}},
  {"Bass boom control", {{U(uDynamicEQFrequency), 90.}, {U(uDynamicEQQ), 1.}, {U(uDynamicEQThreshold), -25.},
    {U(uDynamicEQRange), 6.}, {U(uDynamicEQAttack), 10.}, {U(uDynamicEQRelease), 160.}}},
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
  {"Warm tube edge", {{U(uTubeDrive), 9.}, {U(uTubeBias), 20.}, {U(uTubeHighCut), 10000.}, {U(uTubeOutput), -7.}}},
};
inline const Preset distortion[]{
  {"Hard rhythm", {{U(uDistStyle), 1.}, {U(uDistDrive), 24.}, {U(uDistTone), 4500.}, {U(uDistOutput), -20.}}},
  // Rectification adds the octave-up character of octave fuzz pedals.
  {"Octave-up fuzz", {{U(uDistStyle), 5.}, {U(uDistDrive), 12.}, {U(uDistTone), 5000.}, {U(uDistOutput), -4.}}},
};
// High-gain trims leave typical notes near bypass loudness on a guitar DI.
inline const Preset pedalDrive[]{
  // Drive low, Level high: a mid-focused push that tightens a driven amp.
  {"Screamer boost", {{U(uPedalStyle), 0.}, {U(uPedalDrive), 1.}, {U(uPedalTone), 6000.}, {U(uPedalOutput), 5.}}},
  {"Blues breakup", {{U(uPedalStyle), 7.}, {U(uPedalDrive), 14.}, {U(uPedalOutput), -7.5}}},
  {"Rodent distortion", {{U(uPedalStyle), 1.}, {U(uPedalDrive), 15.}, {U(uPedalTone), 4500.}, {U(uPedalOutput), -17.}}},
  {"Sustaining fuzz", {{U(uPedalStyle), 4.}, {U(uPedalDrive), 18.}, {U(uPedalTone), 3500.}, {U(uPedalOutput), -21.}}},
  {"Bass drive, clean lows",
   {{U(uPedalStyle), 2.}, {U(uPedalDrive), 15.}, {U(uPedalLowKeep), 100.}, {U(uPedalSplit), 180.}, {U(uPedalOutput), -16.}}},
};

// Delay levels: one loud repeat for slapback, two to four repeats for rhythmic and lead echoes.
// New recipes favor practical gaps: quiet ducked support, brighter synced analog/tape,
// sparse stereo heads, short reverse texture, stereo doubling and moderate pad echoes.
// The author's ValhallaDelay Analog Past, ChromeTape for Pads and Space 1 presets
// inform these roles, not parameter conversions: NAM Grid uses different DSP and
// does not combine ducking, saturation, diffusion and independent L/R sync in one module.
// References: https://valhalladsp.com/2019/04/16/valhalladelay-the-controls/
// https://valhalladsp.com/2019/04/16/valhalladelay-the-style-control/
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
  {"Quarter-note ping-pong",
   {{U(uTempoSync), 1.}, {U(uTempoDivision), 5.}, {U(uDelayPingPong), 1.}, {U(uDelayFeedback), 35.},
    {U(uDelayTone), 6000.}, {U(uDelayCharacter), 0.}, {U(uDelayMix), 30.}}},
  {"Quiet ducked support", {{U(uTempoSync), 1.}, {U(uTempoDivision), 5.}, {U(uDelayTimeChange), 1.},
    {U(uDelayFeedback), 28.}, {U(uDelayTone), 6500.}, {U(uDelayCharacter), 70.}, {U(uDuckRelease), 220.}, {U(uDelayMix), 12.}}},
};
inline const Preset analogDelay[]{
  {"Warm lead repeats", {{U(uDelayTime), 420.}, {U(uDelayFeedback), 35.}, {U(uDelayTone), 3200.}, {U(uDelayCharacter), 40.}, {U(uDelayMix), 30.}}},
  {"Dark modulated wash", {{U(uDelayTime), 600.}, {U(uDelayFeedback), 55.}, {U(uDelayTone), 2400.}, {U(uDelayCharacter), 50.}, {U(uDelayMotion), 40.}, {U(uDelayMix), 30.}}},
  // Single dark, saturated repeat, as with analog slapback pedals.
  {"Dark slapback", {{U(uDelayTime), 120.}, {U(uDelayFeedback), 5.}, {U(uDelayTone), 2800.}, {U(uDelayCharacter), 45.}, {U(uDelayMotion), 5.}, {U(uDelayMix), 40.}}},
  {"Bright dotted echoes", {{U(uTempoSync), 1.}, {U(uTempoDivision), 4.}, {U(uDelayFeedback), 30.},
    {U(uDelayTone), 6500.}, {U(uDelayCharacter), 15.}, {U(uDelayMotion), 5.}, {U(uDelayMix), 22.}}},
};
inline const Preset tapeDelay[]{
  {"Tape slapback", {{U(uDelayTime), 120.}, {U(uDelayFeedback), 5.}, {U(uDelayTone), 5500.}, {U(uDelayCharacter), 25.}, {U(uDelayMotion), 8.}, {U(uDelayMix), 35.}}},
  {"Worn tape repeats", {{U(uDelayTime), 430.}, {U(uDelayFeedback), 38.}, {U(uDelayTone), 3000.}, {U(uDelayCharacter), 55.}, {U(uDelayMotion), 35.}, {U(uDelayMix), 30.}}},
  // About one quiet 300 ms repeat: the classic rock lead echo of an EP-3.
  {"Rock lead echo", {{U(uDelayTime), 300.}, {U(uDelayFeedback), 8.}, {U(uDelayTone), 4500.}, {U(uDelayCharacter), 30.}, {U(uDelayMotion), 12.}, {U(uDelayMix), 25.}}},
  {"Clean dotted tape", {{U(uTempoSync), 1.}, {U(uTempoDivision), 4.}, {U(uDelayFeedback), 28.},
    {U(uDelayTone), 6000.}, {U(uDelayCharacter), 12.}, {U(uDelayMotion), 5.}, {U(uDelayMix), 22.}}},
};
inline const Preset multiHead[]{
  {"Rolling three heads", {{U(uTapeHeads), 2.}, {U(uDelayTime), 600.}, {U(uDelayFeedback), 28.}, {U(uDelayMix), 28.}}},
  {"Stereo cascade", {{U(uTapeHeads), 7.}, {U(uTempoSync), 1.}, {U(uTempoDivision), 7.}, {U(uDelayFeedback), 35.}, {U(uDelayMix), 28.}}},
  {"All-head wash", {{U(uTapeHeads), 3.}, {U(uDelayTime), 850.}, {U(uDelayFeedback), 55.}, {U(uDelayTone), 3000.}, {U(uDelayCharacter), 40.}, {U(uDelayMotion), 25.}, {U(uDelayMix), 30.}}},
  {"Sparse stereo heads", {{U(uTapeHeads), 6.}, {U(uTempoSync), 1.}, {U(uTempoDivision), 5.},
    {U(uDelayFeedback), 22.}, {U(uDelayTone), 6000.}, {U(uDelayCharacter), 15.}, {U(uDelayMotion), 5.}, {U(uDelayMix), 20.}}},
};
inline const Preset reverseDelay[]{
  {"Reverse half notes", {{U(uTempoSync), 1.}, {U(uTempoDivision), 7.}, {U(uReverseFeedback), 25.}, {U(uDelayTone), 4500.}, {U(uDelayMix), 35.}}},
  // Mostly reversed playing, like a backwards-tape solo.
  {"Reversed solo", {{U(uDelayTime), 750.}, {U(uReverseFeedback), 30.}, {U(uDelayTone), 5000.}, {U(uDelayMix), 70.}}},
  {"Short reverse texture", {{U(uDelayTime), 220.}, {U(uReverseFeedback), 0.}, {U(uDelayTone), 6500.}, {U(uDelayMix), 22.}}},
};
inline const Preset dualDelay[]{
  {"Quarter and dotted eighth", {{U(uTempoSync), 1.}, {U(uTempoDivision), 5.}, {U(uDualRatio), .75}, {U(uDelayFeedback), 28.}, {U(uDelayMix), 30.}}},
  {"Wide ping-pong", {{U(uDelayTime), 350.}, {U(uDualRatio), 1.5}, {U(uDelayPingPong), 1.}, {U(uDelayFeedback), 35.}, {U(uDelayMix), 28.}}},
  {"Tight stereo double", {{U(uDelayTime), 80.}, {U(uDualRatio), 1.25}, {U(uDualBalance), 50.},
    {U(uDelayFeedback), 0.}, {U(uDelayTone), 8000.}, {U(uDelayPingPong), 0.}, {U(uDelayMix), 15.}}},
};
inline const Preset diffusedDelay[]{
  {"Echo into cloud", {{U(uDelayTime), 650.}, {U(uDelayFeedback), 60.}, {U(uDelayDiffusion), 85.}, {U(uDelayTone), 4500.}, {U(uDelayMotion), 25.}, {U(uDelayMix), 32.}}},
  {"Soft echo clusters", {{U(uDelayTime), 320.}, {U(uDelayFeedback), 30.}, {U(uDelayDiffusion), 40.}, {U(uDelayMix), 22.}}},
  {"Gentle pad echoes", {{U(uDelayTime), 550.}, {U(uDelayFeedback), 37.}, {U(uDelayDiffusion), 68.},
    {U(uDelayTone), 4500.}, {U(uDelayMotion), 15.}, {U(uDelayMix), 22.}}},
};

inline const Preset room[]{
  {"Live room", {{U(uReverbDecay), 1.2}, {U(uReverbSize), 100.}, {U(uReverbPredelay), 12.}, {U(uReverbMix), 18.}}},
  // Short, tight ambience for dry captures and DI-like close sounds.
  {"Small amp room", {{U(uReverbDecay), .45}, {U(uReverbSize), 55.}, {U(uReverbPredelay), 3.}, {U(uReverbMix), 10.}}},
};
inline const Preset plate[]{
  {"Short bright plate", {{U(uReverbDecay), 1.1}, {U(uReverbTone), 8000.}, {U(uReverbPredelay), 10.}, {U(uReverbMix), 14.}}},
  {"Singing lead plate", {{U(uReverbDecay), 2.8}, {U(uReverbPredelay), 35.}, {U(uReverbHighPass), 180.}, {U(uReverbDuck), 25.}, {U(uReverbMix), 22.}}},
  {"Warm rhythm plate", {{U(uReverbDecay), .8}, {U(uReverbSize), 85.}, {U(uReverbTone), 3800.}, {U(uReverbPredelay), 8.}, {U(uReverbMotion), 5.}, {U(uReverbHighPass), 180.}, {U(uReverbMix), 13.}}},
};
inline const Preset hall[]{
  {"Big dark hall", {{U(uReverbDecay), 6.}, {U(uReverbSize), 160.}, {U(uReverbTone), 3500.}, {U(uReverbPredelay), 40.}, {U(uReverbHighPass), 180.}, {U(uReverbMix), 28.}}},
  // A hall that steps back while notes play and blooms in the gaps.
  {"Ducked lead hall", {{U(uReverbDecay), 3.5}, {U(uReverbPredelay), 40.}, {U(uReverbDuck), 40.}, {U(uReverbMix), 25.}}},
  {"Small recital hall", {{U(uReverbDecay), 1.6}, {U(uReverbSize), 85.}, {U(uReverbTone), 7000.}, {U(uReverbPredelay), 18.}, {U(uReverbMotion), 8.}, {U(uReverbMix), 16.}}},
};
inline const Preset cloud[]{
  {"Long ducked cloud", {{U(uReverbDecay), 12.}, {U(uReverbSize), 180.}, {U(uReverbPredelay), 50.}, {U(uReverbHighPass), 220.}, {U(uReverbDuck), 55.}, {U(uReverbMix), 35.}}},
  {"Light moving halo", {{U(uReverbDecay), 3.}, {U(uReverbSize), 110.}, {U(uReverbTone), 7000.}, {U(uReverbPredelay), 20.}, {U(uReverbMotion), 25.}, {U(uReverbHighPass), 200.}, {U(uReverbMix), 20.}}},
};
inline const Preset spring[]{
  {"Small amp spring", {{U(uSpringDecay), 1.2}, {U(uSpringDwell), 1.5}, {U(uSpringDrip), 35.}, {U(uSpringMix), 14.}}},
  // The Dispersive tank's chirp provides the surf "drip"; Dwell drives the tank harder.
  {"Surf drip", {{U(uSpringTank), 1.}, {U(uSpringDecay), 2.5}, {U(uSpringDwell), 7.}, {U(uSpringDrip), 90.}, {U(uReverbTone), 6500.}, {U(uSpringMix), 35.}}},
  {"Short bright splash", {{U(uSpringTank), 1.}, {U(uSpringDecay), .7}, {U(uSpringDwell), 3.}, {U(uSpringDrip), 75.}, {U(uReverbTone), 7000.}, {U(uSpringMix), 22.}}},
};
inline const Preset shimmer[]{
  {"Dark octave-down pad", {{U(uShimmerDecay), 8.}, {U(uShimmerPitch), -12.}, {U(uShimmerTone), 3500.},
    {U(uShimmerAmount), 30.}, {U(uShimmerMix), 30.}}},
  {"Gentle octave halo", {{U(uShimmerDecay), 4.}, {U(uShimmerPitch), 12.}, {U(uShimmerTone), 6500.}, {U(uShimmerAmount), 15.}, {U(uShimmerMod), 20.}, {U(uShimmerMix), 20.}}},
  {"Rising fifth cloud", {{U(uShimmerDecay), 10.}, {U(uShimmerPitch), 7.}, {U(uShimmerTone), 4500.}, {U(uShimmerAmount), 35.}, {U(uShimmerMod), 40.}, {U(uShimmerMix), 32.}}},
};
// Resolution changes in steps: 50% crosses a rate step at 44.1/48/96 kHz,
// whereas 75% can process at the same internal rate as 100%.
inline const Preset airPlate[]{
  {"Compact guitar plate", {{U(uAirPlateSustain), 25.}, {U(uAirPlateResolution), 100.}, {U(uAirPlatePredelay), 10.},
    {U(uAirPlateLowCut), 150.}, {U(uAirPlateMix), 14.}}},
  {"Long dark plate", {{U(uAirPlateSustain), 60.}, {U(uAirPlateResolution), 50.}, {U(uAirPlatePredelay), 35.},
    {U(uAirPlateLowCut), 180.}, {U(uAirPlateMix), 24.}}},
  {"Clear lead plate", {{U(uAirPlateSustain), 42.}, {U(uAirPlateResolution), 100.}, {U(uAirPlatePredelay), 45.}, {U(uAirPlateLowCut), 220.}, {U(uAirPlateMix), 22.}}},
};
inline const Preset airHall[]{
  {"Close guitar hall", {{U(uAirHallSustain), 30.}, {U(uAirHallResolution), 100.}, {U(uAirHallTone), 65.},
    {U(uAirHallEarly), 80.}, {U(uAirHallPosition), 35.}, {U(uAirHallMix), 15.}}},
  {"Distant dark hall", {{U(uAirHallSustain), 60.}, {U(uAirHallResolution), 50.}, {U(uAirHallTone), 30.},
    {U(uAirHallEarly), 20.}, {U(uAirHallPosition), 75.}, {U(uAirHallMix), 25.}}},
};
inline const Preset tape[]{
  {"Light tape rounding", {{U(uTapeDrive), 9.}, {U(uTapeBump), 15.}, {U(uTapeMix), 100.}, {U(uTapeOutput), -9.}}},
  {"Thick parallel tape", {{U(uTapeDrive), 12.}, {U(uTapeBump), 40.}, {U(uTapeMix), 50.}, {U(uTapeOutput), -7.5}}},
};
inline const Preset space[]{
  {"Slow dark space", {{U(uSpaceReplace), 20.}, {U(uSpaceBrightness), 30.}, {U(uSpaceDetune), 20.}, {U(uSpaceSize), 75.}, {U(uSpaceMix), 28.}}},
  {"Wide bright wash", {{U(uSpaceReplace), 8.}, {U(uSpaceBrightness), 45.}, {U(uSpaceDetune), 50.}, {U(uSpaceSize), 95.}, {U(uSpaceMix), 35.}}},
  {"Gentle stereo orbit", {{U(uSpaceReplace), 55.}, {U(uSpaceBrightness), 60.}, {U(uSpaceDetune), 15.}, {U(uSpaceSize), 45.}, {U(uSpaceResolution), 100.}, {U(uSpaceMix), 18.}}},
};
// Lead with practical ambience, then demonstrate distinct mode behavior: feedback-
// damped Virgo versus clear Lyra, Centaurus versus slower Sagittarius build-up,
// and Great Annihilator echo diffusion. Decay length alone does not imply overlap.
// Cosmic Echo recipes use NAM Grid units, not normalized Valhalla preset fields.
// High Warp/Density follows Valhalla's published Controls/Modes guides:
// https://valhalladsp.com/2020/05/06/valhallasupermassive-the-controls/
// https://valhalladsp.com/2020/05/07/valhallasupermassive-the-modes/
// Velvet constellation and Dark orbit adapt the high-Warp, dense, modulated
// character of the author's own The Beauty / Dark Pleiades presets. Pleiades
// is not implemented here; these are independent Gemini/Aurora recipes.
inline const Preset cosmic[]{
  {"Compact Gemini room", {{U(uCosmicStyle), 0.}, {U(uCosmicTime), 80.}, {U(uCosmicWarp), 65.}, {U(uCosmicFeedback), 38.}, {U(uCosmicDensity), 100.}, {U(uCosmicTone), 7500.}, {U(uCosmicMotion), 8.}, {U(uCosmicMix), 14.}}},
  {"Photon lead plate", {{U(uCosmicStyle), 9.}, {U(uCosmicTime), 100.}, {U(uCosmicWarp), 70.}, {U(uCosmicFeedback), 65.}, {U(uCosmicDensity), 90.}, {U(uCosmicTone), 9000.}, {U(uCosmicMotion), 10.}, {U(uCosmicMix), 18.}}},
  {"Soft ambient bed", {{U(uCosmicStyle), 0.}, {U(uCosmicTime), 350.}, {U(uCosmicWarp), 80.}, {U(uCosmicFeedback), 65.}, {U(uCosmicDensity), 100.}, {U(uCosmicTone), 5500.}, {U(uCosmicMotion), 15.}, {U(uCosmicMix), 22.}}},
  {"Clustered echoes", {{U(uCosmicStyle), 6.}, {U(uCosmicTime), 350.}, {U(uCosmicWarp), 15.}, {U(uCosmicDensity), 10.}, {U(uCosmicTone), 6500.}, {U(uCosmicMotion), 5.}, {U(uCosmicMix), 20.}}},
  {"Pointillist echoes", {{U(uCosmicStyle), 2.}, {U(uCosmicTime), 280.}, {U(uCosmicWarp), 12.}, {U(uCosmicFeedback), 48.}, {U(uCosmicDensity), 20.}, {U(uCosmicTone), 4500.}, {U(uCosmicMotion), 5.}, {U(uCosmicMix), 24.}}},
  {"Clear distant repeats", {{U(uCosmicStyle), 5.}, {U(uCosmicTime), 600.}, {U(uCosmicWarp), 0.}, {U(uCosmicFeedback), 45.}, {U(uCosmicDensity), 0.}, {U(uCosmicTone), 7000.}, {U(uCosmicMotion), 0.}, {U(uCosmicMix), 22.}}},
  {"Stereo echo groups", {{U(uCosmicStyle), 7.}, {U(uCosmicTime), 500.}, {U(uCosmicWarp), 0.}, {U(uCosmicDensity), 15.}, {U(uCosmicTone), 6500.}, {U(uCosmicMotion), 5.}, {U(uCosmicMix), 25.}}},
  {"Enveloping swells", {{U(uCosmicStyle), 3.}, {U(uCosmicTime), 450.}, {U(uCosmicWarp), 72.}, {U(uCosmicFeedback), 72.}, {U(uCosmicDensity), 95.}, {U(uCosmicTone), 5000.}, {U(uCosmicMotion), 25.}, {U(uCosmicMix), 30.}}},
  {"Slow swell", {{U(uCosmicStyle), 1.}, {U(uCosmicTime), 400.}, {U(uCosmicMix), 25.}}},
  {"Diffusing echo trail", {{U(uCosmicStyle), 4.}, {U(uCosmicTime), 250.}, {U(uCosmicWarp), 35.}, {U(uCosmicFeedback), 35.}, {U(uCosmicDensity), 60.}, {U(uCosmicTone), 6000.}, {U(uCosmicMotion), 10.}, {U(uCosmicMix), 22.}}},
  {"Velvet constellation", {{U(uCosmicStyle), 0.}, {U(uCosmicTime), 950.}, {U(uCosmicWarp), 82.}, {U(uCosmicFeedback), 77.}, {U(uCosmicDensity), 98.}, {U(uCosmicTone), 5500.}, {U(uCosmicMotion), 45.}, {U(uCosmicMix), 35.}}},
  {"Dark orbit", {{U(uCosmicStyle), 6.}, {U(uCosmicTime), 500.}, {U(uCosmicWarp), 78.}, {U(uCosmicFeedback), 91.}, {U(uCosmicDensity), 100.}, {U(uCosmicTone), 2800.}, {U(uCosmicMotion), 50.}, {U(uCosmicMix), 32.}}},
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
  {"Vibrato mode swirl", {{U(uVibeMode), 1.}, {U(uModRate), 4.5}, {U(uModDepth), 40.}, {U(uModCharacter), 50.}, {U(uModMix), 100.}}},
};
// Leslie speeds: about 0.8 Hz chorale and 6.7 Hz tremolo for the horn.
inline const Preset rotary[]{
  {"Slow chorale", {{U(uRotarySpeed), 0.}, {U(uRotarySlow), .8}, {U(uRotaryDrive), 1.}, {U(uRotarySpread), 60.}}},
  {"Fast tremolo", {{U(uRotarySpeed), 1.}, {U(uRotaryFast), 6.7}, {U(uRotaryDrive), 3.}, {U(uRotaryAccel), 1.5}}},
};

inline const Preset octaver[]{
  {"Octave-down blend", {{U(uOctDry), 100.}, {U(uOctSub), 85.}, {U(uOctSub2), 0.}, {U(uOctTone), 800.}, {U(uOctOutput), -3.}}},
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
    case SlotEffect::DynamicEQ: return dynamicEQ;
    case SlotEffect::TransientShaper: return transientShaper;
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
    case SlotEffect::Tape: return tape;
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
