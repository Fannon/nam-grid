#pragma once
#include "SlotDynamics.h"
#include <initializer_list>
#include <algorithm>

namespace nam_library
{
struct ModuleGroup
{
  const char* name;
  std::initializer_list<SlotEffect> effects;
};
using E = SlotEffect;
inline const ModuleGroup kModuleGroups[]{
  {"Distortion", {E::Distortion, E::Boost, E::TubeBoost, E::Tape, E::ConsoleColour, E::Bitcrusher}},
  {"Dynamics", {E::NaturalGate, E::NoisePrint, E::DigitalBlack, E::SmoothComp, E::PunchComp, E::ButterComp, E::OutboardChannel, E::Pressure, E::Limiter}},
  {"EQ / Filter", {E::ToneEQ, E::CutEQ, E::ParametricEQ, E::ConsoleEQ, E::SynthFilter, E::Wah}},
  {"Modulation", {E::Chorus, E::JunoChorus, E::Flanger, E::Phaser, E::Tremolo, E::Vibrato, E::Rotary, E::UniVibe, E::RingMod}},
  {"Pitch / Frequency", {E::Octaver, E::Doubler, E::FrequencyShifter}},
  {"Delay", {E::DigitalDelay, E::AnalogDelay, E::TapeDelay, E::MultiHeadDelay, E::ReverseDelay, E::DualDelay, E::DiffusedDelay}},
  {"Reverb", {E::RoomReverb, E::PlateReverb, E::CloudReverb, E::HallReverb, E::SpringReverb, E::ShimmerReverb,
               E::AirPlate, E::AirHall, E::GalacticSpace, E::CosmicEcho}},
  {"Creative", {E::Freeze, E::Stutter, E::LoFiTape}},
  {"Utility / Volume", {E::Utility, E::AutoSwell}}
};
inline const ModuleGroup* ModuleGroupFor(SlotEffect effect)
{
  if (effect == E::TightGate) effect = E::NaturalGate;
  for (const auto& group : kModuleGroups)
    if (std::find(group.effects.begin(), group.effects.end(), effect) != group.effects.end()) return &group;
  return nullptr;
}
inline SlotEffect StepModule(SlotEffect effect, int direction)
{
  if (effect == E::TightGate) effect = E::NaturalGate;
  if (const auto* group = ModuleGroupFor(effect))
  {
    const auto current = std::find(group->effects.begin(), group->effects.end(), effect) - group->effects.begin();
    const auto size = static_cast<int>(group->effects.size());
    return group->effects.begin()[(current + (direction < 0 ? -1 : 1) + size) % size];
  }
  return effect;
}
} // namespace nam_library
