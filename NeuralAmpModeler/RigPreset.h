#pragma once
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include "json.hpp"
#ifdef _WIN32
#include <windows.h>
#endif

namespace rig_preset
{
inline constexpr size_t kMaximumBytes = 1024 * 1024;
inline nlohmann::json Read(const std::filesystem::path& path)
{
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) throw std::runtime_error("Cannot open the preset file.");
  const auto size = input.tellg();
  if (size <= 0 || size > kMaximumBytes) throw std::runtime_error("Preset is empty or exceeds 1 MiB.");
  std::string text(static_cast<size_t>(size), '\0');
  input.seekg(0);
  if (!input.read(text.data(), text.size())) throw std::runtime_error("Cannot read the preset file.");
  const auto document = nlohmann::json::parse(text);
  if (!document.is_object() || document.value("format", "") != "nam-grid-preset"
      || !document.contains("version") || !document["version"].is_number_integer() || document["version"] != 1
      || !document.contains("rig") || !document["rig"].is_object())
    throw std::runtime_error("This is not a supported nam-grid preset.");
  return document["rig"];
}

inline void Write(const std::filesystem::path& path, const nlohmann::json& rig)
{
  if (!rig.is_object()) throw std::runtime_error("Invalid rig state.");
  const auto text = nlohmann::json{{"format", "nam-grid-preset"}, {"version", 1}, {"rig", rig}}.dump(2);
  if (text.size() > kMaximumBytes) throw std::runtime_error("Preset exceeds 1 MiB.");
  static std::atomic<unsigned> sequence{0};
  auto temporary = path;
  temporary += ".tmp-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
               + "-" + std::to_string(sequence++);
  try
  {
    {
      std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
      if (!output || !output.write(text.data(), text.size()) || !output.flush())
        throw std::runtime_error("Cannot write the preset file.");
      output.close();
      if (!output) throw std::runtime_error("Cannot finish writing the preset file.");
    }
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
      throw std::runtime_error("Cannot replace the preset file.");
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
} // namespace rig_preset
