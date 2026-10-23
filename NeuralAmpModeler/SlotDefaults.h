#pragma once
#include "EffectParameters.h"

// A module's insertion and editor reset use the same musical starting point.
// Parameter-bank defaults stay neutral for unused controls. Saved slot values
// are complete snapshots and never pass through these insertion defaults.
using SlotValues = std::array<double, 17 + kUtilityControls>;
inline SlotValues SlotDefaultValues(SlotEffect effect)
{
  SlotValues v{0., 0., 1., -55., 120., 60., -18., 120., 0.,
               0., 0., 0., 0., 20000., 0., -18., 1.};
  for (int i = 0; i < kUtilityControls; ++i) v[17 + i] = kEffectParameters[i].initial;
  const auto set = [&](UtilityParam p, double value) { v[17 + p] = value; };
  const auto mod = [&](double rate, double depth, double feedback, double character, double mix) {
    set(uModRate, rate); set(uModDepth, depth); set(uModFeedback, feedback);
    set(uModCharacter, character); set(uModMix, mix);
  };
  const auto delay = [&](double time, double feedback, double tone, double character, double motion, double mix) {
    set(uDelayTime, time); set(uDelayFeedback, feedback); set(uDelayTone, tone);
    set(uDelayCharacter, character); set(uDelayMotion, motion); set(uDelayMix, mix);
  };
  const auto reverb = [&](double decay, double size, double tone, double pre, double motion, double mix, double hp) {
    set(uReverbDecay, decay); set(uReverbSize, size); set(uReverbTone, tone);
    set(uReverbPredelay, pre); set(uReverbMotion, motion); set(uReverbMix, mix); set(uReverbHighPass, hp);
  };
  switch (effect)
  {
    case SlotEffect::NoiseGate:
      v[3] = -55.; v[4] = 150.; break;
    case SlotEffect::SmoothComp:
      v[6] = -24.; v[7] = 150.; v[8] = 3.; set(uCompAttack, 10.); break;
    case SlotEffect::PunchComp:
      v[6] = -18.; v[7] = 100.; v[8] = 2.; set(uPunchAttack, 25.); set(uCompMix, 80.); break;
    case SlotEffect::CutEQ:
      v[12] = 80.; v[13] = 10000.; break;
    case SlotEffect::Boost:
      set(uBoostGain, 3.); break;
    case SlotEffect::SynthFilter:
      set(uFilterResonance, .71); break;
    case SlotEffect::AutoSwell:
      set(uSwellRise, 350.); set(uSwellThreshold, -48.); set(uSwellRelease, 150.); break;
    case SlotEffect::DigitalDelay:
      delay(400., 30., 10000., 0., 0., 20.); break;
    case SlotEffect::AnalogDelay:
      delay(320., 35., 3500., 35., 10., 22.); break;
    case SlotEffect::TapeDelay:
      delay(375., 35., 4500., 30., 20., 22.); break;
    case SlotEffect::MultiHeadDelay:
      delay(500., 30., 5000., 25., 12., 25.); set(uTapeHeads, 1.); break;
    case SlotEffect::DiffusedDelay:
      delay(450., 45., 6500., 0., 15., 25.); set(uDelayDiffusion, 60.); break;
    case SlotEffect::ReverseDelay:
      delay(500., 0., 6000., 0., 0., 30.); break;
    case SlotEffect::DualDelay:
      delay(400., 30., 6500., 0., 0., 25.); set(uDualRatio, .75); break;
    case SlotEffect::RoomReverb:
      reverb(.8, 80., 6500., 8., 10., 12., 120.); break;
    case SlotEffect::PlateReverb:
      reverb(2.4, 100., 6500., 20., 15., 18., 120.); break;
    case SlotEffect::CloudReverb:
      reverb(6., 150., 5000., 30., 40., 30., 150.); break;
    case SlotEffect::HallReverb:
      reverb(3.2, 120., 6000., 25., 20., 22., 120.); break;
    case SlotEffect::SpringReverb:
      set(uReverbTone, 5000.); set(uSpringMix, 18.); break;
    case SlotEffect::ShimmerReverb:
      set(uShimmerMix, 25.); set(uShimmerDecay, 5.); set(uShimmerTone, 6000.);
      set(uShimmerAmount, 25.); set(uShimmerMod, 30.); break;
    case SlotEffect::Chorus:
      mod(.8, 35., 0., 0., 40.); break;
    case SlotEffect::JunoChorus:
      set(uJunoMode, 0.); set(uJunoBrightness, 80.); break;
    case SlotEffect::Flanger:
      mod(.25, 65., 45., 30., 50.); break;
    case SlotEffect::Phaser:
      mod(.35, 65., 15., 20., 50.); break;
    case SlotEffect::Tremolo:
      mod(4., 60., 0., 0., 100.); break;
    case SlotEffect::Vibrato:
      mod(4.5, 15., 0., 0., 100.); break;
    case SlotEffect::UniVibe:
      mod(1.2, 65., 0., 35., 50.); break;
    case SlotEffect::Rotary:
      set(uRotaryDrive, 1.5); set(uRotaryMix, 100.); break;
    case SlotEffect::RingMod:
      set(uRingFrequency, 110.); set(uRingTone, 6000.); set(uRingMix, 30.); break;
    case SlotEffect::Doubler:
      set(uDoubleDetune, 6.); set(uDoubleDelay, 15.); set(uDoubleMix, 25.); break;
    case SlotEffect::FrequencyShifter:
      set(uShiftFrequency, 2.); set(uShiftFeedback, 10.); set(uShiftDelay, 20.);
      set(uShiftMix, 30.); break;
    case SlotEffect::Bitcrusher:
      set(uCrushBits, 10.); set(uCrushRate, 16000.); set(uCrushTone, 6500.); set(uCrushMix, 50.); break;
    case SlotEffect::Distortion:
      set(uDistDrive, 9.); set(uDistTone, 5500.); set(uDistOutput, -6.); break;
    case SlotEffect::TubeBoost:
      set(uTubeOutput, -3.); break;
    case SlotEffect::Tape:
      set(uTapeBump, 25.); break;
    case SlotEffect::LoFiTape:
      set(uLoFiWow, 15.); set(uLoFiFlutter, 10.); set(uLoFiNoise, 5.); set(uLoFiWear, 20.); break;
    // Neutral EQ/utility, modest octave blend, unlatched Freeze, and the
    // already auditioned Smart Gate retain the descriptor defaults.
    case SlotEffect::NAM: case SlotEffect::CabinetIR: case SlotEffect::ToneEQ:
    case SlotEffect::ParametricEQ: case SlotEffect::Wah: case SlotEffect::Utility: case SlotEffect::Limiter:
    case SlotEffect::Octaver: case SlotEffect::Freeze: case SlotEffect::Stutter:
    case SlotEffect::NoisePrint: case SlotEffect::AirPlate: case SlotEffect::AirHall:
    case SlotEffect::GalacticSpace: case SlotEffect::CosmicEcho:
    case SlotEffect::OutboardChannel: case SlotEffect::ConsoleColour: case SlotEffect::ConsoleEQ: case SlotEffect::ButterComp:
    case SlotEffect::DigitalBlack: break;
  }
  return v;
}
