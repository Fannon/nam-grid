#pragma once
#include "../NeuralAmpModeler/ModulePresets.h"
#include <map>

// Use the running plugin's public flags as the independent source of discrete
// domains, rather than copying the parser's descriptor classification into a test.
inline void TestModulePresetValueDomains(const clap_plugin* plugin)
{
  using namespace module_presets;
  const auto* params = static_cast<const clap_plugin_params*>(plugin->get_extension(plugin, CLAP_EXT_PARAMS));
  std::map<std::string, clap_param_info> infos;
  for (uint32_t i = 0; i < params->count(plugin); ++i)
  {
    clap_param_info info{};
    assert(params->get_info(plugin, i, &info));
    if (std::string(info.name).starts_with("Pre 1 ")) infos.emplace(info.name, info);
  }
  int continuous = 0, discrete = 0;
  for (int id = 0; id <= static_cast<int>(SlotEffect::Last); ++id)
  {
    const auto effect = static_cast<SlotEffect>(id);
    if (!Supported(effect)) continue;
    const auto values = SlotDefaultValues(effect);
    for (int control = 0; control < static_cast<int>(values.size()); ++control)
    {
      if (!ModuleControl(effect, control)) continue;
      const auto& info = infos.at("Pre 1 " + std::string(ControlName(control)));
      const bool stepped = (info.flags & CLAP_PARAM_IS_STEPPED) != 0;
      // CLAP normalizes continuous parameters; files store their plain values.
      const auto range = RangeOf(control);
      double fraction = range.minimum + .371 * (range.maximum - range.minimum);
      if (fraction == std::floor(fraction)) fraction += std::min(.25, .5 * (range.maximum - fraction));
      assert(fraction != std::floor(fraction));
      for (int version : {1, kUserVersion})
      {
        auto document = ToJson(effect, values);
        document["version"] = version;
        if (version == 1) document.erase("noiseProfile");
        document["values"][ControlName(control)] = fraction;
        std::vector<Value> parsed;
        const auto error = Parse(document, effect, parsed);
        if (error.empty() == stepped)
          std::cerr << "Module preset domain differs from host: " << info.name << ": " << error << '\n';
        assert(error.empty() != stepped);
        if (!stepped) assert(Apply(effect, parsed, values)[control] == fraction);
      }
      stepped ? ++discrete : ++continuous;
    }
  }
  assert(continuous > 0 && discrete > 0);
  std::cout << "Module preset fractional values checked against host flags: " << continuous
            << " continuous and " << discrete << " discrete controls, formats 1/2\n";
}

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
