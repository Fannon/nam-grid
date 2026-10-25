#pragma once
#include <future>

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
