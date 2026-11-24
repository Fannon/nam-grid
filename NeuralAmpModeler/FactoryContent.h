#pragma once
#include "RigPreset.h"
#include <string>
#include <vector>

namespace factory_content
{
// The startup installer and explicit Settings action share the same recipes.
// Missing files are added; existing files, including user edits, are preserved.
inline std::string Install(const std::filesystem::path& namFolder, const std::filesystem::path& presetFolder,
                           const nlohmann::json& empty, const std::vector<nlohmann::json>& examples)
{
  std::string errors;
  const auto attempt = [&](const auto& install) {
    try { install(); }
    catch (const std::exception& error)
    {
      if (!errors.empty()) errors += '\n';
      errors += error.what();
    }
  };
  if (!namFolder.empty()) attempt([&] { std::filesystem::create_directories(namFolder); });
  if (presetFolder.empty()) return errors;
  attempt([&] { std::filesystem::create_directories(presetFolder); });
  attempt([&] {
    const auto folder = presetFolder / "Drive Examples";
    std::filesystem::create_directories(folder);
    for (const auto& rig : examples)
    {
      const auto file = folder / (rig.at("PresetName").get<std::string>() + ".ngrid");
      if (!std::filesystem::exists(file)) rig_preset::Write(file, rig);
    }
  });
  attempt([&] {
    const auto file = presetFolder / "Empty.ngrid";
    if (!std::filesystem::exists(file))
    {
      auto rig = empty;
      rig["PresetName"] = "Empty";
      rig_preset::Write(file, rig);
    }
  });
  return errors;
}
} // namespace factory_content
