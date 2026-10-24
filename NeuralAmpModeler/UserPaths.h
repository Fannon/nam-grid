#pragma once
#include <cstdlib>
#include <filesystem>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#endif

// Per-user locations, following each platform's conventions:
// - settings: %APPDATA%\NAM Grid (Windows), ~/Library/Application Support/NAM Grid (macOS),
//   $XDG_CONFIG_HOME/nam-grid (Linux);
// - default library content: Documents/NAM Grid with NAM Captures, IRs and Presets.
// NAM_GRID_HOME replaces both roots (tests and portable setups), so nothing else on the
// machine is read or written.
namespace user_paths
{
inline std::filesystem::path Environment(const char* name)
{
#ifdef _WIN32
  // The process environment, not this module's CRT copy, so a host or test can set it late.
  const std::wstring wide(name, name + std::char_traits<char>::length(name));
  wchar_t value[32768]{};
  const DWORD length = GetEnvironmentVariableW(wide.c_str(), value, 32768);
  return length > 0 && length < 32768 ? std::filesystem::path(value) : std::filesystem::path{};
#else
  const char* value = std::getenv(name);
  return value && *value ? std::filesystem::u8path(value) : std::filesystem::path{};
#endif
}
inline std::filesystem::path Home() { return Environment("NAM_GRID_HOME"); }
#ifdef _WIN32
inline std::filesystem::path KnownFolder(int id)
{
  wchar_t folder[MAX_PATH]{};
  return SUCCEEDED(SHGetFolderPathW(nullptr, id, nullptr, SHGFP_TYPE_CURRENT, folder)) ? std::filesystem::path(folder)
                                                                                      : std::filesystem::path{};
}
#endif
inline std::filesystem::path ConfigFolder()
{
  if (const auto home = Home(); !home.empty()) return home / "config";
#ifdef _WIN32
  const auto roaming = KnownFolder(CSIDL_APPDATA);
  return roaming.empty() ? roaming : roaming / "NAM Grid";
#elif defined(__APPLE__)
  const auto user = Environment("HOME");
  return user.empty() ? user : user / "Library" / "Application Support" / "NAM Grid";
#else
  if (const auto xdg = Environment("XDG_CONFIG_HOME"); !xdg.empty()) return xdg / "nam-grid";
  const auto user = Environment("HOME");
  return user.empty() ? user : user / ".config" / "nam-grid";
#endif
}
inline std::filesystem::path SettingsFile()
{
  const auto folder = ConfigFolder();
  return folder.empty() ? folder : folder / "settings.json";
}
inline std::filesystem::path ContentFolder()
{
  if (const auto home = Home(); !home.empty()) return home;
#ifdef _WIN32
  const auto documents = KnownFolder(CSIDL_PERSONAL);
#else
  const auto user = Environment("HOME");
  const auto documents = user.empty() ? user : user / "Documents";
#endif
  return documents.empty() ? documents : documents / "NAM Grid";
}
// Builds before the settings folder kept library.json in the local app-support folder.
inline std::filesystem::path LegacySettingsFile()
{
  if (!Home().empty()) return {};
#ifdef _WIN32
  const auto local = KnownFolder(CSIDL_LOCAL_APPDATA);
#elif defined(__APPLE__)
  const auto user = Environment("HOME");
  const auto local = user.empty() ? user : user / "Library" / "Application Support";
#else
  const std::filesystem::path local;
#endif
  return local.empty() ? local : local / "nam-grid" / "library.json";
}
} // namespace user_paths
