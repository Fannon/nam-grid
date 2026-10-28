#pragma once

// Exercise the real constructor, preferences and factory-file classification without an
// editor or audio device. IsolatedUserHome is established before loading the plugin.
inline void TestLibraryStartup(const clap_plugin_factory* factory, Host& host,
                              const IsolatedUserHome& isolated)
{
  using namespace nam_library;
  const auto indexFile = isolated.home / "cache" / "library-index.json";
  const auto factoryIndex = ReadIndex(indexFile);
  assert(!factoryIndex.empty());
  auto settings = ReadSettings(isolated.file);
  settings.rescanOnStartup = false;
  const auto external = isolated.home / "External captures";
  std::filesystem::create_directories(external);
  settings.namFolders.push_back(PathText(external));
  assert(WriteSettings(isolated.file, settings));
  const auto userNam = external / "User.nam";
  const auto userIR = isolated.home / "IRs" / "User.wav";
  const auto userPreset = isolated.home / "Presets" / "User.ngrid";
  std::ofstream(userNam) << "{}";
  std::ofstream(userIR) << "audio is validated on load";
  rig_preset::Write(userPreset, json{{"PresetName", "User"}});
  for (int scenario = 0; scenario < 5; ++scenario)
  {
    if (scenario == 0) std::filesystem::remove(indexFile);
    if (scenario == 1) assert(WriteIndex(indexFile, {}));
    if (scenario == 2) std::ofstream(indexFile) << "{ broken";
    if (scenario == 3) std::ofstream(indexFile) << json{{"Version", kIndexVersion + 1}, {"Files", json::array()}};
    if (scenario == 4) assert(WriteIndex(indexFile, factoryIndex));
    auto* plugin = factory->create_plugin(factory, &host.host, factory->get_plugin_descriptor(factory, 0)->id);
    assert(plugin && plugin->init(plugin));
    const auto recovered = [&] {
      const auto index = ReadIndex(indexFile);
      for (const auto& file : {userNam, userIR, userPreset})
        if (std::none_of(index.begin(), index.end(), [&](const auto& entry) { return entry.second.capture.path == file; }))
          return false;
      return true;
    };
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!recovered() && std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(recovered());
    plugin->destroy(plugin);
    const auto saved = ReadSettings(isolated.file);
    assert(!saved.rescanOnStartup && saved.namFolders == NormalizeFolders(settings.namFolders));
  }
  std::cout << "Startup with rescanning off: missing, empty, corrupt, incompatible and factory-only indexes recover NAM, IR and preset folders\n";
}
