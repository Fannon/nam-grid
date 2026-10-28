#pragma once
#include "EffectPresentation.h"
#include "ParameterLayout.h"
#include "SlotDefaults.h"
#include <cmath>
#include <initializer_list>
#include <span>

// Factory starting points for one module, chosen from the module editor. Unlike rig
// presets they change only that slot's module controls, through ordinary host parameter
// changes, so they never reload the module and never touch the rest of the rig. Nothing
// about them is saved: a project stores the resulting control values.
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

// Pickup stays as the player set it unless a preset is for a particular instrument.
inline const Preset pedalComp[]{
  {"Studio smooth", {{U(uPedalCompSustain), 35.}, {U(uPedalCompAttack), 1.}}},
  {"Country squash", {{U(uPedalCompSustain), 70.}, {U(uPedalCompAttack), 0.}, {U(uPedalCompTone), 2.}}},
  {"Funk rhythm",
   {{U(uPedalCompSustain), 50.}, {U(uPedalCompAttack), 0.}, {U(uPedalCompTone), 1.5}, {U(uPedalCompBlend), 85.}}},
  {"Parallel sustain",
   {{U(uPedalCompSustain), 85.}, {U(uPedalCompAttack), 1.}, {U(uPedalCompTone), 1.}, {U(uPedalCompBlend), 55.}}},
  {"Lead sustain", {{U(uPedalCompSustain), 75.}, {U(uPedalCompAttack), 1.}, {U(uPedalCompLevel), 2.}}},
  {"Acoustic strum",
   {{U(uPedalCompPickup), 2.}, {U(uPedalCompSustain), 45.}, {U(uPedalCompAttack), 1.}, {U(uPedalCompTone), -1.5},
    {U(uPedalCompBlend), 75.}}},
  {"Acoustic fingerstyle",
   {{U(uPedalCompPickup), 2.}, {U(uPedalCompSustain), 30.}, {U(uPedalCompAttack), 1.}, {U(uPedalCompBlend), 60.}}},
};
inline const Preset smoothComp[]{
  {"Clean sustain",
   {{kSlotCompThreshold, -30.}, {kSlotCompRelease, 250.}, {kSlotCompMakeup, 6.}, {U(uCompAttack), 8.},
    {U(uSmoothRatio), 3.}}},
  {"Acoustic leveler",
   {{kSlotCompThreshold, -26.}, {kSlotCompRelease, 200.}, {kSlotCompMakeup, 4.}, {U(uCompAttack), 15.},
    {U(uCompDetectorCut), 150.}}},
  {"Bass leveler",
   {{kSlotCompThreshold, -24.}, {kSlotCompRelease, 200.}, {kSlotCompMakeup, 4.}, {U(uCompAttack), 15.},
    {U(uSmoothRatio), 4.}}},
};
inline const Preset punchComp[]{
  {"Parallel punch",
   {{kSlotCompThreshold, -30.}, {kSlotCompRelease, 100.}, {kSlotCompMakeup, 6.}, {U(uPunchAttack), 25.},
    {U(uPunchRatio), 6.}, {U(uCompMix), 50.}}},
  {"Funk snap",
   {{kSlotCompThreshold, -22.}, {kSlotCompRelease, 80.}, {kSlotCompMakeup, 4.}, {U(uPunchAttack), 12.},
    {U(uCompMix), 100.}}},
  {"Lead sustain",
   {{kSlotCompThreshold, -32.}, {kSlotCompRelease, 300.}, {kSlotCompMakeup, 8.}, {U(uPunchAttack), 20.},
    {U(uPunchRatio), 8.}, {U(uCompMix), 70.}}},
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

inline std::span<const Preset> For(SlotEffect effect)
{
  switch (effect)
  {
    case SlotEffect::PedalComp: return pedalComp;
    case SlotEffect::SmoothComp: return smoothComp;
    case SlotEffect::PunchComp: return punchComp;
    case SlotEffect::DigitalDelay: return digitalDelay;
    default: return {};
  }
}
// Slot controls the module's editor can show: a preset sets these and nothing else.
inline bool ModuleControl(SlotEffect effect, int control)
{
  using namespace parameter_layout;
  if (control >= kSlotCompThreshold && control <= kSlotCompMakeup)
    return effect == SlotEffect::SmoothComp || effect == SlotEffect::PunchComp;
  if (control >= kSlotGateThreshold && control <= kSlotGateReduction) return effect == SlotEffect::NoiseGate;
  const int p = control - standardControls;
  if (p < 0 || p >= kUtilityControls) return false;
  for (bool sync : {false, true})
    for (bool alternate : {false, true})
      if (effect_presentation::Visible(effect, p, sync, alternate, alternate, alternate)) return true;
  return false;
}
// The player's own setup, kept across presets that do not name it.
inline bool KeepsSetup(SlotEffect effect, int control)
{ return effect == SlotEffect::PedalComp && control == U(uPedalCompPickup); }
// The complete slot values after choosing a preset: module controls the preset leaves out
// return to the module's insertion defaults, everything else keeps its current value.
inline SlotValues Apply(SlotEffect effect, const Preset& preset, const SlotValues& current)
{
  auto result = current;
  const auto defaults = SlotDefaultValues(effect);
  for (int i = 0; i < static_cast<int>(result.size()); ++i)
    if (ModuleControl(effect, i) && !KeepsSetup(effect, i)) result[i] = defaults[i];
  for (const auto& value : preset.values) result[value.control] = value.value;
  return result;
}
// The preset the current values equal, or -1 once a control has moved away from it.
inline int Matching(SlotEffect effect, const SlotValues& current)
{
  const auto presets = For(effect);
  for (size_t index = 0; index < presets.size(); ++index)
  {
    const auto expected = Apply(effect, presets[index], current);
    bool same = true;
    for (size_t i = 0; i < expected.size() && same; ++i)
      same = std::abs(expected[i] - current[i]) <= 1.e-6 * std::max(1., std::abs(expected[i]));
    if (same) return static_cast<int>(index);
  }
  return -1;
}
} // namespace module_presets
