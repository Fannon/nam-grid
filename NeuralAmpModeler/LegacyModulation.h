#pragma once
#include "ParameterLayout.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// The fixed 0.1 assignments (one slot / target / depth per source). Projects keep
// them as saved: they play through Modulation::Apply, so host automation of their
// parameters keeps its meaning. The Modulation page converts them to matrix rows
// only on request, and only where the rows sound exactly the same.
namespace grid_modulation
{
using namespace grid_performance;

// In their editor order.
struct LegacyAssignment
{
  int source, slot, target, depth;
};
inline constexpr std::array<LegacyAssignment, 5> legacyAssignments{{{SourceLFO, LFOSlot, LFOTarget, LFODepth},
                                                                    {SourceEnvelope, EnvSlot, EnvTarget, EnvDepth},
                                                                    {SourceSequencer, StepSlot, StepTarget, StepDepth},
                                                                    {SourceMacro1, Macro1Slot, Macro1Target, Macro1Depth},
                                                                    {SourceMacro2, Macro2Slot, Macro2Target, Macro2Depth}}};
// Plays: a target is chosen and its depth moves something.
inline bool LegacyActive(const Values& v, const LegacyAssignment& legacy)
{
  const int target = static_cast<int>(v[legacy.target]);
  return target > 0 && target < static_cast<int>(targetNames.size()) && v[legacy.depth] != 0.;
}
inline int LegacySlot(const Values& v, const LegacyAssignment& legacy)
{ return std::clamp(static_cast<int>(v[legacy.slot]), 0, gridSlot - 1); }
struct Row
{
  int slot = 0, control = 0;
  double scale = 1.; // Matrix depth per legacy depth.
};
// The matrix row that sounds exactly like a legacy target on its own. Fixed targets
// added or multiplied in physical units; the matrix moves along each knob's curve.
inline Row LegacyRow(int target, int slot, SlotEffect effect)
{
  using parameter_layout::standardControls;
  const auto utility = [&](int p, double scale = 1.) { return Row{slot, standardControls + p, scale}; };
  switch (target)
  {
    case 1: return utility(uWahPosition);
    case 2: return utility(uFilterCutoff, 6. / std::log2(12000. / 40.)); // Six octaves of an exponential range.
    case 3: return utility(uUtilityBalance, .5);                          // +/-100 on a 200-wide range.
    case 4: return utility(uDelayMix);
    case 5:
      // The old target wrote every reverb family's Mix; only the slot's own module reads one.
      for (const int p : {uReverbMix, uAirPlateMix, uAirHallMix, uSpaceMix, uCosmicMix, uShimmerMix, uSpringMix})
        if (UsesUtility(effect, p)) return utility(p);
      return utility(uReverbMix);
    case 6: return {slot, parameter_layout::kSlotTrim, 24. / 80.}; // +/-24 dB on -40..40 dB.
    case 7: return {gridSlot, GridBlend, 1.};
    case 8: return utility(uRingFrequency, 4. / std::log2(5000.)); // Four octaves of 1..5000 Hz.
    default: return {};
  }
}
inline Row LegacyRow(const Values& v, const LegacyAssignment& legacy, const std::array<SlotEffect, gridSlot>& effects)
{
  const int slot = LegacySlot(v, legacy);
  return LegacyRow(static_cast<int>(v[legacy.target]), slot, effects[slot]);
}

// Matrix row each assignment becomes, shared by the project and all its scenes, so
// recalling a scene replaces exactly the rows its assignments became.
struct LegacyPlan
{
  std::array<int, legacyAssignments.size()> rows{-1, -1, -1, -1, -1};
};
// Plans the conversion of every set of values (the project, then each scene, each
// already merged over the project's values). Returns why it cannot be exact, or "".
// Legacy assignments clamp one after another; matrix rows add and clamp once. Both
// agree only when no destination has two assignments, or an assignment and a row.
inline std::string PlanLegacy(const std::vector<Values>& sets, const std::array<SlotEffect, gridSlot>& effects,
                              LegacyPlan& plan)
{
  plan = {};
  std::array<bool, legacyAssignments.size()> needed{};
  for (size_t set = 0; set < sets.size(); ++set)
  {
    const auto& v = sets[set];
    const std::string where = set == 0 ? "the current settings" : "scene " + std::to_string(set);
    for (size_t i = 0; i < legacyAssignments.size(); ++i)
    {
      if (!LegacyActive(v, legacyAssignments[i])) continue;
      needed[i] = true;
      const auto row = LegacyRow(v, legacyAssignments[i], effects);
      for (size_t other = 0; other < i; ++other)
        if (LegacyActive(v, legacyAssignments[other]))
        {
          const auto earlier = LegacyRow(v, legacyAssignments[other], effects);
          if (earlier.slot == row.slot && earlier.control == row.control)
            return "In " + where + ", " + sourceNames[legacyAssignments[other].source] + " and "
                   + sourceNames[legacyAssignments[i].source]
                   + " modulate the same control. As fixed assignments they limit the result after each one; "
                     "matrix rows add first, so near the knob's ends they would sound different.";
        }
      for (int r = 0; r < rows; ++r)
        if (RowActive(v, r) && static_cast<int>(v[RowParam(r, RowSlot)]) == row.slot
            && static_cast<int>(v[RowParam(r, RowDestination)]) == row.control)
          return "In " + where + ", matrix row " + std::to_string(r + 1) + " already modulates the control of the "
                 + sourceNames[legacyAssignments[i].source] + " assignment, so a converted row would not sound the same.";
    }
  }
  std::array<bool, rows> taken{};
  for (size_t i = 0; i < legacyAssignments.size(); ++i)
  {
    if (!needed[i]) continue;
    for (int r = 0; r < rows && plan.rows[i] < 0; ++r)
      if (!taken[r] && std::all_of(sets.begin(), sets.end(), [&](const Values& v) { return v[RowParam(r, RowSource)] <= 0.; }))
      {
        plan.rows[i] = r;
        taken[r] = true;
      }
    if (plan.rows[i] < 0)
      return "There are not enough empty matrix rows, in the current settings and every stored scene, for the fixed assignments.";
  }
  return {};
}
// Writes the planned rows into one set of values and turns off its legacy targets.
// A set where an assignment is off gets an empty row, as recalling it turned that off.
inline void ApplyLegacy(Values& v, const LegacyPlan& plan, const std::array<SlotEffect, gridSlot>& effects)
{
  for (size_t i = 0; i < legacyAssignments.size(); ++i)
  {
    const int r = plan.rows[i];
    if (r < 0) continue;
    const auto& legacy = legacyAssignments[i];
    for (int field = 0; field < RowFields; ++field) v[RowParam(r, field)] = parameters[RowParam(r, field)].initial;
    if (LegacyActive(v, legacy))
    {
      const auto row = LegacyRow(v, legacy, effects);
      v[RowParam(r, RowSource)] = legacy.source;
      v[RowParam(r, RowSlot)] = row.slot;
      v[RowParam(r, RowDestination)] = row.control;
      v[RowParam(r, RowDepth)] = std::clamp(v[legacy.depth] * row.scale, -100., 100.);
    }
    v[legacy.target] = 0.;
  }
}
} // namespace grid_modulation
