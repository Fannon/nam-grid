#pragma once
#include "EffectParameters.h"
#include "GridPerformance.h"
#include <array>
#include <cstddef>
#include <span>
#include <string>

// Permanent host-parameter layout. Host IDs equal iPlug2 parameter indices and
// are allocated in append-only segments: each segment first adds global
// controls, then one block of 16 consecutive IDs (one per slot) for each added
// slot control. Appending a segment, or growing the final segment's slot
// controls, never moves an existing ID. Never reorder, shrink or remove entries.
namespace parameter_layout
{
inline constexpr int slots = 16;
// Per-slot controls shared by every module, in SlotValues order. Module
// controls (UtilityParam) follow at standardControls + UtilityParam.
enum SlotControl
{
  kSlotIn = 0,
  kSlotTrim,
  kSlotEnabled,
  kSlotGateThreshold,
  kSlotGateRelease,
  kSlotGateReduction,
  kSlotCompThreshold,
  kSlotCompRelease,
  kSlotCompMakeup,
  kSlotEQBass,
  kSlotEQMid,
  kSlotEQTreble,
  kSlotEQHighPass,
  kSlotEQLowPass,
  kSlotEQPresence,
  kSlotIRLevel,
  kSlotFullQuality,
  standardControls
};
// Global controls in GlobalID index order: the five fixed globals, then
// grid_performance parameters. New globals append after the last index.
inline constexpr int fixedGlobals = 5;
inline constexpr int globalControls = fixedGlobals + grid_performance::Count;
inline constexpr int slotControls = standardControls + kUtilityControls;

struct Segment
{
  int globals;      // Global controls added by this segment, in GlobalID index order.
  int slotControls; // Slot controls added by this segment, each taking 16 IDs.
};
// Released allocation. Literal counts are deliberate: adding a global or slot
// control fails to compile until it is allocated here. To extend, append
// {newGlobals, 0} or {0, newControls}, or grow the LAST segment's slotControls.
// Never edit the counts of a released segment.
// +1 slot control: Digital Black Hold (state schema 2). The last {0, 6}: Pedal Comp (schema 13).
inline constexpr Segment segments[]{{83, 217}, {0, 9}, {0, 4}, {24, 0}, {0, 6}, {0, 37}, {1, 0}, {1, 0}, {0, 1}, {0, 6}};
inline constexpr Segment released[]{{83, 216}}; // The 0.1.0 baseline layout

constexpr int GlobalID(std::span<const Segment> table, int index)
{
  int id = 0, first = 0;
  for (const auto& segment : table)
  {
    if (index >= first && index < first + segment.globals)
      return id + index - first;
    id += segment.globals + segment.slotControls * slots;
    first += segment.globals;
  }
  return -1;
}
constexpr int SlotID(std::span<const Segment> table, int slot, int control)
{
  if (slot < 0 || slot >= slots)
    return -1;
  int id = 0, first = 0;
  for (const auto& segment : table)
  {
    id += segment.globals;
    if (control >= first && control < first + segment.slotControls)
      return id + (control - first) * slots + slot;
    id += segment.slotControls * slots;
    first += segment.slotControls;
  }
  return -1;
}
// Slot control and owning slot of a host ID; -1 for globals and invalid IDs.
constexpr int ControlOf(std::span<const Segment> table, int id)
{
  int start = 0, first = 0;
  for (const auto& segment : table)
  {
    start += segment.globals;
    if (id >= start && id < start + segment.slotControls * slots)
      return first + (id - start) / slots;
    start += segment.slotControls * slots;
    first += segment.slotControls;
  }
  return -1;
}
constexpr int SlotOf(std::span<const Segment> table, int id)
{
  const int control = ControlOf(table, id);
  return control < 0 ? -1 : (id - SlotID(table, 0, control));
}
constexpr int Total(std::span<const Segment> table)
{
  int total = 0;
  for (const auto& segment : table)
    total += segment.globals + segment.slotControls * slots;
  return total;
}

constexpr int GlobalID(int index) { return GlobalID(segments, index); }
constexpr int SlotID(int slot, int control) { return SlotID(segments, slot, control); }
constexpr int ControlOf(int id) { return ControlOf(segments, id); }
constexpr int SlotOf(int id) { return SlotOf(segments, id); }
// Module control (UtilityParam) of a host ID, or -1.
constexpr int UtilityControl(int id)
{
  const int control = ControlOf(id);
  return control >= standardControls ? control - standardControls : -1;
}

static_assert([] {
  // Every released segment keeps its boundaries; only the last may gain slot controls.
  constexpr size_t count = sizeof(released) / sizeof(released[0]);
  if (sizeof(segments) / sizeof(segments[0]) < count)
    return false;
  for (size_t i = 0; i < count; ++i)
    if (segments[i].globals != released[i].globals
        || (i + 1 < count ? segments[i].slotControls != released[i].slotControls
                          : segments[i].slotControls < released[i].slotControls))
      return false;
  return true;
}(), "Released parameter segments are frozen; append new segments instead");
static_assert([] {
  int globals = 0, controls = 0;
  for (const auto& segment : segments)
  {
    if (segment.globals < 0 || segment.slotControls < 0)
      return false;
    globals += segment.globals;
    controls += segment.slotControls;
  }
  return globals == globalControls && controls == slotControls;
}(), "Allocate every new global/slot control in parameter_layout::segments");
} // namespace parameter_layout

enum EParams
{
  // Global controls, in their permanent first-segment order.
  kOutputLevel = 0,
  kParallelBlend,
  kTunerActive,
  kTunerReference,
  kTunerMute,
  kNumParams = parameter_layout::Total(parameter_layout::segments)
};
static_assert(parameter_layout::GlobalID(kTunerMute) == kTunerMute, "Fixed globals keep their enum IDs");

// Host ID of a grid_performance parameter; never use contiguous arithmetic.
constexpr int PerformanceParam(int parameter) { return parameter_layout::GlobalID(parameter_layout::fixedGlobals + parameter); }
// grid_performance parameter of a host ID, or -1.
constexpr int PerformanceIndex(int id)
{
  for (int parameter = 0; parameter < grid_performance::Count; ++parameter)
    if (PerformanceParam(parameter) == id)
      return parameter;
  return -1;
}

// Saved selectors without host automation: algorithm modes and matrix routing.
// Their lists may grow without reinterpreting recorded automation.
constexpr bool IsStateOnlyParam(int id)
{
  if (IsAlgorithmMode(parameter_layout::UtilityControl(id))) return true;
  const int performance = PerformanceIndex(id);
  return performance >= 0 && grid_performance::StateOnly(performance);
}

constexpr int SlotParam(size_t slot, int control)
{ return parameter_layout::SlotID(static_cast<int>(slot), control); }
constexpr int SlotUtilityParam(size_t slot, int control)
{ return SlotParam(slot, parameter_layout::standardControls + control); }
constexpr int SlotInputParam(size_t slot) { return SlotParam(slot, parameter_layout::kSlotIn); }
constexpr int SlotTrimParam(size_t slot) { return SlotParam(slot, parameter_layout::kSlotTrim); }
constexpr int SlotActiveParam(size_t slot) { return SlotParam(slot, parameter_layout::kSlotEnabled); }
constexpr int SlotDynamicsParam(size_t slot, int control)
{ return SlotParam(slot, parameter_layout::kSlotGateThreshold + control); }
constexpr int SlotEQParam(size_t slot, int control) { return SlotParam(slot, parameter_layout::kSlotEQBass + control); }
constexpr int SlotFullParam(size_t slot) { return SlotParam(slot, parameter_layout::kSlotFullQuality); }

// Slot whose audio snapshot a parameter belongs to, or -1 for globals.
constexpr int SlotControlOwner(int index) { return parameter_layout::SlotOf(index); }
inline std::string GridSlotName(size_t slot)
{ return std::string(slot < 4 ? "Pre " : slot < 8 ? "A " : slot < 12 ? "B " : "Post ") + std::to_string(slot % 4 + 1); }
// Host IDs of one slot, in SlotValues order.
constexpr std::array<int, parameter_layout::slotControls> SlotParamIndices(size_t slot)
{
  std::array<int, parameter_layout::slotControls> result{};
  for (int i = 0; i < parameter_layout::slotControls; ++i)
    result[i] = SlotParam(slot, i);
  return result;
}
static_assert(
  [] {
    for (size_t slot = 0; slot < 16; ++slot)
      for (const int index : SlotParamIndices(slot))
        if (index < 0 || index >= kNumParams
            || SlotControlOwner(index) != static_cast<int>(slot))
          return false;
    for (int parameter = 0; parameter < grid_performance::Count; ++parameter)
      if (SlotControlOwner(PerformanceParam(parameter)) != -1)
        return false;
    return SlotControlOwner(kOutputLevel) == -1;
  }(),
  "Every slot parameter must map back to its owning slot");
