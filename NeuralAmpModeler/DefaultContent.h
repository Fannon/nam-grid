#pragma once
#include <span>

// Content a fresh install writes into the new default library folders, so a first start
// is not blank: one clean amp-and-cab NAM capture and a rig preset that uses it. The files
// live in resources/defaults and are embedded at build time (cmake/EmbedFiles.cmake).
namespace default_content
{
// RiffSmith Clean by Matt Hartman, MIT (resources/defaults/riffsmith-LICENSE.txt).
std::span<const unsigned char> CleanCapture();
// The "Clean Default" rig. Its slot paths name bundled captures by filename only; the
// installer points them at the installed files.
std::span<const unsigned char> CleanPreset();
inline constexpr const char* kCleanCaptureName = "riffsmith-clean.nam";
inline constexpr const char* kCleanPresetName = "Clean Default.ngrid";
} // namespace default_content
