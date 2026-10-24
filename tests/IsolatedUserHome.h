#pragma once
#include <cassert>
#include <filesystem>
#include <string>
#ifdef _WIN32
  #include <windows.h>
#else
  #include <cstdlib>
  #include <unistd.h>
#endif

// Points the plugin's settings, cache and default folders at a private, empty home before the
// plugin loads (NAM_GRID_HOME, see UserPaths.h), so a test or tool never reads or writes the
// user's real configuration or library folders, even if it aborts. Every program that loads
// the plugin creates one first.
struct IsolatedUserHome
{
  std::filesystem::path home, file;
  explicit IsolatedUserHome(const std::string& name)
  : home(std::filesystem::temp_directory_path() / (name + "-" + std::to_string(ProcessID())))
  , file(home / "config" / "settings.json")
  {
    std::error_code error;
    std::filesystem::remove_all(home, error);
    std::filesystem::create_directories(home);
#ifdef _WIN32
    // The process environment, which the plugin reads, not this module's CRT copy.
    const bool set = SetEnvironmentVariableW(L"NAM_GRID_HOME", home.c_str());
#else
    const bool set = setenv("NAM_GRID_HOME", home.c_str(), 1) == 0;
#endif
    assert(set);
    (void)set;
  }
  ~IsolatedUserHome()
  {
    std::error_code error;
    std::filesystem::remove_all(home, error);
  }

private:
  static unsigned long ProcessID()
  {
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return static_cast<unsigned long>(getpid());
#endif
  }
};
