#pragma once
#include <algorithm>
#include "EffectParameters.h"

// Shared controls may have effect-specific wording and layout. Unique controls
// take their labels directly from the host/state parameter descriptors.
namespace effect_presentation
{
inline bool ChoiceDropdown(int p) { return EffectChoices(p).size() >= 3; }
inline const char* ChoiceDescription(int p, int mode)
{
  static constexpr const char* cosmic[]{
    "Dense, smooth ambience", "Slow, swelling echoes", "Dark, sparse repeats",
    "Swelling reverb bloom", "Long, evolving tails", "Clear, sparse repeats"};
  static constexpr const char* distortion[]{
    "Smooth, rounded drive", "Sharper, brighter drive", "Dense, compressed sustain",
    "Changing harmonic patterns", "Uneven clipping, even harmonics", "Octave-like buzz"};
  static constexpr const char* juno[]{"Slower voice", "Both voices", "Faster voice"};
  std::span<const char* const> descriptions;
  switch (p)
  {
    case uCosmicStyle: descriptions = cosmic; break;
    case uDistStyle: descriptions = distortion; break;
    case uPedalStyle: descriptions = effect_modes::pedalHelp; break;
    case uJunoMode: descriptions = juno; break;
    default: break;
  }
  return mode >= 0 && mode < static_cast<int>(descriptions.size()) ? descriptions[mode] : "";
}
// Saved options sit in the editor header: Tempo sync on the left, one mode or option
// on the right. The knob grid holds only the automatable musical controls.
inline bool HeaderControl(int p)
{ return p == uTempoSync || p == uDelayPingPong || p == uReverbStereo || IsAlgorithmMode(p); }
inline bool PingPong(SlotEffect e)
{ return e == SlotEffect::DigitalDelay || e == SlotEffect::AnalogDelay || e == SlotEffect::TapeDelay || e == SlotEffect::DualDelay; }
inline bool Tempo(SlotEffect e)
{ return IsDelay(e) || IsModulation(e) || e == SlotEffect::UniVibe || e == SlotEffect::Stutter; }
inline bool Visible(SlotEffect e, int p, bool sync = false, bool notch = false, bool vibrato = false, bool shelf = false)
{
  if (p == uTempoSync) return Tempo(e);
  if (p == uTempoDivision) return Tempo(e) && sync;
  if (p == uDelayPingPong) return PingPong(e);
  if (p == uReverbStereo) return IsReverb(e) && e != SlotEffect::AirPlate && e != SlotEffect::AirHall && e != SlotEffect::GalacticSpace;
  if (sync && (p == uDelayTime || p == uModRate || p == uStutterTime)) return false;
  if (e == SlotEffect::ParametricEQ && ((p == uGain && notch) || (p == uParamShelfGain && !shelf))) return false;
  if (e == SlotEffect::UniVibe && p == uModMix && vibrato) return false;
  return UsesUtility(e, p);
}
// The eight positions are 0-3 (top row) and 4-7 (bottom row). Enabled lives in
// the header. Main controls fill from the left; Output precedes Mix.
inline constexpr int kOutputPosition = 4;
inline int TablePosition(SlotEffect e, int p)
{
  std::span<const int> order;
  static constexpr int ToneEQOrder[]{-1, -2, -3, uToneMidFrequency, uEQOutput};
  static constexpr int CutEQOrder[]{-1, -2, -3, uEQOutput};
  static constexpr int ConsoleEQOrder[]{uConsoleTreble, uConsoleMid, uConsoleBass, uConsoleTrebleFreq, uConsoleBassFreq, uConsoleLowCut, uConsoleHighCut, uEQOutput};
  static constexpr int BoostOrder[]{uBoostGain, uBoostTilt, -1, uBoostMidGain};
  static constexpr int TubeBoostOrder[]{uTubeDrive, uBoostTilt, -1, uTubeBias, uTubeHighCut, uTubeOutput, uTubeMix};
  static constexpr int NoiseGateOrder[]{-1, -2, -3, uGateAttack, uGateHold, uGateHysteresis};
  static constexpr int SmoothCompOrder[]{-1, -2, -3, uCompAttack, uSmoothRatio, uCompDetectorCut, uCompMix};
  static constexpr int PunchCompOrder[]{-1, -2, -3, uPunchAttack, uPunchRatio, uCompDetectorCut, uCompMix};
  static constexpr int OutboardChannelOrder[]{uStripInput, uStripLowCut, uStripHighCut, uStripCompress, uStripSpeed, uStripSaturate, uStripOutput, uStripMix};
  static constexpr int LimiterOrder[]{uLimiterInput, uLimiterCeiling, uLimiterRelease, uLimiterClip};
  static constexpr int WahOrder[]{uWahPosition, uWahResonance, uWahRange, uWahOutput, uWahMix};
  static constexpr int ChorusOrder[]{uModRate, uModDepth, uModCharacter, uChorusDelay, uChorusSpread, uModMix};
  static constexpr int JunoChorusOrder[]{uJunoBrightness, uJunoWidth, uJunoMix};
  static constexpr int FlangerOrder[]{uModRate, uModDepth, uModFeedback, uModCharacter, uFlangerDelay, uModSpread, uModMix};
  static constexpr int PhaserOrder[]{uModRate, uModDepth, uModFeedback, uModCharacter, uModSpread, uModMix};
  static constexpr int TremoloOrder[]{uModRate, uModDepth, uModCharacter, uModSpread, uModMix};
  static constexpr int VibratoOrder[]{uModRate, uModDepth, uModCharacter, uVibratoRise};
  static constexpr int RotaryOrder[]{uRotarySlow, uRotaryFast, uRotarySpeed, uRotaryAccel, uRotaryDrive, uRotarySpread, uRotaryBalance, uRotaryMix};
  static constexpr int UniVibeOrder[]{uModRate, uModDepth, uModCharacter, uVibeSpread, uModMix};
  static constexpr int RingModOrder[]{uRingFrequency, uRingDepth, uRingTone, uRingMix};
  static constexpr int OctaverOrder[]{uOctDry, uOctSub, uOctSub2, uOctTone, uOctTrackingFilter, uOctOutput};
  static constexpr int DoublerOrder[]{uDoubleDetune, uDoubleDelay, uDoubleSpread, uDoubleHumanize, uDoubleHighCut, uDoubleOutput, uDoubleMix};
  static constexpr int DigitalDelayOrder[]{uDelayTime, uDelayFeedback, uDelayTone, uDelayCharacter, uDuckRelease, uDelayOutput, uDelayMix};
  static constexpr int ReverseDelayOrder[]{uDelayTime, uReverseFeedback, uDelayTone, uDelayOutput, uDelayMix};
  static constexpr int RoomReverbOrder[]{uReverbDecay, uReverbSize, uReverbTone, uReverbPredelay, uReverbMotion, uReverbHighPass, uReverbMix};
  static constexpr int PlateReverbOrder[]{uReverbDecay, uReverbSize, uReverbTone, uReverbPredelay, uReverbMotion, uReverbHighPass, uReverbDuck, uReverbMix};
  static constexpr int CloudReverbOrder[]{uReverbDecay, uReverbSize, uReverbTone, uReverbPredelay, uReverbMotion, uReverbHighPass, uReverbDuck, uReverbMix};
  static constexpr int HallReverbOrder[]{uReverbDecay, uReverbSize, uReverbTone, uReverbPredelay, uReverbMotion, uReverbHighPass, uReverbDuck, uReverbMix};
  static constexpr int AirPlateOrder[]{uAirPlateInput, uAirPlateSustain, uAirPlateResolution, uAirPlatePredelay, uAirPlateLowCut, uAirPlateOutput, uAirPlateMix};
  static constexpr int SpringReverbOrder[]{uSpringDecay, uSpringDwell, uSpringDrip, uReverbTone, uSpringPredelay, uSpringOutput, uSpringMix};
  static constexpr int FreezeOrder[]{uFreezeHold, uFreezeWindow, uFreezeFade, uFreezeTone, uFreezeAttack, uFreezeRelease, uFreezeMix};
  static constexpr int StutterOrder[]{uStutterTime, uStutterHold, uStutterDuty, uStutterFade, uStutterMix};
  static constexpr int LoFiTapeOrder[]{uLoFiSpeed, uLoFiDrive, uLoFiWow, uLoFiFlutter, uLoFiNoise, uLoFiWear, uLoFiOutput, uLoFiMix};
  static constexpr int UtilityOrder[]{uUtilityBalance, uUtilityStereo, uUtilityInvertL, uUtilityInvertR, uUtilityDelay, uUtilitySkew, uUtilityLevel};
  static constexpr int AutoSwellOrder[]{uSwellRise, uSwellThreshold, uSwellRelease, uSwellCurve, uSwellHold, uSwellMix};
  switch (e)
  {
    case SlotEffect::ToneEQ: order = ToneEQOrder; break;
    case SlotEffect::CutEQ: order = CutEQOrder; break;
    case SlotEffect::ConsoleEQ: order = ConsoleEQOrder; break;
    case SlotEffect::Boost: order = BoostOrder; break;
    case SlotEffect::TubeBoost: order = TubeBoostOrder; break;
    case SlotEffect::NoiseGate: order = NoiseGateOrder; break;
    case SlotEffect::SmoothComp: order = SmoothCompOrder; break;
    case SlotEffect::PunchComp: order = PunchCompOrder; break;
    case SlotEffect::OutboardChannel: order = OutboardChannelOrder; break;
    case SlotEffect::Limiter: order = LimiterOrder; break;
    case SlotEffect::Wah: order = WahOrder; break;
    case SlotEffect::Chorus: order = ChorusOrder; break;
    case SlotEffect::JunoChorus: order = JunoChorusOrder; break;
    case SlotEffect::Flanger: order = FlangerOrder; break;
    case SlotEffect::Phaser: order = PhaserOrder; break;
    case SlotEffect::Tremolo: order = TremoloOrder; break;
    case SlotEffect::Vibrato: order = VibratoOrder; break;
    case SlotEffect::Rotary: order = RotaryOrder; break;
    case SlotEffect::UniVibe: order = UniVibeOrder; break;
    case SlotEffect::RingMod: order = RingModOrder; break;
    case SlotEffect::Octaver: order = OctaverOrder; break;
    case SlotEffect::Doubler: order = DoublerOrder; break;
    case SlotEffect::DigitalDelay: order = DigitalDelayOrder; break;
    case SlotEffect::ReverseDelay: order = ReverseDelayOrder; break;
    case SlotEffect::RoomReverb: order = RoomReverbOrder; break;
    case SlotEffect::PlateReverb: order = PlateReverbOrder; break;
    case SlotEffect::CloudReverb: order = CloudReverbOrder; break;
    case SlotEffect::HallReverb: order = HallReverbOrder; break;
    case SlotEffect::AirPlate: order = AirPlateOrder; break;
    case SlotEffect::SpringReverb: order = SpringReverbOrder; break;
    case SlotEffect::Freeze: order = FreezeOrder; break;
    case SlotEffect::Stutter: order = StutterOrder; break;
    case SlotEffect::LoFiTape: order = LoFiTapeOrder; break;
    case SlotEffect::Utility: order = UtilityOrder; break;
    case SlotEffect::AutoSwell: order = AutoSwellOrder; break;
    default: break;
  }
  for (size_t i = 0; i < order.size(); ++i)
    if (order[i] == p || ((order[i] == uModRate || order[i] == uDelayTime || order[i] == uStutterTime) && p == uTempoDivision)) return static_cast<int>(i);

  // Shared family controls whose place differs for one module.
  if (e == SlotEffect::SpringReverb && p == uReverbTone) return 3;
  if (e == SlotEffect::ReverseDelay && p == uDelayTone) return 1;
  if (p == uModCharacter && e != SlotEffect::Flanger && e != SlotEffect::Phaser) return 2; // No feedback control.
  return kEffectParameters[p].position;
}
// Places held by the slot's own dynamics and EQ knobs rather than by module controls.
inline std::array<bool, 8> FixedPlaces(SlotEffect e)
{
  std::array<bool, 8> fixed{};
  if (e == SlotEffect::NoiseGate || e == SlotEffect::SmoothComp || e == SlotEffect::PunchComp
      || e == SlotEffect::ToneEQ || e == SlotEffect::CutEQ)
    fixed[0] = fixed[1] = fixed[2] = true;
  if (e == SlotEffect::ParametricEQ) fixed[4] = fixed[6] = true;
  if (e == SlotEffect::CabinetIR) fixed[kOutputPosition] = true;
  if (e == SlotEffect::Boost || e == SlotEffect::TubeBoost) fixed[2] = true;
  return fixed;
}
// Packs the table places a module uses in any mode, so few controls never sit apart. Up to
// four places form one row, left to right (main controls, then Output, then Mix). Otherwise
// the top row stays and the bottom row fills from the left. A mode only hides controls, so it
// never moves the others. EffectPresentationTests checks every module.
inline int Position(SlotEffect e, int p)
{
  auto used = FixedPlaces(e);
  for (int q = 0; q < kUtilityControls; ++q)
    if (!HeaderControl(q))
      for (bool sync : {false, true})
        for (bool alternate : {false, true})
          if (Visible(e, q, sync, alternate, alternate, alternate)) used[TablePosition(e, q)] = true;
  const int own = TablePosition(e, p);
  const auto placesBefore = [&](int from) { return static_cast<int>(std::count(used.begin() + from, used.begin() + own, true)); };
  if (std::count(used.begin(), used.end(), true) <= 4) return placesBefore(0);
  return own < 4 ? own : kOutputPosition + placesBefore(kOutputPosition);
}
inline const char* Label(SlotEffect e, int p, int mode = 0)
{
  if (e == SlotEffect::PedalDrive && p == uPedalCharacter)
    return effect_modes::pedalCharacterNames[std::clamp(mode, 0, effect_modes::pedalCount - 1)];
  if (p == uDelayCharacter)
    return e == SlotEffect::DigitalDelay ? "Ducking" : e == SlotEffect::AnalogDelay ? "Age" : "Saturation";
  if (p == uModCharacter)
    return e == SlotEffect::Phaser ? "Sweep center" : e == SlotEffect::Flanger ? "Feedback sign"
           : e == SlotEffect::Tremolo ? "Pulse / feel" : e == SlotEffect::UniVibe ? "Sweep color" : "Texture";
  return kEffectParameters[p].label;
}
inline const char* Help(SlotEffect e, int p, int mode = -1)
{
  switch (p)
  {
    case uToneMidFrequency: return "Moves the Guitar EQ mid bell; 800 Hz retains the original response.";
    case uEQOutput: return "Final EQ level for matching bypass loudness after boosts and cuts.";
    case uBoostMidGain: return "Broad 800 Hz mid boost or cut before the next module; zero leaves the original boost unchanged.";
    case uTubeHighCut: return "Filters the saturated path after clipping; maximum bypasses this added filter.";
    case uGateAttack: return "Gate opening time; zero follows the original Natural or Tight mode timing.";
    case uGateHold: return "Time before closure; zero follows the original Natural or Tight mode hold.";
    case uGateHysteresis: return "Closing threshold below the opening threshold; higher values reduce chatter.";
    case uSmoothRatio: return "Compression ratio above the knee; 2.5:1 retains the original Smooth response.";
    case uPunchRatio: return "Compression ratio above the knee; 4:1 retains the original Punch response.";
    case uCompDetectorCut: return "Removes bass only from the compressor detector, preserving audible lows; zero is off.";
    case uStripInput: return "Level into the complete strip, independent of final Output and the parallel dry path.";
    case uLimiterInput: return "Gain before clipping and limiting; pushes the signal into the selected ceiling.";
    case uChorusDelay: return "Base delay around which chorus moves; shorter settings give tighter thickening.";
    case uJunoWidth: return "Width of the wet Juno voices; zero centers them while dry stereo remains intact.";
    case uFlangerDelay: return "Base comb delay; with Depth at zero this produces a parked flanger tone.";
    case uModSpread: return "Phase offset between left and right modulation; zero retains the original motion.";
    case uPhaserStages: return "Select four, six, eight or twelve phase stages; six retains the original phaser.";
    case uVibratoRise: return "Time for pitch modulation to develop after a new phrase; zero gives immediate vibrato.";
    case uRotaryBalance: return "Negative favors the low drum, positive the bright horn; center retains the original balance.";
    case uVibeSpread: return "Offsets the right photocell sweep for stereo movement; zero retains the original sweep.";
    case uRingShape: return "Select a sine, triangle or softened square carrier for different metallic sidebands.";
    case uOctTrackingFilter: return "Low-pass frequency in the tracking detector only; does not filter the dry or generated tone.";
    case uDoubleHumanize: return "Adds slow, independent timing and pitch variation to the double; zero retains fixed offsets.";
    case uDoubleHighCut: return "Softens only the doubled voices; maximum bypasses this added filter.";
    case uDuckRelease: return "Time for ducked echoes to recover in gaps; 150 ms retains the original recovery.";
    case uReverseFeedback: return "Returns reversed playback into the next capture, creating repeating reversed phrases.";
    case uReverbDuck: return "Ducks only the wet reverb while playing and restores it between phrases; zero is off.";
    case uSpringPredelay: return "Time before the spring is excited; zero retains the immediate spring response.";
    case uFreezeAttack: return "Fade-in time for the held layer, independent of the loop-seam Crossfade.";
    case uFreezeRelease: return "Fade-out time when Hold is released; the dry guitar continues unchanged.";
    case uStutterHold: return "Keeps the current slice repeating instead of refreshing it every four windows.";
    case uLoFiDrive: return "Oversampled Spiral saturation before the tape transport; zero bypasses saturation exactly.";
    case uUtilityMode: return "Keep stereo, swap channels, or send the left or right input to both channels.";
    case uUtilitySkew: return "Positive delays the right channel, negative the left, in addition to Fine delay.";
    case uSwellCurve: return "Shape of the fade-in; one retains the original linear swell, higher values enter more slowly.";
    case uSwellHold: return "Time below the closing threshold before a new swell can re-arm; 40 ms retains the original hold.";
    case uSwellMix: return "Blend the swollen signal with dry guitar to retain some pick attack.";

    case uIRInput: return "Gain into convolution. Changes only the wet signal; at Mix zero the original signal passes unchanged.";
    case uIRMix: return "Linear dry/wet blend. Input, Output, cuts and Pre-delay affect only the wet signal. Use 100% on a send.";
    case uIRPredelay: return "Delay before the wet response, leaving dry timing unchanged. Separates reverb from the attack; automation bends the wet pitch.";
    case uIRLowCut: return "12 dB/octave high-pass on the wet response. Removes cabinet rumble or reverb low end; 0 Hz bypasses.";
    case uIRHighCut: return "12 dB/octave low-pass on the wet response. Tames cabinet fizz or darkens reverb and textures; 20000 Hz bypasses.";
    case uIRLength: return "Cabinet preserves the original 8192-sample limit. Long IR keeps up to 30 seconds for reverb and sound design. Switching reloads the file.";
    case uPedalCharacter: return effect_modes::pedalCharacterHelp[std::clamp(mode, 0, effect_modes::pedalCount - 1)];
    case uPedalStyle: return effect_modes::pedalHelp[std::clamp(mode, 0, effect_modes::pedalCount - 1)];
    case uPedalDrive: return "Gain into the pedal stages. Each style has its own filtering and gain structure; match loudness with Output.";
    case uPedalTone: return "Wet high-cut frequency after the voiced drive stages. Does not filter the clean lows restored by Low keep.";
    case uPedalBody: return "Broad bass shelf before the drive stages. Positive is fuller; negative tightens the drive. Preserved clean lows are unchanged.";
    case uPedalSplit: return "Local low-preservation frequency. Low keep at zero disables its effect; this does not change the grid's A/B split.";
    case uPedalLowKeep: return "Remove this share of the low band before distortion and restore it clean afterward. 100% preserves clean lows; the split is gentle.";
    case uPedalOutput: return "Final level after dry/wet blending, including preserved lows. Match bypass loudness before judging the drive.";
    case uPedalMix: return "Blend the complete pedal sound, including Low keep, with latency-aligned dry input. Output follows the blend.";
    case uBlackHold: return "Keeps the gate open this long after the signal last crossed Threshold, so noise near Threshold no longer chatters. A NAM Grid addition; 0 ms is the native gate.";
    case uBlackMode: return "Zero crossing is DigitalBlack's darkening closure. Hysteresis uses CStrip's re-arming gate. Changes fade through dry.";
    case uBlackThreshold: return "Level that opens the gate. The -74 to -10 dB range stays inside the native threshold mapping; neither mode has hold or reduction controls.";
    case uBlackMix: return "Blend the gate and its darkened closing texture with the original signal. Zero restores the dry sound before Output.";
    case uBlackOutput: return "Final gain after dry/wet blending. Match bypass loudness before judging the effect.";
    case uColourDesk: return "Eight console voices. Desk changes fade through dry while the convolution and sag histories reset.";
    case uColourInput: return "Gain into the console colour, in real dB. More level drives the nonlinear convolution and saturation harder.";
    case uColourOutput: return "Wet-path output trim in real dB. Match desk loudness; Mix zero preserves the exact dry signal.";
    case uColourMix: return "Blend console colour with the original stereo signal. Both trims affect only the processed path.";
    case uStripLowCut: case uConsoleLowCut: return "Native Capacitor high-pass amount, with sample-rate pole compensation. Zero is off. Percentage is not a cutoff in Hz.";
    case uStripHighCut: case uConsoleHighCut: return "Native Capacitor low-pass openness, with sample-rate pole compensation. 100 is open; lower values darken. Percentage is not a cutoff in Hz.";
    case uStripCompress: return "CStrip2 drive into its polarity-sensitive compressor. This percentage controls intensity, not threshold or ratio.";
    case uStripSpeed: return "CStrip2 native recovery speed. Higher values recover faster; this is not attack or release in milliseconds.";
    case uStripSaturate: return "Native unity-to-Spiral output section: increases drive and blends saturation. Output independently matches final loudness.";
    case uStripOutput: case uButterOutput: return "Final gain after dry/wet blending, in dB. Match bypass loudness before judging compression.";
    case uStripMix: case uButterMix: return "Parallel dry/wet blend. Output follows this blend. The gain-reduction meter excludes automatic gain recovery and Output.";
    case uButterCompress: return "ButterComp2 intensity and automatic gain recovery, with output-dependent release and independent left/right detectors.";
    case uConsoleTreble: case uConsoleMid: case uConsoleBass: return "Native nonlinear band density, labelled with its nominal -6 to +6 dB mapping. Adds harmonics; measured gain depends on input level.";
    case uConsoleTrebleFreq: case uConsoleBassFreq: return "Native EQ frequency coefficient scale. This is not a measured shelf corner; the broad bands can overlap.";
    case uCosmicStyle: return effect_modes::cosmicHelp[std::clamp(mode, 0, effect_modes::cosmicCount-1)];
    case uCosmicTime: return "Delay-line time scale, from 20 ms to eight seconds. The dial gives more space to long intervals. Warp spreads shorter lines below this value; Sagittarius can build up over several intervals.";
    case uCosmicWarp: return "Spread the delay-line lengths: zero aligns them for echoes, higher values create uneven spacing and more reverb-like texture.";
    case uCosmicFeedback: return "Longer repeats and sustain. In Sagittarius and Centaurus, Feedback also changes how quickly the sound builds up. This is not a decay time in seconds.";
    case uCosmicDensity: return "Rotate signals between the delay lines. Low values leave distinct echoes; high values build a denser stereo field. Interacts with Warp.";
    case uCosmicTone: return "Wet treble cutoff. Virgo filters each feedback pass, darkening successive echoes. The other styles filter their output.";
    case uCosmicMotion: return "Depth of the shared 0.3 Hz stereo modulation. More adds pitch movement and chorus. Stereo width is full and the wet low cut is fixed at 80 Hz.";
    case uCosmicMix: return "Blend the ambient echo/reverb with the original stereo signal. Zero preserves the exact dry sound.";
    case uTapeHeads: return effect_modes::tapeHeadHelp[std::clamp(mode, 0, static_cast<int>(effect_modes::tapeHeadHelp.size())-1)];
    case uDelayDiffusion: return "Smear each repeat through rotating allpass stages. Zero is a clear echo; higher values spread attacks into stereo clouds.";
    case uAirPlateInput: return "Pad the input to Airwindows kPlate140's nonlinear excitation. Zero dB sends the original level.";
    case uAirPlateSustain: return "Native kPlate140 regeneration and tonal blur. More sustains longer; this percentage is not a calibrated decay time.";
    case uAirPlateResolution: case uAirHallResolution: case uSpaceResolution:
      return "Internal processing resolution. Lower settings enlarge/darken the space through undersampling. Rate steps are part of the original algorithm and may be audible when automated.";
    case uAirPlatePredelay: return "Gap before the plate send, in milliseconds. Separates the attack without changing the tank size.";
    case uAirPlateLowCut: return "Remove bass from the plate send. Zero is off; higher values keep driven guitar from making the tank boomy.";
    case uAirHallSustain: return "Native kGuitarHall2 regeneration. More lengthens the hall; limited below its maximum for controlled tails.";
    case uAirHallTone: return "Brightness of the native hall filters. Lower is darker and softer, higher keeps more pick detail.";
    case uAirHallEarly: return "Level of the hall's early reflections; adds a closer, more defined room around the long tail.";
    case uAirHallPosition: return "Selects a different early-reflection delay configuration. The native algorithm has discrete positions, so transitions can be audible.";
    case uSpaceReplace: return "How quickly new notes replace the existing space. Lower settings retain a longer wash; minimum stays above the infinite-hold endpoint.";
    case uSpaceBrightness: return "Treble entering and leaving Galactic3. Lower settings make a darker ambient bed.";
    case uSpaceDetune: return "Stereo pitch motion within Galactic3; higher values make a wider, more obviously moving wash.";
    case uSpaceSize: return "Native Galactic3 delay-buffer scale. Together with Resolution it sets space size; resizing a sounding tank can make pitch/noise artifacts.";
    case uFrequency: return "Center frequency of the bell or notch; Q sets how narrow the affected band is.";
    case uGain: return "Boost or cut the selected band. Notch mode removes this control because notch depth is fixed.";
    case uParamLowQ: return "Resonance at the low cutoff, or the shelf corner. 0.71 gives the original Butterworth cut; higher values emphasize the corner.";
    case uParamHighQ: return "Resonance at the high cutoff. 0.71 gives the original Butterworth cut; higher values emphasize the corner.";
    case uParamShelfGain: return "Boost or cut below the low shelf frequency. Low Q shapes the corner; 0 Hz bypasses the low band.";
    case uParamLowMode: return "Choose a 12 dB/octave low cut or a low shelf with adjustable gain. Frequency and Q stay in the same positions.";
    case uQ: return "Higher Q narrows the bell or notch; lower Q changes a broader part of the guitar tone.";
    case uBoostGain: return "Clean level boost before the next module. Drive a following NAM or distortion harder.";
    case uBoostTilt: return "Tilt around 800 Hz: positive is brighter/thinner, negative is darker/fuller.";
    case uFilterCutoff: return "Resting filter frequency. Envelope opens it upward; resonance emphasizes its edge.";
    case uFilterResonance: return "Emphasis around the cutoff. Higher values create a more vocal, peaky response.";
    case uFilterEnvelope: return "Maximum upward sweep in octaves as you play harder. Zero makes the filter static.";
    case uFilterMode: return "Low-pass darkens, high-pass thins, band-pass isolates a vocal band.";
    case uSwellRise: return "Fade-in time after the input crosses threshold; longer settings soften the pick attack.";
    case uSwellThreshold: return "Input level that triggers a swell. Set below quiet notes but above background noise.";
    case uSwellRelease: return "How quickly the swell closes after the input falls below threshold.";
    case uDelayTime: return e == SlotEffect::MultiHeadDelay ? "Time of the last playback head. Other selected heads occur at one quarter, one half and three quarters of this interval."
                         : e == SlotEffect::ReverseDelay ? "Length of each reversed capture window; not a conventional first-echo delay."
                         : e == SlotEffect::DualDelay ? "Time of the left tap. The right tap is this value multiplied by Time ratio."
                                                      : "Time to the first repeat. Changing it while repeats sound bends their pitch, like tape.";
    case uDelayFeedback: return "Amount returned for another repeat. Higher settings make more repeats and longer tails.";
    case uDelayTone: return "High-cut frequency of the repeats. Lower values darken echoes without dulling the dry guitar.";
    case uDelayCharacter: return e == SlotEffect::DigitalDelay ? "Ducks repeats while you play; echoes recover in the gaps."
                                  : e == SlotEffect::AnalogDelay ? "More darkening and soft saturation on each repeat."
                                                                : "Soft saturation on repeats. Modulation controls the tape wobble separately.";
    case uDelayMotion: return "Pitch movement of repeats. Zero is stable; higher values add chorus-like wobble.";
    case uDualRatio: return "Right delay time divided by left time. 0.75 gives a dotted-eighth partner to a quarter-note left delay.";
    case uDualBalance: return "Balance between the two wet taps; center keeps both at full level. Dry stereo stays unchanged.";
    case uDelayPingPong: return e == SlotEffect::DualDelay
      ? "Cross-feedback mode: feed the mono sum into the left tap, then alternate through left and right delay times."
      : "Feed the mono sum into the left tap and alternate repeats between left and right. Dry stereo stays unchanged.";
    case uReverbDecay: case uSpringDecay: case uShimmerDecay: return "Tail decay time. Higher values let notes overlap into a larger ambient wash.";
    case uReverbSize: return "Tank scale and reflection spacing. Larger settings sound more open; Decay controls how long they last.";
    case uReverbTone: case uShimmerTone: return "Treble damping in the reverb. Lower settings make the tail darker than the dry signal.";
    case uReverbPredelay: return "Gap before the reverb starts; separates the pick attack from the tail.";
    case uReverbMotion: case uShimmerMod: return "Movement of the reverb delay lines; adds a gently shifting tail.";
    case uReverbHighPass: return "Remove bass from the reverb send to keep the tail from getting boomy.";
    case uReverbStereo: return "Stereo wet spread; off folds the reverb to center while preserving dry stereo.";
    case uModRate: return "Speed of the modulation cycle. Tempo sync substitutes a rhythmic division.";
    case uModDepth: return e == SlotEffect::Tremolo ? "How far the volume dips on each pulse." : "Width of the pitch, delay or filter sweep.";
    case uModFeedback: return "Return the swept signal into itself for a more resonant, pronounced effect.";
    case uModCharacter: return e == SlotEffect::Phaser ? "Raises the center of the selected phase sweep."
      : e == SlotEffect::Flanger ? "Moves feedback from positive through zero to negative. Turning it toward negative also darkens the wet path (12k to 3k) and changes the sweep shape."
      : e == SlotEffect::Tremolo ? "Sharpens pulses; stronger playing also speeds them up when tempo sync is off."
      : e == SlotEffect::UniVibe ? "Expands the uneven four-stage sweep for a deeper, more vocal swirl."
      : "Warmer tone and more uneven pitch movement; zero keeps a smooth sine sweep.";
    case uChorusSpread: return "Phase offset between left and right chorus voices; zero is centered, 100 is opposite motion.";
    case uTapeDrive: return "Drive the Airwindows tape saturation harder. Use Output to match loudness.";
    case uTapeBump: return "Low-frequency tape head emphasis; more adds body and bass resonance.";
    case uCompAttack: case uPunchAttack: return "How quickly compression catches a note. Slower attack lets more pick transient through.";
    case uUtilityBalance: return "Attenuate one side of existing stereo; this does not pan a mono voice into new stereo.";
    case uUtilityStereo: return "-100 folds to mono, zero preserves stereo, +100 doubles the side signal. Mono input stays mono.";
    case uUtilityInvertL: case uUtilityInvertR: return "Reverse this channel's polarity for phase alignment; not an on/off mute.";
    case uUtilityDelay: return "Delay both channels by a few milliseconds for parallel-path alignment; this is an audible offset.";
    case uSpringDwell: return "Drive the spring send harder for more excitation and soft saturation.";
    case uSpringDrip: return "Amount of spring dispersion: adds the splashy, metallic drip to attacks.";
    case uWahPosition: return "Heel-to-toe sweep. Automate it or map it to an expression controller in the host.";
    case uWahResonance: return "Higher Q makes a narrower, more pronounced wah peak.";
    case uWahRange: return "Raises both heel and toe endpoints; it changes the register, not the sweep width.";
    case uRotarySlow: case uRotaryFast: return "Target rotation speed; the drum turns at 73 percent of the horn speed.";
    case uRotarySpeed: return "Choose slow or fast target speed. Ramp time controls the transition.";
    case uRotaryAccel: return "Time constant for speed changes; larger values accelerate and slow down more gradually.";
    case uRotaryDrive: return "Soft saturation before the rotating-speaker modulation.";
    case uRotarySpread: return "Stereo separation of horn/drum motion; zero centers the wet signal.";
    case uVibeMode: return "Vibrato uses only the phase-shifted signal; chorus mode retains the selected dry/wet blend.";
    case uLimiterCeiling: return "Peak target. This zero-lookahead limiter cannot guarantee an intersample true-peak ceiling.";
    case uLimiterRelease: return "How quickly gain recovers after a loud peak; short settings can sound more aggressive.";
    case uLimiterClip: return "Soft-clips peaks from 3 dB below the ceiling before limiting, so the gain moves less on fast transients. Adds harmonics; 0% is off.";
    case uOctDry: return "Level of the original guitar alongside the generated sub-octaves.";
    case uOctSub: case uOctSub2: return "Level of this generated sub-octave. Best tracking comes from clean, single-note playing.";
    case uOctTone: return "High-cut frequency for the generated sub-octaves; the dry guitar keeps its original tone.";
    case uDoubleDetune: return "Opposite pitch offsets in cents; higher settings make a wider, more obvious double.";
    case uDoubleDelay: return "Base separation before each detuned voice; increases the double-tracked feel.";
    case uDoubleSpread: return "Spread of detuned voices; zero centers the wet pair, 100 keeps them on opposite sides.";
    case uFreezeHold: return "Capture a fresh window and sustain it. Release to stop the held layer; dry guitar remains audible.";
    case uFreezeWindow: return "Length of the held capture; short windows are more tonal, long windows retain more phrase texture.";
    case uFreezeFade: return "Overlap at the loop seam; smooths joins and shortens the repeat period by the overlap length.";
    case uFreezeTone: return "High-cut frequency of the held layer; lower values soften repeated attacks.";
    case uFreezeMix: return "Add the held layer to the dry signal. This is an additive level, not a dry/wet crossfade.";
    case uStutterTime: return "Repeat period of the captured slice; tempo sync substitutes a musical division.";
    case uStutterDuty: return "Percentage of each slice that sounds; shorter gates leave more rhythmic silence.";
    case uStutterFade: return "Fade at each gate edge; smooths clicks and can soften very short gates.";
    case uCrushBits: return "Bit depth of the quantizer; fewer bits add stronger grain and distortion.";
    case uCrushRate: return "Sample-and-hold rate, capped at the host rate. Lower settings add aliasing and stepped texture.";
    case uCrushTone: case uRingTone: case uShiftTone: case uDistTone: return "High-cut frequency of the processed signal. Lower values tame brightness and harshness.";
    case uRingFrequency: return "Carrier frequency; adds sum/difference sidebands and metallic, often inharmonic tones.";
    case uRingDepth: return "Move from the original amplitude toward full bipolar ring modulation.";
    case uEQMode: return "Notch rejects a narrow band without a Gain control; bell boosts or cuts using Gain.";
    case uJunoMode: return "I is the slower voice, II the faster voice, I+II combines both.";
    case uJunoBrightness: return "Treble retained in the wet voices; lower values soften the chorus.";
    case uShimmerPitch: return "Semitone interval on each pitched reverb pass. Positive climbs, negative falls.";
    case uShimmerAmount: return "Pitched feedback into the reverb; more creates a stronger, longer shimmer.";
    case uShimmerFreeze: return "Sustain the tank and stop feeding new notes; release to return to the normal decay.";
    case uShiftFrequency: return "Fixed hertz offset per pass; unlike a pitch shifter, every partial moves by the same number of hertz.";
    case uShiftFeedback: return "Return shifted audio into the delay for progressively changing repeats.";
    case uShiftDelay: return "Time between shifted feedback passes; short settings smear, long settings make distinct echoes.";
    case uShiftSide: return "Lower sideband at -100, ring modulation at zero, upper sideband at +100.";
    case uShiftStereo: return "Carrier phase difference between channels; widens the shifted texture.";
    case uDistStyle: return mode >= 0 && mode < effect_modes::distortionCount ? effect_modes::distortionHelp[mode]
      : "Choose soft or hard clipping, dense fuzz, wavefolding, asymmetric diode clipping or octave-like rectification.";
    case uDistDrive: case uTubeDrive: return "Gain into the shaper; more drive compresses and adds harmonics. Match loudness with Output.";
    case uDistBias: case uTubeBias: return "Asymmetric clipping adds even harmonics and changes attack texture. The processor removes DC.";
    case uNoiseLearn: return "Learn while playing only the unwanted noise, then turn Learn off before playing guitar.";
    case uNoiseSensitivity: return "Raise the learned noise threshold; too much can reduce quiet sustain.";
    case uNoiseReduction: return "Maximum reduction in noisy bands. Less can retain more natural room tone and note tails.";
    case uNoiseAttack: return "Speed at which the denoiser lets new signal through; short settings preserve pick attacks.";
    case uNoiseRelease: return "Recovery toward noise reduction after notes; longer settings preserve sustain.";
    case uNoiseGateThreshold: return "Optional final gate after denoising. Minimum is Off; raise carefully to avoid cutting sustain.";
    case uGateMode: return "Natural closes gently; Tight clamps gaps more firmly. Threshold, Release and Reduction remain independent.";
    case uTempoSync: return "Use host tempo and a rhythmic division instead of manual time/rate. Falls back to 120 BPM without host tempo.";
    case uTempoDivision: return "Musical interval at the current host tempo. Longer divisions give slower sweeps or longer repeats.";
    case uLoFiSpeed: return "Tape speed in inches per second. Slower is darker and changes transport character without retuning the guitar.";
    case uLoFiWow: return "Slow pitch drift; the green trace shows measured wow motion in cents.";
    case uLoFiFlutter: return "Fast pitch vibration; the amber trace uses the same cents scale as wow.";
    case uLoFiNoise: return "Independent stereo tape hiss, including between notes. Zero turns hiss off.";
    case uLoFiWear: return "Darker tape and irregular level dropouts. This control does not add saturation.";
    case uAirPlateOutput: case uAirHallOutput: case uSpaceOutput:
    case uDelayOutput: case uTapeOutput: case uTubeOutput: case uUtilityLevel: case uSpringOutput:
    case uWahOutput: case uOctOutput: case uDoubleOutput: case uCrushOutput: case uDistOutput: case uLoFiOutput:
      return "Final level after dry/wet blending. Match bypass loudness before judging the effect.";
    default: return "Blend dry and processed sound; higher settings make the effect more prominent. Output, when present, follows the blend.";
  }
}
inline const char* Sound(SlotEffect e)
{
  switch (e)
  {
    case SlotEffect::PedalDrive: return "Ten voiced guitar and bass drives; Low keep restores clean lows within this slot.";
    case SlotEffect::DigitalBlack: return "Zero-crossing gate darkens quiet tails; Hysteresis adds the strip's re-arming behaviour.";
    case SlotEffect::OutboardChannel: return "Capacitor cuts into CStrip2 compression and Spiral colour; Output matches level.";
    case SlotEffect::ConsoleColour: return "Eight nonlinear console voices; Input drives the colour, Output matches loudness.";
    case SlotEffect::ConsoleEQ: return "Nonlinear band density and native cuts; louder notes add harmonics beyond the curve.";
    case SlotEffect::ButterComp: return "Gentle polarity-sensitive glue with output-dependent recovery; Mix retains dry attack.";
    case SlotEffect::NAM: return "Input drives the capture; EQ and Output shape its result.";
    case SlotEffect::CabinetIR: return "Convolution for cabinets, reverbs and textures. Long IR keeps tails up to 30 seconds; Mix blends with dry.";
    case SlotEffect::NoiseGate: return "Threshold opens the gate; Release and Reduction shape the gaps.";
    case SlotEffect::NoisePrint: return "Learn the noise alone; Sensitivity and Reduction protect sustain.";
    case SlotEffect::SmoothComp: return "Gentle leveling; Attack preserves pick, Makeup restores level.";
    case SlotEffect::PunchComp: return "Firmer compression; slow Attack and parallel Mix retain punch.";
    case SlotEffect::Limiter: return "Fast peak control; Ceiling and Release set grip and recovery.";
    case SlotEffect::ToneEQ: return "Bass, Mid and Treble shape the tone; the curve shows the combined response.";
    case SlotEffect::CutEQ: return "Low cut and High cut trim rumble and fizz; Presence adds bite.";
    case SlotEffect::ParametricEQ: return "Bell or notch, a resonant low cut or shelf, and a resonant high cut.";
    case SlotEffect::SynthFilter: return "Cutoff and Resonance shape tone; Envelope follows playing strength.";
    case SlotEffect::Wah: return "Position sweeps the peak; Register moves its heel and toe range.";
    case SlotEffect::Boost: return "Clean boost into the next module; Tilt balances body and bite.";
    case SlotEffect::TubeBoost: return "Softer clipping with asymmetric harmonics; Output matches loudness.";
    case SlotEffect::Distortion: return "Style changes harmonics; High cut softens edges, Mix restores dry attack.";
    case SlotEffect::Tape: return "Drive adds tape saturation; Bump adds low-end body.";
    case SlotEffect::LoFiTape: return "Speed sets tone; Wow/Flutter move pitch, Wear adds dropouts.";
    case SlotEffect::AutoSwell: return "Threshold triggers the fade-in; Release closes the next gap.";
    case SlotEffect::DigitalDelay: return "Clean repeats; Ducking leaves room for notes, then reveals echoes.";
    case SlotEffect::AnalogDelay: return "Age darkens and saturates repeats; Modulation adds pitch drift.";
    case SlotEffect::TapeDelay: return "Saturated repeats; Modulation adds tape-like pitch wobble.";
    case SlotEffect::DualDelay: return "Two delays; Time ratio sets the second rhythm.";
    case SlotEffect::ReverseDelay: return "Plays each window backwards; Time sets its length.";
    case SlotEffect::RoomReverb: return "Short reflections for intimacy; Size changes spacing, Decay the tail.";
    case SlotEffect::PlateReverb: return "Dense, smooth tail; Pre-delay keeps the dry attack distinct.";
    case SlotEffect::CloudReverb: return "Long moving wash; Damping and Low cut keep the tail clear.";
    case SlotEffect::HallReverb: return "Wide reflection spacing and a long tail; Pre-delay adds separation.";
    case SlotEffect::AirPlate: return "Airwindows kPlate140: dense plate character with native sustain and resolution.";
    case SlotEffect::AirHall: return "Airwindows kGuitarHall2: colored hall with selectable early-reflection positions.";
    case SlotEffect::GalacticSpace: return "Airwindows Galactic3: huge stereo space; Replace controls how new notes enter the wash.";
    case SlotEffect::CosmicEcho: return "Valhalla Supermassive-inspired; Warp spreads echoes into a wash, Density then thickens it.";
    case SlotEffect::MultiHeadDelay: return "Eight tape-head patterns; Time sets the last head.";
    case SlotEffect::DiffusedDelay: return "Repeats smear into stereo clouds as Diffusion rises.";
    case SlotEffect::SpringReverb: return "Dwell excites the spring; Drip adds splash, Damping tames treble.";
    case SlotEffect::ShimmerReverb: return "Pitch and Amount build rising/falling tails; Freeze sustains them.";
    case SlotEffect::Chorus: return "Dry plus moving delay; Depth adds detune, Spread separates voices.";
    case SlotEffect::JunoChorus: return "Mode chooses one or both moving voices; Brightness softens their wet tone.";
    case SlotEffect::Flanger: return "Swept comb notches; Feedback sharpens them.";
    case SlotEffect::Phaser: return "Six swept phase stages; Feedback deepens the resonant swirl.";
    case SlotEffect::Tremolo: return "Rate sets pulses, Depth sets dips; Pulse / feel adds sharper motion.";
    case SlotEffect::Vibrato: return "Fully wet pitch motion; Rate and Depth set speed and excursion.";
    case SlotEffect::Rotary: return "Fast switches the rotation speed; Ramp time sets the transition.";
    case SlotEffect::UniVibe: return "Uneven phase sweep; Vibrato mode removes the dry blend.";
    case SlotEffect::Octaver: return "Sub-octave layers follow single notes; Dry retains the pick attack.";
    case SlotEffect::Doubler: return "Opposite detuning and delay suggest a second take; Spread widens it.";
    case SlotEffect::FrequencyShifter: return "Hertz offsets make metallic sidebands; Feedback shifts each repeat.";
    case SlotEffect::Freeze: return "Hold adds a captured layer; Crossfade smooths and shortens its loop.";
    case SlotEffect::Stutter: return "Window sets repeats; Gate length and Edge fade carve each pulse.";
    case SlotEffect::Bitcrusher: return "Bits add grain; Sample rate adds aliasing, High cut softens the result.";
    case SlotEffect::RingMod: return "Frequency and Depth create metallic sidebands; Mix restores dry tone.";
    case SlotEffect::Utility: return "Balance, Width and polarity reshape stereo; Fine delay aligns paths.";
    default: return "";
  }
}
} // namespace effect_presentation
