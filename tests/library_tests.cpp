#include "IPlugPlatform.h"
#include "ModuleMenu.h"
#include <array>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include "json.hpp"
#include "RigPresetTests.h"
#include "EffectPresentationTests.h"
#include "LibraryIgnoreTests.h"

using namespace nam_library;
using Menu = iplug::igraphics::IPopupMenu;
using json = nlohmann::json;
#include "LibraryBrowserTests.h"

int main(int argc, char** argv)
{
  if (argc == 2)
  {
    const auto library = Scan(std::filesystem::u8path(argv[1]));
    std::array<int, 7> counts{};
    int inferred = 0;
    for (const auto& capture : library.captures)
    {
      ++counts[static_cast<int>(capture.gear)];
      inferred += capture.inferred;
      if (capture.gear == Gear::Unknown)
        std::cout << "Unknown: " << PathText(capture.relative) << '\n';
    }
    std::cout << library.captures.size() << " captures, " << inferred << " inferred, " << library.unreadable
              << " unreadable. " << library.error << '\n';
    for (int i = 0; i < 7; ++i)
      std::cout << GearName(static_cast<Gear>(i)) << ": " << counts[i] << '\n';
    return library.error.empty() ? 0 : 1;
  }
  const auto root = std::filesystem::current_path() / "library-fixtures";
  std::filesystem::create_directories(root);
  TestRigPresetFiles(root);
  TestEffectPresentation();
  for (const auto& group : kModuleGroups)
    for (size_t i = 0; i < group.effects.size(); ++i)
    {
      const auto effect = group.effects.begin()[i];
      assert(ModuleGroupFor(effect) == &group);
      assert(StepModule(effect, 1) == group.effects.begin()[(i + 1) % group.effects.size()]);
      assert(StepModule(StepModule(effect, 1), -1) == effect);
    }
  auto write = [&](const std::filesystem::path& relative, const json& metadata) {
    const auto path = root / relative;
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path) << json{{"metadata", metadata}, {"weights", {1, 2, 3}}, {"fixture_id", PathText(relative)}}.dump();
    return path;
  };
  for (const auto& [label, expected] : std::vector<std::pair<std::string, Gear>>{{"amp_cab", Gear::AmpCab},
                                                                                 {"full-rig", Gear::AmpCab},
                                                                                 {"amp_pedal_cab", Gear::AmpCab},
                                                                                 {"Amp Head", Gear::AmpHead},
                                                                                 {"cabinet", Gear::Cabinet},
                                                                                 {"pedal", Gear::Pedal},
                                                                                 {"studio", Gear::Outboard},
                                                                                 {"outboard", Gear::Outboard},
                                                                                 {"spaces", Gear::Spaces},
                                                                                 {"amp", Gear::Unknown},
                                                                                 {"something-new", Gear::Unknown}})
  {
    const auto path = write("Categories/" + label + ".nam", {{"gear_type", label}});
    const auto capture = Inspect(path, root);
    assert(capture.gear == expected && !capture.inferred);
  }
  // Generic amp metadata does not imply cabinet inclusion. An explicit filename can supply that hint.
  auto direct = Inspect(write("Hints/Example - DI - A2.nam", {{"gear_type", "amp"}}), root);
  assert(direct.gear == Gear::AmpHead && direct.inferred);
  auto rig = Inspect(write("Hints/Example - Cab - A2.nam", json::object()), root);
  assert(rig.gear == Gear::AmpCab && rig.inferred);
  auto pedal = Inspect(write("Pedals/Unknown.nam", json::object()), root);
  assert(pedal.gear == Gear::Pedal && pedal.inferred);
  auto explicitPedal = Inspect(write("Amps/Strange - Cab - A2.nam", {{"gear_type", "pedal"}}), root);
  assert(explicitPedal.gear == Gear::Pedal && !explicitPedal.inferred);
  // Folder hints include the selected root and propagate through unrelated maker/model subfolders.
  for (const auto& [folder, expected] : std::vector<std::pair<std::string, Gear>>{{"Amp + Cab", Gear::AmpCab},
                                                                                  {"Full Rigs", Gear::AmpCab},
                                                                                  {"Amp Heads", Gear::AmpHead},
                                                                                  {"Direct Amps", Gear::AmpHead},
                                                                                  {"IRs", Gear::Cabinet},
                                                                                  {"Cabinets", Gear::Cabinet},
                                                                                  {"Stompboxes", Gear::Pedal},
                                                                                  {"Pedals", Gear::Pedal},
                                                                                  {"Acoustic & Line", Gear::Outboard},
                                                                                  {"Studio Preamps", Gear::Outboard},
                                                                                  {"Outboard", Gear::Outboard},
                                                                                  {"Spaces", Gear::Spaces},
                                                                                  {"Rooms", Gear::Spaces},
                                                                                  {"Amps", Gear::Unknown},
                                                                                  {"Spaceship", Gear::Unknown}})
  {
    const auto selectedRoot = root / "FolderHints" / folder;
    const auto path =
      write(std::filesystem::path("FolderHints") / folder / "Maker" / "Model" / "capture.nam", json::object());
    for (const auto& scanRoot : {root, selectedRoot, selectedRoot / ""})
    {
      const auto capture = Inspect(path, scanRoot);
      assert(capture.gear == expected && capture.inferred == (expected != Gear::Unknown));
    }
  }
  auto nearest = Inspect(write("Pedals/Studio Preamps/Maker/Unknown.nam", json::object()), root);
  assert(nearest.gear == Gear::Outboard && nearest.inferred);
  auto acousticPedal = Inspect(write("Acoustic & Line/AD2.nam", {{"gear_type", "pedal"}}), root);
  assert(acousticPedal.gear == Gear::Pedal && !acousticPedal.inferred);
  for (const auto& [filename, expected] :
       std::vector<std::pair<std::string, Gear>>{{"amp CAB.nam", Gear::AmpCab},
                                                 {"amp_cAb_A2.nam", Gear::AmpCab},
                                                 {"amp DI.nam", Gear::AmpHead},
                                                 {"amp-di-A2.nam", Gear::AmpHead},
                                                 {"amp No Cab.nam", Gear::AmpHead},
                                                 {"amp CAB DI.nam", Gear::Unknown},
                                                 {"Diablo Caballero.nam", Gear::Unknown}})
  {
    auto capture = Inspect(write(std::filesystem::path("ExportMarkers") / filename, {{"gear_type", "amp"}}), root);
    assert(capture.gear == expected);
  }
  assert(Inspect(write("_Export/Friedman CAB.nam", json::object()), root).gear == Gear::AmpCab);
  assert(Inspect(write("Amps/Head DI.nam", json::object()), root).gear == Gear::AmpHead);
  assert(Inspect(write("Full Rigs/Head DI.nam", json::object()), root).gear == Gear::AmpHead);
  assert(Inspect(write("Pedals/Boost DI.nam", json::object()), root).gear == Gear::Pedal);
  assert(Inspect(write("Outboard/Preamp DI.nam", json::object()), root).gear == Gear::Outboard);
  assert(Inspect(write("Amps/Pedals/Boost DI.nam", json::object()), root).gear == Gear::Pedal);
  assert(Inspect(write("_Export/Unknown DI.nam", json::object()), root).gear == Gear::Unknown);
  // Metadata may come after weights; nested keys must not replace top-level gear fields.
  const auto late = root / "late.nam";
  std::ofstream(late)
    << R"({"weights":[1,2,3],"metadata":{"gear_type":"outboard","training":{"gear_type":"pedal"},"gear_model":"Neve"}})";
  auto lateCapture = Inspect(late, root);
  assert(lateCapture.gear == Gear::Outboard && lateCapture.model == "Neve");
  std::ofstream(root / "bad.nam") << "{broken";
  assert(Inspect(root / "bad.nam", root).gear == Gear::Unknown);
  write(std::filesystem::u8path("Maker/Chime/Grün.NAM"),
        {{"gear_type", "amp_cab"}, {"name", "Silver clean"}, {"gear_make", "Vox"}, {"gear_model", "AC30"}});
  write("Other/Chime/Grun.nam", {{"gear_type", "pedal"}});
  std::ofstream(root / "ignored.wav") << "wav";
  auto library = Scan(root);
  assert(library.error.empty() && library.unreadable == 0);
  auto unicode = std::find_if(
    library.captures.begin(), library.captures.end(), [](const Capture& capture) { return capture.model == "AC30"; });
  assert(unicode != library.captures.end());
  assert((Filter{"VOX clean", -1}.Matches(*unicode)));
  assert((!Filter{"VOX dirt", -1}.Matches(*unicode)));
  assert((!Filter{"", static_cast<int>(Gear::Pedal)}.Matches(*unicode)));
  // Exercise the exact native submenu builder, including a deeply nested leaf's stable path tag/checkmark.
  Menu menu;
  FillMenu(menu, library, Filter{"Vox clean", -1}, PathText(unicode->path));
  assert(menu.NItems() == 1 && std::string(menu.GetItemText(0)) == "Maker");
  auto* sub = menu.GetItem(0)->GetSubmenu();
  assert(sub && std::string(sub->GetItemText(0)) == "Chime");
  auto* leaf = sub->GetItem(0)->GetSubmenu()->GetItem(0);
  assert(leaf && leaf->GetChecked());
  assert(library.captures[leaf->GetTag()].path == unicode->path);
  assert(std::string(leaf->GetText()) == PathText(unicode->path.stem())); // Filename only, no category suffix.
  // Use the production module builder: optional entries must never reorder groups
  // or detach the final separator from the library section.
  for (bool quality : {false, true})
    for (bool empty : {false, true})
      for (bool filtered : {false, true})
      {
        const Snapshot noFiles;
        const auto& entries = empty ? noFiles : library;
        FillModuleMenu(menu, entries, Filter{filtered ? "Vox clean" : "", -1}, PathText(unicode->path),
                       SlotEffect::SpringReverb, quality, true, empty);
        int index = 0;
        for (const char* label : {"Clear slot", "Choose .nam file...", "Choose cabinet IR (.wav)..."})
          assert(std::string(menu.GetItemText(index++)) == label);
        assert(menu.GetItem(index++)->GetIsSeparator());
        std::array<int, static_cast<int>(SlotEffect::Last) + 1> seen{};
        for (const char* label : {"Distortion", "Dynamics", "EQ / Filter", "Modulation", "Pitch / Frequency",
                                  "Delay", "Reverb", "Creative", "Utility / Volume"})
        {
          assert(std::string(menu.GetItemText(index)) == label);
          auto* group = menu.GetItem(index++)->GetSubmenu();
          assert(group);
          for (int i = 0; i < group->NItems(); ++i)
          {
            const auto* item = group->GetItem(i);
            if (item->GetTag() == -16) { assert(std::string(label) == "Delay"); continue; }
            const int type = -item->GetTag() - 20;
            assert(type > 0 && IsValidSlotEffect(static_cast<SlotEffect>(type)));
            if (type == static_cast<int>(SlotEffect::NoisePrint))
              assert(std::string(label) == "Dynamics" && std::string(item->GetText()) == "Noise Gate (Smart)");
            if (type == static_cast<int>(SlotEffect::NoiseGate))
              assert(std::string(label) == "Dynamics" && std::string(item->GetText()) == "Noise Gate");
            ++seen[type];
            assert(item->GetChecked() == (type == static_cast<int>(SlotEffect::SpringReverb)));
          }
        }
        for (int i = 1; i <= static_cast<int>(SlotEffect::Last); ++i)
          assert(
            seen[i]
            == (IsValidSlotEffect(static_cast<SlotEffect>(i)) && i != static_cast<int>(SlotEffect::CabinetIR) ? 1 : 0));
        assert(menu.GetItem(index++)->GetIsSeparator());
        if (quality)
        {
          assert(std::string(menu.GetItemText(index)) == "A2 Lite");
          assert(menu.GetItem(index)->GetChecked() && menu.GetItem(index++)->GetTag() == -15);
        }
        if (filtered)
          assert(std::string(menu.GetItemText(index++)) == "Clear search and gear filter");
        if (empty)
        {
          assert(std::string(menu.GetItemText(index)) == "Scanning library...");
          assert(!menu.GetItem(index++)->GetEnabled());
        }
        assert(std::string(menu.GetItemText(index)) == "NAM captures");
        assert(menu.GetItem(index++)->GetSubmenu());
        assert(std::string(menu.GetItemText(index)) == "Cabinet IRs");
        assert(menu.GetItem(index++)->GetSubmenu());
        assert(index == menu.NItems());
        for (int item = 0, separators = 0; item < menu.NItems(); ++item)
        {
          separators += menu.GetItem(item)->GetIsSeparator();
          assert(separators <= 2);
        }
      }
  FillMenu(menu, library, Filter{"does-not-exist", -1}, "");
  assert(menu.NItems() == 1 && !menu.GetItem(0)->GetEnabled());
  assert(std::string(menu.GetItemText(0)).find("does-not-exist") != std::string::npos);
  assert(!Scan(root / "missing").error.empty());
  assert(Scan(root, [] { return true; }).captures.empty());
  const auto preferences = root / "settings" / "library.json";
  const auto folderText = PathText(root / std::filesystem::u8path("Grün captures"));
  assert(WriteFolderSetting(preferences, folderText));
  assert(ReadFolderSetting(preferences) == folderText);
  std::ofstream(preferences) << "{broken";
  assert(ReadFolderSetting(preferences).empty());
  assert(WriteFolderSetting(preferences, ""));
  assert(ReadFolderSetting(preferences).empty());
  // Mixed NAM/IR folders, overlapping roots and duplicate source basenames.
  const auto sourceA = root / "Sources" / "A" / "Library";
  const auto sourceB = root / "Sources" / "B" / "Library";
  const auto namFile = write("Sources/A/Library/Amps/clean.nam", {{"gear_type", "amp_head"}});
  const auto wavFile = sourceB / "Cabs" / "cab.WaV";
  std::filesystem::create_directories(wavFile.parent_path());
  std::ofstream(wavFile) << "candidate WAV; actual decode occurs only on load";
  const auto mixed = ScanFolders({PathText(sourceA), PathText(sourceB), PathText(sourceA / "Amps")});
  assert(mixed.error.empty() && mixed.captures.size() == 2);
  const auto& ir = mixed.captures[1];
  assert(ir.isIR && ir.gear == Gear::Cabinet && !ir.inferred);
  assert(mixed.captures[0].relative.begin()->string() == "Amps"); // Shallowest overlapping root wins.
  assert(ir.relative.begin()->string() == "Library (2)");
  assert((Filter{"IR WAV", static_cast<int>(Gear::Cabinet)}.Matches(ir)));
  assert((!Filter{"NAM", -1}.Matches(ir)));
  FillMenu(menu, mixed, Filter{"", -1}, PathText(wavFile));
  assert(menu.NItems() == 2);
  auto* irLeaf = menu.GetItem(1)->GetSubmenu()->GetItem(0)->GetSubmenu()->GetItem(0);
  assert(irLeaf->GetChecked() && mixed.captures[irLeaf->GetTag()].isIR);
  // Filtering the file type retains original snapshot indices for selection.
  FillMenu(menu, mixed, Filter{}, PathText(wavFile), 1);
  assert(menu.NItems() == 1 && !menu.GetItem(0)->GetSubmenu());
  irLeaf = menu.GetItem(0);
  assert(irLeaf->GetChecked() && mixed.captures[irLeaf->GetTag()].isIR);
  FillMenu(menu, mixed, Filter{}, PathText(mixed.captures[0].path), 0);
  assert(menu.NItems() == 1 && !menu.GetItem(0)->GetSubmenu());
  auto* namLeaf = menu.GetItem(0);
  assert(namLeaf->GetChecked() && !mixed.captures[namLeaf->GetTag()].isIR);
  assert(std::filesystem::equivalent(mixed.captures[namLeaf->GetTag()].path, namFile));
  auto grouped = mixed;
  auto ampEntry = mixed.captures[0], pedalEntry = ampEntry;
  ampEntry.relative = "NAM Models/Amps/clean.nam";
  pedalEntry.relative = "NAM Models/Pedals/drive.nam"; pedalEntry.path = root / "drive.nam";
  grouped.captures = {ampEntry, pedalEntry};
  FillMenu(menu, grouped, Filter{}, PathText(pedalEntry.path), 0);
  assert(menu.NItems() == 2 && std::string(menu.GetItemText(0)) == "Amps" && std::string(menu.GetItemText(1)) == "Pedals");
  const auto* chosenPedal = menu.GetItem(1)->GetSubmenu()->GetItem(0);
  assert(chosenPedal->GetChecked() && chosenPedal->GetTag() == 1);
  // A file directly in the category prevents skipping its sibling folder.
  auto directEntry = ampEntry; directEntry.relative = "direct.nam"; directEntry.path = root / "direct.nam";
  grouped.captures.push_back(directEntry);
  FillMenu(menu, grouped, Filter{}, "", 0);
  assert(menu.NItems() == 2 && std::string(menu.GetItemText(0)) == "NAM Models");
  assert(!menu.GetItem(1)->GetSubmenu() && menu.GetItem(1)->GetTag() == 2);
  // Two distinct source folders remain visible when neither is shared.
  grouped.captures = {ampEntry, pedalEntry};
  grouped.captures[1].relative = "Other Models/Pedals/drive.nam";
  FillMenu(menu, grouped, Filter{}, "", 0);
  assert(menu.NItems() == 2 && std::string(menu.GetItemText(0)) == "NAM Models" && std::string(menu.GetItemText(1)) == "Other Models");
  // Files cannot split folder entries, including in nested menus and unsorted snapshots.
  grouped.captures.clear();
  for (const char* name : {"Collection/Alpha.nam", "Collection/Zulu/cab.nam", "Collection/beta.nam",
                           "Collection/Bravo/leaf.nam", "Collection/Charlie/Z.nam",
                           "Collection/Charlie/alpha.nam", "Collection/Charlie/Deep/child.nam"})
  {
    auto entry = ampEntry; entry.relative = name; entry.path = root / entry.relative;
    grouped.captures.push_back(entry);
  }
  FillMenu(menu, grouped, Filter{}, PathText(grouped.captures[0].path), 0);
  assert(menu.NItems() == 5);
  for (int i = 0; i < 3; ++i) assert(menu.GetItem(i)->GetSubmenu());
  assert(std::string(menu.GetItemText(0)) == "Bravo" && std::string(menu.GetItemText(1)) == "Charlie"
         && std::string(menu.GetItemText(2)) == "Zulu");
  assert(!menu.GetItem(3)->GetSubmenu() && menu.GetItem(3)->GetChecked() && menu.GetItem(3)->GetTag() == 0);
  assert(!menu.GetItem(4)->GetSubmenu() && menu.GetItem(4)->GetTag() == 2);
  auto* charlie = menu.GetItem(1)->GetSubmenu();
  assert(charlie->GetItem(0)->GetSubmenu() && std::string(charlie->GetItemText(0)) == "Deep");
  assert(charlie->GetItem(1)->GetTag() == 5 && charlie->GetItem(2)->GetTag() == 4);
  const auto partial = ScanFolders({PathText(sourceA), PathText(root / "missing")});
  assert(partial.captures.size() == 1 && !partial.error.empty());
  const auto folders = std::vector<std::string>{PathText(sourceA), PathText(sourceB)};
  assert(WriteFolderSettings(preferences, folders));
  assert(ReadFolderSettings(preferences) == folders);
  assert(NormalizeFolders({folders[0], folders[0], folders[1]}).size() == 2);
  std::ofstream(preferences) << json{{"CaptureFolder", folders[0]}}; // Prototype key is not read.
  assert(ReadFolderSettings(preferences).empty());
  std::ofstream(preferences) << json{{"NamFolders", {1, "bad"}}};
  assert(ReadFolderSettings(preferences).empty());
  std::ofstream(preferences) << json{{"CaptureFolders", folders}}; // The pre-split combined list is not migrated.
  assert(ReadSettings(preferences).namFolders.empty() && ReadSettings(preferences).irFolders.empty());
  assert(ReadSettings(preferences).rescanOnStartup);
  assert(ReadSettings(preferences).showMainMacros);
  assert(WriteSettings(preferences, {folders, false, false}));
  // Replacement is atomic: no temporary siblings remain and the file always parses.
  for (const auto& sibling : std::filesystem::directory_iterator(preferences.parent_path()))
    assert(sibling.path().filename().string().find(".tmp-") == std::string::npos);
  assert(json::parse(std::ifstream(preferences)).is_object());
  assert(!ReadSettings(preferences).rescanOnStartup && ReadSettings(preferences).namFolders == folders);
  assert(!ReadSettings(preferences).showVisualizations);
  auto hiddenMacros = ReadSettings(preferences);
  hiddenMacros.showMainMacros = false;
  hiddenMacros.presetCrossfade = true;
  assert(WriteSettings(preferences, hiddenMacros));
  assert(!ReadSettings(preferences).showMainMacros);
  assert(WriteFolderSettings(preferences, {folders[0]}));
  assert(!ReadSettings(preferences).rescanOnStartup); // Editing folders must retain the startup preference.
  assert(!ReadSettings(preferences).showVisualizations);
  assert(!ReadSettings(preferences).showMainMacros && ReadSettings(preferences).presetCrossfade);
  assert(WriteSettings(preferences, {folders, true}));
  assert(ReadSettings(preferences).rescanOnStartup);
  assert(ReadSettings(preferences).showVisualizations);
  {
    // First use: the config folder and settings.json appear, and every list that was never
    // chosen gets its default folder, created below the content folder.
    const auto home = root / ("UserSettings-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto settingsFile = home / "config" / "settings.json";
    const auto first = PrepareUserSettings(settingsFile, {}, home);
    assert(first.saved && std::filesystem::exists(settingsFile));
    assert(first.settings.namFolders == std::vector<std::string>{PathText(home / "NAM Captures")});
    assert(first.settings.irFolders == std::vector<std::string>{PathText(home / "IRs")});
    assert(first.settings.presetFolders == std::vector<std::string>{PathText(home / "Presets")});
    assert(first.newPresetFolder == home / "Presets");
    for (const char* folder : {"NAM Captures", "IRs", "Presets"})
    {
      // Each default folder has a commented .ignore that shows the feature but hides nothing.
      assert(std::filesystem::is_directory(home / folder) && std::filesystem::exists(home / folder / ".ignore"));
      IgnoreRules rules(home / folder);
      assert(!rules.Ignored(home / folder / "DI.nam", false) && !rules.Ignored(home / folder / "Old captures", true));
    }
    // Later starts keep the file as it is and report no new preset folder.
    const auto again = PrepareUserSettings(settingsFile, {}, home);
    assert(again.newPresetFolder.empty() && again.settings.namFolders == first.settings.namFolders);
    // An explicitly emptied list stays empty; a list missing from the file is still defaulted.
    auto chosen = again.settings;
    chosen.irFolders.clear();
    assert(WriteSettings(settingsFile, chosen));
    auto stored = json::parse(std::ifstream(settingsFile));
    stored.erase("PresetFolders");
    std::ofstream(settingsFile) << stored;
    const auto edited = PrepareUserSettings(settingsFile, {}, home);
    assert(edited.settings.irFolders.empty() && edited.settings.namFolders == first.settings.namFolders);
    assert(edited.settings.presetFolders == first.settings.presetFolders && edited.newPresetFolder == home / "Presets");
    assert(ReadSettings(settingsFile).irChosen && ReadSettings(settingsFile).presetChosen);
    // A legacy library.json migrates once: its folders and preferences are kept, and its empty
    // lists (older builds had no defaults) receive the defaults.
    const auto migrated = home / "migrated" / "settings.json", legacy = home / "legacy" / "library.json";
    assert(WriteSettings(legacy, {folders, false, false}));
    const auto moved = PrepareUserSettings(migrated, legacy, home / "content");
    assert(moved.settings.namFolders == folders && !moved.settings.rescanOnStartup && !moved.settings.showVisualizations);
    assert(moved.settings.irFolders == std::vector<std::string>{PathText(home / "content" / "IRs")});
    assert(ReadSettings(migrated).namFolders == folders && std::filesystem::exists(legacy));
  }
  {
    // The JSON library index: a rescan reuses an unchanged file (same size and modification
    // time) without reading it, reads a changed one again and forgets a deleted one.
    const auto base = root / ("Index-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto library = base / "library", amp = library / "amp.nam", indexFile = base / "cache" / "library-index.json";
    std::filesystem::create_directories(library);
    const auto write = [&](const char* name) {
      std::ofstream(amp, std::ios::binary) << json{{"metadata", {{"name", name}, {"gear_type", "amp"}}}}.dump();
    };
    const std::vector<std::string> roots{PathText(library)};
    const auto scan = [&](IndexCache& index) {
      index.next.clear();
      const auto snapshot = ScanLibrary(roots, {}, {}, {}, &index);
      assert(snapshot.captures.size() <= 1 && index.next.size() == snapshot.captures.size());
      return snapshot.captures.empty() ? Capture{} : snapshot.captures.front();
    };
    write("Amp One");
    IndexCache index;
    const auto first = scan(index);
    assert(first.name == "Amp One" && !first.hash.empty() && first.relative == "amp.nam");
    assert(WriteIndex(indexFile, index.next));
    index.previous = ReadIndex(indexFile);
    assert(index.previous.size() == 1);
    const auto& stored = index.previous.begin()->second.capture;
    assert(stored.name == "Amp One" && stored.hash == first.hash && stored.gear == first.gear && stored.kind == FileKind::NAM);
    // Same size and restored time: the stale name and hash prove the file was not read again.
    const auto time = std::filesystem::last_write_time(amp);
    write("Amp Two");
    std::filesystem::last_write_time(amp, time);
    const auto reused = scan(index);
    assert(reused.name == "Amp One" && reused.hash == first.hash && reused.relative == "amp.nam");
    // A newer modification time makes the scan read the file.
    std::filesystem::last_write_time(amp, time + std::chrono::seconds(2));
    const auto reread = scan(index);
    assert(reread.name == "Amp Two" && reread.hash != first.hash);
    index.previous = index.next;
    std::filesystem::remove(amp);
    scan(index);
    assert(index.next.empty());
    // A damaged or older index is ignored: it only costs one full scan.
    std::ofstream(indexFile, std::ios::binary) << "{ not json";
    assert(ReadIndex(indexFile).empty());
    std::ofstream(indexFile, std::ios::binary) << json{{"Version", kIndexVersion + 1}, {"Files", json::array()}}.dump();
    assert(ReadIndex(indexFile).empty());
  }
  // Content identity is independent of filenames, roots, extension case and metadata hints.
  const auto identities = root / ("HashIdentity-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(identities / "deep" / "nested");
  const auto shallow = identities / "first.nam", deep = identities / "deep" / "nested" / "copy.NAM";
  std::ofstream(shallow, std::ios::binary) << "abc";
  std::filesystem::copy_file(shallow, deep, std::filesystem::copy_options::overwrite_existing);
  assert(ContentHash(shallow) == "a9993e364706816aba3e25717850c26c9cd0d89d");
  assert(ContentHash(deep) == ContentHash(shallow));
  std::ofstream(identities / "empty.bin", std::ios::binary);
  assert(ContentHash(identities / "empty.bin") == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
  // Content identity is a recall aid, not a menu filter: identical bytes stay listed at every
  // path so a pack folder remains complete; the count only reports repeats.
  const auto scanned = Scan(identities);
  assert(scanned.captures.size() == 2 && scanned.duplicates == 1);
  const auto containsPath = [](const Snapshot& snapshot, const std::filesystem::path& path) {
    return std::any_of(snapshot.captures.begin(), snapshot.captures.end(),
                       [&](const Capture& capture) { return capture.path == path; });
  };
  assert(containsPath(scanned, shallow) && containsPath(scanned, deep));
  const auto contentHash = ContentHash(deep);
  assert(FindHash(scanned, contentHash, false));
  assert(!FindHash(scanned, contentHash, true));
  std::filesystem::remove(deep); // A deleted copy must not block relinking to the surviving one.
  assert(FindHash(scanned, contentHash, false)->path == shallow);
  std::filesystem::copy_file(shallow, deep, std::filesystem::copy_options::overwrite_existing);
  const auto tie = ScanFolders({PathText(deep.parent_path()), PathText(identities)});
  // The same physical file reached through overlapping roots is still indexed once.
  assert(tie.captures.size() == 2 && tie.duplicates == 1 && containsPath(tie, deep) && containsPath(tie, shallow));
  const auto sibling = identities / "second.NAM";
  std::ofstream(sibling) << "different";
  std::ofstream(identities / "cab.wav") << "abc";
  const auto siblings = SiblingFiles(shallow, false);
  assert(siblings == std::vector<std::filesystem::path>({shallow, sibling}));
  assert(SiblingFiles(shallow, true) == std::vector<std::filesystem::path>({identities / "cab.wav"}));
  const auto typed = Scan(identities);
  assert(typed.captures.size() == 4); // A WAV and NAM with identical bytes are separate loader types.
  assert(FindHash(typed, ContentHash(shallow), true)->isIR);
  assert(ContentHash(shallow, [] { return true; }).empty());
  std::ofstream(identities / "large.bin", std::ios::binary) << std::string(131073, 'a');
  int chunks = 0;
  assert(ContentHash(identities / "large.bin", [&] { return ++chunks == 2; }).empty() && chunks == 2);
  // A later folder request cancels stale results and can safely outlive a menu snapshot.
  Worker worker;
  worker.Request(PathText(root));
  worker.Request(PathText(root / "missing"));
  std::shared_ptr<const Snapshot> result;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!(result = worker.TakeResult()) && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(result && result->root == root / "missing" && !result->error.empty());
  TestLibraryBrowser(root);
  TestLibraryIgnoreFiles(root);
  std::cout << "Library scan, typed browser, ranked search, preset folders, metadata, selection and cancellation passed\n";
}
