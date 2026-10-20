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
std::string ReadFolderSetting(const std::filesystem::path& settingsFile);
bool WriteFolderSetting(const std::filesystem::path& settingsFile, const std::string& folder);

struct Capture
{
  std::filesystem::path path, relative;
  std::string name, maker, model;
  Gear gear = Gear::Unknown;
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
};

// Shared by the background worker and deterministic scanner tests.
Capture Inspect(const std::filesystem::path& file, const std::filesystem::path& root);
Snapshot Scan(const std::filesystem::path& root, const std::function<bool()>& cancelled = {});

// Latest request wins. Results are handed back to the control thread, never to ProcessBlock.
class Worker
{
public:
  Worker();
  ~Worker();
  void Request(const std::string& root);
  std::shared_ptr<const Snapshot> TakeResult();

private:
  void Run();
  std::mutex mMutex;
  std::condition_variable mChanged;
  std::string mRoot;
  std::shared_ptr<const Snapshot> mResult;
  std::atomic<unsigned> mRevision{0};
  std::atomic<bool> mStop{false};
  std::thread mThread;
};
} // namespace nam_library
