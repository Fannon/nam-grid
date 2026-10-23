#pragma once

// Control/UI-side library only. No inference objects, model weights, or audio-thread access.
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <functional>
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

// Shared by the background worker and deterministic scanner tests.
Capture Inspect(const std::filesystem::path& file, const std::filesystem::path& root);
std::string ContentHash(const std::filesystem::path& file, const std::function<bool()>& cancelled = {});
const Capture* FindHash(const Snapshot& library, const std::string& hash, bool isIR);
std::vector<std::filesystem::path> SiblingFiles(const std::filesystem::path& selected, bool isIR);
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
