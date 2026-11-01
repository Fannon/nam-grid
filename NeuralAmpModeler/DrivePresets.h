#pragma once
#include "SlotDefaults.h"
#include "ParameterLayout.h"
#include "RigPreset.h"
#include <vector>

// Small recipes over the current empty-rig snapshot, so examples acquire every
// new state's defaults without duplicating complete parameter banks in JSON.
namespace drive_presets
{
inline std::vector<nlohmann::json> Make(const nlohmann::json& empty)
{
  std::vector<nlohmann::json> result;
  const auto slot = [](nlohmann::json& rig, int index, SlotEffect effect) {
    rig["SlotEffects"][index] = static_cast<int>(effect);
    const auto values = SlotDefaultValues(effect);
    for (int p = 0; p < kUtilityControls; ++p)
      rig[GridSlotName(index) + " " + kEffectParameters[p].name] = values[17 + p];
  };
  for (int style = 0; style < effect_modes::pedalCount; ++style)
  {
    auto rig = empty;
    rig["PresetName"] = std::string("Drive - ") + effect_modes::pedalNames[style];
    slot(rig, 0, SlotEffect::PedalDrive);
    rig["Pre 1 Pedal Style"] = style;
    if (style == static_cast<int>(effect_modes::PedalStyle::BassEdge))
    {
      rig["Pre 1 Pedal Low Keep"] = 100.;
      rig["Pre 1 Pedal Drive"] = 15.;
      rig["Pre 1 Pedal Output"] = -3.;
    }
    if (style == static_cast<int>(effect_modes::PedalStyle::ParallelOD))
      rig["Pre 1 Pedal Drive"] = 6.;
    if (style == static_cast<int>(effect_modes::PedalStyle::CascadedPreamp))
      rig["Pre 1 Pedal Drive"] = 3.;
    if (style == static_cast<int>(effect_modes::PedalStyle::BluesBreaker)
        || style == static_cast<int>(effect_modes::PedalStyle::TransparentOD))
      rig["Pre 1 Pedal Drive"] = 6.;
    result.push_back(std::move(rig));
  }
  for (int voice = 0; voice < 3; ++voice)
  {
    auto rig = empty;
    slot(rig, 0, SlotEffect::Boost);
    rig["PresetName"] = voice == 0 ? "Boost - Flat" : voice == 1 ? "Boost - Tight" : "Boost - Warm";
    rig["Pre 1 Boost Gain"] = 6.;
    rig["Pre 1 Boost Tilt"] = voice == 0 ? 0. : voice == 1 ? 3. : -2.;
    rig["Pre 1 EQ Low Cut"] = voice == 1 ? 90. : 0.;
    result.push_back(std::move(rig));
  }
  for (int edge = 0; edge < 2; ++edge)
  {
    auto rig = empty;
    slot(rig, 8, SlotEffect::PedalDrive);
    rig["PresetName"] = edge ? "Bass - Split Edge" : "Bass - Split Warm";
    rig["Frequency Split"] = 1.;
    rig["Lane Mode"] = grid_performance::Parallel;
    rig["Split Frequency"] = 180.;
    rig["A/B Blend"] = 50.;
    rig["B 1 Pedal Style"] =
      static_cast<int>(edge ? effect_modes::PedalStyle::BassEdge : effect_modes::PedalStyle::CascadedPreamp);
    rig["B 1 Pedal Drive"] = edge ? 15. : 6.;
    rig["B 1 Pedal Output"] = edge ? -6. : -3.;
    // Lows already travel in A. A second local split would divide B again.
    rig["B 1 Pedal Low Keep"] = 0.;
    result.push_back(std::move(rig));
  }
  return result;
}

// Called on the control thread, only for fresh default folders or an explicit
// export. Preserve all existing files, including user-edited factory examples.
inline void Install(const std::filesystem::path& folder, const nlohmann::json& empty)
{
  std::filesystem::create_directories(folder);
  for (const auto& rig : Make(empty))
  {
    const auto file = folder / (rig.at("PresetName").get<std::string>() + ".ngrid");
    if (!std::filesystem::exists(file))
      rig_preset::Write(file, rig);
  }
}
} // namespace drive_presets
