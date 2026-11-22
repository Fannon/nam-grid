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

// Each module offers established starting points; delay banks include shared rhythmic roles
// voiced for each module, since changing delay types requires replacing the module. A module
// whose insertion default already is its usual starting point may have none. Settings follow published pedal-manual starting points, documented player settings and
// professional plugin presets, translated to these controls and checked with offline audio. Names
// describe the result. Values use NAM Grid units; unlisted controls use insertion defaults.
// These arrays and their order are NOT persistent identities; only applied values are saved.

// Pedal Comp's Sustain range is calibrated against a Ross-type pedal. Pickup stays as the
// player set it unless a preset is for a particular instrument.
inline const Preset pedalComp[]{
  {"Parallel keys", {{U(uPedalCompSustain), 48.}, {U(uPedalCompAttack), 1.}, {U(uPedalCompFocus), 12.},
    {U(uPedalCompBlend), 38.}}},
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
// Outboard Channel sits after the amp and cab. Its Compress works relative to full scale,
// so Input lifts guitar-level signals into it; Output is measured to stay within 0.5 dB
// of bypass on a matching distorted rhythm, clean phrase or lead line. Cuts are relative:
// Low cut 8/10/13% is about 45/70/120 Hz; High cut 85/75% about 9/5.5 kHz.
inline const Preset outboardChannel[]{
  {"Tight rhythm bus", {{U(uStripInput), 3.}, {U(uStripLowCut), 10.}, {U(uStripHighCut), 85.}, {U(uStripCompress), 45.},
    {U(uStripSpeed), 35.}, {U(uStripSaturate), 20.}, {U(uStripOutput), 0.}}},
  {"Clean parallel glue", {{U(uStripInput), 6.}, {U(uStripLowCut), 8.}, {U(uStripCompress), 60.}, {U(uStripSpeed), 60.},
    {U(uStripSaturate), 10.}, {U(uStripOutput), -.5}, {U(uStripMix), 65.}}},
  {"Pushed lead", {{U(uStripInput), 3.}, {U(uStripLowCut), 13.}, {U(uStripHighCut), 75.}, {U(uStripCompress), 52.},
    {U(uStripSpeed), 20.}, {U(uStripSaturate), 35.}, {U(uStripOutput), 2.}}},
};
inline const Preset limiter[]{
  {"Louder, controlled",
   {{U(uLimiterInput), 6.}, {U(uLimiterCeiling), -1.}, {U(uLimiterRelease), 100.}, {U(uLimiterClip), 30.}}},
};

// Transient Shaper compares envelopes, so presets work at any input level. Output trims
// balance the shaping on guitar and bass. Gentle note sustain blends a modest tail
// lift under the dry attack; boosts still need headroom.
inline const Preset transientShaper[]{
  {"Shorten the room", {{U(uTransientShaperAttack), 0.}, {U(uTransientShaperSustain), -6.}, {U(uTransientShaperRecovery), 85.},
    {U(uTransientShaperMix), 75.}}},
  {"Defined picking", {{U(uTransientShaperAttack), 5.}, {U(uTransientShaperSustain), -2.}, {U(uTransientShaperRecovery), 150.},
    {U(uTransientShaperOutput), -.5}}},
  {"Soft acoustic", {{U(uTransientShaperAttack), -6.}, {U(uTransientShaperSustain), 1.}, {U(uTransientShaperRecovery), 150.}}},
  {"Tight palm mutes", {{U(uTransientShaperAttack), 2.}, {U(uTransientShaperSustain), -8.}, {U(uTransientShaperRecovery), 120.},
    {U(uTransientShaperSensitivity), 70.}, {U(uTransientShaperOutput), 1.5}}},
  {"Gentle note sustain", {{U(uTransientShaperAttack), 0.}, {U(uTransientShaperSustain), 4.}, {U(uTransientShaperRecovery), 220.},
    {U(uTransientShaperSensitivity), 45.}, {U(uTransientShaperMix), 60.}, {U(uTransientShaperOutput), -1.}}},
};
// Multiband Dynamics restores Threshold and splits with each recipe. Output trims match loudness within about
// 1 dB on a guitar DI and a bass DI peaking near -6 dBFS; a hotter amp output comes out
// 2-3 dB quieter on the heavier presets until Threshold is raised.
inline const Preset multiband[]{
  {"Pad detail", {{U(uMultibandDepth), 34.}, {U(uMultibandDownward), 18.}, {U(uMultibandUpward), 32.},
    {U(uMultibandTime), 225.}, {U(uMultibandOutput), 1.}}},
  {"Gentle glue", {{U(uMultibandDepth), 100.}, {U(uMultibandDownward), 30.}, {U(uMultibandUpward), 0.},
    {U(uMultibandTime), 150.}, {U(uMultibandOutput), 5.}}},
  {"Over the top", {{U(uMultibandDepth), 100.}, {U(uMultibandDownward), 100.}, {U(uMultibandUpward), 100.},
    {U(uMultibandTime), 100.}, {U(uMultibandOutput), 7.}}},

  {"Fast and tight", {{U(uMultibandDepth), 100.}, {U(uMultibandDownward), 60.}, {U(uMultibandUpward), 0.},
    {U(uMultibandTime), 50.}, {U(uMultibandLow), -1.}, {U(uMultibandHigh), 1.}, {U(uMultibandOutput), 7.},
    {U(uMultibandLowSplit), 120.}, {U(uMultibandHighSplit), 3000.}}},
  {"Clean sparkle", {{U(uMultibandDepth), 50.}, {U(uMultibandDownward), 20.}, {U(uMultibandUpward), 70.},
    {U(uMultibandTime), 120.}, {U(uMultibandHigh), 2.}, {U(uMultibandOutput), 1.5}}},
};
inline const Preset toneEQ[]{
  // The "smile" curve: fuller lows and highs around scooped low mids.
  {"Scooped mids",
   {{parameter_layout::kSlotEQBass, 3.}, {parameter_layout::kSlotEQMid, -4.}, {U(uToneMidFrequency), 500.},
    {parameter_layout::kSlotEQTreble, 2.}}},
  {"Mid-forward lead", {{parameter_layout::kSlotEQBass, -1.5}, {parameter_layout::kSlotEQMid, 3.}, {U(uEQOutput), -2.}}},
  {"Dark jazz clean", {{parameter_layout::kSlotEQBass, 1.5}, {parameter_layout::kSlotEQTreble, -5.}}},
};
// Console EQ's broad first-order bands colour like a desk channel: boosts add harmonics on
// louder notes, so small moves suit it. Output matches bypass loudness after an amp.
inline const Preset consoleEQ[]{
  {"Warm", {{U(uConsoleTreble), -3.}, {U(uConsoleTrebleFreq), 3000.}, {U(uConsoleBass), 2.5}, {U(uEQOutput), -2.}}},
  {"Bright and open", {{U(uConsoleTreble), 3.}, {U(uConsoleTrebleFreq), 3500.}, {U(uConsoleBass), -1.5},
    {U(uConsoleLowCut), 8.}, {U(uEQOutput), 1.5}}},
  {"Fat", {{U(uConsoleBass), 4.}, {U(uConsoleBassFreq), 400.}, {U(uConsoleMid), 1.}, {U(uConsoleLowCut), 6.},
    {U(uEQOutput), -3.}}},
  // High-gain fizz sits above about 5 kHz; a lower Treble freq also tames pick harshness.
  {"Smooth top", {{U(uConsoleTreble), -4.}, {U(uConsoleTrebleFreq), 3000.}, {U(uConsoleHighCut), 95.}}},
  // Less low end and a little presence so a guitar sits in a band mix.
  {"Lean for the mix", {{U(uConsoleBass), -4.}, {U(uConsoleBassFreq), 500.}, {U(uConsoleTreble), 1.5}, {U(uConsoleLowCut), 10.},
    {U(uEQOutput), 4.}}},
};
inline const Preset cutEQ[]{
  {"Tight high gain", {{parameter_layout::kSlotEQHighPass, 110.}, {parameter_layout::kSlotEQLowPass, 7000.}}},
  {"Bass cleanup", {{parameter_layout::kSlotEQHighPass, 35.}, {parameter_layout::kSlotEQLowPass, 12000.}}},
  // A narrow 300 Hz-3 kHz band with a presence peak: the telephone/AM-radio effect.
  {"Telephone band", {{parameter_layout::kSlotEQHighPass, 300.}, {parameter_layout::kSlotEQLowPass, 3000.},
    {parameter_layout::kSlotEQPresence, 3.}, {U(uEQOutput), 4.}}},
};
// Graphic EQ: Voicing is part of each recipe. Lead mid boost is meant to be about 2-3 dB
// louder for solos and Tight into amp drives the next stage; the others match loudness
// within about 0.3 dB on a guitar or bass DI.
inline const Preset graphicEQ[]{
  {"Warm keys", {{U(uGraphicVoicing), 2.}, {U(uGraphicBand2), 1.5}, {U(uGraphicBand5), -1.},
    {U(uGraphicBand6), -2.}, {U(uGraphicBand7), -3.}}},
  {"Lead mid boost", {{U(uGraphicVoicing), 0.}, {U(uGraphicBand1), -3.}, {U(uGraphicBand2), 0.}, {U(uGraphicBand3), 3.},
    {U(uGraphicBand4), 6.}, {U(uGraphicBand5), 3.}, {U(uGraphicBand6), 0.}, {U(uGraphicBand7), -2.}, {U(uGraphicLevel), -1.}}},
  {"Tight into amp", {{U(uGraphicVoicing), 0.}, {U(uGraphicBand1), -9.}, {U(uGraphicBand2), -4.}, {U(uGraphicBand3), 0.},
    {U(uGraphicBand4), 2.}, {U(uGraphicBand5), 3.}, {U(uGraphicBand6), 1.}, {U(uGraphicBand7), -1.}, {U(uGraphicLevel), 4.}}},
  {"Bass clarity", {{U(uGraphicVoicing), 1.}, {U(uGraphicBand1), 2.}, {U(uGraphicBand2), 1.}, {U(uGraphicBand3), -2.},
    {U(uGraphicBand4), -3.}, {U(uGraphicBand5), 2.}, {U(uGraphicBand6), 3.}, {U(uGraphicBand7), 0.}, {U(uGraphicLevel), 1.}}},
  {"Acoustic air", {{U(uGraphicVoicing), 2.}, {U(uGraphicBand1), -3.}, {U(uGraphicBand2), -2.}, {U(uGraphicBand3), -1.},
    {U(uGraphicBand4), 0.}, {U(uGraphicBand5), 1.}, {U(uGraphicBand6), 2.}, {U(uGraphicBand7), 2.}, {U(uGraphicLevel), 1.5}}},
};
inline const Preset parametricEQ[]{
  {"Low-mid mud cut", {{U(uFrequency), 300.}, {U(uGain), -3.}, {U(uQ), .8}, {parameter_layout::kSlotEQHighPass, 60.}}},
  {"Harshness cut", {{U(uFrequency), 3500.}, {U(uGain), -3.}, {U(uQ), 1.5}}},
};
// Dynamic EQ thresholds are absolute, so each preset names where it sits. They were set so the
// band is cut on about a quarter to a third of played audio and rests between peaks: guitar DI
// peaking near -6 dBFS, the bundled clean capture's output, and a bass DI peaking near -6 dBFS.
inline const Preset dynamicEQ[]{
  {"Resonant synth tamer", {{U(uDynamicEQFrequency), 1800.}, {U(uDynamicEQQ), 2.2}, {U(uDynamicEQThreshold), -30.},
    {U(uDynamicEQRange), 4.}, {U(uDynamicEQAttack), 3.}, {U(uDynamicEQRelease), 100.}}},
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
  {"Bass envelope", {{U(uFilterCutoff), 200.}, {U(uFilterResonance), 2.5}, {U(uFilterEnvelope), 3.5}}},
  // Band-pass quack for clean funk strumming. Lower Sensitivity follows the picking instead
  // of opening fully on every note; Output restores the level band-pass removes.
  {"Funk quack", {{U(uFilterMode), 2.}, {U(uFilterCutoff), 400.}, {U(uFilterResonance), 3.}, {U(uFilterEnvelope), 2.5},
    {U(uFilterSensitivity), -6.}, {U(uFilterOutput), 2.5}}},
  // A long Decay closes each note slowly, like a held "wow".
  {"Slow vowel", {{U(uFilterCutoff), 300.}, {U(uFilterResonance), 3.}, {U(uFilterEnvelope), 3.}, {U(uFilterDecay), 400.}}},
  // Bright at rest; each attack darkens the note and it opens again as it fades.
  {"Down sweep", {{U(uFilterCutoff), 3000.}, {U(uFilterResonance), 2.5}, {U(uFilterEnvelope), 3.}, {U(uFilterDown), 1.},
    {U(uFilterDecay), 150.}, {U(uFilterOutput), -1.5}}},
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
  {"Mid push", {{U(uBoostGain), 4.}, {U(uBoostMidGain), 5.}, {parameter_layout::kSlotEQHighPass, 60.}}},
};
inline const Preset tube[]{
  {"Warm tube edge", {{U(uTubeDrive), 9.}, {U(uTubeBias), 20.}, {U(uTubeHighCut), 10000.}, {U(uTubeOutput), -7.}}},
  {"Hot preamp grit", {{U(uTubeDrive), 18.}, {U(uTubeBias), 40.}, {parameter_layout::kSlotEQHighPass, 80.},
    {U(uTubeHighCut), 7000.}, {U(uTubeOutput), -16.}}},
};
inline const Preset distortion[]{
  {"Parallel drum grit", {{U(uDistStyle), 1.}, {U(uDistDrive), 18.}, {U(uDistTone), 6500.},
    {U(uDistMix), 22.}, {U(uDistOutput), -4.}}},
  {"Clean lows, warm grit", {{U(uDistStyle), 0.}, {U(uDistDrive), 9.}, {U(uDistTone), 6000.},
    {U(uDistLowPreserve), 220.}, {U(uDistOutput), -6.}, {U(uDistMix), 60.}}},
  {"Hard rhythm", {{U(uDistStyle), 1.}, {U(uDistDrive), 24.}, {U(uDistTone), 4500.}, {U(uDistOutput), -20.}}},
  // Rectification adds the octave-up character of octave fuzz pedals.
  {"Octave-up fuzz", {{U(uDistStyle), 5.}, {U(uDistDrive), 12.}, {U(uDistTone), 5000.}, {U(uDistOutput), -4.}}},
  // Strong bias starves the fuzz so decays break up and gate.
  {"Sputtering gated fuzz", {{U(uDistStyle), 2.}, {U(uDistDrive), 20.}, {U(uDistTone), 4000.}, {U(uDistBias), 70.},
    {U(uDistOutput), -18.}}},
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
  {"Pick edge", {{U(uPedalStyle), 8.}, {U(uPedalDrive), 8.}, {U(uPedalTone), 7500.},
    {U(uPedalMix), 55.}, {U(uPedalOutput), -3.}}},
};
// Levels leave a guitar DI about 1.5 dB above bypass RMS; the clipped signal's lower crest
// factor already sounds louder. Metal settings follow common scooped, tightened rhythm setups.
inline const Preset pedalDistortion[]{
  {"Classic rock distortion", {{U(uPDistStyle), 0.}, {U(uPDistGain), 45.}, {U(uPDistTreble), -2.}, {U(uPDistLevel), -1.}}},
  {"British crunch", {{U(uPDistStyle), 2.}, {U(uPDistGain), 40.}, {U(uPDistBass), 1.}, {U(uPDistLevel), -1.}}},
  {"Scooped metal rhythm", {{U(uPDistStyle), 4.}, {U(uPDistGain), 70.}, {U(uPDistTight), 40.}, {U(uPDistBass), 4.},
    {U(uPDistMid), -6.}, {U(uPDistMidFrequency), 700.}, {U(uPDistLevel), -1.5}}},
  // Raw distortion with gain well above the default; Level trims the rise in loudness.
  {"Raw garage grit", {{U(uPDistStyle), 1.}, {U(uPDistGain), 70.}, {U(uPDistTreble), -1.}, {U(uPDistLevel), -3.}}},
  // Hot-rodded lead box: more gain, the mid hump pushed up, and a smooth top.
  {"Singing lead", {{U(uPDistStyle), 3.}, {U(uPDistGain), 65.}, {U(uPDistMid), 3.}, {U(uPDistMidFrequency), 1200.},
    {U(uPDistTreble), -2.}, {U(uPDistLevel), -3.5}}},
  {"Parallel chainsaw", {{U(uPDistStyle), 5.}, {U(uPDistGain), 60.}, {U(uPDistTight), 45.},
    {U(uPDistMix), 32.}, {U(uPDistTreble), -3.}, {U(uPDistLevel), -1.}}},
};

// Delay levels: one loud repeat for slapback, two to four repeats for rhythmic and lead echoes.
// Synced recipes deliberately share rhythms across modules, with tone, motion and feedback
// suited to each topology. Digital crossfades tempo jumps; analog/tape keep pitch glide.
// Reverse divisions set capture windows; multi-head divisions set the LAST head.
// Dual ping-pong alternates unequal times; parallel pairs keep independent pulse trains.
// Community roles: https://www.reddit.com/r/guitarpedals/comments/1k7acuc/
// https://line6.com/support/topic/1856-dotted-8th-settings/
// Head timing: https://www.strymon.net/support/volante/
// The author's ValhallaDelay Analog Past, ChromeTape for Pads and Space 1 presets
// inform these roles, not parameter conversions: NAM Grid uses different DSP and
// does not combine ducking, saturation, diffusion and independent L/R sync in one module.
// References: https://valhalladsp.com/2019/04/16/valhalladelay-the-controls/
// https://valhalladsp.com/2019/04/16/valhalladelay-the-style-control/
inline const Preset digitalDelay[]{
  // Straight, unducked support before the more specialized rhythmic recipes.
  {"Clear quarter-note lead", {{U(uTempoSync), 1.}, {U(uTempoDivision), 5.}, {U(uDelayTimeChange), 1.},
    {U(uDelayFeedback), 32.}, {U(uDelayTone), 8000.}, {U(uDelayCharacter), 0.}, {U(uDelayMix), 25.}}},
  {"Slapback",
   {{U(uDelayTime), 110.}, {U(uDelayFeedback), 0.}, {U(uDelayTone), 8000.}, {U(uDelayCharacter), 0.},
    {U(uDelayMix), 35.}}},
  {"Dotted eighth",
   {{U(uTempoSync), 1.}, {U(uTempoDivision), 4.}, {U(uDelayTimeChange), 1.}, {U(uDelayFeedback), 35.}, {U(uDelayTone), 6000.},
    {U(uDelayCharacter), 0.}, {U(uDelayMix), 30.}}},
  // Long lead repeats that duck under played notes and bloom in the gaps.
  {"Ducked lead",
   {{U(uDelayTime), 480.}, {U(uDelayFeedback), 40.}, {U(uDelayTone), 7000.}, {U(uDelayCharacter), 60.},
    {U(uDelayMix), 30.}}},
  {"Quarter-note ping-pong",
   {{U(uTempoSync), 1.}, {U(uTempoDivision), 5.}, {U(uDelayTimeChange), 1.}, {U(uDelayPingPong), 1.}, {U(uDelayFeedback), 35.},
    {U(uDelayTone), 6000.}, {U(uDelayCharacter), 0.}, {U(uDelayMix), 30.}}},
  {"Quiet ducked support", {{U(uTempoSync), 1.}, {U(uTempoDivision), 5.}, {U(uDelayTimeChange), 1.},
    {U(uDelayFeedback), 28.}, {U(uDelayTone), 6500.}, {U(uDelayCharacter), 70.}, {U(uDuckRelease), 220.}, {U(uDelayMix), 12.}}},
  {"Clear eighth-note rhythm", {{U(uTempoSync), 1.0}, {U(uTempoDivision), 3.0}, {U(uDelayTimeChange), 1.0}, {U(uDelayFeedback), 22.0}, {U(uDelayTone), 10000.0}, {U(uDelayCharacter), 0.0}, {U(uDelayMix), 24.0}}},
  {"Half-note space", {{U(uTempoSync), 1.0}, {U(uTempoDivision), 7.0}, {U(uDelayTimeChange), 1.0}, {U(uDelayPingPong), 1.0}, {U(uDelayFeedback), 48.0}, {U(uDelayTone), 6500.0}, {U(uDelayCharacter), 25.0}, {U(uDelayMix), 25.0}, {U(uDuckRelease), 280.0}}},
  // Converter voices: modulated 12-bit rack repeats, and 8-bit repeats that coarsen on each pass.
  {"Modulated rack lead", {{U(uDigitalDelayVoice), 1.}, {U(uDelayTime), 325.}, {U(uDelayFeedback), 30.},
    {U(uDelayTone), 7500.}, {U(uDelayMotion), 35.}, {U(uDelayMix), 25.}}},
  {"Lo-fi echoes", {{U(uDigitalDelayVoice), 2.}, {U(uDelayTime), 380.}, {U(uDelayFeedback), 47.},
    {U(uDelayMotion), 8.}, {U(uDelayMix), 30.}}},
};
inline const Preset analogDelay[]{
  {"Warm lead repeats", {{U(uDelayTime), 420.}, {U(uDelayFeedback), 35.}, {U(uDelayTone), 3200.}, {U(uDelayCharacter), 40.}, {U(uDelayMix), 30.}}},
  {"Dark modulated wash", {{U(uDelayTime), 600.}, {U(uDelayFeedback), 55.}, {U(uDelayTone), 2400.}, {U(uDelayCharacter), 50.}, {U(uDelayMotion), 40.}, {U(uDelayMix), 30.}}},
  // Single dark, saturated repeat, as with analog slapback pedals.
  {"Dark slapback", {{U(uDelayTime), 120.}, {U(uDelayFeedback), 5.}, {U(uDelayTone), 2800.}, {U(uDelayCharacter), 45.}, {U(uDelayMotion), 5.}, {U(uDelayMix), 40.}}},
  {"Bright dotted echoes", {{U(uTempoSync), 1.}, {U(uTempoDivision), 4.}, {U(uDelayFeedback), 30.},
    {U(uDelayTone), 6500.}, {U(uDelayCharacter), 15.}, {U(uDelayMotion), 5.}, {U(uDelayMix), 22.}}},
  {"Warm quarter notes", {{U(uTempoSync), 1.0}, {U(uTempoDivision), 5.0}, {U(uDelayFeedback), 32.0}, {U(uDelayTone), 3800.0}, {U(uDelayCharacter), 35.0}, {U(uDelayMotion), 10.0}, {U(uDelayMix), 24.0}}},
  {"Soft eighth-note pulse", {{U(uTempoSync), 1.0}, {U(uTempoDivision), 3.0}, {U(uDelayFeedback), 24.0}, {U(uDelayTone), 4200.0}, {U(uDelayCharacter), 25.0}, {U(uDelayMotion), 5.0}, {U(uDelayMix), 22.0}}},
  {"Dark dotted lead", {{U(uTempoSync), 1.0}, {U(uTempoDivision), 4.0}, {U(uDelayFeedback), 40.0}, {U(uDelayTone), 3000.0}, {U(uDelayCharacter), 48.0}, {U(uDelayMotion), 14.0}, {U(uDelayMix), 28.0}}},
  {"Modulated half-note bed", {{U(uTempoSync), 1.0}, {U(uTempoDivision), 7.0}, {U(uDelayFeedback), 52.0}, {U(uDelayTone), 2600.0}, {U(uDelayCharacter), 45.0}, {U(uDelayMotion), 32.0}, {U(uDelayMix), 27.0}}},
};
inline const Preset tapeDelay[]{
  {"Tape slapback", {{U(uDelayTime), 120.}, {U(uDelayFeedback), 5.}, {U(uDelayTone), 5500.}, {U(uDelayCharacter), 25.}, {U(uDelayMotion), 8.}, {U(uDelayMix), 35.}}},
  {"Worn tape repeats", {{U(uDelayTime), 430.}, {U(uDelayFeedback), 38.}, {U(uDelayTone), 3000.}, {U(uDelayCharacter), 55.}, {U(uDelayMotion), 35.}, {U(uDelayMix), 30.}}},
  // About one quiet 300 ms repeat: the classic rock lead echo of an EP-3.
  {"Rock lead echo", {{U(uDelayTime), 300.}, {U(uDelayFeedback), 8.}, {U(uDelayTone), 4500.}, {U(uDelayCharacter), 30.}, {U(uDelayMotion), 12.}, {U(uDelayMix), 25.}}},
  {"Clean dotted tape", {{U(uTempoSync), 1.}, {U(uTempoDivision), 4.}, {U(uDelayFeedback), 28.},
    {U(uDelayTone), 6000.}, {U(uDelayCharacter), 12.}, {U(uDelayMotion), 5.}, {U(uDelayMix), 22.}}},
  {"Warm quarter-note tape", {{U(uTempoSync), 1.0}, {U(uTempoDivision), 5.0}, {U(uDelayFeedback), 34.0}, {U(uDelayTone), 4800.0}, {U(uDelayCharacter), 32.0}, {U(uDelayMotion), 12.0}, {U(uDelayMix), 25.0}}},
  {"Eighth-note tape bounce", {{U(uTempoSync), 1.0}, {U(uTempoDivision), 3.0}, {U(uDelayFeedback), 25.0}, {U(uDelayTone), 5500.0}, {U(uDelayCharacter), 22.0}, {U(uDelayMotion), 6.0}, {U(uDelayMix), 24.0}}},
  {"Dotted-quarter tape drift", {{U(uTempoSync), 1.0}, {U(uTempoDivision), 6.0}, {U(uDelayFeedback), 43.0}, {U(uDelayTone), 3800.0}, {U(uDelayCharacter), 45.0}, {U(uDelayMotion), 24.0}, {U(uDelayMix), 28.0}}},
  {"Half-note tape haze", {{U(uTempoSync), 1.0}, {U(uTempoDivision), 7.0}, {U(uDelayFeedback), 52.0}, {U(uDelayTone), 3200.0}, {U(uDelayCharacter), 48.0}, {U(uDelayMotion), 30.0}, {U(uDelayMix), 28.0}}},
};
inline const Preset multiHead[]{
  {"Rolling three heads", {{U(uTapeHeads), 2.}, {U(uDelayTime), 600.}, {U(uDelayFeedback), 28.}, {U(uDelayMix), 28.}}},
  {"Stereo cascade", {{U(uTapeHeads), 7.}, {U(uTempoSync), 1.}, {U(uTempoDivision), 7.}, {U(uDelayFeedback), 35.}, {U(uDelayMix), 28.}}},
  {"All-head wash", {{U(uTapeHeads), 3.}, {U(uDelayTime), 850.}, {U(uDelayFeedback), 55.}, {U(uDelayTone), 3000.}, {U(uDelayCharacter), 40.}, {U(uDelayMotion), 25.}, {U(uDelayMix), 30.}}},
  {"Sparse stereo heads", {{U(uTapeHeads), 6.}, {U(uTempoSync), 1.}, {U(uTempoDivision), 5.},
    {U(uDelayFeedback), 22.}, {U(uDelayTone), 6000.}, {U(uDelayCharacter), 15.}, {U(uDelayMotion), 5.}, {U(uDelayMix), 20.}}},
  {"Eighth and quarter heads", {{U(uTempoSync), 1.0}, {U(uTempoDivision), 5.0}, {U(uTapeHeads), 1.0}, {U(uDelayFeedback), 26.0}, {U(uDelayTone), 5500.0}, {U(uDelayCharacter), 25.0}, {U(uDelayMotion), 7.0}, {U(uDelayMix), 26.0}}},
  {"Dotted and quarter heads", {{U(uTempoSync), 1.0}, {U(uTempoDivision), 5.0}, {U(uTapeHeads), 5.0}, {U(uDelayFeedback), 30.0}, {U(uDelayTone), 6000.0}, {U(uDelayCharacter), 20.0}, {U(uDelayMotion), 5.0}, {U(uDelayMix), 28.0}}},
  {"Synced rolling heads", {{U(uTempoSync), 1.0}, {U(uTempoDivision), 5.0}, {U(uTapeHeads), 2.0}, {U(uDelayFeedback), 25.0}, {U(uDelayTone), 5000.0}, {U(uDelayCharacter), 28.0}, {U(uDelayMotion), 8.0}, {U(uDelayMix), 27.0}}},
  {"Half-note head cloud", {{U(uTempoSync), 1.0}, {U(uTempoDivision), 7.0}, {U(uTapeHeads), 3.0}, {U(uDelayFeedback), 48.0}, {U(uDelayTone), 3200.0}, {U(uDelayCharacter), 42.0}, {U(uDelayMotion), 22.0}, {U(uDelayMix), 30.0}}},
};
inline const Preset reverseDelay[]{
  {"Reverse half notes", {{U(uTempoSync), 1.}, {U(uTempoDivision), 7.}, {U(uReverseFeedback), 25.}, {U(uDelayTone), 4500.}, {U(uDelayMix), 35.}}},
  // Mostly reversed playing, like a backwards-tape solo.
  {"Reversed solo", {{U(uDelayTime), 750.}, {U(uReverseFeedback), 30.}, {U(uDelayTone), 5000.}, {U(uDelayMix), 70.}}},
  {"Short reverse texture", {{U(uDelayTime), 220.}, {U(uReverseFeedback), 0.}, {U(uDelayTone), 6500.}, {U(uDelayMix), 22.}}},
  {"Eighth-note reverse flicker", {{U(uTempoSync), 1.0}, {U(uTempoDivision), 3.0}, {U(uReverseFeedback), 8.0}, {U(uDelayTone), 6500.0}, {U(uDelayMix), 28.0}}},
  {"Quarter-note reverse bloom", {{U(uTempoSync), 1.0}, {U(uTempoDivision), 5.0}, {U(uReverseFeedback), 22.0}, {U(uDelayTone), 5500.0}, {U(uDelayMix), 38.0}}},
  {"Dotted-quarter reverse wash", {{U(uTempoSync), 1.0}, {U(uTempoDivision), 6.0}, {U(uReverseFeedback), 50.0}, {U(uDelayTone), 3500.0}, {U(uDelayMix), 38.0}}},
};
inline const Preset dualDelay[]{
  {"Quarter and dotted eighth", {{U(uTempoSync), 1.}, {U(uTempoDivision), 5.}, {U(uDualRatio), .75}, {U(uDelayFeedback), 28.}, {U(uDelayMix), 30.}}},
  {"Wide ping-pong", {{U(uDelayTime), 350.}, {U(uDualRatio), 1.5}, {U(uDelayPingPong), 1.}, {U(uDelayFeedback), 35.}, {U(uDelayMix), 28.}}},
  {"Tight stereo double", {{U(uDelayTime), 80.}, {U(uDualRatio), 1.25}, {U(uDualBalance), 50.},
    {U(uDelayFeedback), 0.}, {U(uDelayTone), 8000.}, {U(uDelayPingPong), 0.}, {U(uDelayMix), 15.}}},
  {"Eighth and dotted eighth", {{U(uTempoSync), 1.0}, {U(uTempoDivision), 3.0}, {U(uDualRatio), 1.5}, {U(uDualBalance), 50.0}, {U(uDelayFeedback), 26.0}, {U(uDelayTone), 7500.0}, {U(uDelayPingPong), 0.0}, {U(uDelayMix), 28.0}}},
  {"Quarter and eighth", {{U(uTempoSync), 1.0}, {U(uTempoDivision), 5.0}, {U(uDualRatio), 0.5}, {U(uDualBalance), 50.0}, {U(uDelayFeedback), 25.0}, {U(uDelayTone), 7000.0}, {U(uDelayPingPong), 0.0}, {U(uDelayMix), 26.0}}},
  {"Quarter and triplet", {{U(uTempoSync), 1.0}, {U(uTempoDivision), 5.0}, {U(uDualRatio), 2. / 3.}, {U(uDualBalance), 50.0}, {U(uDelayFeedback), 28.0}, {U(uDelayTone), 6500.0}, {U(uDelayPingPong), 0.0}, {U(uDelayMix), 27.0}}},
  {"Synced dotted ping-pong", {{U(uTempoSync), 1.0}, {U(uTempoDivision), 5.0}, {U(uDualRatio), 0.75}, {U(uDualBalance), 50.0}, {U(uDelayFeedback), 36.0}, {U(uDelayTone), 6500.0}, {U(uDelayPingPong), 1.0}, {U(uDelayMix), 28.0}}},
  {"Half and dotted quarter", {{U(uTempoSync), 1.0}, {U(uTempoDivision), 7.0}, {U(uDualRatio), 0.75}, {U(uDualBalance), 50.0}, {U(uDelayFeedback), 44.0}, {U(uDelayTone), 5000.0}, {U(uDelayPingPong), 0.0}, {U(uDelayMix), 28.0}}},
};
inline const Preset diffusedDelay[]{
  {"Echo into cloud", {{U(uDelayTime), 650.}, {U(uDelayFeedback), 60.}, {U(uDelayDiffusion), 85.}, {U(uDelayTone), 4500.}, {U(uDelayMotion), 25.}, {U(uDelayMix), 32.}}},
  // Oil-can echo units: short, dark repeats smeared by the oil and a wobbling disc.
  {"Oil can echo", {{U(uDelayTime), 180.}, {U(uDelayFeedback), 50.}, {U(uDelayDiffusion), 45.}, {U(uDelayTone), 2600.},
    {U(uDelayMotion), 55.}, {U(uDelayMix), 30.}}},
  {"Gentle pad echoes", {{U(uDelayTime), 550.}, {U(uDelayFeedback), 37.}, {U(uDelayDiffusion), 68.},
    {U(uDelayTone), 4500.}, {U(uDelayMotion), 15.}, {U(uDelayMix), 22.}}},
  {"Soft dotted-eighth rhythm", {{U(uTempoSync), 1.0}, {U(uTempoDivision), 4.0}, {U(uDelayFeedback), 32.0}, {U(uDelayDiffusion), 28.0}, {U(uDelayTone), 7000.0}, {U(uDelayMotion), 6.0}, {U(uDelayMix), 25.0}}},
  {"Quarter-note echo halo", {{U(uTempoSync), 1.0}, {U(uTempoDivision), 5.0}, {U(uDelayFeedback), 38.0}, {U(uDelayDiffusion), 48.0}, {U(uDelayTone), 6000.0}, {U(uDelayMotion), 12.0}, {U(uDelayMix), 25.0}}},
  {"Triplet echo clusters", {{U(uTempoSync), 1.0}, {U(uTempoDivision), 2.0}, {U(uDelayFeedback), 30.0}, {U(uDelayDiffusion), 42.0}, {U(uDelayTone), 6500.0}, {U(uDelayMotion), 8.0}, {U(uDelayMix), 24.0}}},
  {"Dotted-quarter dream", {{U(uTempoSync), 1.0}, {U(uTempoDivision), 6.0}, {U(uDelayFeedback), 52.0}, {U(uDelayDiffusion), 72.0}, {U(uDelayTone), 4500.0}, {U(uDelayMotion), 22.0}, {U(uDelayMix), 29.0}}},
  {"Half-note cloud", {{U(uTempoSync), 1.0}, {U(uTempoDivision), 7.0}, {U(uDelayFeedback), 62.0}, {U(uDelayDiffusion), 90.0}, {U(uDelayTone), 3800.0}, {U(uDelayMotion), 28.0}, {U(uDelayMix), 32.0}}},
};

inline const Preset room[]{
  {"Live room", {{U(uReverbDecay), 1.2}, {U(uReverbSize), 100.}, {U(uReverbPredelay), 12.}, {U(uReverbMix), 18.}}},
  // Short, tight ambience for dry captures and DI-like close sounds.
  {"Small amp room", {{U(uReverbDecay), .45}, {U(uReverbSize), 55.}, {U(uReverbPredelay), 3.}, {U(uReverbMix), 10.}}},
  // Early reflections without an audible tail: width and depth for dry rhythm parts.
  {"Bright ambience", {{U(uReverbDecay), .5}, {U(uReverbSize), 70.}, {U(uReverbTone), 11000.}, {U(uReverbPredelay), 0.},
    {U(uReverbHighPass), 250.}, {U(uReverbMix), 20.}}},
  {"Big live room", {{U(uReverbDecay), 1.8}, {U(uReverbSize), 160.}, {U(uReverbTone), 4500.}, {U(uReverbPredelay), 18.},
    {U(uReverbMotion), 15.}, {U(uReverbMix), 18.}}},
};
inline const Preset plate[]{
  {"Long silver plate", {{U(uReverbDecay), 4.8}, {U(uReverbPredelay), 55.}, {U(uReverbHighPass), 250.},
    {U(uReverbTone), 7200.}, {U(uReverbMotion), 18.}, {U(uReverbMix), 24.}}},
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
  {"Percussion mist", {{U(uReverbDecay), 1.25}, {U(uReverbSize), 75.}, {U(uReverbPredelay), 26.},
    {U(uReverbHighPass), 320.}, {U(uReverbTone), 7200.}, {U(uReverbMotion), 8.}, {U(uReverbMix), 19.}}},
  {"Wide synth bed", {{U(uReverbDecay), 7.}, {U(uReverbSize), 155.}, {U(uReverbPredelay), 12.},
    {U(uReverbHighPass), 190.}, {U(uReverbTone), 5000.}, {U(uReverbMotion), 48.}, {U(uReverbMix), 38.}}},
  {"Chord afterglow", {{U(uReverbDecay), 5.}, {U(uReverbPredelay), 65.}, {U(uReverbHighPass), 260.},
    {U(uReverbTone), 3400.}, {U(uReverbDuck), 38.}, {U(uReverbMix), 24.}}},
  {"Long ducked cloud", {{U(uReverbDecay), 12.}, {U(uReverbSize), 180.}, {U(uReverbPredelay), 50.}, {U(uReverbHighPass), 220.}, {U(uReverbDuck), 55.}, {U(uReverbMix), 35.}}},
  {"Light moving halo", {{U(uReverbDecay), 3.}, {U(uReverbSize), 110.}, {U(uReverbTone), 7000.}, {U(uReverbPredelay), 20.}, {U(uReverbMotion), 25.}, {U(uReverbHighPass), 200.}, {U(uReverbMix), 20.}}},
};
inline const Preset spring[]{
  {"Warm tank slap", {{U(uSpringPredelay), 82.}, {U(uSpringDecay), .55}, {U(uSpringDwell), 2.},
    {U(uReverbTone), 3200.}, {U(uSpringDrip), 18.}, {U(uSpringMix), 19.}}},
  {"Small amp spring", {{U(uSpringDecay), 1.2}, {U(uSpringDwell), 1.5}, {U(uSpringDrip), 35.}, {U(uSpringMix), 14.}}},
  // The Dispersive tank's chirp provides the surf "drip"; Dwell drives the tank harder.
  {"Surf drip", {{U(uSpringTank), 1.}, {U(uSpringDecay), 2.5}, {U(uSpringDwell), 7.}, {U(uSpringDrip), 90.}, {U(uReverbTone), 6500.}, {U(uSpringMix), 35.}}},
  {"Short bright splash", {{U(uSpringTank), 1.}, {U(uSpringDecay), .7}, {U(uSpringDwell), 3.}, {U(uSpringDrip), 75.}, {U(uReverbTone), 7000.}, {U(uSpringMix), 22.}}},
};
inline const Preset shimmer[]{
  {"Slow octave bloom", {{U(uShimmerBloom), 92.}, {U(uShimmerDecay), 14.}, {U(uShimmerTone), 7400.},
    {U(uShimmerAmount), 50.}, {U(uShimmerMod), 55.}, {U(uShimmerMix), 48.}}},
  {"Octave on the pick", {{U(uShimmerDecay), 1.4}, {U(uShimmerAmount), 45.}, {U(uShimmerMod), 6.},
    {U(uShimmerTone), 4800.}, {U(uShimmerMix), 18.}}},
  {"Low fifth swell", {{U(uShimmerPitch), -7.}, {U(uShimmerDecay), 6.}, {U(uShimmerAmount), 38.},
    {U(uShimmerMod), 14.}, {U(uShimmerTone), 4000.}, {U(uShimmerMix), 26.}}},
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
  {"Velvet tracking room", {{U(uCosmicStyle), 10.}, {U(uCosmicTime), 100.}, {U(uCosmicWarp), 60.}, {U(uCosmicFeedback), 70.}, {U(uCosmicDensity), 85.}, {U(uCosmicTone), 6200.}, {U(uCosmicMotion), 8.}, {U(uCosmicMix), 17.}}},
  {"Tile room", {{U(uCosmicStyle), 10.}, {U(uCosmicTime), 45.}, {U(uCosmicWarp), 83.}, {U(uCosmicFeedback), 30.}, {U(uCosmicDensity), 75.}, {U(uCosmicTone), 11000.}, {U(uCosmicMotion), 0.}, {U(uCosmicMix), 18.}}},
  {"Solstice afterglow", {{U(uCosmicStyle), 11.}, {U(uCosmicTime), 145.}, {U(uCosmicWarp), 75.}, {U(uCosmicFeedback), 76.}, {U(uCosmicDensity), 90.}, {U(uCosmicTone), 7800.}, {U(uCosmicMotion), 23.}, {U(uCosmicMix), 22.}}},
  {"Flooded nave", {{U(uCosmicStyle), 12.}, {U(uCosmicTime), 309.}, {U(uCosmicWarp), 86.}, {U(uCosmicFeedback), 65.3}, {U(uCosmicDensity), 67.5}, {U(uCosmicTone), 9000.}, {U(uCosmicMotion), 16.9}, {U(uCosmicMix), 39.4}}},
  {"Distant stone", {{U(uCosmicStyle), 12.}, {U(uCosmicTime), 580.}, {U(uCosmicWarp), 85.}, {U(uCosmicFeedback), 20.}, {U(uCosmicDensity), 68.}, {U(uCosmicTone), 9000.}, {U(uCosmicMotion), 0.}, {U(uCosmicMix), 19.}}},
  {"Compact Gemini room", {{U(uCosmicStyle), 0.}, {U(uCosmicTime), 80.}, {U(uCosmicWarp), 65.}, {U(uCosmicFeedback), 38.}, {U(uCosmicDensity), 100.}, {U(uCosmicTone), 7500.}, {U(uCosmicMotion), 8.}, {U(uCosmicMix), 14.}}},
  {"Photon lead plate", {{U(uCosmicStyle), 9.}, {U(uCosmicTime), 100.}, {U(uCosmicWarp), 70.}, {U(uCosmicFeedback), 65.}, {U(uCosmicDensity), 90.}, {U(uCosmicTone), 9000.}, {U(uCosmicMotion), 10.}, {U(uCosmicMix), 18.}}},
  // Imported verbatim from the author's Aurora Hall Medium user preset.
  {"Aurora Hall Medium", {{U(uCosmicStyle), 6.0}, {U(uCosmicTime), 223.79993441233353}, {U(uCosmicWarp), 92.53975891179422}, {U(uCosmicFeedback), 53.12700236474072}, {U(uCosmicDensity), 50.26504676358348}, {U(uCosmicTone), 5820.088880230463}, {U(uCosmicMotion), 16.963387498747824}, {U(uCosmicMix), 18.899712527662025}}},
  {"Clustered echoes", {{U(uCosmicStyle), 6.}, {U(uCosmicTime), 350.}, {U(uCosmicWarp), 15.}, {U(uCosmicDensity), 10.}, {U(uCosmicTone), 6500.}, {U(uCosmicMotion), 5.}, {U(uCosmicMix), 20.}}},
  {"Pointillist echoes", {{U(uCosmicStyle), 2.}, {U(uCosmicTime), 280.}, {U(uCosmicWarp), 12.}, {U(uCosmicFeedback), 48.}, {U(uCosmicDensity), 20.}, {U(uCosmicTone), 4500.}, {U(uCosmicMotion), 5.}, {U(uCosmicMix), 24.}}},
  {"Clear distant repeats", {{U(uCosmicStyle), 5.}, {U(uCosmicTime), 600.}, {U(uCosmicWarp), 0.}, {U(uCosmicFeedback), 45.}, {U(uCosmicDensity), 0.}, {U(uCosmicTone), 7000.}, {U(uCosmicMotion), 0.}, {U(uCosmicMix), 22.}}},
  {"Stereo echo groups", {{U(uCosmicStyle), 7.}, {U(uCosmicTime), 500.}, {U(uCosmicWarp), 0.}, {U(uCosmicDensity), 15.}, {U(uCosmicTone), 6500.}, {U(uCosmicMotion), 5.}, {U(uCosmicMix), 25.}}},
  {"Diffusing echo trail", {{U(uCosmicStyle), 4.}, {U(uCosmicTime), 250.}, {U(uCosmicWarp), 35.}, {U(uCosmicFeedback), 35.}, {U(uCosmicDensity), 60.}, {U(uCosmicTone), 6000.}, {U(uCosmicMotion), 10.}, {U(uCosmicMix), 22.}}},
  {"Soft ambient bed", {{U(uCosmicStyle), 0.}, {U(uCosmicTime), 350.}, {U(uCosmicWarp), 80.}, {U(uCosmicFeedback), 65.}, {U(uCosmicDensity), 100.}, {U(uCosmicTone), 5500.}, {U(uCosmicMotion), 15.}, {U(uCosmicMix), 22.}}},
  {"Enveloping swells", {{U(uCosmicStyle), 3.}, {U(uCosmicTime), 450.}, {U(uCosmicWarp), 72.}, {U(uCosmicFeedback), 72.}, {U(uCosmicDensity), 95.}, {U(uCosmicTone), 5000.}, {U(uCosmicMotion), 25.}, {U(uCosmicMix), 30.}}},
  {"Slow swell", {{U(uCosmicStyle), 1.}, {U(uCosmicTime), 400.}, {U(uCosmicMix), 25.}}},
  {"Chord bloom", {{U(uCosmicStyle), 8.}, {U(uCosmicTime), 700.}, {U(uCosmicWarp), 75.}, {U(uCosmicFeedback), 90.}, {U(uCosmicDensity), 90.}, {U(uCosmicTone), 5000.}, {U(uCosmicMotion), 25.}, {U(uCosmicMix), 30.}}},
  {"Velvet constellation", {{U(uCosmicStyle), 0.}, {U(uCosmicTime), 950.}, {U(uCosmicWarp), 82.}, {U(uCosmicFeedback), 77.}, {U(uCosmicDensity), 98.}, {U(uCosmicTone), 5500.}, {U(uCosmicMotion), 45.}, {U(uCosmicMix), 35.}}},
  {"Dark orbit", {{U(uCosmicStyle), 6.}, {U(uCosmicTime), 500.}, {U(uCosmicWarp), 78.}, {U(uCosmicFeedback), 91.}, {U(uCosmicDensity), 100.}, {U(uCosmicTone), 2800.}, {U(uCosmicMotion), 50.}, {U(uCosmicMix), 32.}}},
};

inline const Preset chorus[]{
  // Slow and shallow: felt more than heard on clean rhythm parts.
  {"Subtle clean width", {{U(uModRate), .6}, {U(uModDepth), 20.}, {U(uChorusSpread), 80.}, {U(uModMix), 25.}}},
  {"Lush 80s chorus", {{U(uChorusVoices), 1.}, {U(uModRate), .8}, {U(uModDepth), 55.}, {U(uModMix), 50.}}},
  // A deep, single-voice pedal chorus with the depth near maximum.
  {"Deep watery chorus", {{U(uModRate), 1.1}, {U(uModDepth), 75.}, {U(uModCharacter), 20.}, {U(uChorusSpread), 50.},
    {U(uModMix), 50.}}},
};
inline const Preset juno[]{
  {"Wide two voices", {{U(uJunoMode), 1.}, {U(uJunoWidth), 100.}, {U(uJunoMix), 45.}}},
  {"Mode II sweep", {{U(uJunoMode), 2.}, {U(uJunoMix), 50.}}},
  {"Fast vintage shimmer", {{U(uJunoMode), 3.}, {U(uJunoMix), 50.}}},
};
inline const Preset flanger[]{
  {"Slow jet sweep", {{U(uModRate), .12}, {U(uModDepth), 75.}, {U(uModFeedback), 55.}, {U(uModMix), 45.}}},
  {"Light comb motion", {{U(uModRate), .4}, {U(uModDepth), 30.}, {U(uModFeedback), 15.}, {U(uFlangerDelay), 5.}, {U(uModMix), 30.}}},
  // Depth at zero parks the comb, like a flanger's filter-matrix setting.
  {"Parked metallic comb", {{U(uModDepth), 0.}, {U(uModFeedback), 60.}, {U(uFlangerDelay), 6.}, {U(uModMix), 50.}}},
};
inline const Preset phaser[]{
  // Four stages, no feedback and a slow rate: the subtle vintage script-pedal sweep.
  {"Slow vintage phase", {{U(uPhaserStages), 0.}, {U(uModRate), .3}, {U(uModDepth), 55.}, {U(uModFeedback), 0.}, {U(uModMix), 50.}}},
  {"Deep stereo phase", {{U(uPhaserStages), 3.}, {U(uModRate), .18}, {U(uModDepth), 75.}, {U(uModFeedback), 35.}, {U(uModSpread), 70.}, {U(uModMix), 50.}}},
  // Four stages with strong feedback: the vocal "color" setting of vintage phasers.
  {"Resonant vintage phase", {{U(uPhaserStages), 0.}, {U(uModRate), .5}, {U(uModDepth), 70.}, {U(uModFeedback), 60.},
    {U(uModCharacter), 30.}, {U(uModMix), 50.}}},
};
inline const Preset tremolo[]{
  {"Amp tremolo", {{U(uModRate), 4.5}, {U(uModDepth), 45.}}},
  {"Hard eighth-note chop", {{U(uTempoSync), 1.}, {U(uTempoDivision), 3.}, {U(uModDepth), 100.}, {U(uModCharacter), 100.}}},
  // Opposite left/right phase turns the tremolo into a panner on a stereo rig.
  {"Stereo auto-pan", {{U(uModRate), 1.2}, {U(uModDepth), 80.}, {U(uModSpread), 100.}}},
};
// Early-1960s split-band amp tremolos filter bands near 150 and 650 Hz at about 5-7 Hz;
// one split near their geometric middle gives the same bass/treble alternation.
inline const Preset harmonicTremolo[]{
  {"Slow stereo pad", {{U(uModRate), 0.32}, {U(uModDepth), 46.}, {U(uModSpread), 85.},
    {U(uHarmonicCrossover), 740.}, {U(uModMix), 75.}}},
  {"Brown-panel amp", {{U(uModRate), 5.5}, {U(uModDepth), 60.}, {U(uModCharacter), 15.}, {U(uHarmonicCrossover), 400.}}},
  {"Slow swirl", {{U(uModRate), 1.8}, {U(uModDepth), 75.}, {U(uModCharacter), 0.}, {U(uHarmonicCrossover), 900.}, {U(uModSpread), 50.}}},
  {"Synced throb", {{U(uTempoSync), 1.}, {U(uTempoDivision), 3.}, {U(uModDepth), 85.}, {U(uModCharacter), 60.}, {U(uHarmonicCrossover), 500.}}},
  {"Subtle shimmer", {{U(uModRate), 7.}, {U(uModDepth), 30.}, {U(uModCharacter), 0.}, {U(uHarmonicCrossover), 1200.}}},
};
inline const Preset vibrato[]{
  {"Seasick warble", {{U(uModRate), .8}, {U(uModDepth), 45.}}},
  // Vibrato that develops over each phrase, like a held finger vibrato.
  {"Delayed lead vibrato", {{U(uModRate), 5.5}, {U(uModDepth), 18.}, {U(uVibratoRise), 700.}}},
  // Pitch-vibrato amps of the late 1950s ran near 6 Hz with an uneven, warm sweep.
  {"Vintage amp vibrato", {{U(uModRate), 6.}, {U(uModDepth), 25.}, {U(uModCharacter), 40.}}},
  {"Slow tape drift", {{U(uModRate), .35}, {U(uModDepth), 20.}, {U(uModCharacter), 80.}}},
};
inline const Preset vibe[]{
  {"Vibrato mode swirl", {{U(uVibeMode), 1.}, {U(uModRate), 4.5}, {U(uModDepth), 40.}, {U(uModCharacter), 50.}, {U(uModMix), 100.}}},
  {"Fast chorus churn", {{U(uModRate), 5.}, {U(uModDepth), 55.}, {U(uModCharacter), 35.}, {U(uModMix), 50.}}},
};
// Leslie speeds: about 0.8 Hz chorale and 6.7 Hz tremolo for the horn.
inline const Preset rotary[]{
  {"Slow chorale", {{U(uRotarySpeed), 0.}, {U(uRotarySlow), .8}, {U(uRotaryDrive), 1.}, {U(uRotarySpread), 60.}}},
  {"Fast tremolo", {{U(uRotarySpeed), 1.}, {U(uRotaryFast), 6.7}, {U(uRotaryDrive), 3.}, {U(uRotaryAccel), 1.5}}},
  {"Overdriven cabinet", {{U(uRotarySpeed), 1.}, {U(uRotaryFast), 6.}, {U(uRotaryDrive), 10.}, {U(uRotaryAccel), 1.2},
    {U(uRotarySpread), 90.}}},
  // Guitar rotary cabinets turn only a drum rotor; the drum runs at 73% of Fast rate, so about 6 Hz.
  {"Drum rotor only", {{U(uRotarySpeed), 1.}, {U(uRotaryFast), 8.5}, {U(uRotaryDrive), 3.}, {U(uRotaryBalance), -100.},
    {U(uRotarySpread), 70.}}},
};

// Insertion already supplies a modest fifth harmony. These add distinct roles.
inline const Preset pitchShift[]{
  {"Fifth underneath", {{U(uPitchInterval), -7.}, {U(uPitchTone), 4200.}, {U(uPitchMix), 26.},
    }},
  {"Shifted fifth only", {{U(uPitchInterval), 7.}, {U(uPitchTone), 10000.}, {U(uPitchMix), 100.}}},
  {"Octave above", {{U(uPitchInterval), 12.}, {U(uPitchTone), 9000.}, {U(uPitchMix), 22.}}},
  {"Octave below", {{U(uPitchInterval), -12.}, {U(uPitchTone), 7000.}, {U(uPitchMix), 30.}}},
  {"Subtle detuned blend", {{U(uPitchInterval), 0.}, {U(uPitchFine), 8.}, {U(uPitchTone), 12000.}, {U(uPitchMix), 20.}}},
};
inline const Preset octaver[]{
  {"Octave-down blend", {{U(uOctDry), 100.}, {U(uOctSub), 85.}, {U(uOctSub2), 0.}, {U(uOctTone), 800.}, {U(uOctOutput), -3.}}},
  {"Synth bass", {{U(uOctDry), 0.}, {U(uOctSub), 100.}, {U(uOctSub2), 0.}, {U(uOctTone), 600.}, {U(uOctOutput), -4.}}},
  // Dry, one and two octaves down together, as on two-octave pedals.
  {"Two-octave blend", {{U(uOctDry), 100.}, {U(uOctSub), 60.}, {U(uOctSub2), 45.}, {U(uOctTone), 1000.}, {U(uOctOutput), -2.}}},
};
inline const Preset doubler[]{
  {"Wide loose double", {{U(uDoubleDetune), 10.}, {U(uDoubleDelay), 22.}, {U(uDoubleHumanize), 40.}, {U(uDoubleHighCut), 7000.}, {U(uDoubleMix), 32.}, {U(uDoubleOutput), 2.5}}},
  {"Tight rhythm double", {{U(uDoubleDetune), 4.}, {U(uDoubleDelay), 9.}, {U(uDoubleHumanize), 20.}, {U(uDoubleMix), 30.}, {U(uDoubleOutput), 3.}}},
  {"Thick lead double", {{U(uDoubleDetune), 15.}, {U(uDoubleDelay), 18.}, {U(uDoubleHumanize), 60.}, {U(uDoubleHighCut), 9000.},
    {U(uDoubleMix), 40.}, {U(uDoubleOutput), 3.}}},
};
inline const Preset stutter[]{
  {"Sixteenth-note chops", {{U(uTempoSync), 1.}, {U(uTempoDivision), 1.}, {U(uStutterDuty), 40.}, {U(uStutterFade), 3.}, {U(uStutterMix), 75.}}},
  {"Eighth-note gate", {{U(uTempoSync), 1.}, {U(uTempoDivision), 3.}, {U(uStutterDuty), 50.}, {U(uStutterFade), 2.}, {U(uStutterMix), 100.}}},
  {"Triplet chop", {{U(uTempoSync), 1.}, {U(uTempoDivision), 2.}, {U(uStutterDuty), 65.}, {U(uStutterFade), 3.}, {U(uStutterMix), 100.}}},
};
// Ring Modulator: Depth 50% with the Square carrier switches the gain between 0 and 1, a hard
// tremolo; carriers below about 50 Hz roughen the note, higher ones turn chords into bells.
inline const Preset ringMod[]{
  {"Square chopper", {{U(uRingShape), 2.}, {U(uRingFrequency), 6.}, {U(uRingDepth), 50.}, {U(uRingTone), 20000.}, {U(uRingMix), 100.}}},
  {"Robot growl", {{U(uRingFrequency), 30.}, {U(uRingDepth), 100.}, {U(uRingTone), 5000.}, {U(uRingMix), 55.}}},
  {"Metallic bells", {{U(uRingFrequency), 660.}, {U(uRingDepth), 100.}, {U(uRingTone), 5000.}, {U(uRingMix), 35.}}},
  // The LFO sweeps the carrier up and down: the swooping, sci-fi ring modulator sound.
  {"Sci-fi sweep", {{U(uRingFrequency), 300.}, {U(uRingLfoRate), .6}, {U(uRingSweep), 60.}, {U(uRingTone), 6000.}, {U(uRingMix), 50.}}},
  {"Synced warble", {{U(uTempoSync), 1.}, {U(uTempoDivision), 5.}, {U(uRingFrequency), 180.}, {U(uRingSweep), 35.},
    {U(uRingTone), 5000.}, {U(uRingMix), 45.}}},
};
inline const Preset crusher[]{
  {"Broken small sampler", {{U(uCrushBits), 6.}, {U(uCrushRate), 6000.}, {U(uCrushTone), 3500.}, {U(uCrushMix), 65.}, {U(uCrushOutput), -2.}}},
  // 12 bits at 26.04 kHz, the format of classic 1980s sampling drum machines.
  {"12-bit sampler", {{U(uCrushBits), 12.}, {U(uCrushRate), 26040.}, {U(uCrushTone), 12000.}, {U(uCrushMix), 100.}}},
};
// The LoFi Tape Machine guide's starting points.
inline const Preset lofi[]{
  {"Subtle drift", {{U(uLoFiSpeed), 15.}, {U(uLoFiWow), 10.}, {U(uLoFiFlutter), 5.}, {U(uLoFiWear), 10.}}},
  {"Worn cassette", {{U(uLoFiSpeed), 3.75}, {U(uLoFiWow), 35.}, {U(uLoFiFlutter), 25.}, {U(uLoFiNoise), 20.}, {U(uLoFiWear), 50.}}},
  {"Damaged tape", {{U(uLoFiSpeed), 1.875}, {U(uLoFiWow), 70.}, {U(uLoFiFlutter), 60.}, {U(uLoFiNoise), 35.}, {U(uLoFiWear), 90.}}},
};
inline const Preset swell[]{
  {"Slow chord bloom", {{U(uSwellRise), 800.}, {U(uSwellRelease), 300.}, {U(uSwellCurve), 1.5}, {U(uSwellHold), 100.}}},
  {"Long ambient fade", {{U(uSwellRise), 1600.}, {U(uSwellRelease), 600.}, {U(uSwellCurve), 2.}, {U(uSwellHold), 150.}}},
};
// Volume Pedal: insertion is a muting swell pedal with an audio taper on CC 11.
inline const Preset volumePedal[]{
  // Heel at rhythm level, toe 4 dB louder for solos; Curve -100 spreads the boost over the travel.
  {"Rhythm to lead boost", {{U(uVolumeHeel), 0.}, {U(uVolumeToe), 4.}, {U(uVolumeCurve), -100.}, {U(uVolumeSmoothing), 30.}}},
  // A later rise and long glide for violin-like swells.
  {"Slow swells", {{U(uVolumeCurve), 40.}, {U(uVolumeSmoothing), 150.}}},
  // Linear amplitude, for controllers whose pedal already has a volume taper.
  {"Linear pedal", {{U(uVolumeCurve), -50.}}},
};
// A small shift against a short delay makes notches that keep moving one way (barberpole);
// a larger shift inside long feedback moves each repeat further from the played pitch.
inline const Preset frequencyShifter[]{
  {"Barberpole sweep", {{U(uShiftFrequency), -1.}, {U(uShiftFeedback), 35.}, {U(uShiftDelay), 1.}, {U(uShiftMix), 50.}}},
  {"Spiraling echoes", {{U(uShiftFrequency), 30.}, {U(uShiftFeedback), 60.}, {U(uShiftDelay), 375.}, {U(uShiftTone), 5000.},
    {U(uShiftMix), 30.}}},
};

inline const Preset spaceTapeEcho[]{
  {"Slap and spring", {{U(uSpaceHeads), 0}, {U(uDelayTime), 280}, {U(uDelayFeedback), 17}, {U(uEchoSpring), 36}}},
  {"Three-head dub", {{U(uSpaceHeads), 6}, {U(uTempoSync), 1}, {U(uTempoDivision), 5}, {U(uDelayFeedback), 54},
    {U(uDelayMotion), 12}, {U(uDelayCharacter), 35}, {U(uEchoSpring), 24}, {U(uDelayMix), 31}}},
  {"Clean long head", {{U(uSpaceHeads), 2}, {U(uDelayTone), 9000}, {U(uDelayMotion), 8}, {U(uDelayCharacter), 5}, {U(uEchoSpring), 0}}},
  {"Dark spring wash", {{U(uSpaceHeads), 6}, {U(uDelayTime), 260}, {U(uDelayFeedback), 50}, {U(uDelayTone), 3200},
    {U(uDelayMotion), 45}, {U(uDelayCharacter), 40}, {U(uEchoSpring), 40}, {U(uDelayMix), 30}}},
  // Just past the self-oscillation point: repeats keep building; lower Feedback to let them fall.
  {"Runaway dub", {{U(uSpaceHeads), 4}, {U(uDelayTime), 380}, {U(uDelayFeedback), 78}, {U(uDelayTone), 3800},
    {U(uDelayMotion), 30}, {U(uDelayCharacter), 45}, {U(uEchoSpring), 25}, {U(uDelayMix), 28}}},
  {"Short and long heads", {{U(uSpaceHeads), 4}, {U(uTempoSync), 1}, {U(uTempoDivision), 5}, {U(uDelayFeedback), 25},
    {U(uDelayTone), 7200}, {U(uDelayMotion), 8}, {U(uDelayCharacter), 12}, {U(uEchoSpring), 8}, {U(uDelayMix), 23}}},
  {"Worn twin-head trail", {{U(uSpaceHeads), 5}, {U(uTempoSync), 1}, {U(uTempoDivision), 6}, {U(uDelayFeedback), 57},
    {U(uDelayTone), 3600}, {U(uDelayMotion), 48}, {U(uDelayCharacter), 52}, {U(uEchoSpring), 32}, {U(uDelayMix), 30}}},
};
inline const Preset bucketBrigade[]{
  {"Compact slap", {{U(uBBDVoice), 0}, {U(uDelayTime), 123}, {U(uDelayMotion), 0}, {U(uDelayFeedback), 18}, {U(uDelayMix), 18}}},
  {"Chorused dotted eighth", {{U(uBBDVoice), 1}, {U(uTempoSync), 1}, {U(uTempoDivision), 4}, {U(uDelayMotion), 38}}},
  {"Vibrato trails", {{U(uBBDVoice), 2}, {U(uDelayTime), 455}, {U(uEchoRate), .95}, {U(uDelayFeedback), 48}, {U(uDelayMotion), 23}}},
  {"Clear analog lead", {{U(uBBDVoice), 3}, {U(uDelayTime), 292}, {U(uDelayMotion), 7}, {U(uDelayTone), 8200}}},
  {"Driven repeats", {{U(uBBDVoice), 4}, {U(uDelayTime), 360}, {U(uDelayFeedback), 54}, {U(uDelayCharacter), 65}}},
  {"Short chorus double", {{U(uBBDVoice), 1}, {U(uDelayTime), 26}, {U(uDelayFeedback), 6}, {U(uDelayTone), 7800},
    {U(uDelayCharacter), 10}, {U(uEchoRate), .95}, {U(uDelayMotion), 57}, {U(uDelayMix), 24}}},
};
inline const Preset pitchDelay[]{
  {"Octave halo", {{U(uPitchDelayMode), 0}, {U(uEchoInterval), 12}, {U(uEchoShiftBlend), 24},
    {U(uEchoDetune), 6}, {U(uDelayFeedback), 55}, {U(uTempoSync), 1}, {U(uTempoDivision), 5}}},
  {"Climbing fifths", {{U(uEchoInterval), 7}, {U(uEchoShiftBlend), 80}, {U(uDelayTime), 320}, {U(uDelayFeedback), 44}}},
  {"Backwards octave", {{U(uPitchDelayMode), 1}, {U(uEchoInterval), -12}, {U(uEchoShiftBlend), 75},
    {U(uDelayTime), 540}, {U(uDelayMix), 33}}},
  {"Wide octaves", {{U(uPitchDelayMode), 2}, {U(uEchoInterval), 12}, {U(uEchoShiftBlend), 46}, {U(uEchoDetune), 8}, {U(uDelayTone), 5400}}},
  {"Descending fourths", {{U(uPitchDelayMode), 0}, {U(uEchoInterval), -5}, {U(uEchoShiftBlend), 70},
    {U(uTempoSync), 1}, {U(uTempoDivision), 4}, {U(uDelayFeedback), 40}, {U(uDelayTone), 6000}, {U(uDelayMix), 28}}},
  {"Detuned double", {{U(uPitchDelayMode), 0}, {U(uEchoInterval), 0}, {U(uEchoDetune), 14},
    {U(uDelayTime), 24}, {U(uDelayFeedback), 0}, {U(uDelayTone), 9500}, {U(uDelayMix), 23}}},
};
inline const Preset feedbackFXDelay[]{
  {"Swept dotted echoes", {{U(uFeedbackFX), 0}, {U(uTempoSync), 1}, {U(uTempoDivision), 4},
    {U(uEchoRateSync), 1}, {U(uEchoRate), .25}, {U(uEchoDepth), 65}}},
  {"Slow phaser trail", {{U(uFeedbackFX), 1}, {U(uEchoRate), .26}, {U(uEchoDepth), 72},
    {U(uEchoResonance), 50}, {U(uDelayFeedback), 54}}},
  {"Pulsing quarter notes", {{U(uFeedbackFX), 2}, {U(uTempoSync), 1}, {U(uTempoDivision), 5},
    {U(uEchoRateSync), 1}, {U(uEchoRate), 2}, {U(uEchoDepth), 80}, {U(uDelayFeedback), 57}}},
  {"Metallic fragments", {{U(uFeedbackFX), 3}, {U(uEchoRingRate), 140}, {U(uEchoDepth), 64}, {U(uDelayTone), 5700}}},
  {"Tremolo slap", {{U(uFeedbackFX), 2}, {U(uDelayTime), 105}, {U(uDelayFeedback), 22},
    {U(uEchoRate), 4.2}, {U(uEchoDepth), 55}, {U(uDelayTone), 6500}, {U(uDelayMix), 26}}},
  {"Low ring flutter", {{U(uFeedbackFX), 3}, {U(uEchoRingRate), 22}, {U(uEchoDepth), 38},
    {U(uDelayTime), 275}, {U(uDelayFeedback), 42}, {U(uDelayTone), 4600}, {U(uDelayMix), 25}}},
};
inline const Preset patternDelay[]{
  {"Even stereo heads", {{U(uPatternFeedback), 0}, {U(uTempoSync), 1}, {U(uTempoDivision), 5}, {U(uTapLevel2), 60}}},
  {"Accelerating taps", {{U(uPatternFeedback), 0}, {U(uTapTime1), 28}, {U(uTapTime2), 63},
    {U(uTapTime3), 86}, {U(uTapTime4), 100}, {U(uTapLevel1), 58}}},
  {"Spreading swell", {{U(uPatternFeedback), 1}, {U(uDelayTime), 700}, {U(uTapTime1), 27},
    {U(uTapTime2), 49}, {U(uTapTime3), 78}, {U(uDelayFeedback), 68}}},
  {"Four-line halo", {{U(uPatternFeedback), 2}, {U(uDelayTime), 650}, {U(uTapTime1), 28},
    {U(uTapTime2), 43}, {U(uTapTime3), 69}, {U(uDelayFeedback), 76}, {U(uDelayMix), 35}}},
  {"Drum echo", {{U(uPatternFeedback), 3}, {U(uDelayTime), 295}, {U(uDelayTone), 4800},
    {U(uTapLevel2), 0}, {U(uDelayFeedback), 53}, {U(uEchoWidth), 35}}},
  {"Dotted-eighth reply", {{U(uPatternFeedback), 0}, {U(uTempoSync), 1}, {U(uTempoDivision), 5}, {U(uDelayFeedback), 22},
    {U(uTapLevel1), 0}, {U(uTapLevel2), 0}, {U(uTapTime3), 75}, {U(uTapPan3), -70},
    {U(uTapTime4), 100}, {U(uTapLevel4), 65}, {U(uTapPan4), 70}, {U(uDelayMix), 24}}},
};
inline const Preset granularDelay[]{
  {"Scattered chords", {{U(uGrainDelayMode), 0}, {U(uGrainLength), 105}, {U(uGrainScatter), 60},
    {U(uGrainDensity), 78}, {U(uDelayFeedback), 48}}},
  {"Reverse shards", {{U(uGrainDelayMode), 1}, {U(uGrainLength), 150}, {U(uEchoInterval), 7},
    {U(uDelayTime), 530}, {U(uDelayMix), 35}}},
  {"Broken repeats", {{U(uGrainDelayMode), 2}, {U(uGrainLength), 58},
    {U(uGrainScatter), 78}, {U(uDelayFeedback), 52}, {U(uDelayMix), 35}}},
  {"Subtle grain double", {{U(uGrainDelayMode), 0}, {U(uDelayTime), 82}, {U(uGrainLength), 36},
    {U(uGrainDensity), 76}, {U(uGrainScatter), 22}, {U(uDelayFeedback), 8}, {U(uDelayTone), 9000}, {U(uDelayMix), 18}}},
  {"Quarter-note reverse swells", {{U(uGrainDelayMode), 1}, {U(uTempoSync), 1}, {U(uTempoDivision), 5}, {U(uGrainLength), 225},
    {U(uGrainDensity), 28}, {U(uGrainScatter), 14}, {U(uDelayFeedback), 43}, {U(uDelayTone), 5200}, {U(uDelayMix), 31}}},
  {"Low-octave stutter", {{U(uGrainDelayMode), 2}, {U(uTempoSync), 1}, {U(uTempoDivision), 3}, {U(uEchoInterval), -12},
    {U(uGrainLength), 62}, {U(uGrainDensity), 68}, {U(uGrainScatter), 85}, {U(uDelayFeedback), 38}, {U(uDelayMix), 32}}},
  {"Shimmer trails", {{U(uGrainDelayMode), 3}, {U(uDelayTime), 460}, {U(uGrainLength), 180}, {U(uGrainDensity), 40},
    {U(uGrainScatter), 25}, {U(uDelayFeedback), 62}, {U(uDelayTone), 6500}, {U(uDelayMix), 32}}},
  {"Quarter-note shimmer wash", {{U(uGrainDelayMode), 3}, {U(uTempoSync), 1}, {U(uTempoDivision), 5}, {U(uGrainLength), 260},
    {U(uGrainDensity), 30}, {U(uGrainScatter), 12}, {U(uDelayFeedback), 72}, {U(uDelayTone), 5200}, {U(uDelayMix), 30}}},
};
// Granulator presets: one starting point per mode, voiced on picked notes and chords.
// Mode values follow effect_modes::GranulatorMode.
inline const Preset granulator[]{
  {"Grain cloud", {{U(uGranulatorMode), 0}, {U(uGranulatorActivity), 60}, {U(uGranulatorSize), 160},
    {U(uGranulatorSpace), 30}, {U(uGranulatorMix), 40}}},
  {"Ambient haze", {{U(uGranulatorMode), 1}, {U(uGranulatorActivity), 75}, {U(uGranulatorSize), 320},
    {U(uGranulatorShape), 65}, {U(uGranulatorRepeats), 45}, {U(uGranulatorSpace), 60}, {U(uGranulatorMix), 45}}},
  {"Octave shimmer", {{U(uGranulatorMode), 2}, {U(uGranulatorActivity), 65}, {U(uGranulatorSize), 240},
    {U(uGranulatorShape), 70}, {U(uGranulatorFilter), 15}, {U(uGranulatorRepeats), 40}, {U(uGranulatorSpace), 55}, {U(uGranulatorMix), 35}}},
  {"Low drone tunnel", {{U(uGranulatorMode), 3}, {U(uGranulatorActivity), 70}, {U(uGranulatorSize), 400},
    {U(uGranulatorShape), 60}, {U(uGranulatorRepeats), 55}, {U(uGranulatorSpace), 45}, {U(uGranulatorMix), 40}}},
  {"Phrase mosaic", {{U(uGranulatorMode), 4}, {U(uGranulatorPosition), 25}, {U(uGranulatorActivity), 45},
    {U(uGranulatorSize), 220}, {U(uGranulatorRepeats), 25}, {U(uGranulatorSpace), 35}, {U(uGranulatorMix), 45}}},
  {"Reverse swells", {{U(uGranulatorMode), 5}, {U(uGranulatorActivity), 40}, {U(uGranulatorSize), 380},
    {U(uGranulatorShape), 80}, {U(uGranulatorRepeats), 30}, {U(uGranulatorSpace), 40}, {U(uGranulatorMix), 45}}},
  {"Rhythmic blocks", {{U(uGranulatorMode), 6}, {U(uGranulatorPosition), 0}, {U(uGranulatorActivity), 55},
    {U(uGranulatorSize), 120}, {U(uGranulatorShape), 15}, {U(uGranulatorSpace), 15}, {U(uGranulatorMix), 45}}},
  {"Glitch interrupt", {{U(uGranulatorMode), 7}, {U(uGranulatorActivity), 60}, {U(uGranulatorSize), 90},
    {U(uGranulatorShape), 25}, {U(uGranulatorFilter), 10}, {U(uGranulatorSpace), 20}, {U(uGranulatorMix), 50}}},
  {"Fifths arpeggio", {{U(uGranulatorMode), 8}, {U(uGranulatorPosition), 0}, {U(uTempoSync), 1}, {U(uTempoDivision), 3},
    {U(uGranulatorSize), 140}, {U(uGranulatorShape), 30}, {U(uGranulatorRepeats), 30}, {U(uGranulatorSpace), 35}, {U(uGranulatorMix), 40}}},
  {"Dark frozen bed", {{U(uGranulatorMode), 1}, {U(uGranulatorActivity), 85}, {U(uGranulatorSize), 500},
    {U(uGranulatorShape), 55}, {U(uGranulatorFilter), -45}, {U(uGranulatorRepeats), 60}, {U(uGranulatorSpace), 70}, {U(uGranulatorMix), 50}}},
  {"Pick triplet", {{U(uGranulatorMode), 9}, {U(uGranulatorPosition), 0}, {U(uGranulatorSize), 80},
    {U(uTempoSync), 1}, {U(uTempoDivision), 2}, {U(uGranulatorShape), 15}, {U(uGranulatorRepeats), 0},
    {U(uGranulatorSpace), 8}, {U(uGranulatorMix), 48}}},
  {"Frozen chord keys", {{U(uGranulatorMode), 9}, {U(uGranulatorPosition), 8}, {U(uGranulatorSize), 240},
    {U(uGranulatorActivity), 65}, {U(uGranulatorShape), 45}, {U(uGranulatorFilter), -18},
    {U(uGranulatorRepeats), 0}, {U(uGranulatorSpace), 38}, {U(uGranulatorMix), 65}}},
  {"Close double", {{U(uGranulatorMode), 0}, {U(uGranulatorPosition), 0}, {U(uGranulatorSize), 25},
    {U(uGranulatorActivity), 52}, {U(uGranulatorFilter), -25}, {U(uGranulatorRepeats), 0},
    {U(uGranulatorSpace), 0}, {U(uGranulatorMix), 15}}},
  {"Reverse pickup", {{U(uGranulatorMode), 5}, {U(uTempoSync), 1}, {U(uTempoDivision), 3},
    {U(uGranulatorPosition), 0}, {U(uGranulatorSize), 145}, {U(uGranulatorShape), 94},
    {U(uGranulatorRepeats), 0}, {U(uGranulatorSpace), 12}, {U(uGranulatorMix), 40}}},
  {"Half-time staircase", {{U(uGranulatorMode), 8}, {U(uTempoSync), 1}, {U(uTempoDivision), 7},
    {U(uGranulatorPosition), 2}, {U(uGranulatorSize), 460}, {U(uGranulatorShape), 76},
    {U(uGranulatorRepeats), 18}, {U(uGranulatorSpace), 52}, {U(uGranulatorMix), 55}}},
  {"Broken sixteenths", {{U(uGranulatorMode), 7}, {U(uTempoSync), 1}, {U(uTempoDivision), 1},
    {U(uGranulatorPosition), 4}, {U(uGranulatorSize), 42}, {U(uGranulatorShape), 10},
    {U(uGranulatorFilter), 18}, {U(uGranulatorRepeats), 12}, {U(uGranulatorSpace), 0}, {U(uGranulatorMix), 60}}},
};

inline std::span<const Preset> For(SlotEffect effect)
{
  switch (effect)
  {
    case SlotEffect::DynamicEQ: return dynamicEQ;
    case SlotEffect::TransientShaper: return transientShaper;
    case SlotEffect::Multiband: return multiband;
    case SlotEffect::GraphicEQ: return graphicEQ;
    case SlotEffect::PedalComp: return pedalComp;
    case SlotEffect::SmoothComp: return smoothComp;
    case SlotEffect::PunchComp: return punchComp;
    case SlotEffect::ButterComp: return butterComp;
    case SlotEffect::OutboardChannel: return outboardChannel;
    case SlotEffect::Limiter: return limiter;
    case SlotEffect::SpaceTapeEcho: return spaceTapeEcho;
    case SlotEffect::BucketBrigade: return bucketBrigade;
    case SlotEffect::PitchDelay: return pitchDelay;
    case SlotEffect::FeedbackFXDelay: return feedbackFXDelay;
    case SlotEffect::PatternDelay: return patternDelay;
    case SlotEffect::GranularDelay: return granularDelay;
    case SlotEffect::DigitalDelay: return digitalDelay;
    case SlotEffect::ToneEQ: return toneEQ;
    case SlotEffect::CutEQ: return cutEQ;
    case SlotEffect::ParametricEQ: return parametricEQ;
    case SlotEffect::Boost: return boost;
    case SlotEffect::SynthFilter: return filter;
    case SlotEffect::AutoSwell: return swell;
    case SlotEffect::VolumePedal: return volumePedal;
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
    case SlotEffect::HarmonicTremolo: return harmonicTremolo;
    case SlotEffect::Vibrato: return vibrato;
    case SlotEffect::UniVibe: return vibe;
    case SlotEffect::Rotary: return rotary;
    case SlotEffect::Wah: return wah;
    case SlotEffect::PitchShift: return pitchShift;
    case SlotEffect::Octaver: return octaver;
    case SlotEffect::Doubler: return doubler;
    case SlotEffect::Stutter: return stutter;
    case SlotEffect::RingMod: return ringMod;
    case SlotEffect::ConsoleEQ: return consoleEQ;
    case SlotEffect::Granulator: return granulator;
    case SlotEffect::Bitcrusher: return crusher;
    case SlotEffect::TubeBoost: return tube;
    case SlotEffect::Distortion: return distortion;
    case SlotEffect::PedalDrive: return pedalDrive;
    case SlotEffect::PedalDistortion: return pedalDistortion;
    case SlotEffect::LoFiTape: return lofi;
    case SlotEffect::FrequencyShifter: return frequencyShifter;
    default: return {};
  }
}
} // namespace module_presets
