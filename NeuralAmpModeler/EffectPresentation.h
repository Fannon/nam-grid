#pragma once
#include "EffectParameters.h"

// Shared controls may have effect-specific wording and layout. Unique controls
// take their labels directly from the host/state parameter descriptors.
namespace effect_presentation
{
inline bool HeaderControl(int p)
{ return p == uTempoSync || p == uDelayPingPong || p == uReverbStereo || p == uTapeHeads || p == uCosmicStyle || p == uColourDesk || p == uBlackMode; }
inline bool PingPong(SlotEffect e)
{ return e == SlotEffect::DigitalDelay || e == SlotEffect::AnalogDelay || e == SlotEffect::TapeDelay || e == SlotEffect::DualDelay; }
inline bool Tempo(SlotEffect e)
{ return IsDelay(e) || IsModulation(e) || e == SlotEffect::UniVibe || e == SlotEffect::Stutter; }
inline bool Visible(SlotEffect e, int p, bool sync = false, bool notch = false, bool vibrato = false)
{
  if (p == uTempoSync) return Tempo(e);
  if (p == uTempoDivision) return Tempo(e) && sync;
  if (p == uDelayPingPong) return PingPong(e);
  if (p == uReverbStereo) return IsReverb(e) && e != SlotEffect::AirPlate && e != SlotEffect::AirHall && e != SlotEffect::GalacticSpace;
  if (sync && (p == uDelayTime || p == uModRate || p == uStutterTime)) return false;
  if (e == SlotEffect::ParametricEQ && p == uGain && notch) return false;
  if (e == SlotEffect::UniVibe && p == uModMix && vibrato) return false;
  return UsesUtility(e, p);
}
// Every module's output gain sits bottom-left, below its first control, as for NAM and IR.
inline constexpr int kOutputPosition = 4;
inline int DesignedPosition(SlotEffect e, int p)
{
  // Keep Spring's shared damping control in one compact, unambiguous sequence.
  if (e == SlotEffect::SpringReverb)
  {
    if (p == uSpringDrip) return 2;
    if (p == uReverbTone) return 3;
    if (p == uSpringMix) return 4;
    if (p == uSpringOutput) return 5;
  }
  if (e == SlotEffect::ReverseDelay)
  {
    if (p == uDelayTone) return 1;
    if (p == uDelayMix) return 2;
    if (p == uDelayOutput) return 3;
  }
  return kEffectParameters[p].position;
}
// The module's output gain control ("... Output" host parameter), or -1.
inline int OutputControl(SlotEffect e)
{
  for (int p = 0; p < kUtilityControls; ++p)
  {
    const std::string_view name = kEffectParameters[p].name;
    if (UsesUtility(e, p) && name.size() >= 7 && name.substr(name.size() - 7) == " Output")
      return p;
  }
  return -1;
}
inline int Position(SlotEffect e, int p)
{
  const int position = DesignedPosition(e, p);
  const int output = OutputControl(e);
  if (output < 0)
    return position;
  if (p == output)
    return kOutputPosition;
  // Move Output to its place; the other controls keep their order and close the gap.
  const int from = DesignedPosition(e, output);
  if (from < kOutputPosition && position > from && position <= kOutputPosition)
    return position - 1;
  if (from > kOutputPosition && position >= kOutputPosition && position < from)
    return position + 1;
  return position;
}
inline const char* Label(SlotEffect e, int p)
{
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
                                                         : "Time to the first repeat. In Dual Delay the right time is this value multiplied by Time ratio.";
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
    case uModCharacter: return e == SlotEffect::Phaser ? "Raises the center of the six-stage phase sweep."
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
    case SlotEffect::DigitalBlack: return "Zero-crossing gate darkens quiet tails; Hysteresis adds the strip's re-arming behaviour.";
    case SlotEffect::OutboardChannel: return "Capacitor cuts into CStrip2 compression and Spiral colour; Output matches level.";
    case SlotEffect::ConsoleColour: return "Eight nonlinear console voices; Input trim drives colour, Output trim matches loudness.";
    case SlotEffect::ConsoleEQ: return "Nonlinear band density and native cuts; louder notes add harmonics beyond the curve.";
    case SlotEffect::ButterComp: return "Gentle polarity-sensitive glue with output-dependent recovery; Mix retains dry attack.";
    case SlotEffect::NAM: return "Input drives the capture; EQ and Output shape its result.";
    case SlotEffect::CabinetIR: return "Cabinet convolution; the traces compare input and output level.";
    case SlotEffect::NoiseGate: return "Threshold opens the gate; Release and Reduction shape the gaps.";
    case SlotEffect::NoisePrint: return "Learn the noise alone; Sensitivity and Reduction protect sustain.";
    case SlotEffect::SmoothComp: return "Gentle leveling; Attack preserves pick, Makeup restores level.";
    case SlotEffect::PunchComp: return "Firmer compression; slow Attack and parallel Mix retain punch.";
    case SlotEffect::Limiter: return "Fast peak control; Ceiling and Release set grip and recovery.";
    case SlotEffect::ToneEQ: case SlotEffect::CutEQ: case SlotEffect::ParametricEQ: return "The curve shows the combined EQ; compare against the 0 dB line.";
    case SlotEffect::SynthFilter: return "Cutoff and Resonance shape tone; Envelope follows playing strength.";
    case SlotEffect::Wah: return "Position sweeps the peak; Range moves its heel/toe register.";
    case SlotEffect::Boost: return "Clean boost into the next module; Tilt balances body and bite.";
    case SlotEffect::TubeBoost: return "Softer clipping with asymmetric harmonics; Output matches loudness.";
    case SlotEffect::Distortion: return "Style changes harmonics; High cut softens edges, Mix restores dry attack.";
    case SlotEffect::Tape: return "Drive adds tape saturation; Bump adds low-end body.";
    case SlotEffect::LoFiTape: return "Speed sets tone; Wow/Flutter move pitch, Wear adds dropouts.";
    case SlotEffect::AutoSwell: return "Threshold triggers the fade-in; Release closes the next gap.";
    case SlotEffect::DigitalDelay: return "Clean repeats; Ducking leaves room for notes, then reveals echoes.";
    case SlotEffect::AnalogDelay: return "Age darkens and saturates repeats; Modulation adds pitch drift.";
    case SlotEffect::TapeDelay: return "Saturated repeats; Modulation adds tape-like pitch wobble.";
    case SlotEffect::DualDelay: return "Ratio makes two rhythms; Ping-pong alternates the feedback path.";
    case SlotEffect::ReverseDelay: return "Reverses fresh windows; Time sets window length, High cut softens them.";
    case SlotEffect::RoomReverb: return "Short reflections for intimacy; Size changes spacing, Decay the tail.";
    case SlotEffect::PlateReverb: return "Dense, smooth tail; Pre-delay keeps the dry attack distinct.";
    case SlotEffect::CloudReverb: return "Long moving wash; Damping and Low cut keep the tail clear.";
    case SlotEffect::HallReverb: return "Wide reflection spacing and a long tail; Pre-delay adds separation.";
    case SlotEffect::AirPlate: return "Airwindows kPlate140: dense plate character with native sustain and resolution.";
    case SlotEffect::AirHall: return "Airwindows kGuitarHall2: colored hall with selectable early-reflection positions.";
    case SlotEffect::GalacticSpace: return "Airwindows Galactic3: huge stereo space; Replace controls how new notes enter the wash.";
    case SlotEffect::CosmicEcho: return "Valhalla Supermassive-inspired; Warp spreads echoes, Density blends them.";
    case SlotEffect::MultiHeadDelay: return "Eight head patterns; stereo modes spread taps and cross feedback. Time sets the last head.";
    case SlotEffect::DiffusedDelay: return "Repeats spread into stereo clouds; Diffusion changes the shape of each echo.";
    case SlotEffect::SpringReverb: return "Dwell excites the spring; Drip adds splash, Damping tames treble.";
    case SlotEffect::ShimmerReverb: return "Pitch and Amount build rising/falling tails; Freeze sustains them.";
    case SlotEffect::Chorus: return "Dry plus moving delay; Depth adds detune, Spread separates voices.";
    case SlotEffect::JunoChorus: return "I/II choose moving voices; Brightness softens their wet tone.";
    case SlotEffect::Flanger: return "Short swept delay makes comb notches; Feedback makes them sharper.";
    case SlotEffect::Phaser: return "Six swept phase stages; Feedback deepens the resonant swirl.";
    case SlotEffect::Tremolo: return "Rate sets pulses, Depth sets dips; Pulse / feel adds sharper motion.";
    case SlotEffect::Vibrato: return "Fully wet pitch motion; Rate and Depth set speed and excursion.";
    case SlotEffect::Rotary: return "Slow/Fast changes rotation; Ramp time controls the speed transition.";
    case SlotEffect::UniVibe: return "Uneven phase sweep; Vibrato mode removes the dry blend.";
    case SlotEffect::Octaver: return "Sub-octave layers follow single notes; Dry retains the pick attack.";
    case SlotEffect::Doubler: return "Opposite detuning and delay suggest a second take; Spread widens it.";
    case SlotEffect::FrequencyShifter: return "Hertz offsets make metallic sidebands; Feedback shifts each repeat.";
    case SlotEffect::Freeze: return "Hold adds a captured layer; Crossfade smooths and shortens its loop.";
    case SlotEffect::Stutter: return "Window sets repeats; Gate length and Edge fade carve each pulse.";
    case SlotEffect::Bitcrusher: return "Bits add grain; Sample rate adds aliasing, High cut softens the result.";
    case SlotEffect::RingMod: return "Carrier and Depth create metallic sidebands; Mix restores dry tone.";
    case SlotEffect::Utility: return "Balance, Width and polarity reshape stereo; Fine delay aligns paths.";
    default: return "";
  }
}
} // namespace effect_presentation
