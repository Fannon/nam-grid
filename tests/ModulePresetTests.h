#pragma once
#include "ModulePresets.h"

void TestModulePresetSharedControls()
{
  using namespace parameter_layout;
  using namespace module_presets;
  // Independent list of the shared knobs actually shown by these editors. A
  // round trip based only on ModuleControl would also pass when a knob is missing.
  for (const auto& [effect, controls] : std::initializer_list<std::pair<SlotEffect, std::vector<int>>>{
    {SlotEffect::ToneEQ, {kSlotEQBass, kSlotEQMid, kSlotEQTreble}},
    {SlotEffect::CutEQ, {kSlotEQHighPass, kSlotEQLowPass, kSlotEQPresence}},
    {SlotEffect::ParametricEQ, {kSlotEQHighPass, kSlotEQLowPass}},
    {SlotEffect::Boost, {kSlotEQHighPass}}, {SlotEffect::TubeBoost, {kSlotEQHighPass}}})
  {
    auto edited = SlotDefaultValues(effect);
    for (const int control : controls) edited[control] = RangeOf(control).maximum;
    const auto document = ToJson(effect, edited);
    std::vector<Value> parsed;
    assert(Parse(document, effect, parsed).empty());
    const auto restored = Apply(effect, parsed, SlotDefaultValues(effect));
    for (const int control : controls)
    {
      assert(document["values"].contains(ControlName(control)));
      assert(restored[control] == edited[control]);
    }
    // Resetting a partial preset also resets these knobs, not unrelated state.
    const auto reset = Apply(effect, std::span<const Value>{}, edited);
    for (const int control : controls) assert(reset[control] == SlotDefaultValues(effect)[control]);
  }
}

void TestModulePresetCoverage()
{
  using namespace module_presets;
  for (int id = 0; id <= static_cast<int>(SlotEffect::Last); ++id)
  {
    const auto effect = static_cast<SlotEffect>(id);
    const bool supported = effect != SlotEffect::NAM && effect != SlotEffect::CabinetIR;
    assert(Supported(effect) == supported);
    assert(!supported || !ToJson(effect, SlotDefaultValues(effect))["values"].empty());
    // Deliberate selection policy: gates need input calibration and Utility needs
    // routing/phase context. Others have factory recipes only for established uses
    // that differ from the insertion default (docs/presets.md); keep the menus short.
    const bool calibrated = effect == SlotEffect::NoiseGate || effect == SlotEffect::NoisePrint
                            || effect == SlotEffect::DigitalBlack || effect == SlotEffect::Utility;
    assert(!calibrated || For(effect).empty());
    assert(For(effect).size() <= (effect == SlotEffect::PedalDrive ? 5u : 4u));
    for (const auto& preset : For(effect))
    {
      std::vector<int> seen;
      for (const auto& v : preset.values)
      {
        assert(v.control >= 0 && v.control < static_cast<int>(SlotValues{}.size()));
        assert(std::find(seen.begin(), seen.end(), v.control) == seen.end());
        seen.push_back(v.control);
        const auto range = RangeOf(v.control);
        assert(std::isfinite(v.value) && v.value >= range.minimum && v.value <= range.maximum);
      }
      const auto applied = Apply(effect, preset, SlotDefaultValues(effect));
      // Browsing factory recipes must never trigger a live capture/learn/latch.
      for (int p : {uFreezeHold, uShimmerFreeze, uStutterHold, uNoiseLearn})
        assert(applied[U(p)] == 0.);
      std::vector<Value> saved;
      assert(Parse(ToJson(effect, applied), effect, saved).empty());
      assert(Apply(effect, saved, SlotDefaultValues(effect)) == applied);
    }
  }
  assert(!Supported(static_cast<SlotEffect>(-1)) && !Supported(static_cast<SlotEffect>(999)));
}
