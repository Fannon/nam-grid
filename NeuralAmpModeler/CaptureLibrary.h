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
struct Settings
{
  std::vector<std::string> folders;
  bool rescanOnStartup = true;
  bool showVisualizations = true;
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
  std::filesystem::path path, relative;
  std::string name, maker, model;
  std::string hash;
  size_t folderDepth = 0;
  Gear gear = Gear::Unknown;
  bool isIR = false;
  bool inferred = false; // Filename/folder hint, rather than an explicit metadata category.
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
  size_t duplicates = 0;
};

// Shared by the background worker and deterministic scanner tests.
Capture Inspect(const std::filesystem::path& file, const std::filesystem::path& root);
std::string ContentHash(const std::filesystem::path& file, const std::function<bool()>& cancelled = {});
const Capture* FindHash(const Snapshot& library, const std::string& hash, bool isIR);
std::vector<std::filesystem::path> SiblingFiles(const std::filesystem::path& selected, bool isIR);
Snapshot Scan(const std::filesystem::path& root, const std::function<bool()>& cancelled = {});

Snapshot ScanFolders(const std::vector<std::string>& roots, const std::function<bool()>& cancelled = {});

// Latest request wins. Results are handed back to the control thread, never to ProcessBlock.
class Worker
{
public:
  Worker();
  ~Worker();
  void Request(const std::string& root);
  void Request(const std::vector<std::string>& roots);
  std::shared_ptr<const Snapshot> TakeResult();

private:
  void Run();
  std::mutex mMutex;
  std::condition_variable mChanged;
  std::vector<std::string> mRoots;
  std::shared_ptr<const Snapshot> mResult;
  std::atomic<unsigned> mRevision{0};
  std::atomic<bool> mStop{false};
  std::thread mThread;
};
} // namespace nam_library
