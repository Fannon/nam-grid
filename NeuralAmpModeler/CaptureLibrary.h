#pragma once

// Control/UI-side library only. No inference objects, model weights, or audio-thread access.
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace nam_library
{
// SlotPreset is reserved for a future reader/loader. It is never offered by today's browser.
enum class FileKind { NAM, IR, RigPreset, SlotPreset, Module };
const char* KindName(FileKind kind);
enum class Gear
{
  Unknown,
  AmpCab,
  AmpHead,
  Cabinet,
  Pedal,
  Outboard,
  Spaces
};
const char* GearName(Gear gear);
std::string PathText(const std::filesystem::path& path);
// Each source list indexes only its own file kind; a folder may appear in several lists.
struct Settings
{
  std::vector<std::string> namFolders;
  bool rescanOnStartup = true;
  bool showVisualizations = true;
  std::vector<std::string> presetFolders;
  std::vector<std::string> irFolders;
  bool presetCrossfade = false; // Rig preset switches crossfade instead of a short dip.
};
Settings ReadSettings(const std::filesystem::path& settingsFile);
bool WriteSettings(const std::filesystem::path& settingsFile, const Settings& settings);
std::vector<std::string> ReadFolderSettings(const std::filesystem::path& settingsFile);
bool WriteFolderSettings(const std::filesystem::path& settingsFile, const std::vector<std::string>& folders);
std::vector<std::string> NormalizeFolders(const std::vector<std::string>& folders);
std::string ReadFolderSetting(const std::filesystem::path& settingsFile);
bool WriteFolderSetting(const std::filesystem::path& settingsFile, const std::string& folder);

struct Capture
{
  std::filesystem::path path, relative, source;
  std::string name, maker, model;
  std::string hash;
  size_t folderDepth = 0;
  Gear gear = Gear::Unknown;
  bool isIR = false;
  bool inferred = false; // Filename/folder hint, rather than an explicit metadata category.
  FileKind kind = FileKind::NAM;
  int module = -1; // Built-in module ID; populated by the UI catalog, never by a file reader.
};

struct Filter
{
  std::string search;
  int gear = -1; // All categories; otherwise a Gear value, including Unknown.
  bool Matches(const Capture& capture) const;
};

struct Snapshot
{
  std::filesystem::path root;
  std::vector<Capture> captures;
  std::string error;
  size_t unreadable = 0;
  // Byte-identical entries are all kept and listed; this only counts the repeats.
  size_t duplicates = 0;
};

// `.ignore` files (also `.ignore.txt`, as Windows Explorer names new text files) exclude
// files and folders from a library folder like .gitignore: one pattern per line, `#`
// comments, `!` re-includes, a trailing `/` matches folders only, any other `/` anchors
// the pattern to the .ignore's folder, and `*`, `?`, `[...]` and `**` wildcards. Matching
// ignores case. A file applies to its folder and subfolders within `root`; later and
// deeper patterns win, and a file inside an excluded folder cannot be re-included.
class IgnoreRules
{
public:
  explicit IgnoreRules(std::filesystem::path root) : mRoot(std::move(root)) {}
  // Whether `path` (inside root) or one of its folders below root is excluded.
  bool Ignored(const std::filesystem::path& path, bool directory);
  // Only the entry itself, for scans that never descend into excluded folders.
  bool Matches(const std::filesystem::path& path, bool directory);

private:
  struct Pattern
  {
    std::vector<std::string> segments;
    bool negate = false, directoryOnly = false;
  };
  const std::vector<Pattern>& Patterns(const std::filesystem::path& folder);
  std::filesystem::path mRoot;
  std::map<std::filesystem::path, std::vector<Pattern>> mFolders;
};

// Shared by the background worker and deterministic scanner tests.
Capture Inspect(const std::filesystem::path& file, const std::filesystem::path& root);
std::string ContentHash(const std::filesystem::path& file, const std::function<bool()>& cancelled = {});
const Capture* FindHash(const Snapshot& library, const std::string& hash, bool isIR);
// Files of the same kind in the selected file's folder, by name. Files that the .ignore
// rules of a containing library folder in `roots` exclude are left out.
std::vector<std::filesystem::path> SiblingFiles(const std::filesystem::path& selected, bool isIR,
                                                const std::vector<std::string>& roots = {});
Snapshot Scan(const std::filesystem::path& root, const std::function<bool()>& cancelled = {});

// Scans roots for both NAM captures and IRs.
Snapshot ScanFolders(const std::vector<std::string>& roots, const std::function<bool()>& cancelled = {});
Snapshot ScanLibrary(const std::vector<std::string>& namRoots, const std::vector<std::string>& irRoots,
                     const std::vector<std::string>& presetRoots, const std::function<bool()>& cancelled = {});

// Latest request wins. Results are handed back to the control thread, never to ProcessBlock.
class Worker
{
public:
  Worker();
  ~Worker();
  // Single-list requests index NAM captures and IRs together.
  void Request(const std::string& root);
  void Request(const std::vector<std::string>& roots);
  void Request(const std::vector<std::string>& namRoots, const std::vector<std::string>& irRoots,
               const std::vector<std::string>& presetRoots);
  std::shared_ptr<const Snapshot> TakeResult();

private:
  void Run();
  std::mutex mMutex;
  std::condition_variable mChanged;
  std::vector<std::string> mNamRoots, mIrRoots, mPresetRoots;
  std::shared_ptr<const Snapshot> mResult;
  std::atomic<unsigned> mRevision{0};
  std::atomic<bool> mStop{false};
  std::thread mThread;
};
} // namespace nam_library
