#pragma once

// Control/UI-side library only. No inference objects, model weights, or audio-thread access.
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
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
  // Whether each list was ever set; an explicitly emptied list stays empty.
  bool namChosen = false, irChosen = false, presetChosen = false;
  bool showMainMacros = true;
};
Settings ReadSettings(const std::filesystem::path& settingsFile);
struct UserSettings
{
  Settings settings;
  // The default NAM Captures and Presets folders, when they were just adopted.
  std::filesystem::path newNamFolder, newPresetFolder;
  bool saved = true;
};
// Loads the per-user settings. On first use it migrates a legacy file, then gives every list
// that was never chosen its default folder below contentFolder (NAM Captures, IRs, Presets),
// creating those folders, and writes the settings file, creating its folder.
UserSettings PrepareUserSettings(const std::filesystem::path& settingsFile, const std::filesystem::path& legacyFile,
                                 const std::filesystem::path& contentFolder);
bool WriteSettings(const std::filesystem::path& settingsFile, const Settings& settings);
std::vector<std::string> ReadFolderSettings(const std::filesystem::path& settingsFile);
bool WriteFolderSettings(const std::filesystem::path& settingsFile, const std::vector<std::string>& folders);
std::vector<std::string> NormalizeFolders(const std::vector<std::string>& folders);
std::string ReadFolderSetting(const std::filesystem::path& settingsFile);
bool WriteFolderSetting(const std::filesystem::path& settingsFile, const std::string& folder);

struct LibraryHint
{
  std::string key, value;
  bool fromFolder = false;
  std::string aliases; // Search-only spellings; never displayed as extra tags.
};
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
  std::vector<LibraryHint> hints; // Derived on inspect/index read; never persisted as metadata.
};
std::vector<LibraryHint> InferFilenameHints(const Capture& capture);
std::vector<LibraryHint> InferLibraryHints(const Capture& capture);
std::string HintText(const Capture& capture, const std::string& key = {}, int origin = -1);
std::string HintSearchText(const Capture& capture);

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

// Browser stars are a personal mark on a module, capture, IR or rig preset, kept in
// favorites.json beside the user's presets rather than in a rig. Levels cycle yellow, orange,
// red, then back to none.
inline constexpr int kStarLevels = 3;
struct Star
{
  int level = 0;
  std::string name; // The file or module name when starred: a hint for people reading the file.
};
using Stars = std::map<std::string, Star>;
// Keys hold no machine paths where avoidable, so a synced favorites.json works on every computer:
// "Module:<id>" (append-only IDs survive renames); "NAM:<sha1>" and "IR:<sha1>" by content, so a
// moved, renamed or duplicated file shares one star; "RigPreset:<path>", relative to the
// favorites folder when the preset lives there and absolute otherwise, because a preset's
// content changes with every save. Empty when the file has no content hash yet: no star.
std::string StarKey(const Capture& file, const std::filesystem::path& favoritesFolder);
int NextStar(int level);
// favorites.json in the first preset folder (where Save as starts), or in the config folder
// when no preset folder is set.
std::filesystem::path FavoritesFolder(const std::vector<std::string>& presetFolders);
std::filesystem::path FavoritesFile(const std::vector<std::string>& presetFolders,
                                    const std::filesystem::path& configFolder);
// A missing file reads as no stars and a damaged entry costs only itself. A file that exists
// but cannot be read or parsed returns nothing, so callers never write over it.
std::optional<Stars> ReadFavorites(const std::filesystem::path& file);
bool WriteFavorites(const std::filesystem::path& file, const Stars& stars);
const Capture* FindHash(const Snapshot& library, const std::string& hash, bool isIR);
// Files of the same kind in the selected file's folder, by name. Files that the .ignore
// rules of a containing library folder in `roots` exclude are left out.
std::vector<std::filesystem::path> SiblingFiles(const std::filesystem::path& selected, bool isIR,
                                                const std::vector<std::string>& roots = {});
Snapshot Scan(const std::filesystem::path& root, const std::function<bool()>& cancelled = {});

// Scans roots for both NAM captures and IRs.
Snapshot ScanFolders(const std::vector<std::string>& roots, const std::function<bool()>& cancelled = {});
// What a scan learned about each file, keyed by path. A file whose size and modification time
// still match is not opened or hashed again. Stored as JSON in the cache folder; a missing or
// damaged index only costs one full scan. Bump kIndexVersion when Inspect, gear inference or the
// content hash changes, so older entries are ignored.
inline constexpr int kIndexVersion = 1;
struct IndexEntry
{
  std::uintmax_t size = 0;
  long long modified = 0;
  Capture capture; // Without root-relative fields; those depend on the folder that found it.
};
using IndexEntries = std::unordered_map<std::string, IndexEntry>;
struct IndexCache
{
  IndexEntries previous; // Read from the file or the last scan.
  IndexEntries next;     // Filled by the current scan: exactly the files it found.
};
IndexEntries ReadIndex(const std::filesystem::path& file);
bool WriteIndex(const std::filesystem::path& file, const IndexEntries& entries);
Snapshot ScanLibrary(const std::vector<std::string>& namRoots, const std::vector<std::string>& irRoots,
                     const std::vector<std::string>& presetRoots, const std::function<bool()>& cancelled = {},
                     IndexCache* index = nullptr);

// Latest request wins. Results are handed back to the control thread, never to ProcessBlock.
class Worker
{
public:
  // With an index file, scans reuse and refresh it; without one, every scan reads every file.
  using Scanner = std::function<decltype(ScanLibrary)>;
  explicit Worker(std::filesystem::path indexFile = {}, Scanner scanner = ScanLibrary);
  ~Worker();
  // Single-list requests index NAM captures and IRs together.
  void Request(const std::string& root);
  void Request(const std::vector<std::string>& roots);
  // With rescan=false, restore the selected roots from the index on the worker. A missing,
  // invalid, empty or factory-only index always falls back to a scan of all selected roots.
  void Request(const std::vector<std::string>& namRoots, const std::vector<std::string>& irRoots,
               const std::vector<std::string>& presetRoots, bool rescan = true,
               const std::vector<std::string>& factoryFiles = {});
  // Abandon a pending or running scan without scanning empty roots or rewriting the index.
  void Cancel();
  std::shared_ptr<const Snapshot> TakeResult();

private:
  void Run();
  std::mutex mMutex;
  std::condition_variable mChanged;
  std::vector<std::string> mNamRoots, mIrRoots, mPresetRoots;
  std::vector<std::string> mFactoryFiles;
  bool mRescan = true;
  std::shared_ptr<const Snapshot> mResult;
  std::atomic<unsigned> mRevision{0};
  std::atomic<bool> mStop{false};
  bool mCancelRequested = false; // Guarded by mMutex.
  std::filesystem::path mIndexFile;
  IndexCache mIndex; // Worker thread only.
  bool mIndexLoaded = false;
  Scanner mScanner;
  std::thread mThread;
};
} // namespace nam_library
