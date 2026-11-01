#include "DefaultContent.h"
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

int main(int argc, char** argv)
{
  if (argc != 2) return 1;
  const std::filesystem::path root(argv[1]);
  const auto capture = default_content::CleanCapture();
  if (capture != Read(root / default_content::kCleanCaptureName))
    throw std::runtime_error("Factory capture bytes changed");
  const auto preset = default_content::CleanPreset();
  const auto original = Read(root / default_content::kCleanPresetName);
  if (nlohmann::json::parse(preset) != nlohmann::json::parse(original))
    throw std::runtime_error("Factory preset values changed");
  if (preset.size() >= original.size()) throw std::runtime_error("Factory preset is not minified");
  // Results have independent ownership; decoding again cannot invalidate a caller's data.
  auto second = default_content::CleanCapture();
  second.front() ^= 1;
  if (capture != default_content::CleanCapture()) throw std::runtime_error("Factory data mutated");
  const auto scratch = std::filesystem::temp_directory_path()
    / ("nam-grid-factory-content-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  struct Cleanup
  {
    std::filesystem::path path;
    ~Cleanup() { std::error_code error; std::filesystem::remove_all(path, error); }
  } cleanup{scratch};
  const auto nam = scratch / "NAM Captures", rigs = scratch / "Presets";
  const auto clean = rigs / default_content::kCleanPresetName;
  const auto emptyFile = rigs / "Empty.ngrid", example = rigs / "Drive Examples" / "Example.ngrid";
  const nlohmann::json empty = {{"PresetName", "Test"}}, recipe = {{"PresetName", "Example"}};
  const auto install = [&] { return factory_content::Install(nam, rigs, empty, {recipe}); };
  if (!install().empty() || Read(nam / default_content::kCleanCaptureName) != capture
      || rig_preset::Read(emptyFile)["PresetName"] != "Empty" || rig_preset::Read(example) != recipe)
    throw std::runtime_error("Factory installation is incomplete");
  bool relinked = false;
  const auto installedClean = rig_preset::Read(clean);
  for (const auto& value : installedClean.at("SlotPaths"))
    if (std::filesystem::u8path(value.get<std::string>()).filename() == default_content::kCleanCaptureName)
    {
      if (std::filesystem::u8path(value.get<std::string>()) != nam / default_content::kCleanCaptureName)
        throw std::runtime_error("Installed factory preset was not relinked");
      relinked = true;
    }
  if (!relinked) throw std::runtime_error("Installed factory preset has no capture");
  // A repeated install must retain user-edited content, byte for byte.
  const std::vector<std::filesystem::path> files{nam / default_content::kCleanCaptureName, clean, emptyFile, example};
  for (const auto& file : files) std::ofstream(file, std::ios::binary) << "user-edited content";
  if (!install().empty()) throw std::runtime_error("Repeated installation failed");
  for (const auto& file : files)
  {
    const auto bytes = Read(file);
    if (std::string(bytes.begin(), bytes.end()) != "user-edited content")
      throw std::runtime_error("Factory installation overwrote an existing file");
  }
  std::filesystem::remove(nam / default_content::kCleanCaptureName);
  std::filesystem::remove(example);
  if (!install().empty() || Read(nam / default_content::kCleanCaptureName) != capture
      || rig_preset::Read(example) != recipe)
    throw std::runtime_error("Factory installation did not restore missing content");
  const auto blocked = scratch / "blocked";
  std::ofstream(blocked) << "file, not a folder";
  if (factory_content::Install(blocked, blocked, empty, {recipe}).empty())
    throw std::runtime_error("Factory installation concealed a filesystem error");
  std::cout << "Factory capture bytes and preset values preserved\n";
}
