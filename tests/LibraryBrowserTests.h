#pragma once
#include "LibraryBrowser.h"

inline void TestLibraryBrowser(const std::filesystem::path& fixtureRoot)
{
  using namespace nam_library;
  // Catalog abbreviations expand to searchable hints without replacing metadata.
  Capture tagged;
  tagged.path = "TWO CRS + BAD Era 30 - Edge - BAL3 KOT SM57 Gain 5 - Cab - AMG.nam";
  tagged.maker = "Explicit maker"; tagged.model = "Explicit model";
  tagged.hints = InferFilenameHints(tagged);
  const auto hints = HintText(tagged);
  assert(hints.find("vendor: Two-Rock") != std::string::npos && hints.find("vendor: Bad Cat") != std::string::npos);
  assert(hints.find("creator: AmalgamAudio") != std::string::npos);
  assert(hints.find("eq: Balanced variant 3") != std::string::npos);
  assert(hints.find("chain: King of Tone") != std::string::npos && hints.find("mic/cab: Shure SM57") != std::string::npos);
  assert(hints.find("settings: Gain 5") != std::string::npos && tagged.maker == "Explicit maker" && tagged.model == "Explicit model");
  Snapshot hintLibrary; hintLibrary.captures = {tagged};
  assert(Browse(hintLibrary, FileKind::NAM, "maker:\"two-rock\" creator:amalgam eq:balanced gain:breakup").size() == 1);
  assert(Browse(hintLibrary, FileKind::NAM, "\"king of tone\" shure -creator:junkie").size() == 1);
  assert(Browse(hintLibrary, FileKind::NAM, "-eq:balanced").empty());
  assert((Filter{"AmalgamAudio balanced", -1}.Matches(tagged)));
  Capture compact; compact.path = "A FEN DLX CL.nam"; compact.hints = InferFilenameHints(compact);
  assert(HintText(compact).find("model: Fender Deluxe Reverb Reissue") != std::string::npos);
  assert(Search("fender \"deluxe reverb\" gain:clean").Score(compact) >= 0);
  Capture dual; dual.path = "DUAL CRS-ER30 Drive BAL KLN CAB.nam"; dual.hints = InferFilenameHints(dual);
  const auto dualModels = DetailValues(dual, "model");
  assert((dualModels == std::vector<std::string>{"Two-Rock CRS", "Bad Cat Era 30"}));
  for (const auto& value : dualModels) assert(Search(DetailQuery("model", value)).Score(dual) >= 0);
  const auto dualVendors = DetailValues(dual, "vendor");
  assert(dualVendors.size() == 2);
  for (const auto& value : dualVendors) assert(Search(DetailQuery("vendor", value)).Score(dual) >= 0);
  Capture quoted; quoted.model = "My \"Custom\" Amp";
  assert(Search(DetailQuery("model", quoted.model)).Score(quoted) >= 0);
  Capture metadataVendor; metadataVendor.maker = "My Studio";
  assert(Search(DetailQuery("vendor", metadataVendor.maker)).Score(metadataVendor) >= 0);
  Capture unknown; unknown.path = "FENderish unbalanced AMGain MARvel.nam";
  assert(InferFilenameHints(unknown).empty());
  unknown.path = "MAR JTM45 - Clean - Mesa OS - Cab - TNJ.nam"; unknown.hints = InferFilenameHints(unknown);
  assert(HintText(unknown, "vendor") == "vendor: Marshall");
  assert(HintText(unknown, "creator") == "creator: Tone Junkie");
  Capture ir; ir.path = "Mesa2x12 V30_SM57_BAL.wav"; ir.kind = FileKind::IR; ir.hints = InferFilenameHints(ir);
  assert(Search("shure balanced \"vintage 30\"").Score(ir) >= 0 && HintText(ir, "format").empty());
  Capture rig = tagged; rig.kind = FileKind::RigPreset;
  assert(InferFilenameHints(rig).empty());
  Capture revised;
  revised.path = "VOX AC30 TB Super Twin 1964 + FEN Super Reverb 1965 - Edge - Boost WRM KLN Mesa412OS V30 SM57 - Cab - AA.nam";
  revised.hints = InferFilenameHints(revised);
  assert((DetailValues(revised, "model") == std::vector<std::string>{"VOX AC30 Top Boost Super Twin 1964", "Fender Super Reverb 1965"}));
  assert(HintText(revised, "gain") == "gain: Edge of breakup"); // Boost is a setting, not another gain category.
  assert(Search("creator:AMG model:TB30 eq:WRM speaker:V30").Score(revised) >= 0);
  assert(Search("-creator:AMG").Score(revised) < 0);
  assert((Filter{"AMG TB30", -1}.Matches(revised)));
  assert(HintText(revised, "mic/cab").find("Mesa Rectifier Oversized") != std::string::npos);
  assert(HintText(revised, "settings").find("Volume 30") == std::string::npos); // V30 is a speaker.
  revised.path = "VOX AC30 Top Boost 1965 - Clean - Cab - TNJ.nam"; revised.hints = InferFilenameHints(revised);
  assert(HintText(revised, "gain") == "gain: Clean" && HintText(revised, "channel").empty());
  revised.path = "MES Badlander 100W - Crunch - Bold Clean CleanPushed2 Fredman1 - Cab - 2DR.nam";
  revised.hints = InferFilenameHints(revised);
  assert(HintText(revised, "gain") == "gain: Crunch");
  revised.path = "BOSS AD-2 Acoustic Preamp - Color - Resonance 75pct - LEM.nam"; revised.hints = InferFilenameHints(revised);
  assert(HintText(revised, "settings") == "settings: Resonance 75%");
  revised.path = "UAD 610-B - Clean - Gain -10 Line Input EQ Off - GLU.nam"; revised.hints = InferFilenameHints(revised);
  assert(HintText(revised, "settings").find("Gain -10") != std::string::npos);
  revised.path = "WIZ Modern Classic MCII - High Gain - Ch3 G7 V6 - Head - AA.nam"; revised.hints = InferFilenameHints(revised);
  assert(HintText(revised, "vendor") == "vendor: Wizard");
  assert(HintText(revised, "format") == "format: Direct input (no cabinet)");
  assert(HintText(revised, "settings").find("Gain 7") != std::string::npos);
  revised.path = "FEN Super Head - Clean - BAL - Cab - AA.nam"; revised.hints = InferFilenameHints(revised);
  assert(HintText(revised, "format") == "format: Amp + Cab"); // A model name does not override the format field.
  revised.path = "I MES DRF HG.nam"; revised.hints = InferFilenameHints(revised);
  assert(Search("model:\"dual rectifier\" gain:\"high gain\"").Score(revised) >= 0);
  Capture folderHints; folderHints.path = "Library/Pedals/Ibanez/TS9/Take 01.nam"; folderHints.source = "Library";
  folderHints.hints = InferLibraryHints(folderHints);
  assert(HintText(folderHints, "vendor") == "vendor: Ibanez");
  assert(HintText(folderHints, "category") == "category: Pedal");
  folderHints.path = "Library/Amalgam/Fender/Clean/Take 01.nam";
  folderHints.hints = InferLibraryHints(folderHints);
  assert(Search("creator:amalgam vendor:fender gain:clean").Score(folderHints) >= 0);
  assert(HintText(folderHints, {}, 0).empty() && !HintText(folderHints, {}, 1).empty());
  folderHints.path = "Marshall/Library/Amalgam/Clean/Take 01.nam"; folderHints.source = "Marshall/Library";
  folderHints.hints = InferLibraryHints(folderHints);
  assert(HintText(folderHints, "vendor").empty()); // An ancestor outside the source is not evidence.
  folderHints.path = "Library/Fender/Marshall/Take 01.nam"; folderHints.source = "Library";
  folderHints.hints = InferLibraryHints(folderHints);
  assert(HintText(folderHints, "vendor") == "vendor: Marshall"); // Nearest folder wins.
  folderHints.path = "Library/Fender/FRI BE-100 - Crunch - FAT - AMG.nam";
  folderHints.hints = InferLibraryHints(folderHints);
  assert(HintText(folderHints, "vendor") == "vendor: Friedman"); // Filename beats folder.
  ir.path = "Library/OwnHammer/V30 Mix.wav"; ir.source = "Library"; ir.hints = InferLibraryHints(ir);
  assert(HintText(ir, "creator") == "creator: OwnHammer"); // MIX is not a NAM creator suffix in an IR.
  folderHints.path = "Library/Packs/My Pack/Creators/My Studio/Vendors/My Brand/Models/My Amp/Take.nam";
  folderHints.hints = InferLibraryHints(folderHints);
  assert(Search("creator:\"my studio\" vendor:\"my brand\" model:\"my amp\" collection:\"my pack\"").Score(folderHints) >= 0);
  Snapshot folderOrder;
  for (const auto& name : {"Zoo/Z.nam", "_Top10/A.nam", "Amps/Z/Z.nam", "Amps/_Best/A.nam", "Amps/A.nam"})
  { Capture f; f.path = std::filesystem::path("library") / name; folderOrder.captures.push_back(f); }
  const auto ordered = BrowserFolders(folderOrder, FileKind::NAM, {"library"});
  assert(ordered.size() == 7 && ordered[2].label == "_Top10" && ordered[3].label == "Amps"
         && ordered[4].label == "_Best" && ordered[5].label == "Z" && ordered[6].label == "Zoo");
  const auto root = fixtureRoot / "browser-fixtures";
  const auto captures = root / "captures", presets = root / "presets";
  std::filesystem::create_directories(captures / "Clean");
  std::filesystem::create_directories(captures / "Cleanish");
  std::filesystem::create_directories(presets / "Acoustic");
  const auto inferredFile = root / "FEN Twin - Clean - BRI - Cab - TNJ.nam";
  std::ofstream(inferredFile) << "{}";
  const auto inspected = Inspect(inferredFile, root);
  assert(HintText(inspected, "eq") == "eq: Bright EQ");
  IndexEntries cache;
  cache["test"].capture = inspected;
  const auto cacheFile = root / "hints-index.json";
  assert(WriteIndex(cacheFile, cache));
  const auto restoredHints = ReadIndex(cacheFile);
  assert(restoredHints.size() == 1 && HintText(restoredHints.begin()->second.capture) == HintText(inspected));
  const auto contextRoot = root / "Folder Library";
  const auto contextFile = contextRoot / "Pedals" / "Ibanez" / "Take.nam";
  std::filesystem::create_directories(contextFile.parent_path());
  std::ofstream(contextFile) << "{}";
  IndexCache contextIndex;
  const auto contextScan = ScanLibrary({PathText(contextRoot)}, {}, {}, {}, &contextIndex);
  assert(contextScan.captures.size() == 1 && HintText(contextScan.captures.front(), "vendor") == "vendor: Ibanez");
  assert(WriteIndex(cacheFile, contextIndex.next));
  std::atomic<int> contextScans{0};
  Worker contextWorker(cacheFile, [&](const auto& n, const auto& i, const auto& p, const auto& cancel, IndexCache* index) {
    ++contextScans; return ScanLibrary(n, i, p, cancel, index);
  });
  contextWorker.Request({PathText(contextRoot)}, {}, {}, false);
  std::shared_ptr<const Snapshot> contextRestored;
  const auto contextDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!(contextRestored = contextWorker.TakeResult()) && std::chrono::steady_clock::now() < contextDeadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(contextRestored && contextScans == 0 && contextRestored->captures.size() == 1
         && HintText(contextRestored->captures.front()) == HintText(contextScan.captures.front()));
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
  const auto contentFolders = ContentFolders(copies, grun);
  assert(contentFolders.size() == 2 && contentFolders[0] == PathText(nam.parent_path())
         && contentFolders[1] == PathText(copy.path.parent_path()));
  assert(ContentCopyInFolder(copies, grun, copy.path.parent_path()) == copy.path);
  assert(ContentCopyInFolder(copies, copy, nam.parent_path()) == nam);
  assert(ContentCopyInFolder(copies, grun, root / "missing").empty());
  assert(ContentCopyInFolder(copies, unhashed, copy.path.parent_path()).empty());
  auto anotherCopy = copy; anotherCopy.path = copy.path.parent_path() / "Another name.nam";
  copies.captures.push_back(anotherCopy);
  assert(ContentFolders(copies, grun).size() == 2); // Several copies in one folder list it once.
  assert(ContentCopyInFolder(copies, copy, copy.path.parent_path()) == copy.path); // Keep the current copy selected.
  copies.captures.pop_back();
  auto otherKind = copy; otherKind.kind = FileKind::IR; otherKind.path = root / "IRs" / "Copy.wav";
  copies.captures.push_back(otherKind);
  assert(ContentFolders(copies, grun).size() == 2); // Matching bytes of another kind are unrelated.
  assert(ContentCopyInFolder(copies, grun, otherKind.path.parent_path()).empty());
  copies.captures.pop_back();
  assert(ContentFolders(copies, unhashed) == std::vector<std::string>{PathText(unhashed.path.parent_path())});
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
