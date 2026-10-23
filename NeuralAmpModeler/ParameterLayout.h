#pragma once
#include "EffectParameters.h"
#include "GridPerformance.h"
#include <cstddef>
#include <string>

// Stable layout v1: globals first, then one consecutive block per FX control.
// A control's ID never depends on the number of controls in a slot. Appending a
// control therefore appends IDs; no prototype layout or dummy capacity is kept.
namespace parameter_layout
{
inline constexpr int slots = 16;
inline constexpr int tunerReference = 284;
inline constexpr int performanceBase = 286;
inline constexpr int tunerActive = 330;
inline constexpr int utilityBase = 331;

constexpr int UtilityID(int slot, int control)
{ return utilityBase + control * slots + slot; }
constexpr int UtilityControl(int id, int count = kUtilityControls)
{ return id >= utilityBase && id < utilityBase + count * slots ? (id - utilityBase) / slots : -1; }
constexpr int UtilitySlot(int id, int count = kUtilityControls)
{ return id >= utilityBase && id < utilityBase + count * slots ? (id - utilityBase) % slots : -1; }
// Future global controls need explicit allocation after existing parameters.
// Do not silently move the FX base when expanding this fixed global bank.
static_assert(grid_performance::Count == 44, "Allocate new global/performance IDs explicitly");
} // namespace parameter_layout

enum EParams
{
  kInputLevel = 0,
  kNoiseGateThreshold,
  kToneBass,
  kToneMid,
  kToneTreble,
  kOutputLevel,
  kNoiseGateActive,
  kEQActive,
  kIRToggle,
  kCalibrateInput,
  kInputCalibrationLevel,
  kOutputMode,
  kSlim,
  kDriveInput,
  kDriveTrim,
  kDriveActive,
  kAmpTrim,
  kAmpActive,
  kGridSlotBase,
  kParallelBlend = kGridSlotBase + 14 * 3,
  kDynamicsBase,
  kEQBase = kDynamicsBase + 16 * 6,
  kQualityBase = kEQBase + 16 * 7,
  kUtilityBase = parameter_layout::utilityBase,
  kTunerReference = parameter_layout::tunerReference,
  kTunerMute,
  kGridParamBase = parameter_layout::performanceBase,
  kTunerActive = parameter_layout::tunerActive,
  kNumParams = parameter_layout::utilityBase + 16 * kUtilityControls
};

constexpr int kDualNumParams = kGridSlotBase;
constexpr int kGridV2NumParams = kDynamicsBase;
constexpr int kGridV3NumParams = kEQBase;
constexpr int SlotUtilityParam(size_t slot, int control)
{ return parameter_layout::UtilityID(static_cast<int>(slot), control); }
constexpr int SlotEQParam(size_t slot, int control)
{ return kEQBase + static_cast<int>(slot) * 7 + control; }
constexpr int SlotFullParam(size_t slot)
{ return slot == 1 ? kSlim : kQualityBase + static_cast<int>(slot) - (slot > 1 ? 1 : 0); }
constexpr int SlotDynamicsParam(size_t slot, int control)
{ return kDynamicsBase + static_cast<int>(slot) * 6 + control; }
constexpr int SlotInputParam(size_t slot)
{ return slot == 0 ? kDriveInput : slot == 1 ? kInputLevel : kGridSlotBase + (slot - 2) * 3; }
constexpr int SlotTrimParam(size_t slot)
{ return slot == 0 ? kDriveTrim : slot == 1 ? kAmpTrim : SlotInputParam(slot) + 1; }
constexpr int SlotActiveParam(size_t slot)
{ return slot == 0 ? kDriveActive : slot == 1 ? kAmpActive : SlotInputParam(slot) + 2; }

// Only parameters collected into the per-slot audio snapshot belong here.
constexpr int SlotControlOwner(int index)
{
  if (parameter_layout::UtilitySlot(index) >= 0)
    return parameter_layout::UtilitySlot(index);
  if (index >= kEQBase && index < kQualityBase)
    return (index - kEQBase) / 7;
  if (index >= kDynamicsBase && index < kEQBase)
    return (index - kDynamicsBase) / 6;
  if (index >= kGridSlotBase && index < kParallelBlend)
    return 2 + (index - kGridSlotBase) / 3;
  if (index >= kDriveInput && index <= kDriveActive)
    return 0;
  if (index == kInputLevel || index == kAmpTrim || index == kAmpActive)
    return 1;
  return -1;
}
inline std::string GridSlotName(size_t slot)
{ return std::string(slot < 4 ? "Pre " : slot < 8 ? "A " : slot < 12 ? "B " : "Post ") + std::to_string(slot % 4 + 1); }
constexpr std::array<int, 17 + kUtilityControls> SlotParamIndices(size_t slot)
{
  std::array<int, 17 + kUtilityControls> result{SlotInputParam(slot), SlotTrimParam(slot), SlotActiveParam(slot)};
  for (int i = 0; i < 6; ++i)
    result[3 + i] = SlotDynamicsParam(slot, i);
  for (int i = 0; i < 7; ++i)
    result[9 + i] = SlotEQParam(slot, i);
  result[16] = SlotFullParam(slot);
  for (int i = 0; i < kUtilityControls; ++i)
    result[17 + i] = SlotUtilityParam(slot, i);
  return result;
}
static_assert(
  [] {
    for (size_t slot = 0; slot < 16; ++slot)
    {
      const auto indices = SlotParamIndices(slot);
      for (size_t i = 0; i < indices.size(); ++i)
        if (i != 16 && SlotControlOwner(indices[i]) != static_cast<int>(slot))
          return false;
    }
    return SlotControlOwner(kOutputLevel) == -1 && SlotControlOwner(kGridParamBase) == -1;
  }(),
  "Every cached slot parameter must invalidate its owning bank");
