#pragma once
#include "LibraryBrowser.h"

inline void TestLibraryBrowser(const std::filesystem::path& fixtureRoot)
{
  using namespace nam_library;
  const auto root = fixtureRoot / "browser-fixtures";
  const auto captures = root / "captures", presets = root / "presets";
  std::filesystem::create_directories(captures / "Clean");
  std::filesystem::create_directories(captures / "Cleanish");
  std::filesystem::create_directories(presets / "Acoustic");
  const auto nam = captures / "Clean" / std::filesystem::u8path("Gr\xc3\xbcn Clean.nam");
  std::ofstream(nam) << R"({"metadata":{"name":"Silver clean","gear_make":"Vox","gear_model":"AC30","gear_type":"amp_head"}})";
  std::ofstream(captures / "Cleanish" / "Other.nam") << "{}";
  std::ofstream(captures / "Clean" / "Cab.WAV") << "load validates audio";
  const auto preset = presets / "Acoustic" / "Clean.NGRID";
  rig_preset::Write(preset, {{"PresetName", "Warm acoustic"}});
  rig_preset::Write(presets / "Clean.ngrid", {{"PresetName", "Other clean"}});
  rig_preset::Write(captures / "not-a-capture.ngrid", nlohmann::json::object());
  std::ofstream(presets / "not-a-preset.nam") << "{}";
  std::ofstream(presets / "invalid.ngrid") << "{}";
  std::ofstream(presets / "oversized.ngrid") << std::string(rig_preset::kMaximumBytes + 1, ' ');
  const auto library = ScanLibrary({PathText(captures)}, {PathText(captures)}, {PathText(presets), PathText(presets / "Acoustic")});
  assert(library.captures.size() == 5 && library.unreadable == 2);
  assert(Browse(library, FileKind::NAM, "").size() == 2);
  assert(Browse(library, FileKind::IR, "").size() == 1);
  assert(Browse(library, FileKind::RigPreset, "").size() == 2);
  assert(Browse(library, FileKind::SlotPreset, "").empty());
  // Each source list indexes only its own kind, even for the same folder.
  const auto namOnly = ScanLibrary({PathText(captures)}, {}, {});
  assert(Browse(namOnly, FileKind::NAM, "").size() == 2 && Browse(namOnly, FileKind::IR, "").empty());
  const auto irOnly = ScanLibrary({}, {PathText(captures)}, {});
  assert(irOnly.captures.size() == 1 && Browse(irOnly, FileKind::IR, "").size() == 1);
  const auto matches = Browse(library, FileKind::NAM, "GRUN maker:VOX model:ac30 \"silver clean\" -pedal");
  assert(matches.size() == 1 && library.captures[matches.front()].path == nam);
  assert(Browse(library, FileKind::NAM, "-\"silver clean\"").size() == 1);
  assert(Browse(library, FileKind::NAM, "name:clean -maker:vox").empty());
  assert(Browse(library, FileKind::NAM, "vox -model:ac30").empty());
  assert(Browse(library, FileKind::NAM, "gear:\"amp head\"").size() == 1);
  assert(Browse(library, FileKind::NAM, "", captures / "Clean").size() == 1);
  assert(Browse(library, FileKind::NAM, "", {}, static_cast<int>(Gear::Pedal)).empty());
  assert(Browse(library, FileKind::RigPreset, "warm acoustic").size() == 1);
  assert(!Within(captures / "Cleanish" / "Other.nam", captures / "Clean"));
  assert(!Within(captures / std::filesystem::u8path("Gr\xc3\xbcn") / "file.nam", captures / "Grun"));
  assert(!SamePath(captures / std::filesystem::u8path("Gr\xc3\xbcn.nam"), captures / "Grun.nam"));
#ifdef _WIN32
  assert(SamePath(captures / std::filesystem::u8path("Gr\xc3\xbcn.NAM"), captures / std::filesystem::u8path("GR\xc3\x9cN.nam")));
#endif
  const auto folders = BrowserFolders(library, FileKind::NAM, {PathText(captures), PathText(root / "missing")});
  assert(folders.size() == 5 && folders.front().path.empty() && folders.back().rootIndex == 1);
  assert(std::none_of(folders.begin(), folders.end(), [&](const auto& f) { return f.path == presets; }));
  // Roots holding only other kinds are hidden; their configured index is kept for the rest.
  assert(BrowserFolders(library, FileKind::NAM, {PathText(presets)}).size() == 1);
  const auto presetFolders = BrowserFolders(library, FileKind::RigPreset, {PathText(captures), PathText(presets)});
  assert(presetFolders.size() == 3 && presetFolders[1].path == presets && presetFolders[1].rootIndex == 1);
  Snapshot ranking;
  Capture exact; exact.path = root / "Clean.nam";
  Capture prefix; prefix.path = root / "Clean bright.nam";
  Capture metadata; metadata.path = root / "A.nam"; metadata.name = "clean";
  ranking.captures = {metadata, prefix, exact};
  assert((Browse(ranking, FileKind::NAM, "clean") == std::vector<size_t>{2, 1, 0}));
  assert((Browse(ranking, FileKind::NAM, "clean", {}, -1, false) == std::vector<size_t>{0, 2, 1}));
  for (const auto kind : {FileKind::NAM, FileKind::IR, FileKind::RigPreset, FileKind::SlotPreset, FileKind::Module})
  {
    assert(Allows(BrowserContext::Cell, kind) == (kind == FileKind::NAM || kind == FileKind::IR || kind == FileKind::Module));
    assert(Allows(BrowserContext::Rig, kind) == (kind == FileKind::RigPreset));
    assert(Allows(BrowserContext::Library, kind) == (kind != FileKind::SlotPreset));
  }
  // Presets must never appear in a capture menu or participate in model/IR relinking.
  Menu menu;
  FillMenu(menu, library, Filter{"warm acoustic", -1}, "");
  assert(menu.NItems() == 1 && !menu.GetItem(0)->GetEnabled());
  Snapshot spoof;
  Capture presetEntry; presetEntry.kind = FileKind::RigPreset; presetEntry.hash = "hash";
  spoof.captures.push_back(presetEntry);
  assert(!FindHash(spoof, "hash", false) && !FindHash(spoof, "hash", true));
  const auto settings = root / "settings.json";
  assert(WriteSettings(settings, {{PathText(captures)}, false, false, {PathText(presets)}, {PathText(captures)}}));
  assert(WriteFolderSettings(settings, {}));
  const auto roundTrip = ReadSettings(settings);
  assert(roundTrip.namFolders.empty() && roundTrip.presetFolders == std::vector<std::string>{PathText(presets)});
  assert(roundTrip.irFolders == std::vector<std::string>{PathText(captures)});
  assert(!roundTrip.rescanOnStartup && !roundTrip.showVisualizations);
  assert(NormalizeFolders({PathText(captures / ""), PathText(captures)}).size() == 1);
  assert(BoundedQuery(std::string(511, 'a') + "\xc3\xbc") == std::string(511, 'a'));
  assert(ScanLibrary({}, {}, {PathText(presets)}, [] { return true; }).captures.empty());
  Worker worker;
  worker.Request({PathText(captures)}, {PathText(captures)}, {PathText(presets)});
  std::shared_ptr<const Snapshot> result;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!(result = worker.TakeResult()) && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(result && Browse(*result, FileKind::RigPreset, "").size() == 2);
  // Only this test's fixed child directory is removed.
  assert(root.parent_path() == fixtureRoot && root.filename() == "browser-fixtures");
  std::filesystem::remove_all(root);
}
