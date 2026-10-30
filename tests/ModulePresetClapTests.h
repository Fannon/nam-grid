#pragma once
#include "../NeuralAmpModeler/ModulePresets.h"

// Factory recipes are not saved identities. Simulate a replacement recipe under
// the same name and verify an earlier user file, .ngrid and host state still
// recall every original value, including unrelated routing and other slots.
template <class Save, class Load>
void TestModulePresetRecall(json cleared, Save save, Load load, const std::filesystem::path& root)
{
  using namespace module_presets;
  for (int id = 0; id <= static_cast<int>(SlotEffect::Last); ++id)
  {
    const auto effect = static_cast<SlotEffect>(id);
    if (For(effect).empty()) continue;
    const auto& original = For(effect).front();
    auto current = SlotDefaultValues(effect);
    current[parameter_layout::kSlotEnabled] = 0.;
    current[parameter_layout::kSlotIn] = 7.;
    const auto chosen = Apply(effect, original, current);
    const auto userFile = root / "factory-copy.json";
    SaveUser(userFile, effect, chosen);
    auto rig = cleared;
    rig["SlotEffects"][15] = id;
    rig["A/B Blend"] = 37.;
    rig["Pre 2 Boost Gain"] = 5.;
    for (int c = 0; c < static_cast<int>(chosen.size()); ++c)
      rig["Post 4 " + std::string(ControlName(c))] = chosen[c];
    load(rig);
    const auto saved = save();
    assert(saved.config() == rig);
    const auto rigFile = root / "factory-copy.ngrid";
    rig_preset::Write(rigFile, rig);

    auto replacementValues = std::vector<Value>(original.values);
    const auto range = RangeOf(replacementValues.front().control);
    replacementValues.front().value = replacementValues.front().value == range.minimum ? range.maximum : range.minimum;
    const auto revised = Apply(effect, replacementValues, current);
    assert(revised != chosen);
    auto changed = rig;
    for (int c = 0; c < static_cast<int>(revised.size()); ++c)
      changed["Post 4 " + std::string(ControlName(c))] = revised[c];
    load(changed);
    assert(save().config() == changed);
    // Read the frozen values with the original recipe no longer involved.
    std::ifstream input(userFile);
    std::vector<Value> userValues;
    assert(Parse(json::parse(input), effect, userValues).empty());
    assert(Apply(effect, userValues, revised) == chosen);
    auto recalled = rig_preset::Read(rigFile);
    load(recalled);
    assert(save().config() == rig);
    load(changed);
    auto project = saved.config();
    load(project);
    assert(save().config() == rig);
  }
  std::filesystem::remove(root / "factory-copy.json");
  std::filesystem::remove(root / "factory-copy.ngrid");
  load(cleared);
}
