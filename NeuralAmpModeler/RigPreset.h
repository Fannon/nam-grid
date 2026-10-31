#pragma once
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>
#include "json.hpp"
#ifdef _WIN32
#include <windows.h>
#endif

namespace rig_preset
{
// Bounds both preset files and host project state, including fully divergent scenes.
inline constexpr size_t kMaximumBytes = 8 * 1024 * 1024;
inline nlohmann::json Read(const std::filesystem::path& path)
{
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) throw std::runtime_error("Cannot open the preset file.");
  const auto size = input.tellg();
  if (size <= 0 || size > kMaximumBytes) throw std::runtime_error("Preset is empty or exceeds 8 MiB.");
  std::string text(static_cast<size_t>(size), '\0');
  input.seekg(0);
  if (!input.read(text.data(), text.size())) throw std::runtime_error("Cannot read the preset file.");
  const auto document = nlohmann::json::parse(text, [](int depth, nlohmann::json::parse_event_t, nlohmann::json&) {
    if (depth > 128) throw std::runtime_error("Preset nesting exceeds the supported limit.");
    return true;
  });
  if (!document.is_object() || document.value("format", "") != "nam-grid-preset"
      || !document.contains("version") || !document["version"].is_number_integer() || document["version"] != 1
      || !document.contains("rig") || !document["rig"].is_object())
    throw std::runtime_error("This is not a supported NAM Grid preset.");
  return document["rig"];
}

// Writes a sibling temporary file, then replaces the destination in one step, so
// readers never see partial JSON. Concurrent writers resolve as last-writer-wins.
inline void WriteAtomically(const std::filesystem::path& path, const std::string& text)
{
  static std::atomic<unsigned> sequence{0};
  auto temporary = path;
  temporary += ".tmp-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
               + "-" + std::to_string(sequence++);
  try
  {
    {
      std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
      if (!output || !output.write(text.data(), text.size()) || !output.flush())
        throw std::runtime_error("Cannot write the file.");
      output.close();
      if (!output) throw std::runtime_error("Cannot finish writing the file.");
    }
#ifdef _WIN32
    // Readers opened without FILE_SHARE_DELETE (including standard streams) can
    // briefly prevent replacement. Retry off the audio thread, keeping the old
    // file intact; persistent locks and other errors must still fail promptly.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
    while (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
      const auto error = GetLastError();
      if ((error != ERROR_SHARING_VIOLATION && error != ERROR_ACCESS_DENIED)
          || std::chrono::steady_clock::now() >= deadline)
        throw std::runtime_error("Cannot replace the file (Windows error " + std::to_string(error) + ").");
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
#else
    std::filesystem::rename(temporary, path);
#endif
  }
  catch (...)
  {
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    throw;
  }
}

inline void Write(const std::filesystem::path& path, const nlohmann::json& rig)
{
  if (!rig.is_object()) throw std::runtime_error("Invalid rig state.");
  const auto text = nlohmann::json{{"format", "nam-grid-preset"}, {"version", 1}, {"rig", rig}}.dump(2);
  if (text.size() > kMaximumBytes) throw std::runtime_error("Preset exceeds 8 MiB.");
  WriteAtomically(path, text);
}
} // namespace rig_preset
