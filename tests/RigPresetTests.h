#pragma once
#include "RigPreset.h"
#include <thread>

#ifdef _WIN32
inline void TestAtomicWriteWithReader(const std::filesystem::path& root)
{
  const auto file = root / "reader-contention.json";
  rig_preset::WriteAtomically(file, "{\"old\":true}");
  // Ordinary Windows readers allow reads/writes but deny replacing their open file.
  const auto reader = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  assert(reader != INVALID_HANDLE_VALUE);
  std::atomic<bool> started{false};
  bool saved = false;
  std::thread writer([&] {
    started = true;
    try { rig_preset::WriteAtomically(file, "{\"new\":true}"); saved = true; }
    catch (const std::exception&) {}
  });
  while (!started.load()) std::this_thread::yield();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  assert(CloseHandle(reader));
  writer.join();
  assert(saved); // A brief reader must not permanently discard the completed save.
  assert(nlohmann::json::parse(std::ifstream(file)).contains("new"));

  const auto locked = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  assert(locked != INVALID_HANDLE_VALUE);
  bool rejected = false;
  const auto start = std::chrono::steady_clock::now();
  try { rig_preset::WriteAtomically(file, "{\"lost\":true}"); }
  catch (const std::exception&) { rejected = true; }
  assert(std::chrono::steady_clock::now() - start < std::chrono::seconds(2));
  assert(CloseHandle(locked));
  assert(rejected && nlohmann::json::parse(std::ifstream(file)).contains("new"));
  for (const auto& entry : std::filesystem::directory_iterator(root))
    assert(entry.path().filename().string().find(".tmp-") == std::string::npos);
}
#endif

void TestRigPresetFiles(const std::filesystem::path& root)
{
#ifdef _WIN32
  TestAtomicWriteWithReader(root);
#endif
  const auto file = root / std::filesystem::u8path("Clean r\xc3\xa9verb.ngrid");
  const nlohmann::json rig{{"schema", 12}, {"SlotPaths", {"amp.nam", "cabinet.wav"}},
                           {"Input", -3.}, {"Output", -8.}, {"PerformanceScenes", {nlohmann::json::object()}}};
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
  std::ofstream(invalid) << std::string(140, '[') + "0" + std::string(140, ']');
  rejected = false;
  try { rig_preset::Read(invalid); }
  catch (const std::runtime_error& error) { rejected = std::string(error.what()).find("nesting") != std::string::npos; }
  assert(rejected);
  for (const auto& entry : std::filesystem::directory_iterator(root))
    assert(entry.path().filename().string().find(".tmp-") == std::string::npos);
}
