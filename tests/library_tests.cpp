#include "IPlugPlatform.h"
#include "ModuleMenu.h"
#include <array>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include "json.hpp"

using namespace nam_library;
using Menu = iplug::igraphics::IPopupMenu;
using json = nlohmann::json;

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
  auto write = [&](const std::filesystem::path& relative, const json& metadata) {
    const auto path = root / relative;
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path) << json{{"metadata", metadata}, {"weights", {1, 2, 3}}}.dump();
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
        if (empty)
        {
          assert(std::string(menu.GetItemText(index)) == "Scanning library...");
          assert(!menu.GetItem(index++)->GetEnabled());
        }
        if (filtered)
          assert(std::string(menu.GetItemText(index++)) == "Clear search and gear filter");
        for (const char* label :
             {"Choose .nam file...", "Choose cabinet IR (.wav)...", "Clear slot", "Noise Gate", "Compressor", "EQ",
              "Boost", "Reverb", "Delay", "Slapback preset", "Modulation", "Rotary / Vibe", "Dynamics / Pitch", "Creative", "Frequency Shifter", "Filter / Volume", "Airwindows Tape", "Tube Boost", "Utility"})
          assert(std::string(menu.GetItemText(index++)) == label);
        auto* reverbs = menu.GetItem(index - 12)->GetSubmenu();
        assert(reverbs && reverbs->NItems() == 5);
        assert(reverbs->GetItem(3)->GetChecked() && reverbs->GetItem(3)->GetTag() == -46);
        assert(menu.GetItem(index++)->GetIsSeparator());
        if (quality)
        {
          assert(std::string(menu.GetItemText(index)) == "A2 Lite");
          assert(menu.GetItem(index)->GetChecked() && menu.GetItem(index++)->GetTag() == -15);
        }
        assert(index < menu.NItems()); // Original library tree/empty placeholder survives.
        for (int item = 0, separators = 0; item < menu.NItems(); ++item)
        {
          separators += menu.GetItem(item)->GetIsSeparator();
          assert(separators <= 1);
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
  assert(mixed.captures[0].relative.begin()->string() == "Library");
  assert(ir.relative.begin()->string() == "Library (2)");
  assert((Filter{"IR WAV", static_cast<int>(Gear::Cabinet)}.Matches(ir)));
  assert((!Filter{"NAM", -1}.Matches(ir)));
  FillMenu(menu, mixed, Filter{"", -1}, PathText(wavFile));
  assert(menu.NItems() == 2);
  auto* irLeaf = menu.GetItem(1)->GetSubmenu()->GetItem(0)->GetSubmenu()->GetItem(0);
  assert(irLeaf->GetChecked() && mixed.captures[irLeaf->GetTag()].isIR);
  const auto partial = ScanFolders({PathText(sourceA), PathText(root / "missing")});
  assert(partial.captures.size() == 1 && !partial.error.empty());
  const auto folders = std::vector<std::string>{PathText(sourceA), PathText(sourceB)};
  assert(WriteFolderSettings(preferences, folders));
  assert(ReadFolderSettings(preferences) == folders);
  assert(NormalizeFolders({folders[0], folders[0], folders[1]}).size() == 2);
  std::ofstream(preferences) << json{{"CaptureFolder", folders[0]}};
  assert(ReadFolderSettings(preferences) == std::vector<std::string>{folders[0]});
  std::ofstream(preferences) << json{{"CaptureFolders", {1, "bad"}}};
  assert(ReadFolderSettings(preferences).empty());
  assert(ReadSettings(preferences).rescanOnStartup);
  assert(WriteSettings(preferences, {folders, false}));
  assert(!ReadSettings(preferences).rescanOnStartup && ReadSettings(preferences).folders == folders);
  assert(WriteFolderSettings(preferences, {folders[0]}));
  assert(!ReadSettings(preferences).rescanOnStartup); // Editing folders must retain the startup preference.
  assert(WriteSettings(preferences, {folders, true}));
  assert(ReadSettings(preferences).rescanOnStartup);
  // A later folder request cancels stale results and can safely outlive a menu snapshot.
  Worker worker;
  worker.Request(PathText(root));
  worker.Request(PathText(root / "missing"));
  std::shared_ptr<const Snapshot> result;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!(result = worker.TakeResult()) && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(result && result->root == root / "missing" && !result->error.empty());
  std::cout << "Library scan, metadata, hints, search, nested selection, missing folders and cancellation passed\n";
}
