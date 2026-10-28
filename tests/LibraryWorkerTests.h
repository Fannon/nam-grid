#pragma once
#include <future>

inline void TestLibraryWorkerRecovery(const std::filesystem::path& fixtureRoot)
{
  using namespace nam_library;
  using json = nlohmann::json;
  const auto base = fixtureRoot / ("Recovery-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  const auto nam = base / "NAM", ir = base / "IR", presets = base / "Presets";
  const auto indexFile = base / "cache" / "library-index.json";
  for (const auto& folder : {nam, ir, presets}) std::filesystem::create_directories(folder);
  const std::vector<std::string> namRoots{PathText(nam)}, irRoots{PathText(ir)}, presetRoots{PathText(presets)};
  const auto factoryNam = nam / "factory.nam", factoryPreset = presets / "factory.ngrid";
  const std::vector<std::string> factoryFiles{PathText(factoryNam), PathText(factoryPreset)};
  std::ofstream(factoryNam) << "{}";
  rig_preset::Write(factoryPreset, json{{"PresetName", "Factory"}});
  IndexCache index;
  ScanLibrary(namRoots, irRoots, presetRoots, {}, &index);
  const auto factoryIndex = index.next;
  assert(factoryIndex.size() == 2);
  const auto userNam = nam / "user.nam", userIR = ir / "user.wav", userPreset = presets / "user.ngrid";
  std::ofstream(userNam) << "{}";
  std::ofstream(userIR) << "audio is validated on load";
  rig_preset::Write(userPreset, json{{"PresetName", "User"}});
  const auto waitResult = [](Worker& worker) {
    std::shared_ptr<const Snapshot> result;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!(result = worker.TakeResult()) && std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(result);
    return result;
  };
  // Every unusable saved index recovers all three file kinds even with startup rescan off.
  for (int scenario = 0; scenario < 5; ++scenario)
  {
    if (scenario == 0) std::filesystem::remove(indexFile);
    if (scenario == 1) { assert(WriteIndex(indexFile, {})); std::ofstream(indexFile) << "{ broken"; }
    if (scenario == 2) std::ofstream(indexFile) << json{{"Version", kIndexVersion + 1}, {"Files", json::array()}};
    if (scenario == 3) assert(WriteIndex(indexFile, {}));
    if (scenario == 4) assert(WriteIndex(indexFile, factoryIndex));
    std::atomic<int> scans{0};
    Worker worker(indexFile, [&](const auto& n, const auto& i, const auto& p, const auto& cancelled, IndexCache* cache) {
      ++scans;
      return ScanLibrary(n, i, p, cancelled, cache);
    });
    worker.Request(namRoots, irRoots, presetRoots, false, factoryFiles);
    const auto result = waitResult(worker);
    assert(result->captures.size() == 5 && result->error.empty() && scans == 1);
    assert(ReadIndex(indexFile).size() == 5);
  }
  {
    // A usable cache restores entries, source roots and relative paths without rescanning.
    // Its missing file stays visible until an explicit rescan removes it.
    std::filesystem::remove(userNam);
    std::atomic<int> scans{0};
    Worker worker(indexFile, [&](const auto& n, const auto& i, const auto& p, const auto& cancelled, IndexCache* cache) {
      ++scans;
      return ScanLibrary(n, i, p, cancelled, cache);
    });
    worker.Request(namRoots, irRoots, presetRoots, false, factoryFiles);
    const auto cached = waitResult(worker);
    assert(cached->captures.size() == 5 && scans == 0);
    for (const auto& file : cached->captures)
    {
      const auto source = file.kind == FileKind::NAM ? nam : file.kind == FileKind::IR ? ir : presets;
      assert(file.source == source && file.relative == file.path.filename() && file.folderDepth == 0);
    }
    worker.Request(namRoots, irRoots, presetRoots);
    assert(waitResult(worker)->captures.size() == 4 && scans == 1);
    assert(ReadIndex(indexFile).size() == 4);
    // Cached views retain the live scanner's overlap handling, folder labels and duplicates.
    const std::vector<std::string> overlapRoots{PathText(base), PathText(nam)};
    const auto expected = ScanLibrary(overlapRoots, irRoots, presetRoots);
    worker.Request(overlapRoots, irRoots, presetRoots, false, factoryFiles);
    const auto overlap = waitResult(worker);
    assert(overlap->captures.size() == expected.captures.size() && overlap->duplicates == expected.duplicates && scans == 1);
    for (size_t i = 0; i < overlap->captures.size(); ++i)
    {
      const auto& a = overlap->captures[i];
      const auto& b = expected.captures[i];
      assert(a.path == b.path && a.source == b.source && a.relative == b.relative && a.folderDepth == b.folderDepth);
    }
    // Entries from other roots or file kinds cannot make an empty selected library usable.
    worker.Request({}, {}, namRoots, false, factoryFiles);
    assert(waitResult(worker)->captures.empty() && scans == 2);
  }
  // Offline roots, unreadable files and scan exceptions preserve the last good index.
  for (int failure = 0; failure < 3; ++failure)
  {
    assert(WriteIndex(indexFile, factoryIndex));
    const auto original = json::parse(std::ifstream(indexFile));
    const auto invalid = presets / "invalid.ngrid";
    if (failure == 1) std::ofstream(invalid) << "{ broken";
    Worker worker(indexFile, [&](const auto& n, const auto& i, const auto& p, const auto& cancelled, IndexCache* cache) {
      if (failure == 2) throw std::runtime_error("scan failed");
      return ScanLibrary(n, i, p, cancelled, cache);
    });
    auto roots = namRoots;
    if (failure == 0) roots.push_back(PathText(base / "offline"));
    worker.Request(roots, irRoots, presetRoots);
    const auto result = waitResult(worker);
    assert(!result->error.empty() || result->unreadable > 0);
    assert(json::parse(std::ifstream(indexFile)) == original);
    std::filesystem::remove(invalid);
  }
}

// Hold a real scan in progress so cancellation cannot accidentally test only
// the idle-worker path. No filesystem timing or large fixture is needed.
inline void TestLibraryWorkerCancellation(const std::filesystem::path& indexFile,
                                           const std::vector<std::string>& roots)
{
  using namespace nam_library;
  const auto original = nlohmann::json::parse(std::ifstream(indexFile));
  const auto waitResult = [](Worker& worker, std::chrono::milliseconds timeout) {
    std::shared_ptr<const Snapshot> result;
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!(result = worker.TakeResult()) && std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return result;
  };
  for (bool replace : {false, true})
  {
    std::promise<void> entered, release;
    auto ready = entered.get_future(), proceed = release.get_future();
    std::atomic<int> scans{0};
    Worker worker(indexFile, [&](const auto& nam, const auto& ir, const auto& presets,
                                const auto& cancelled, IndexCache* index) {
      if (++scans == 1) { entered.set_value(); proceed.wait(); }
      return ScanLibrary(nam, ir, presets, cancelled, index);
    });
    worker.Request(std::vector<std::string>{});
    assert(ready.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    worker.Cancel();
    if (replace) worker.Request(roots, {}, {});
    release.set_value();
    if (replace)
    {
      const auto result = waitResult(worker, std::chrono::seconds(5));
      assert(result && result->captures.size() == 1 && scans == 2);
    }
    else
    {
      assert(!waitResult(worker, std::chrono::milliseconds(100)));
      assert(scans == 1);
      assert(nlohmann::json::parse(std::ifstream(indexFile)) == original);
      // A new request still works after the cancellation has been consumed.
      worker.Request(roots, {}, {});
      const auto result = waitResult(worker, std::chrono::seconds(5));
      assert(result && result->captures.size() == 1 && scans == 2);
    }
    assert(nlohmann::json::parse(std::ifstream(indexFile)) == original);
  }
}
