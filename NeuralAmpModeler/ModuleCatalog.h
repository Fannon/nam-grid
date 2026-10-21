#pragma once
#include "SlotDynamics.h"
#include <initializer_list>

namespace nam_library
{
struct ModuleGroup
{
  const char* name;
  std::initializer_list<SlotEffect> effects;
};
using E = SlotEffect;
inline const ModuleGroup kModuleGroups[]{
  {"Distortion", {E::Distortion, E::Boost, E::TubeBoost, E::Tape, E::Bitcrusher}},
  {"Dynamics", {E::NaturalGate, E::NoisePrint, E::SmoothComp, E::PunchComp, E::Limiter}},
  {"EQ / Filter", {E::ToneEQ, E::CutEQ, E::ParametricEQ, E::SynthFilter, E::Wah}},
  {"Modulation", {E::Chorus, E::JunoChorus, E::Flanger, E::Phaser, E::Tremolo, E::Vibrato, E::Rotary, E::UniVibe, E::RingMod}},
  {"Pitch / Frequency", {E::Octaver, E::Doubler, E::FrequencyShifter}},
  {"Delay", {E::DigitalDelay, E::AnalogDelay, E::TapeDelay, E::ReverseDelay, E::DualDelay}},
  {"Reverb", {E::RoomReverb, E::PlateReverb, E::CloudReverb, E::HallReverb, E::SpringReverb, E::ShimmerReverb}},
  {"Creative", {E::Freeze, E::Stutter, E::LoFiTape}},
  {"Utility / Volume", {E::Utility, E::AutoSwell}}
};
inline SlotEffect StepModule(SlotEffect effect, int direction)
{
  if (effect == E::TightGate) effect = E::NaturalGate;
  for (const auto& group : kModuleGroups)
    for (size_t i = 0; i < group.effects.size(); ++i)
      if (group.effects.begin()[i] == effect)
      {
        const auto size = static_cast<int>(group.effects.size());
        return group.effects.begin()[(static_cast<int>(i) + (direction < 0 ? -1 : 1) + size) % size];
      }
  return effect;
}
} // namespace nam_library
