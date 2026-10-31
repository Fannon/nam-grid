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
  // Browser stars cycle none -> yellow -> orange -> red -> none.
  assert(NextStar(0) == 1 && NextStar(1) == 2 && NextStar(2) == 3 && NextStar(3) == 0 && NextStar(-1) == 0);
  // Captures and IRs are starred by content: a moved, renamed or copied file keeps its star,
  // different bytes never share one, and the same bytes as NAM and as IR stay separate.
  const auto byPath = [&](const std::filesystem::path& path) -> const Capture& {
    return *std::find_if(library.captures.begin(), library.captures.end(), [&](const auto& c) { return c.path == path; });
  };
  const auto& grun = byPath(nam);
  assert(StarKey(grun, presets) == "NAM:" + grun.hash && grun.hash.size() == 40);
  assert(StarKey(grun, presets) != StarKey(byPath(captures / "Cleanish" / "Other.nam"), presets));
  auto copy = grun; copy.path = root / "Elsewhere" / "Renamed.nam"; copy.source = root;
  assert(StarKey(copy, presets) == StarKey(grun, presets) && StarKey(copy, {}) == StarKey(grun, presets));
  auto asIr = grun; asIr.kind = FileKind::IR;
  assert(StarKey(asIr, presets) == "IR:" + grun.hash);
  auto unhashed = grun; unhashed.hash.clear();
  assert(StarKey(unhashed, presets).empty()); // No identity yet, so no star rather than a wrong one.
  // A module's star follows its append-only ID, not its display name or category.
  Capture boost; boost.kind = FileKind::Module; boost.module = 3; boost.path = "Drive/Boost";
  auto renamed = boost; renamed.path = "Distortion/Overdrive";
  Capture fuzz; fuzz.kind = FileKind::Module; fuzz.module = 4; fuzz.path = "Drive/Fuzz";
  assert(StarKey(boost, {}) == "Module:3" && StarKey(renamed, presets) == "Module:3" && StarKey(fuzz, {}) == "Module:4");
  // Rig presets change content on every save, so they are starred by path: relative to the
  // favorites folder when they live in it (portable with the folder), absolute elsewhere.
  const auto& warm = byPath(preset);
  const auto warmKey = StarKey(warm, warm.source);
  assert(warmKey == "RigPreset:" + PathText(warm.relative.generic_u8string()) && warmKey.find('\\') == std::string::npos);
  assert(StarKey(warm, root / "Other presets") == "RigPreset:" + PathText(std::filesystem::path(warm.path.generic_u8string())));
  Capture nested; nested.kind = FileKind::RigPreset; nested.source = presets;
  nested.path = presets / "Live" / "Lead.ngrid"; nested.relative = std::filesystem::path("Live") / "Lead.ngrid";
  assert(StarKey(nested, presets) == "RigPreset:Live/Lead.ngrid");
  // The Starred filter keeps starred results in their search order and matches each by its own key.
  const auto allNams = Browse(library, FileKind::NAM, "", {}, -1, false);
  assert(allNams.size() == 2 && KeepStarred(library, allNams, {}, presets).empty());
  const auto starredNams = KeepStarred(library, allNams, {{StarKey(grun, presets), {3}}}, presets);
  assert(starredNams.size() == 1 && library.captures[starredNams.front()].path == nam);
  assert(KeepStarred(library, allNams, {{"IR:" + grun.hash, {1}}}, presets).empty());
  assert(KeepStarred(library, Browse(library, FileKind::NAM, "vox"), {{StarKey(grun, presets), {1}}}, presets).size() == 1);
  const auto allPresets = Browse(library, FileKind::RigPreset, "", {}, -1, false);
  const auto starredPresets = KeepStarred(library, allPresets, {{warmKey, {2}}}, warm.source);
  assert(starredPresets.size() == 1 && library.captures[starredPresets.front()].path == preset);
  Snapshot modules;
  modules.captures = {boost, fuzz};
  assert((KeepStarred(modules, {0, 1}, {{"Module:4", {2}}}, {}) == std::vector<size_t>{1}));
  // Identical copies are both kept: one star, every copy.
  Snapshot copies;
  copies.captures = {grun, copy, byPath(captures / "Cleanish" / "Other.nam")};
  assert((KeepStarred(copies, {0, 1, 2}, {{StarKey(grun, {}), {1}}}, {}) == std::vector<size_t>{0, 1}));
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
  // Stars live in favorites.json in the first preset folder, falling back to the config folder.
  assert(FavoritesFile({PathText(presets), PathText(captures)}, root / "config") == presets / "favorites.json");
  assert(FavoritesFile({}, root / "config") == root / "config" / "favorites.json");
  assert(FavoritesFolder({PathText(presets)}) == presets && FavoritesFolder({}).empty() && FavoritesFile({}, {}).empty());
  const auto favorites = presets / "favorites.json";
  assert(ReadFavorites(favorites) && ReadFavorites(favorites)->empty()); // Missing reads as no stars.
  assert(WriteFavorites(favorites, {{StarKey(grun, presets), {2, "Gr\xc3\xbcn Clean.nam"}}, {"Module:5", {3}}, {warmKey, {1, "Clean.NGRID"}}}));
  const auto favored = *ReadFavorites(favorites);
  assert(favored.size() == 3 && favored.at(StarKey(grun, presets)).level == 2
         && favored.at(StarKey(grun, presets)).name == "Gr\xc3\xbcn Clean.nam" && favored.at("Module:5").level == 3
         && favored.at("Module:5").name.empty() && favored.at(warmKey).level == 1);
  // The file is not a library entry: rescanning the preset folder still finds only presets.
  assert(Browse(ScanLibrary({}, {}, {PathText(presets)}), FileKind::RigPreset, "").size() == 2);
  // A damaged star costs only itself. A damaged file reads as nothing, so it is never overwritten.
  std::ofstream(favorites) << R"({"Version":1,"Stars":{"Module:1":{"Level":9},"":{"Level":1},"Module:2":{"Level":"red"},)"
                              R"("Module:3":{"Level":2,"Name":7},"Module:4":3,"Module:6":{"Level":1,"Name":"Fuzz"}}})";
  const auto damaged = *ReadFavorites(favorites);
  assert(damaged.size() == 2 && damaged.at("Module:3").level == 2 && damaged.at("Module:3").name.empty()
         && damaged.at("Module:6").name == "Fuzz");
  for (const auto* text : {"{broken", "[]", R"({"Stars":[]})"})
  {
    std::ofstream(favorites) << text;
    assert(!ReadFavorites(favorites));
  }
  std::ofstream(favorites) << R"({"Version":2})"; // A newer file without stars is still readable.
  assert(ReadFavorites(favorites) && ReadFavorites(favorites)->empty());
  std::filesystem::remove(favorites);
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
