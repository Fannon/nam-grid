#pragma once
// NAM Grid for MPC: fixed library folders instead of the desktop library browser.
//
// Captures (.nam) and impulse responses (.wav) are found in two places:
//   user:    <drive>/NAM Grid/Captures and <drive>/NAM Grid/IRs, on the drive that holds
//            the plugin's "VST2 Plugins" folder (or $NAM_GRID_MPC_LIBRARY when set);
//   factory: Content/Captures and Content/IRs inside the plugin folder.
// Subfolders are searched. Entries are saved by root and relative path, so adding files
// never changes which file a project uses.
//
// On Linux the scan uses the 64-bit POSIX directory calls: the 32-bit ARM C++ library's
// std::filesystem uses 32-bit stat, which fails on file systems with large inode numbers.
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>
#if defined(__linux__)
  #include <dirent.h>
  #include <sys/stat.h>
#endif

namespace mpc_grid
{
struct LibraryEntry
{
  std::string root;     // "user" or "factory"
  std::string relative; // Path below the root's folder with '/' separators, e.g. "Fender/Twin.nam"
  std::string path;     // Absolute path to load
  std::string display;  // File name without extension
};
struct LibraryRoot
{
  std::string key;
  std::string folder; // Holds Captures/ and IRs/ (UTF-8)
};

inline std::string Lower(std::string s)
{
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}
inline std::string Stem(const std::string& name)
{
  const auto slash = name.find_last_of("/\\");
  const auto file = slash == std::string::npos ? name : name.substr(slash + 1);
  const auto dot = file.find_last_of('.');
  return dot == std::string::npos || dot == 0 ? file : file.substr(0, dot);
}
// "Packs/Fender/Twin.nam" -> "Packs/Fender"; "Twin.nam" -> "".
inline std::string FolderOf(const std::string& relative)
{
  const auto slash = relative.find_last_of('/');
  return slash == std::string::npos ? std::string() : relative.substr(0, slash);
}
inline bool HasExtension(const std::string& name, const char* extension)
{
  const std::string ext = extension;
  return name.size() > ext.size() && Lower(name.substr(name.size() - ext.size())) == ext;
}

// <drive>/NAM Grid for a plugin folder at <drive>/VST2 Plugins/<plugin folder> (UTF-8 paths).
inline std::string DriveLibraryFolder(std::string pluginFolder)
{
  while (!pluginFolder.empty() && (pluginFolder.back() == '/' || pluginFolder.back() == '\\')) pluginFolder.pop_back();
  const auto pluginSlash = pluginFolder.find_last_of("/\\");
  if (pluginSlash == std::string::npos) return {};
  const auto parent = pluginFolder.substr(0, pluginSlash);
  const auto parentSlash = parent.find_last_of("/\\");
  const auto parentName = parentSlash == std::string::npos ? parent : parent.substr(parentSlash + 1);
  if (Lower(parentName) != "vst2 plugins" || parentSlash == std::string::npos) return {};
  return parent.substr(0, parentSlash) + "/NAM Grid";
}

inline constexpr size_t kMaxLibraryEntries = 2048;
inline constexpr int kMaxLibraryDepth = 8;

// Appends every file with `extension` below `folder` (relative paths from `prefix`).
inline void ScanFolder(const std::string& folder, const std::string& prefix, const char* extension, int depth,
                       std::vector<std::pair<std::string, std::string>>& found)
{
  if (depth > kMaxLibraryDepth || found.size() >= kMaxLibraryEntries) return;
#if defined(__linux__)
  DIR* dir = opendir(folder.c_str());
  if (!dir) return;
  while (const auto* entry = readdir64(dir))
  {
    if (found.size() >= kMaxLibraryEntries) break;
    const std::string name = entry->d_name;
    // Skip hidden files and folders, including macOS "._" resource forks on FAT drives.
    if (name.empty() || name[0] == '.') continue;
    const auto path = folder + "/" + name;
    bool directory = entry->d_type == DT_DIR, regular = entry->d_type == DT_REG;
    if (entry->d_type == DT_UNKNOWN || entry->d_type == DT_LNK)
    {
      struct stat64 st;
      if (stat64(path.c_str(), &st) != 0) continue;
      directory = S_ISDIR(st.st_mode);
      regular = S_ISREG(st.st_mode);
    }
    if (directory) ScanFolder(path, prefix + name + "/", extension, depth + 1, found);
    else if (regular && HasExtension(name, extension)) found.emplace_back(prefix + name, path);
  }
  closedir(dir);
#else
  std::error_code ec;
  for (std::filesystem::directory_iterator it(std::filesystem::u8path(folder), ec), end; !ec && it != end; it.increment(ec))
  {
    if (found.size() >= kMaxLibraryEntries) break;
    const auto name = it->path().filename().u8string();
    if (name.empty() || name[0] == '.') continue;
    const auto path = folder + "/" + std::string(name.begin(), name.end());
    const std::string relative = prefix + std::string(name.begin(), name.end());
    if (it->is_directory(ec)) ScanFolder(path, relative + "/", extension, depth + 1, found);
    else if (it->is_regular_file(ec) && HasExtension(relative, extension)) found.emplace_back(relative, path);
  }
#endif
}

// Lists one kind of file below every root, user files first, each sorted by path.
inline std::vector<LibraryEntry> ScanLibrary(const std::vector<LibraryRoot>& roots, const char* subfolder,
                                             const char* extension)
{
  std::vector<LibraryEntry> result;
  for (const auto& root : roots)
  {
    if (root.folder.empty()) continue;
    std::vector<std::pair<std::string, std::string>> found;
    ScanFolder(root.folder + "/" + subfolder, "", extension, 0, found);
    // By folder, then file name, so each folder's files stay together.
    std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) {
      const auto da = Lower(FolderOf(a.first)), db = Lower(FolderOf(b.first));
      return da != db ? da < db : Lower(a.first) < Lower(b.first);
    });
    for (auto& [relative, path] : found)
      if (result.size() < kMaxLibraryEntries) result.push_back({root.key, relative, path, Stem(relative)});
  }
  return result;
}

// The folders that hold files, in list order. Entries of one folder are contiguous.
struct LibraryFolder
{
  std::string root, folder, display;
  int first = 0, count = 0;
};
inline std::string FolderDisplay(const std::string& root, const std::string& folder)
{
  if (folder.empty()) return root == "factory" ? "Factory" : "Main";
  // The last two levels fit the MPC screen and keep same-named subfolders apart.
  const auto last = folder.find_last_of('/');
  const auto before = last == std::string::npos ? std::string::npos : folder.find_last_of('/', last - 1);
  const auto shown = before == std::string::npos ? folder : folder.substr(before + 1);
  return root == "factory" ? "Factory/" + shown : shown;
}
inline std::vector<LibraryFolder> LibraryFolders(const std::vector<LibraryEntry>& entries)
{
  std::vector<LibraryFolder> folders;
  for (int i = 0; i < static_cast<int>(entries.size()); ++i)
  {
    const auto folder = FolderOf(entries[i].relative);
    if (folders.empty() || folders.back().root != entries[i].root || folders.back().folder != folder)
      folders.push_back({entries[i].root, folder, FolderDisplay(entries[i].root, folder), i, 0});
    ++folders.back().count;
  }
  return folders;
}
inline int FolderOfEntry(const std::vector<LibraryFolder>& folders, int entry)
{
  for (int f = 0; f < static_cast<int>(folders.size()); ++f)
    if (entry >= folders[f].first && entry < folders[f].first + folders[f].count) return f;
  return -1;
}

inline int FindEntry(const std::vector<LibraryEntry>& entries, const std::string& root, const std::string& relative)
{
  for (size_t i = 0; i < entries.size(); ++i)
    if (entries[i].root == root && entries[i].relative == relative) return static_cast<int>(i);
  return -1;
}

// Reads a whole file through stdio (no stat), or returns false.
inline bool ReadWholeFile(const std::string& path, std::string& out)
{
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  out.clear();
  char buffer[65536];
  size_t n;
  while ((n = std::fread(buffer, 1, sizeof buffer, f)) > 0)
  {
    out.append(buffer, n);
    if (out.size() > (256u << 20)) break; // Captures are a few MB; refuse anything absurd.
  }
  const bool ok = !std::ferror(f) && out.size() <= (256u << 20);
  std::fclose(f);
  return ok;
}

// A File parameter covers the list from 0 to 1, one equal step per file.
inline int IndexFromNormalized(double norm, size_t count)
{
  if (count == 0) return -1;
  return std::clamp(static_cast<int>(std::lround(std::clamp(norm, 0., 1.) * static_cast<double>(count - 1))), 0,
                    static_cast<int>(count) - 1);
}
inline double NormalizedFromIndex(int index, size_t count)
{
  return count > 1 && index > 0 ? std::min(1., static_cast<double>(index) / static_cast<double>(count - 1)) : 0.;
}
} // namespace mpc_grid
