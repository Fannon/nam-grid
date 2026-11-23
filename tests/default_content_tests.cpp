#include "FactoryContent.h"
#include <json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

std::vector<unsigned char> Read(const std::filesystem::path& path)
{
  std::ifstream stream(path, std::ios::binary);
  if (!stream) throw std::runtime_error("Cannot read factory fixture");
  return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

int main()
{
  const auto scratch = std::filesystem::temp_directory_path()
    / ("nam-grid-factory-content-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  struct Cleanup
  {
    std::filesystem::path path;
    ~Cleanup() { std::error_code error; std::filesystem::remove_all(path, error); }
  } cleanup{scratch};
  const auto nam = scratch / "NAM Captures", rigs = scratch / "Presets";
  const auto clean = rigs / "Clean Default.ngrid", userCapture = nam / "my-capture.nam";
  const auto emptyFile = rigs / "Empty.ngrid", example = rigs / "Drive Examples" / "Example.ngrid";
  const nlohmann::json empty = {{"PresetName", "Test"}}, recipe = {{"PresetName", "Example"}};
  const auto install = [&] { return factory_content::Install(nam, rigs, empty, {recipe}); };
  if (!install().empty() || !std::filesystem::is_empty(nam)
      || rig_preset::Read(emptyFile)["PresetName"] != "Empty" || rig_preset::Read(example) != recipe)
    throw std::runtime_error("Factory installation is incomplete");
  // A repeated install must retain user-edited content, byte for byte.
  if (std::filesystem::exists(clean)) throw std::runtime_error("Legacy preset installed on fresh start");
  const std::vector<std::filesystem::path> files{userCapture, clean, emptyFile, example};
  for (const auto& file : files) std::ofstream(file, std::ios::binary) << "user-edited content";
  if (!install().empty()) throw std::runtime_error("Repeated installation failed");
  for (const auto& file : files)
  {
    const auto bytes = Read(file);
    if (std::string(bytes.begin(), bytes.end()) != "user-edited content")
      throw std::runtime_error("Factory installation overwrote an existing file");
  }
  std::filesystem::remove(example);
  if (!install().empty() || Read(userCapture).empty()
      || rig_preset::Read(example) != recipe)
    throw std::runtime_error("Factory installation did not restore missing content");
  const auto blocked = scratch / "blocked";
  std::ofstream(blocked) << "file, not a folder";
  if (factory_content::Install(blocked, blocked, empty, {recipe}).empty())
    throw std::runtime_error("Factory installation concealed a filesystem error");
  std::cout << "Factory rigs install without captures and preserve existing files\n";
}
