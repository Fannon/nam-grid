#include "DefaultContent.h"
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
  std::cout << "Factory capture bytes and preset values preserved\n";
}
