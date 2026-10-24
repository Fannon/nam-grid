#pragma once
#include <span>

// Linux CLAP plugins are single files without a resource bundle, so the editor font is
// embedded at build time (cmake/EmbedFiles.cmake). Windows loads it from resources/main.rc
// and macOS from the plugin bundle.
namespace embedded_font
{
std::span<const unsigned char> Roboto();
} // namespace embedded_font
