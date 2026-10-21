#pragma once
#include "RigPreset.h"

void TestRigPresetFiles(const std::filesystem::path& root)
{
  const auto file = root / std::filesystem::u8path("Clean r\xc3\xa9verb.ngrid");
  const nlohmann::json rig{{"schema", 12}, {"SlotPaths", {"amp.nam", "cabinet.wav"}},
                           {"Global Input", -3.}, {"Output", -8.}, {"PerformanceScenes", {nlohmann::json::object()}}};
  rig_preset::Write(file, rig);
  assert(rig_preset::Read(file) == rig);
  auto changed = rig; changed["Output"] = -12.;
  rig_preset::Write(file, changed); // Replacing an existing preset is supported.
  assert(rig_preset::Read(file) == changed);
  bool rejected = false;
  try { rig_preset::Write(file, nlohmann::json{{"too large", std::string(rig_preset::kMaximumBytes, 'x')}}); }
  catch (const std::exception&) { rejected = true; }
  assert(rejected && rig_preset::Read(file) == changed); // Failed saves preserve the old file.
  const auto invalid = root / "invalid.ngrid";
  for (const char* text : {"", "{", "[]", "{\"format\":\"other\",\"version\":1,\"rig\":{}}",
                          "{\"format\":\"nam-grid-preset\",\"version\":2,\"rig\":{}}",
                          "{\"format\":\"nam-grid-preset\",\"version\":1,\"rig\":[]}"})
  {
    std::ofstream(invalid) << text;
    rejected = false;
    try { rig_preset::Read(invalid); } catch (const std::exception&) { rejected = true; }
    assert(rejected);
  }
  std::ofstream(invalid) << std::string(rig_preset::kMaximumBytes + 1, ' ');
  rejected = false;
  try { rig_preset::Read(invalid); } catch (const std::exception&) { rejected = true; }
  assert(rejected);
  for (const auto& entry : std::filesystem::directory_iterator(root))
    assert(entry.path().filename().string().find(".tmp-") == std::string::npos);
}
