#pragma once
#include "DefaultContent.h"
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
  const auto capture = namFolder.empty() ? namFolder : namFolder / default_content::kCleanCaptureName;
  if (!capture.empty()) attempt([&] {
    std::filesystem::create_directories(namFolder);
    if (!std::filesystem::exists(capture))
    {
      const auto bytes = default_content::CleanCapture();
      std::ofstream output;
      output.exceptions(std::ios::failbit | std::ios::badbit);
      output.open(capture, std::ios::binary);
      output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
      output.close();
    }
  });
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
  attempt([&] {
    const auto file = presetFolder / default_content::kCleanPresetName;
    if (capture.empty() || !std::filesystem::is_regular_file(capture) || std::filesystem::exists(file)) return;
    const auto bytes = default_content::CleanPreset();
    auto document = nlohmann::json::parse(bytes.begin(), bytes.end());
    auto& rig = document.at("rig");
    for (auto& path : rig.at("SlotPaths"))
    {
      const auto name = path.get<std::string>();
      if (!name.empty() && !std::filesystem::u8path(name).has_parent_path())
      {
        const auto installed = (namFolder / std::filesystem::u8path(name)).u8string();
        path = std::string(reinterpret_cast<const char*>(installed.data()), installed.size());
      }
    }
    rig_preset::Write(file, rig);
  });
  return errors;
}
} // namespace factory_content
