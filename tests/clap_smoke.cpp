#include "WavFixtures.h"
#include "IsolatedUserHome.h"
#include "A2Fixture.h"
#include "ClapTestHost.h"
#include <atomic>
#include <thread>
#include <cstdlib>
#include "WinEditorCapture.h"
#include "WinEditMenuDriver.h"
#ifdef _WIN32
#include <commctrl.h>
#endif
#include "../NeuralAmpModeler/EffectParameters.h"
#include "../NeuralAmpModeler/SlotDefaults.h"
#include "../NeuralAmpModeler/ModuleCatalog.h"
#include "../NeuralAmpModeler/GridPerformance.h"
#include "../NeuralAmpModeler/ParameterLayout.h"
#include "RoadmapClapTests.h"
#include "ModulationClapTests.h"
#include "EffectModeClapTests.h"
#include "PedalDriveClapTests.h"
#include "PedalCompClapTests.h"
#include "ModulePresetClapTests.h"
#include "PresetGainClapTests.h"
#include "CachedControlsClapTests.h"
#include "PerformanceSceneClapTests.h"
#include <cstdlib>
#include "WinFileDialogDriver.h"
#include "LinuxEditorSmoke.h"
#include "../NeuralAmpModeler/GridLayout.h"
#include "../NeuralAmpModeler/EffectPresentation.h"
#include "../NeuralAmpModeler/CaptureLibrary.h"
#include "LibraryStartupClapTests.h"
// Header box centres (GridLayout.h), so tests follow the header layout.
constexpr int kPreviousX = grid_layout::previousLeft + grid_layout::arrowWidth / 2;
constexpr int kNextX = grid_layout::nextLeft + grid_layout::arrowWidth / 2;
constexpr int kNewX = grid_layout::newLeft + grid_layout::fileButtonWidth / 2;
constexpr int kSaveX = grid_layout::saveLeft + grid_layout::fileButtonWidth / 2;
constexpr int kModulationX = (grid_layout::modulationLeft + grid_layout::modulationRight) / 2;
constexpr int kTunerX = (grid_layout::tunerLeft + grid_layout::tunerRight) / 2;
int main(int argc, char** argv)
{
  std::cout << std::unitbuf;
  assert(argc == 3 || argc == 4);
#ifdef _WIN32
  if (argc == 4 && std::strcmp(argv[3], "--library-startup-only") == 0)
  {
    // Canonical scan paths can differ from the host's temporary-path spelling.
    // Exercise that distinction on every Windows startup test, including local runs.
    auto temporary = std::filesystem::temp_directory_path().native();
    CharUpperBuffW(temporary.data(), static_cast<DWORD>(temporary.size()));
    const bool set = SetEnvironmentVariableW(L"TMP", temporary.c_str());
    assert(set);
  }
#endif
  IsolatedUserHome settingsBackup("nam-grid-smoke"); // Never the user's settings or library.
#ifdef _WIN32
  const bool fxVisuals = std::getenv("NAM_GRID_FX_VISUALS") != nullptr;
  const bool browserOnly = std::getenv("NAM_GRID_BROWSER_SMOKE") != nullptr;
  auto library = LoadLibraryW(std::filesystem::path(argv[1]).c_str());
  assert(library);
  auto* entry = reinterpret_cast<const clap_plugin_entry*>(GetProcAddress(library, "clap_entry"));
#else
  auto library = dlopen(argv[1], RTLD_NOW);
  assert(library);
  auto* entry = reinterpret_cast<const clap_plugin_entry*>(dlsym(library, "clap_entry"));
#endif
  assert(entry && entry->init(argv[1]));
  auto* factory = static_cast<const clap_plugin_factory*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
  assert(factory && factory->get_plugin_count(factory) == 1);
  const auto* descriptor = factory->get_plugin_descriptor(factory, 0);
  assert(std::strcmp(descriptor->name, "NAM Grid") == 0);
  assert(std::strcmp(descriptor->id, "io.github.fannon.nam-grid") == 0);
  assert(std::strcmp(descriptor->vendor, "Simon Heimler") == 0);
  Host host;
  {
    // A first start creates settings.json, the three default folders and an Empty preset.
    auto* first = factory->create_plugin(factory, &host.host, descriptor->id);
    assert(first && first->init(first));
    const auto settings = nam_library::ReadSettings(settingsBackup.file);
    assert(settings.namChosen && settings.irChosen && settings.presetChosen);
    assert(settings.namFolders == nam_library::NormalizeFolders({nam_library::PathText(settingsBackup.home / "NAM Captures")}));
    assert(settings.irFolders == nam_library::NormalizeFolders({nam_library::PathText(settingsBackup.home / "IRs")}));
    assert(settings.presetFolders == nam_library::NormalizeFolders({nam_library::PathText(settingsBackup.home / "Presets")}));
    const auto empty = rig_preset::Read(settingsBackup.home / "Presets" / "Empty.ngrid");
    for (int slot = 0; slot < 16; ++slot)
      assert(empty["SlotEffects"][slot] == static_cast<int>(SlotEffect::NAM) && empty["SlotPaths"][slot] == "");
    // The bundled capture lands in NAM Captures, and Clean Default points at that copy.
    const auto capture = settingsBackup.home / "NAM Captures" / "riffsmith-clean.nam";
    assert(std::filesystem::is_regular_file(capture));
    assert(nam_library::ContentHash(capture) == nam_library::ContentHash(
      std::filesystem::path(argv[2]) / "NeuralAmpModeler/resources/defaults/riffsmith-clean.nam"));
    const auto clean = rig_preset::Read(settingsBackup.home / "Presets" / "Clean Default.ngrid");
    assert(clean["PresetName"] == "Clean Default" && clean["SlotPaths"][4] == nam_library::PathText(capture));
    assert(clean["SlotHashes"][4] == nam_library::ContentHash(capture));
    // Fresh installs include all drive examples, without capture dependencies.
    for (const auto& example : drive_presets::Make(empty))
    {
      const auto file = settingsBackup.home / "Presets" / "Drive Examples"
                        / (example["PresetName"].get<std::string>() + ".ngrid");
      assert(rig_preset::Read(file) == example);
    }
    // Bundled rigs ship without stored scenes: a stale scene would change modules on recall.
    for (const auto& rig : {empty, clean})
      for (const auto& scene : rig["PerformanceScenes"])
        assert(scene.empty());
    // The first scan must include installed defaults, without a manual rescan.
    const auto indexFile = settingsBackup.home / "cache" / "library-index.json";
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!std::filesystem::exists(indexFile) && std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(std::filesystem::exists(indexFile));
    const auto index = nam_library::ReadIndex(indexFile);
    for (const auto& file : {capture, settingsBackup.home / "Presets" / "Empty.ngrid",
                             settingsBackup.home / "Presets" / "Clean Default.ngrid"})
      assert(std::any_of(index.begin(), index.end(), [&](const auto& entry) {
        return std::filesystem::equivalent(entry.second.capture.path, file);
      }));
    first->destroy(first);
    std::cout << "First start: settings folder, default library folders, bundled capture, presets and first index passed\n";
    if (argc == 4 && std::strcmp(argv[3], "--library-startup-only") == 0)
    {
      TestLibraryStartup(factory, host, settingsBackup);
      entry->deinit();
#ifdef _WIN32
      FreeLibrary(library);
#else
      dlclose(library);
#endif
      return 0;
    }
    // The rest of the test starts from its own preferences and no default folders.
    nam_library::Settings preferences;
    preferences.showVisualizations = true;
    preferences.presetCrossfade = false; // The preset switch measurement starts from Short fade.
    assert(nam_library::WriteSettings(settingsBackup.file, preferences));
  }
  auto* plugin = factory->create_plugin(factory, &host.host, factory->get_plugin_descriptor(factory, 0)->id);
  assert(plugin && plugin->init(plugin));
  auto* state = static_cast<const clap_plugin_state*>(plugin->get_extension(plugin, CLAP_EXT_STATE));
  auto* params = static_cast<const clap_plugin_params*>(plugin->get_extension(plugin, CLAP_EXT_PARAMS));
  auto* ports = static_cast<const clap_plugin_audio_ports*>(plugin->get_extension(plugin, CLAP_EXT_AUDIO_PORTS));
  assert(state && params && ports);
  auto save = [&] {
    Stream s;
    clap_ostream out{&s, Stream::write};
    assert(state->save(plugin, &out));
    return s;
  };
  auto loadBytes = [&](Stream s) {
    clap_istream in{&s, Stream::read};
    return state->load(plugin, &in);
  };
  auto load = [&](json& config) {
    // These fixtures explicitly choose paths. Derive their new identities from
    // those files; raw loadBytes below separately tests saved-hash relocation.
    config["SlotHashes"] = json::array();
    for (int i = 0; i < 16; ++i)
    {
      const int effect = config["SlotEffects"][i].get<int>();
      const auto path = config["SlotPaths"][i].get<std::string>();
      config["SlotHashes"].push_back(
        (effect == static_cast<int>(SlotEffect::NAM) || effect == static_cast<int>(SlotEffect::CabinetIR)) && !path.empty()
          && std::filesystem::is_regular_file(std::filesystem::u8path(path))
          ? nam_library::ContentHash(std::filesystem::u8path(path)) : "");
    }
    Stream s;
    s.string("###nam-grid###");
    s.string(config.dump());
    assert(loadBytes(std::move(s)));
    // Identity resolution may choose a shallower indexed copy of the same file.
    // Keep round-trip expectations canonical; dedicated tests below assert the
    // chosen path/hash independently. All parameter expectations remain intact.
    const auto canonical = save().config();
    config["SlotPaths"] = canonical["SlotPaths"];
    config["SlotHashes"] = canonical["SlotHashes"];
  };
  auto loadRaw = [&](const json& config) {
    Stream s;
    s.string("###nam-grid###");
    s.string(config.dump());
    return loadBytes(std::move(s));
  };
  // Library folders are per-user settings, never project state: tests choose them in the
  // isolated settings.json, which every state load reads.
  const auto pathTexts = [](const std::vector<std::filesystem::path>& paths) {
    std::vector<std::string> texts;
    for (const auto& path : paths) texts.push_back(nam_library::PathText(path));
    return nam_library::NormalizeFolders(texts);
  };
  auto userFolders = [&] { return nam_library::ReadSettings(settingsBackup.file); };
  auto useFolders = [&](std::vector<std::filesystem::path> nam, std::vector<std::filesystem::path> ir,
                        std::vector<std::filesystem::path> presets) {
    auto settings = userFolders();
    settings.namFolders = pathTexts(nam); settings.irFolders = pathTexts(ir); settings.presetFolders = pathTexts(presets);
    assert(nam_library::WriteSettings(settingsBackup.file, settings));
  };
  auto base = save().config();
  assert(base["Lane Mode"] == grid_performance::Sequential);
  // The existing routing regressions explicitly exercise the released parallel path.
  base["Lane Mode"] = grid_performance::Parallel;
  load(base);
  assert(!base.contains("NamFolders") && !base.contains("IrFolders") && !base.contains("PresetFolders"));
  {
    // The bundled Clean Default rig loads as saved, with its capture from the default folder.
    auto clean = rig_preset::Read(settingsBackup.home / "Presets" / "Clean Default.ngrid");
    clean["Tuner"] = 0.; clean["Tuner Reference"] = 440.; clean["Tuner Mute"] = 1.;
    assert(loadRaw(clean));
    const auto loaded = save().config();
    assert(loaded["SlotPaths"][4] == clean["SlotPaths"][4] && loaded["SlotEffects"] == clean["SlotEffects"]);
    assert(loaded["Pre 1 Black Hold"] == 0.); // Saved before Hold existed: schema 1 defaults it.
    assert(loaded["Lane Mode"] == grid_performance::Sequential);
    assert(loadRaw(base));
  }
  assert(base["Tuner"] == 0. && base["schema"] == kEffectStateSchema);
  {
    auto crossfade = base;
    auto legacy = base;
    legacy["schema"] = kEffectParameters[uDelayTimeChange].introducedIn - 1;
    for (int slot = 0; slot < 16; ++slot)
    {
      const auto key = GridSlotName(slot) + " " + kEffectParameters[uDelayTimeChange].name;
      assert(base[key] == 0.); // New inserts and existing rigs retain Glide.
      crossfade[key] = 1.;
      crossfade["PerformanceScenes"][0][key] = 1.;
      legacy.erase(key);
      for (auto& scene : legacy["PerformanceScenes"]) scene.erase(key);
      clap_param_info info{};
      assert(params->get_info(plugin, SlotUtilityParam(slot, uDelayTimeChange), &info));
      assert(!(info.flags & CLAP_PARAM_IS_AUTOMATABLE));
    }
    assert(loadRaw(crossfade));
    assert(save().config() == crossfade);
    for (const double invalid : {-1., .5, 2.})
    {
      auto bad = crossfade;
      bad["Pre 1 Delay Time Change"] = invalid;
      assert(!loadRaw(bad));
      assert(save().config() == crossfade);
    }
    auto missing = crossfade;
    missing.erase("Pre 1 Delay Time Change");
    assert(!loadRaw(missing));
    assert(save().config() == crossfade);
    assert(loadRaw(legacy));
    assert(save().config() == base);
    assert(loadRaw(base));
  }
  // Appended two-way modes (Chorus Voices, Spring Tank): new and older rigs keep mode 0,
  // mode 1 round-trips, and invalid or missing values are rejected.
  for (const int control : {uChorusVoices, uSpringTank})
  {
    auto chosen = base;
    auto legacy = base;
    legacy["schema"] = kEffectParameters[control].introducedIn - 1;
    for (int slot = 0; slot < 16; ++slot)
    {
      const auto key = GridSlotName(slot) + " " + kEffectParameters[control].name;
      assert(base[key] == 0.);
      chosen[key] = 1.;
      chosen["PerformanceScenes"][0][key] = 1.;
      legacy.erase(key);
      for (auto& scene : legacy["PerformanceScenes"]) scene.erase(key);
      clap_param_info info{};
      assert(params->get_info(plugin, SlotUtilityParam(slot, control), &info));
      assert(!(info.flags & CLAP_PARAM_IS_AUTOMATABLE));
    }
    const auto first = std::string("Pre 1 ") + kEffectParameters[control].name;
    assert(loadRaw(chosen));
    assert(save().config() == chosen);
    for (const double invalid : {-1., .5, 2.})
    {
      auto bad = chosen;
      bad[first] = invalid;
      assert(!loadRaw(bad));
      assert(save().config() == chosen);
    }
    auto missing = chosen;
    missing.erase(first);
    assert(!loadRaw(missing));
    assert(loadRaw(legacy));
    assert(save().config() == base);
    assert(loadRaw(base));
  }
  {
    assert(base["Mix Law"] == 0.);
    auto constantDry = base;
    constantDry["Mix Law"] = 1.;
    assert(loadRaw(constantDry));
    assert(save().config()["Mix Law"] == 1.);
    auto legacy = base;
    legacy["schema"] = 8;
    legacy.erase("Mix Law");
    assert(loadRaw(legacy));
    assert(save().config()["Mix Law"] == 0.);
    assert(loadRaw(base));
  }
  {
    assert(base["Trails"] == 0.);
    auto trails = base;
    trails["Trails"] = 1.;
    assert(loadRaw(trails));
    assert(save().config()["Trails"] == 1.);
    for (const int schema : {9, 10})
    {
      assert(loadRaw(trails));
      auto legacy = base;
      legacy["schema"] = schema;
      legacy.erase("Trails");
      assert(loadRaw(legacy));
      assert(save().config()["Trails"] == 0.);
    }
    assert(loadRaw(base));
  }

  {
    auto legacyEQ = base;
    legacyEQ["schema"] = 4;
    for (int slot = 0; slot < 16; ++slot)
      for (int p = uParamLowQ; p <= uParamLowMode; ++p)
        legacyEQ.erase(GridSlotName(slot) + " " + kEffectParameters[p].name);
    assert(loadRaw(legacyEQ));
    assert(save().config() == base);
  }
  assert(params->count(plugin) == 5 + grid_performance::Count + 16 * (17 + kUtilityControls));
  // Every slot exposes the same host controls; no upstream single-amp parameters remain.
  for (const char* name : {"Threshold", "Bass", "ToneStack", "IRToggle", "OutputMode", "CalibrateInput",
                           "Slim", "Drive" " Input", "Amp" " Trim", "DrivePath", "NAMPath", "IRPath"})
    assert(!base.contains(name));
  for (const char* name : {"Pre 1 NAM Input", "Pre 2 NAM Input", "Pre 3 NAM Input", "Post 4 NAM Input"})
    for (double db : {-40., -30., 30., 40.})
    {
      clap_param_info info{};
      bool found = false;
      for (uint32_t i = 0; i < params->count(plugin); ++i)
      {
        assert(params->get_info(plugin, i, &info));
        if (std::strcmp(info.name, name) == 0)
        {
          found = true;
          break;
        }
      }
      assert(found);
      double value = 0.;
      char text[64]{};
      assert(params->text_to_value(plugin, info.id, std::to_string(db).c_str(), &value));
      assert(params->value_to_text(plugin, info.id, value, text, sizeof(text)));
      assert(std::abs(std::stod(text) - db) < 1.e-9);
    }
  const auto fixtures = std::filesystem::current_path() / "fixtures";
  std::filesystem::create_directories(fixtures);
  auto a2 = fixture::model(3, 123), other = fixture::model(8, 456);
  const auto path1 = fixtures / "drive.nam", path2 = fixtures / "amp.nam";
  std::ofstream(path1) << a2.dump();
  std::ofstream(path2) << other.dump();
  base["SlotPaths"][0] = path1.string();
  base["SlotPaths"][1] = path2.string();
  base["Pre 1 NAM Input"] = 2.;
  base["Pre 1 NAM Output"] = -3.;
  base["Pre 2 NAM Input"] = 1.;
  base["Pre 2 NAM Output"] = -2.;
  base["Output"] = -6.;
  useFolders({fixtures}, {fixtures}, {});
  load(base);
  assert(save().config() == base);
  auto recalled = save();
  assert(loadBytes(recalled));
  assert(save().config() == base);
  // Macro names are rig metadata, with defaults for older presets and atomic rejection
  // of malformed names. They never change the stable automation parameter names or values.
  {
    auto named = base;
    named["MacroNames"] = json::array({"Drive", "Space", "Motion", "Level"});
    load(named);
    assert(save().config() == named);
    auto legacy = base;
    legacy.erase("MacroNames");
    load(legacy);
    assert(save().config() == base);
    for (const auto& badNames : {json("Drive"), json::array({"Drive"}), json::array({"Drive", 2, "Motion", "Level"}),
                                json::array({std::string(129, 'x'), "Space", "Motion", "Level"}),
                                json::array({"Drive\n", "Space", "Motion", "Level"})})
    {
      auto malformed = base;
      malformed["MacroNames"] = badNames;
      assert(!loadRaw(malformed));
      assert(save().config() == base);
    }
  }
  // Only the current header and schema load. Prototype/upstream formats, unknown
  // schemas and newer versions are rejected without mutating the current rig.
  for (const char* header : {"###NAMGrid" "-v1###", "###NAMDual-v1###", "###NeuralAmpModeler###"})
  {
    Stream foreign;
    foreign.string(header);
    foreign.string(base.dump());
    assert(!loadBytes(foreign));
    assert(save().config() == base);
  }
  // Integers outside int must not wrap into a valid schema or module ID.
  for (const json& schema : {json(0), json(kEffectStateSchema + 1), json(26), json(99), json(4294967297ll),
                             json(-4294967295ll), json(18446744073709551615ull), json(1.)})
  {
    auto unsupported = base;
    unsupported["schema"] = schema;
    assert(!loadRaw(unsupported));
    assert(save().config() == base);
  }
  for (const char* required : {"schema", "SlotPaths", "SlotEffects", "SlotHashes", "NoisePrintProfiles",
                               "PerformanceScenes", "PerformanceSceneDeltas", "Pre 1 NAM Input", "Post 4 Black Mode", "Tuner"})
  {
    auto incomplete = base;
    incomplete.erase(required);
    assert(!loadRaw(incomplete));
    assert(save().config() == base);
  }
  // Malformed state must leave the previously committed state intact.
  for (int slot : {0, 1, 15})
  {
    auto invalidPath = base;
    invalidPath["SlotPaths"][slot] = std::string("bad\0path", 8);
    assert(!loadRaw(invalidPath));
    assert(save().config() == base);
  }
  // The byte strings themselves must not silently truncate at an embedded NUL.
  for (bool corruptHeader : {false, true})
  {
    Stream malformed;
    malformed.string(std::string("###nam-grid###") + (corruptHeader ? std::string("\0junk", 5) : ""));
    malformed.string(base.dump() + (corruptHeader ? "" : std::string("\0junk", 5)));
    assert(!loadBytes(malformed));
    assert(save().config() == base);
  }
  // Folder lists in a state (from earlier builds or another machine) are ignored: loading a
  // project never replaces this user's library folders.
  for (const char* folderField : {"NamFolders", "IrFolders", "PresetFolders"})
  {
    const auto before = userFolders();
    auto withFolders = base;
    withFolders[folderField] = json::array({(fixtures / "elsewhere").string()});
    assert(loadRaw(withFolders));
    assert(save().config() == base);
    const auto after = userFolders();
    assert(after.namFolders == before.namFolders && after.irFolders == before.irFolders
           && after.presetFolders == before.presetFolders);
  }
  auto namEQState = base;
  namEQState["Pre 1 EQ Bass"] = 6.;
  namEQState["Pre 1 EQ Mid"] = -3.;
  load(namEQState);
  assert(save().config() == namEQState);
  auto utilityState = base;
  utilityState["SlotEffects"][0] = static_cast<int>(SlotEffect::ParametricEQ);
  utilityState["SlotPaths"][0] = "";
  utilityState["Pre 1 Parametric Frequency"] = 1200.;
  utilityState["Pre 1 Parametric Gain"] = -5.;
  utilityState["Pre 1 Parametric Q"] = 1.3;
  utilityState["SlotEffects"][15] = static_cast<int>(SlotEffect::Boost);
  utilityState["Post 4 Boost Gain"] = 9.;
  load(utilityState);
  assert(save().config() == utilityState);
  // A built-in module cannot carry a file path.
  auto pathOnEffect = utilityState;
  pathOnEffect["SlotPaths"][0] = path1.string();
  assert(!loadRaw(pathOnEffect));
  assert(save().config() == utilityState);
  useFolders({fixtures, fixtures / "IRs"}, {fixtures / "IRs"}, {});
  auto multipleFolders = base;
  load(multipleFolders);
  assert(save().config() == multipleFolders && userFolders().namFolders.size() == 2);
  load(base);
  Stream bad;
  bad.string("###nam-grid###");
  bad.string("{bad JSON");
  assert(!loadBytes(bad));
  assert(save().config() == base);
  for (int length : {-1, 2147483647})
  {
    Stream malformed;
    auto* bytes = reinterpret_cast<uint8_t*>(&length);
    malformed.bytes.insert(malformed.bytes.end(), bytes, bytes + 4);
    assert(!loadBytes(malformed));
    assert(save().config() == base);
  }
  for (const json& badEffect : {json(-1), json(63), json(static_cast<int>(SlotEffect::Last) + 1), json(1.5), json("gate"),
                                json(18446744073709551615ull), json(4294967297ll), json(-4294967295ll)})
  {
    auto malformedEffect = base;
    malformedEffect["SlotEffects"][15] = badEffect;
    assert(!loadRaw(malformedEffect));
    assert(save().config() == base);
  }
  // Missing/invalid paths remain available for relinking, while their DSP is cleared.
  auto missing = base;
  missing["SlotPaths"][0] = (fixtures / "missing.nam").string();
  load(missing);
  assert(save().config() == missing);
  std::ofstream(fixtures / "invalid.nam") << "invalid";
  missing["SlotPaths"][1] = (fixtures / "invalid.nam").string();
  load(missing);
  assert(save().config() == missing);
  load(base);
  clap_audio_port_info inInfo{}, outInfo{};
  // Invalid later-slot paths or automation data must not partially change committed state.
  for (int badSlot : {4, 8, 15})
  {
    auto malformedGrid = base;
    malformedGrid["SlotPaths"][badSlot] = 123;
    Stream badGrid;
    badGrid.string("###nam-grid###");
    badGrid.string(malformedGrid.dump());
    assert(!loadBytes(badGrid));
    assert(save().config() == base);
  }
  auto missingLater = base;
  missingLater["SlotPaths"][15] = (fixtures / "missing-out4.nam").string();
  load(missingLater);
  assert(save().config() == missingLater);
  load(base);
  assert(ports->get(plugin, 0, true, &inInfo));
  assert(ports->get(plugin, 0, false, &outInfo));
  assert(inInfo.channel_count <= 2 && outInfo.channel_count <= 2);
  std::array<std::array<double, 512>, 2> input{}, output{};
  double *ins[] = {input[0].data(), input[1].data()}, *outs[] = {output[0].data(), output[1].data()};
  clap_audio_buffer in{nullptr, ins, inInfo.channel_count, 0, 0}, out{nullptr, outs, outInfo.channel_count, 0, 0};
  clap_input_events events{nullptr, [](const clap_input_events*) -> uint32_t { return 0; },
                           [](const clap_input_events*, uint32_t) -> const clap_event_header* { return nullptr; }};
  std::vector<clap_id> algorithmAutomation;
  clap_output_events sent{
    &algorithmAutomation, [](const clap_output_events* list, const clap_event_header* event) {
      if (event->space_id != CLAP_CORE_EVENT_SPACE_ID)
        return true;
      clap_id id = CLAP_INVALID_ID;
      if (event->type == CLAP_EVENT_PARAM_VALUE)
        id = reinterpret_cast<const clap_event_param_value*>(event)->param_id;
      else if (event->type == CLAP_EVENT_PARAM_GESTURE_BEGIN || event->type == CLAP_EVENT_PARAM_GESTURE_END)
        id = reinterpret_cast<const clap_event_param_gesture*>(event)->param_id;
      if (id != CLAP_INVALID_ID && IsAlgorithmMode(parameter_layout::UtilityControl(static_cast<int>(id))))
        static_cast<std::vector<clap_id>*>(list->ctx)->push_back(id);
      return true;
    }};
  clap_process process{};
  process.audio_inputs = &in;
  process.audio_outputs = &out;
  process.audio_inputs_count = process.audio_outputs_count = 1;
  process.in_events = &events;
  process.out_events = &sent;
  for (double rate : {48000., 44100., 96000.})
  {
    assert(plugin->activate(plugin, rate, 1, 512));
    assert(plugin->start_processing(plugin));
    auto start = std::chrono::steady_clock::now();
    double energy = 0.;
    for (int b = 0; b < 120; ++b)
    {
      int n = std::array<int, 6>{512, 1, 127, 64, 256, 3}[b % 6];
      for (int i = 0; i < n; ++i)
        input[0][i] = input[1][i] = .1 * std::sin((b * 512 + i) * .03);
      process.frames_count = n;
      assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
      for (int i = 0; i < n; ++i)
      {
        assert(std::isfinite(output[0][i]));
        energy += output[0][i] * output[0][i];
      }
      if (host.callback)
      {
        host.callback = false;
        plugin->on_main_thread(plugin);
      }
      pump();
    }
    auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    std::cerr << "energy=" << energy << " input/output channels=" << inInfo.channel_count << "/"
              << outInfo.channel_count << '\n';
    assert(energy > 0.);
    auto* latency = static_cast<const clap_plugin_latency*>(plugin->get_extension(plugin, CLAP_EXT_LATENCY));
    std::cout << "CLAP two distinct A2 files, state recall, variable blocks at " << rate << " Hz: " << ms
              << " ms / 19,260 samples; latency " << latency->get(plugin) << " samples\n";
    // Exercise a live same-rate replacement/clear and host automation.
    auto* info = params;
    clap_param_info automation{};
    for (uint32_t index = 0; index < info->count(plugin); ++index)
    {
      assert(info->get_info(plugin, index, &automation));
      if (std::strcmp(automation.name, "Pre 1 NAM Output") == 0)
        break;
    }
    clap_event_param_value change{};
    change.header.size = sizeof(change);
    change.header.type = CLAP_EVENT_PARAM_VALUE;
    change.param_id = automation.id;
    assert(params->text_to_value(plugin, automation.id, "-8", &change.value));
    change.note_id = -1;
    change.port_index = change.channel = change.key = -1;
    clap_input_events automationEvents{&change, [](const clap_input_events*) -> uint32_t { return 1; },
                                       [](const clap_input_events* e, uint32_t) -> const clap_event_header* {
                                         return &static_cast<clap_event_param_value*>(e->ctx)->header;
                                       }};
    params->flush(plugin, &automationEvents, &sent);
    double value = 0.;
    assert(params->get_value(plugin, automation.id, &value) && std::abs(value - change.value) < 1e-9);
    assert(std::abs(save().config()["Pre 1 NAM Output"].get<double>() + 8.) < 1e-9);
    for (int replacement = 0; replacement < 6; ++replacement)
    {
      auto next = base;
      if (replacement % 2 == 0)
        next["SlotPaths"][0] = "";
      load(next);
      if (host.callback)
      {
        host.callback = false;
        plugin->on_main_thread(plugin);
      }
      if (host.restart)
      {
        host.restart = false;
        plugin->stop_processing(plugin);
        plugin->deactivate(plugin);
        assert(plugin->activate(plugin, rate, 1, 512));
        assert(plugin->start_processing(plugin));
      }
      process.frames_count = 512;
      for (int block = 0; block < 4; ++block)
      {
        assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
        pump();
      }
    }
    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);
  }
  // Full grid: all 16 selections and their independent controls survive recall and process audio.
  auto fullGrid = base;
  for (int slot = 2; slot < 16; ++slot)
  {
    fullGrid["SlotPaths"][slot] = (slot % 2 ? path2 : path1).string();
    const auto label = std::string(slot < 4    ? "Pre "
                                   : slot < 8  ? "A "
                                   : slot < 12 ? "B "
                                               : "Post ")
                       + std::to_string(slot % 4 + 1);
    fullGrid[label + " NAM Input"] = .1 * slot;
    fullGrid[label + " NAM Output"] = -.2 * slot;
    fullGrid[label + " Enabled"] = slot % 3 == 0 ? 0. : 1.;
  }
  fullGrid["A/B Blend"] = 37.;
  load(fullGrid);
  assert(save().config() == fullGrid);
  for (double rate : {48000., 44100., 96000.})
  {
    assert(plugin->activate(plugin, rate, 1, 512));
    assert(plugin->start_processing(plugin));
    auto* latency = static_cast<const clap_plugin_latency*>(plugin->get_extension(plugin, CLAP_EXT_LATENCY));
    assert(latency->get(plugin) == (rate == 48000. ? 0 : rate == 44100. ? 324 : 504));
    for (int block = 0; block < 12; ++block)
    {
      process.frames_count = std::array<int, 6>{1, 3, 127, 64, 256, 512}[block % 6];
      assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
      for (uint32_t i = 0; i < process.frames_count; ++i)
        assert(std::isfinite(output[0][i]));
    }
    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);
  }
  assert(save().config() == fullGrid);
  auto cleared = base;
  cleared["SlotPaths"][0] = "";
  cleared["SlotPaths"][1] = "";
  load(cleared);
  assert(save().config() == cleared);
  if (const char* cacheOnly = std::getenv("NAM_GRID_CACHE_SMOKE"); cacheOnly && std::strcmp(cacheOnly, "1") == 0)
  {
    TestCachedControlsClap(plugin, cleared, load, fixtures);
    plugin->destroy(plugin); entry->deinit();
#ifdef _WIN32
    FreeLibrary(library);
#else
    dlclose(library);
#endif
    return 0;
  }
  auto* configs =
    static_cast<const clap_plugin_audio_ports_config*>(plugin->get_extension(plugin, CLAP_EXT_AUDIO_PORTS_CONFIG));
  assert(configs);
  for (uint32_t index = 0; index < configs->count(plugin); ++index)
  {
    clap_audio_ports_config config{};
    assert(configs->get(plugin, index, &config));
    assert(configs->select(plugin, config.id));
    assert(ports->get(plugin, 0, true, &inInfo) && ports->get(plugin, 0, false, &outInfo));
    in.channel_count = inInfo.channel_count;
    out.channel_count = outInfo.channel_count;
    assert(plugin->activate(plugin, 48000., 1, 512));
    assert(plugin->start_processing(plugin));
    process.frames_count = 512;
    assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
    for (int i = 0; i < 512; ++i)
    {
      assert(std::isfinite(output[0][i]));
      if (out.channel_count == 2)
        assert(output[0][i] == output[1][i]);
    }
    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);
  }
  // New utility controls survive project recall and act independently on real
  // stereo ports, including the final slot's expanded parameter stride.
  for (uint32_t index = 0; index < configs->count(plugin); ++index)
  {
    clap_audio_ports_config cfg{};
    assert(configs->get(plugin, index, &cfg));
    if (cfg.main_input_channel_count == 2 && cfg.main_output_channel_count == 2)
    {
      assert(configs->select(plugin, cfg.id));
      break;
    }
  }
  assert(ports->get(plugin, 0, true, &inInfo) && ports->get(plugin, 0, false, &outInfo));
  in.channel_count = inInfo.channel_count;
  out.channel_count = outInfo.channel_count;
  assert(in.channel_count == 2 && out.channel_count == 2);
  auto stereoUtility = cleared;
  stereoUtility["SlotEffects"][15] = static_cast<int>(SlotEffect::Utility);
  stereoUtility["Post 4 Utility Output"] = -6.;
  stereoUtility["Post 4 Utility Balance"] = -100.;
  stereoUtility["Post 4 Utility Invert L"] = 1.;
  stereoUtility["Tuner Reference"] = 442.;
  stereoUtility["Tuner Mute"] = 0.;
  load(stereoUtility);
  assert(save().config() == stereoUtility);
  assert(plugin->activate(plugin, 48000., 1, 512));
  assert(plugin->start_processing(plugin));
  for (int b = 0; b < 3; ++b)
  {
    for (int i = 0; i < 512; ++i)
    {
      input[0][i] = .2 * std::sin((b * 512 + i) * .05);
      input[1][i] = .1 * std::cos((b * 512 + i) * .03);
    }
    process.frames_count = 512;
    assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
    for (int i = 0; i < 512; ++i)
      assert(output[1][i] == 0.);
    assert(std::abs(output[0][100]) > .001);
  }
  plugin->stop_processing(plugin);
  plugin->deactivate(plugin);
  // A real CLAP transport drives repeats and can change tempo on an active instance.
  auto syncState = cleared;
  syncState["SlotEffects"][0] = static_cast<int>(SlotEffect::DigitalDelay);
  syncState["Pre 1 Tempo Sync"] = 1.;
  syncState["Pre 1 Tempo Division"] = 5.;
  syncState["Pre 1 Delay Mix"] = 100.;
  syncState["Pre 1 Delay Feedback"] = 0.;
  syncState["Pre 1 Delay Character"] = 0.;
  load(syncState);
  assert(save().config() == syncState);
  assert(plugin->activate(plugin, 48000., 1, 512));
  assert(plugin->start_processing(plugin));
  clap_event_transport transport{};
  transport.header.size = sizeof(transport);
  transport.header.type = CLAP_EVENT_TRANSPORT;
  transport.flags = CLAP_TRANSPORT_HAS_TEMPO;
  process.transport = &transport;
  for (double bpm : {120., 90., 60.})
  {
    transport.tempo = bpm;
    // Let time smoothing settle and previous repeats finish before the next impulse.
    for (int b = 0; b < 300; ++b)
    {
      input[0].fill(0.);
      input[1].fill(0.);
      process.frames_count = 512;
      assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
    }
    const int expected = static_cast<int>(48000. * 60. / bpm);
    int first = -1;
    for (int pos = 0; pos < expected + 512; pos += 512)
    {
      input[0].fill(0.);
      input[1].fill(0.);
      if (pos == 0)
        input[0][0] = input[1][0] = 1.;
      process.frames_count = 512;
      assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
      for (int i = 0; i < 512; ++i)
        if (first < 0 && std::abs(output[0][i]) > 1.e-5)
          first = pos + i;
    }
    assert(std::abs(first - expected) <= 1);
  }
  process.transport = nullptr;
  plugin->stop_processing(plugin);
  plugin->deactivate(plugin);
  load(cleared);
  clap_param_info masterInfo{};
  for (uint32_t i = 0; i < params->count(plugin); ++i)
  {
    assert(params->get_info(plugin, i, &masterInfo));
    if (std::strcmp(masterInfo.name, "Output") == 0)
      break;
  }
  assert(std::strcmp(masterInfo.name, "Output") == 0);
  auto setMaster = [&](double db) {
    clap_event_param_value value{};
    value.header.size = sizeof(value);
    value.header.type = CLAP_EVENT_PARAM_VALUE;
    value.param_id = masterInfo.id;
    value.note_id = value.port_index = value.channel = value.key = -1;
    assert(params->text_to_value(plugin, masterInfo.id, std::to_string(db).c_str(), &value.value));
    clap_input_events update{&value, [](const clap_input_events*) -> uint32_t { return 1; },
                             [](const clap_input_events* e, uint32_t) -> const clap_event_header* {
                               return &static_cast<clap_event_param_value*>(e->ctx)->header;
                             }};
    params->flush(plugin, &update, &sent);
  };
  for (double rate : {44100., 48000., 96000.})
  {
    std::vector<double> reference;
    for (int partition : {1, 3, 64, 127, 512})
    {
      auto masterState = cleared;
      masterState["Output"] = 0.;
      load(masterState);
      assert(plugin->activate(plugin, rate, 1, 512));
      assert(plugin->start_processing(plugin));
      int position = 0;
      auto render = [&](int n) {
        process.frames_count = n;
        for (int i = 0; i < n; ++i)
          input[0][i] = input[1][i] = .2 * std::cos(2. * 3.141592653589793 * 375. * (position + i) / rate);
        assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
        position += n;
      };
      for (int i = 0; i < 20; ++i)
        render(512);
      const double previous = output[0][511];
      setMaster(-20.);
      std::vector<double> result;
      result.reserve(4096);
      for (int pos = 0; pos < 4096;)
      {
        const int n = std::min(partition, 4096 - pos);
        render(n);
        if (pos == 0)
          assert(std::abs(output[0][0] - previous) < .012);
        for (int i = 0; i < n; ++i)
        {
          assert(std::isfinite(output[0][i]));
          result.push_back(output[0][i]);
        }
        pos += n;
      }
      if (reference.empty())
        reference = result;
      else
        for (int i = 0; i < 4096; ++i)
          assert(std::abs(reference[i] - result[i]) < 1.e-10);
      // Startup from a recalled level is immediate, not a fade from unity.
      plugin->stop_processing(plugin);
      plugin->deactivate(plugin);
      masterState["Output"] = -20.;
      load(masterState);
      assert(plugin->activate(plugin, rate, 1, 512));
      assert(plugin->start_processing(plugin));
      position = 0;
      render(1);
      const double dc = 1. / (1. + 2. * 3.141592653589793 * 5. / rate);
      assert(std::abs(output[0][0] - .02 * dc) < 1.e-8);
      plugin->stop_processing(plugin);
      plugin->deactivate(plugin);
    }
  }
  load(cleared);
  process.frames_count = 512;
  // A tiny PCM16 WAV exercises the reused IR loader/convolver and project recall.
  auto irFile = fixtures / "cabinet.wav";
  {
    std::ofstream wav(irFile, std::ios::binary);
    auto number = [&](uint32_t value, int bytes) {
      for (int b = 0; b < bytes; ++b)
        wav.put(static_cast<char>((value >> (8 * b)) & 255));
    };
    wav.write("RIFF", 4);
    number(44, 4);
    wav.write("WAVEfmt ", 8);
    number(16, 4);
    number(1, 2);
    number(1, 2);
    number(48000, 4);
    number(96000, 4);
    number(2, 2);
    number(16, 2);
    wav.write("data", 4);
    number(8, 4);
    number(16384, 2);
    number(8192, 2);
    number(4096, 2);
    number(0, 2);
  }
  // All four built-ins occupy independently saved slots in every row, including parallel lanes.
  auto dynamics = cleared;
  for (int slot = 0; slot < 16; ++slot)
    dynamics["SlotEffects"][slot] = static_cast<int>(SlotEffect::NoiseGate) + slot % 3; // gate, smooth, punch
  dynamics["Pre 1 Gate Threshold"] = -65.;
  dynamics["A 1 Gate Release"] = 75.;
  dynamics["B 3 Comp Release"] = 240.;
  dynamics["Post 4 Comp Makeup"] = 3.;
  load(dynamics);
  assert(save().config() == dynamics);
  auto effectsBytes = save();
  load(base);
  assert(loadBytes(effectsBytes));
  assert(save().config() == dynamics);
  for (double rate : {44100., 48000., 96000.})
  {
    assert(plugin->activate(plugin, rate, 1, 512));
    assert(plugin->start_processing(plugin));
    const auto* latency = static_cast<const clap_plugin_latency*>(plugin->get_extension(plugin, CLAP_EXT_LATENCY));
    assert(latency->get(plugin) == 0);
    double energy = 0.;
    for (int block = 0; block < 250; ++block)
    {
      const int n = std::array<int, 6>{1, 3, 64, 127, 256, 512}[block % 6];
      process.frames_count = n;
      for (int i = 0; i < n; ++i)
        input[0][i] = input[1][i] = .3 * std::sin((block * 512 + i) * .03);
      assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
      for (int i = 0; i < n; ++i)
      {
        assert(std::isfinite(output[0][i]));
        energy += output[0][i] * output[0][i];
      }
    }
    assert(energy > .01);
    clap_param_info target{};
    bool found = false;
    for (uint32_t i = 0; i < params->count(plugin); ++i)
    {
      assert(params->get_info(plugin, i, &target));
      if (std::strcmp(target.name, "Post 4 Comp Makeup") == 0)
      {
        found = true;
        break;
      }
    }
    assert(found);
    clap_event_param_value event{};
    event.header.size = sizeof(event);
    event.header.type = CLAP_EVENT_PARAM_VALUE;
    event.param_id = target.id;
    event.note_id = event.port_index = event.channel = event.key = -1;
    assert(params->text_to_value(plugin, target.id, "6", &event.value));
    clap_input_events changes{&event, [](const clap_input_events*) -> uint32_t { return 1; },
                              [](const clap_input_events* e, uint32_t) -> const clap_event_header* {
                                return &static_cast<clap_event_param_value*>(e->ctx)->header;
                              }};
    process.in_events = &changes;
    assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
    process.in_events = &events;
    assert(save().config()["Post 4 Comp Makeup"] == 6.);
    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);
    load(dynamics);
  }
  // Movable EQ and IR slots, including a file-based IR in a former NAM alias position.
  auto eqIR = cleared;
  eqIR["SlotEffects"][0] = static_cast<int>(SlotEffect::CabinetIR);
  eqIR["SlotPaths"][0] = irFile.string();
  eqIR["Pre 1 IR Output"] = -12.;
  eqIR["SlotEffects"][3] = static_cast<int>(SlotEffect::ToneEQ);
  eqIR["Pre 4 EQ Bass"] = 3.;
  eqIR["Pre 4 EQ Mid"] = -4.;
  eqIR["Pre 4 EQ Treble"] = 2.;
  eqIR["SlotEffects"][12] = static_cast<int>(SlotEffect::CutEQ);
  eqIR["Post 1 EQ Low Cut"] = 75.;
  eqIR["Post 1 EQ High Cut"] = 7500.;
  eqIR["Post 1 EQ Presence"] = -2.;
  load(eqIR);
  assert(save().config() == eqIR);
  {
    const auto stereoFile = fixtures / "stereo.wav";
    wav_fixture::Write(stereoFile, 16, false, {16384, 8192, 0, 4096}, -1, -1, false, 2);
    auto stereo = eqIR;
    stereo["SlotPaths"][0] = stereoFile.string();
    load(stereo);
    assert(save().config() == stereo);
    const auto recalled = save();
    load(cleared);
    assert(loadBytes(recalled));
    assert(save().config() == stereo);
    load(eqIR);
  }
  const auto eqIRBytes = save();
  {
    auto shaped = eqIR;
    for (int slot : {0, 15})
    {
      const auto prefix = GridSlotName(slot) + " ";
      shaped["SlotEffects"][slot] = static_cast<int>(SlotEffect::CabinetIR);
      shaped["SlotPaths"][slot] = irFile.string();
      shaped[prefix + "IR Input"] = 6.; shaped[prefix + "IR Mix"] = 25.;
      shaped[prefix + "IR Pre-delay"] = 37.5; shaped[prefix + "IR Low Cut"] = 200.;
      shaped[prefix + "IR High Cut"] = 5000.;
      shaped[prefix + "IR Length"] = 1.;
    }
    load(shaped);
    assert(save().config() == shaped);
    assert(loadBytes(save()));
    assert(save().config() == shaped);
    auto legacy = eqIR;
    legacy["schema"] = 6;
    for (int slot = 0; slot < 16; ++slot)
      for (int p = uIRInput; p <= uIRLength; ++p)
      {
        const auto key = GridSlotName(slot) + " " + kEffectParameters[p].name;
        legacy.erase(key);
        for (auto& scene : legacy["PerformanceScenes"]) scene.erase(key);
      }
    load(legacy);
    assert(save().config() == eqIR);
    for (const json& bad : {json(-1), json(2), json(.5)})
    {
      auto invalid = eqIR; invalid["Pre 1 IR Length"] = bad;
      assert(!loadRaw(invalid));
      assert(save().config() == eqIR);
    }
  }
  load(base);
  assert(loadBytes(eqIRBytes));
  assert(save().config() == eqIR);
  for (double rate : {44100., 48000., 96000.})
  {
    assert(plugin->activate(plugin, rate, 1, 512));
    assert(plugin->start_processing(plugin));
    const auto* latency = static_cast<const clap_plugin_latency*>(plugin->get_extension(plugin, CLAP_EXT_LATENCY));
    assert(latency->get(plugin) == 0);
    double energy = 0.;
    for (int b = 0; b < 120; ++b)
    {
      process.frames_count = std::array<int, 6>{1, 3, 64, 127, 256, 512}[b % 6];
      for (uint32_t i = 0; i < process.frames_count; ++i)
        input[0][i] = input[1][i] = .2 * std::sin((b * 512 + i) * .03);
      assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
      for (uint32_t i = 0; i < process.frames_count; ++i)
      {
        assert(std::isfinite(output[0][i]));
        energy += output[0][i] * output[0][i];
      }
    }
    assert(energy > .001);
    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);
  }
  auto missingIR = eqIR;
  missingIR["SlotPaths"][0] = (fixtures / "missing-slot.wav").string();
  load(missingIR);
  assert(save().config() == missingIR);
  auto invalidIR = eqIR;
  invalidIR["SlotPaths"][0] = (fixtures / "invalid.nam").string();
  load(invalidIR);
  assert(save().config() == invalidIR);
  load(base);
  // Full state recall and processing of every new effect at all supported test rates.
  auto timeEffects = cleared;
  for (int slot = 0; slot < 16; ++slot)
    timeEffects["SlotEffects"][slot] = static_cast<int>(SlotEffect::SynthFilter) + slot % 14; // SynthFilter..Tape
  timeEffects["Pre 3 Delay Time"] = 123.;
  timeEffects["Pre 4 Delay Character"] = 67.;
  timeEffects["A 2 Reverb Decay"] = 3.4;
  timeEffects["B 1 Mod Rate"] = 2.3;
  timeEffects["Post 2 Tape Drive"] = 5.;
  timeEffects["SlotEffects"][14] = static_cast<int>(SlotEffect::TubeBoost);
  timeEffects["Post 3 Tube Drive"] = 12.;
  timeEffects["SlotEffects"][0] = static_cast<int>(SlotEffect::SpringReverb);
  timeEffects["SlotEffects"][1] = static_cast<int>(SlotEffect::Wah);
  timeEffects["Pre 1 Spring Dwell"] = 5.;
  timeEffects["Pre 1 Spring Drip"] = 73.;
  timeEffects["Pre 2 Wah Position"] = 28.;
  timeEffects["Pre 2 Wah Resonance"] = 4.;
  timeEffects["Pre 3 Delay Ping-Pong"] = 1.;
  timeEffects["B 1 Chorus Spread"] = 85.;
  timeEffects["Post 3 Tube Asymmetry"] = 63.;
  timeEffects["Post 3 Tube Mix"] = 47.;
  timeEffects["A 2 Reverb Low Cut"] = 175.;
  timeEffects["A 2 Reverb Modulation"] = 71.;
  timeEffects["Pre 4 Delay Modulation"] = 83.;
  timeEffects["Pre 4 Delay Output"] = -4.;
  load(timeEffects);
  assert(save().config() == timeEffects);
  auto timeSaved = save();
  load(base);
  assert(loadBytes(timeSaved));
  assert(save().config() == timeEffects);
  for (double rate : {44100., 48000., 88200., 96000., 192000.})
  {
    assert(plugin->activate(plugin, rate, 1, 512));
    assert(plugin->start_processing(plugin));
    const auto* latency = static_cast<const clap_plugin_latency*>(plugin->get_extension(plugin, CLAP_EXT_LATENCY));
    assert(latency->get(plugin) == 0);
    const auto* tail = static_cast<const clap_plugin_tail*>(plugin->get_extension(plugin, CLAP_EXT_TAIL));
    assert(tail && tail->get(plugin) == static_cast<uint32_t>(rate * 60.));
    assert(tail->get(plugin) != std::numeric_limits<uint32_t>::max());

    for (int b = 0; b < 40; ++b)
    {
      process.frames_count = std::array<int, 6>{1, 3, 64, 127, 256, 512}[b % 6];
      for (uint32_t i = 0; i < process.frames_count; ++i)
        input[0][i] = input[1][i] = .2 * std::sin((b * 512 + i) * .03);
      assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
      for (uint32_t i = 0; i < process.frames_count; ++i)
        assert(std::isfinite(output[0][i]) && std::isfinite(output[1][i]));
    }
    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);
  }
  // The real host tail extension must stay bounded even with every slot a delay.
  auto allDelays = cleared;
  for (int slot = 0; slot < 16; ++slot)
    allDelays["SlotEffects"][slot] = static_cast<int>(SlotEffect::DigitalDelay);
  for (double rate : {44100., 48000., 96000.})
  {
    load(allDelays);
    assert(plugin->activate(plugin, rate, 1, 512));
    assert(plugin->start_processing(plugin));
    const auto* tail = static_cast<const clap_plugin_tail*>(plugin->get_extension(plugin, CLAP_EXT_TAIL));
    assert(tail && tail->get(plugin) == static_cast<uint32_t>(rate * 60.));
    process.frames_count = 64;
    input[0].fill(0.);
    input[1].fill(0.);
    assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
    assert(tail->get(plugin) == static_cast<uint32_t>(rate * 60.));
    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);
  }
  // One module must report enough time for Trails, including the final fade. In
  // particular, an ordinary reverb reports 47 s rather than the rig-wide 60 s cap.
  for (const auto& [effect, limit] : std::array{
         std::pair{SlotEffect::DigitalDelay, 30.}, std::pair{SlotEffect::HallReverb, 45.},
         std::pair{SlotEffect::GalacticSpace, 60.}, std::pair{SlotEffect::CosmicEcho, 60.}})
  {
    auto oneTrail = cleared;
    oneTrail["Trails"] = 1.;
    oneTrail["SlotEffects"][0] = static_cast<int>(effect);
    load(oneTrail);
    constexpr double rate = 48000.;
    assert(plugin->activate(plugin, rate, 1, 512));
    assert(plugin->start_processing(plugin));
    process.frames_count = 64;
    input[0].fill(0.);
    input[1].fill(0.);
    assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
    const auto* tail = static_cast<const clap_plugin_tail*>(plugin->get_extension(plugin, CLAP_EXT_TAIL));
    assert(tail && tail->get(plugin) >= static_cast<uint32_t>(rate * limit));
    assert(tail->get(plugin) <= static_cast<uint32_t>(rate * 60.));
    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);
  }
#include "NoisePrintClapTests.inc"
#include "LoFiTapeClapTests.inc"
  load(base);
  TestRoadmapClap(plugin, cleared, save, load);
  TestModulationMatrixClap(plugin, cleared, save, load, loadRaw);
  TestEffectModeClap(plugin, cleared, save, load);
  TestPedalDriveClap(plugin, cleared, save, load);
  TestPedalCompClap(plugin, cleared, save, load);
  TestModulePresetRecall(cleared, save, load, settingsBackup.home);
  TestModulePresetValueDomains(plugin);
  TestGlobalInputGain(plugin, cleared, load);
  TestCachedControlsClap(plugin, cleared, load, fixtures);
  TestPerformanceSceneState(plugin, cleared, save, load, loadRaw, settingsBackup.home);
  if (const char* headless = std::getenv("NAM_GRID_HEADLESS_SMOKE"); headless && std::strcmp(headless, "1") == 0)
  {
    plugin->destroy(plugin); entry->deinit();
#ifdef _WIN32
    FreeLibrary(library);
#else
    dlclose(library);
#endif
    std::cout << "Headless CLAP DSP, state, slot automation and model lifecycle checks passed\n";
    return 0;
  }
#ifdef __linux__
  TestLinuxEditor(plugin, host, [&] { return save().config(); }, argc == 4 ? argv[3] : nullptr);
#endif
#ifdef _WIN32
  // Create native editor attached to a hidden parent, without launching a visible application.
  const auto libraryFolder = fixtures / "library";
  const auto libraryFirst = libraryFolder / "Amps" / "Nested" / "amp.nam";
  const auto librarySecond = libraryFolder / "Pedals" / "Nested" / "drive.nam";
  std::filesystem::create_directories(libraryFirst.parent_path());
  std::filesystem::create_directories(librarySecond.parent_path());
  std::filesystem::copy_file(path2, libraryFirst, std::filesystem::copy_options::overwrite_existing);
  std::filesystem::copy_file(path1, librarySecond, std::filesystem::copy_options::overwrite_existing);
  // A second indexed copy of the same capture that sorts after the first: choosing it must not
  // be redirected to the alphabetically earlier copy.
  const auto libraryTwin = libraryFolder / "Pedals" / "Nested" / "z-drive.nam";
  std::filesystem::copy_file(path1, libraryTwin, std::filesystem::copy_options::overwrite_existing);
  const auto irLibrary = fixtures / "ir-library";
  std::filesystem::create_directories(irLibrary);
  const auto libraryIR = irLibrary / "cabinet.wav";
  std::filesystem::copy_file(irFile, libraryIR, std::filesystem::copy_options::overwrite_existing);
  auto browserState = cleared;
  useFolders({libraryFolder}, {irLibrary}, {});
  load(browserState);
  auto* gui = static_cast<const clap_plugin_gui*>(plugin->get_extension(plugin, CLAP_EXT_GUI));
  assert(gui);
  assert(gui->create(plugin, CLAP_WINDOW_API_WIN32, false));
  uint32_t editorWidth = 0, editorHeight = 0;
  assert(gui->get_size(plugin, &editorWidth, &editorHeight));
  assert(editorWidth == grid_layout::width && editorHeight == grid_layout::height);
  auto parent =
    CreateWindowExW(0, L"STATIC", L"NAM test", 0, 0, 0, grid_layout::width, grid_layout::height, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
  assert(parent);
  clap_window window{};
  window.api = CLAP_WINDOW_API_WIN32;
  window.win32 = parent;
  assert(gui->set_parent(plugin, &window));
  pump();
  if (argc == 4)
  {
    std::filesystem::create_directories(argv[3]);
    // Opt-in preview mode shows only this host's editor, without activating the window.
    SetWindowLongPtr(parent, GWL_STYLE, WS_POPUP);
    SetWindowPos(parent, HWND_TOP, 0, 0, grid_layout::width, grid_layout::height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
  }
  auto* editor = GetWindow(parent, GW_CHILD);
  assert(editor);
  // Background indexing runs independently; pump the native UI timer to publish its result.
  auto pumpFor = [&](int milliseconds) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    while (std::chrono::steady_clock::now() < deadline)
    {
      pump();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  };
  pumpFor(250);
  std::cout << "Native editor ready; checking slot edit menus\n";
  auto click = [&](int x, int y) {
    SendMessage(editor, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
    SendMessage(editor, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
    pumpFor(250);
  };
  // The bypass follows variable-width badges. Seek the first control after them,
  // stopping as soon as the actual Enabled parameter changes; the module menu is
  // further right. This also exercises layout before any paint in hidden hosts.
  const auto toggleHeader = [&] {
    double before = 0., after = 0.;
    assert(params->get_value(plugin, SlotActiveParam(0), &before));
    for (int x = 72; x < 280; x += 6)
    {
      SendMessage(editor, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, grid_layout::enabledY));
      SendMessage(editor, WM_LBUTTONUP, 0, MAKELPARAM(x, grid_layout::enabledY));
      pumpFor(1);
      assert(params->get_value(plugin, SlotActiveParam(0), &after));
      if (after != before) { pumpFor(30); return; }
    }
    assert(false && "Module header bypass did not respond");
  };
  const auto optionX = [](int p, SlotEffect effect = SlotEffect::NAM) {
    const auto bounds = effect_presentation::HeaderBounds(effect, p);
    return static_cast<int>((bounds.left + bounds.right) * .5f);
  };
  // The tile ID row selects without opening the browser; the name button below opens it.
  const auto selectTile = [&](int x, int y) {
    click(x, grid_layout::cardTop + grid_layout::rowPitch * ((y - grid_layout::cardTop) / grid_layout::rowPitch) + 14);
  };
  const auto shiftClick = [&](int x, int y) {
    SendMessage(editor, WM_LBUTTONDOWN, MK_LBUTTON | MK_SHIFT, MAKELPARAM(x, y));
    SendMessage(editor, WM_LBUTTONUP, MK_SHIFT, MAKELPARAM(x, y));
    pumpFor(250);
  };
  const auto selectChoice = [&](int control, int choice, int current, bool dismiss = false, SlotEffect effect = SlotEffect::NAM) {
    const bool header = effect_presentation::HeaderControl(control);
    const int position = effect_presentation::Position(effect, control);
    const int x = header ? optionX(control, effect) : grid_layout::KnobX(position);
    const int y = header ? grid_layout::optionsY : grid_layout::ToggleY(position);
    win_edit_menu::SelectChoice(editor, x, y, EffectChoices(control), choice, current, dismiss);
    pumpFor(50);
  };
  const auto checkLaneButtons = [&] {
    click(865, 530); // Mute A.
    assert(save().config()["Lane A Mute"] == 1. && save().config()["Lane A Solo"] == 0.);
    click(888, 530); // Solo A also unmutes A.
    assert(save().config()["Lane A Mute"] == 0. && save().config()["Lane A Solo"] == 1.);
    click(888, 572); // Solo B replaces Solo A.
    assert(save().config()["Lane A Solo"] == 0. && save().config()["Lane B Solo"] == 1.);
    assert(save().config()["Lane B Mute"] == 0.);
    click(865, 572); // Mute B clears Solo B.
    assert(save().config()["Lane B Mute"] == 1. && save().config()["Lane B Solo"] == 0.);
    click(865, 572); // Restore the full parallel mix.
    assert(save().config()["Lane A Mute"] == 0. && save().config()["Lane B Mute"] == 0.);
  };
  if (std::getenv("NAM_GRID_ROUTING_SMOKE"))
  {
    const int modeX = (grid_layout::parameterLeft + 100 + grid_layout::parameterRight - 304) / 2;
    const auto capture = [&](const char* name) {
      // Give routing screenshots real lane meters in the isolated host.
      if (std::strncmp(name, "routing-", 8) == 0)
      {
        assert(plugin->activate(plugin, 48000., 1, 512)); assert(plugin->start_processing(plugin));
        process.frames_count = 512;
        for (int block = 0; block < 32; ++block)
        {
          for (int i = 0; i < 512; ++i)
          {
            input[0][i] = .16 * std::sin((block * 512 + i) * .031);
            input[1][i] = .12 * std::sin((block * 512 + i) * .031 + .2);
          }
          assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
        }
        plugin->stop_processing(plugin); plugin->deactivate(plugin); pumpFor(20);
      }
      if (argc == 4) CaptureEditor(editor, std::filesystem::path(argv[3]) / name);
    };
    load(cleared);
    click(grid_layout::width - 36, 328);
    assert(save().config()["Lane Mode"] == grid_performance::Parallel); // Open without switching.
    capture("routing-parallel.bmp");
    click(grid_layout::width - 36, 328); // Already open: switch mode without closing.
    assert(save().config()["Lane Mode"] == grid_performance::Sequential);
    capture("routing-sequential.bmp");
    click(modeX, grid_layout::moduleButtonY);
    assert(save().config()["Lane Mode"] == grid_performance::Parallel);
    const auto dragRouting = [&](int position) {
      const int x = grid_layout::KnobX(position), y = grid_layout::KnobGestureY(position);
      SendMessage(editor, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x,y));
      SendMessage(editor, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(x,y-30));
      SendMessage(editor, WM_LBUTTONUP, 0, MAKELPARAM(x,y-30)); pumpFor(50);
    };
    dragRouting(4); dragRouting(5);
    assert(save().config()["Lane B Level"] != 0. && save().config()["Lane A Level"] == 0.);
    assert(save().config()["Lane B Balance"] != 0. && save().config()["Lane A Balance"] == 0.);
    checkLaneButtons();
    click(grid_layout::parameterLeft + 88, grid_layout::optionsY);
    assert(save().config()["Frequency Split"] == 1.);
    click(grid_layout::parameterRight - 210, grid_layout::moduleButtonY);
    assert(save().config()["Split Slope"] == 1.);
    click(grid_layout::parameterRight - 210, grid_layout::moduleButtonY);
    assert(save().config()["Split Slope"] == 0.);
    capture("routing-split.bmp");
    POINT originalCursor{}; GetCursorPos(&originalCursor);
    const auto hover = [&](int x, int y) {
      POINT at{x,y}; ClientToScreen(editor,&at); SetCursorPos(at.x,at.y);
      SendMessage(editor,WM_MOUSEMOVE,0,MAKELPARAM(x,y)); pumpFor(150);
    };
    hover(800,630); capture("routing-split-hover.bmp");
    click((grid_layout::settingsLeft + grid_layout::settingsRight)/2, grid_layout::headerY);
    capture("settings-rig-behaviour.bmp");
    click(130,532); click(370,532);
    assert(save().config()["Trails"] == 1. && save().config()["Mix Law"] == 1.);
    capture("settings-rig-behaviour-on.bmp");
    assert(save().config()["Trails"] == 1. && save().config()["Mix Law"] == 1.);
    click(956,28);
    for (const auto effect : {SlotEffect::ToneEQ, SlotEffect::Multiband, SlotEffect::PedalDrive,
                              SlotEffect::Wah, SlotEffect::RingMod, SlotEffect::OutboardChannel})
    {
      auto rig=cleared; rig["SlotEffects"][0]=static_cast<int>(effect);
      load(rig); selectTile(170,120);
      const auto before=save().config();
      hover(effect==SlotEffect::PedalDrive?900:780,600);
      const auto name="frequency-hover-"+std::to_string(static_cast<int>(effect))+".bmp";
      capture(name.c_str());
      assert(save().config()==before);
    }
    click(95,80); // Signal Monitor input.
    const auto before=save().config();
    hover(780,600); capture("signal-monitor-frequency-hover.bmp");
    assert(save().config()==before);
    SetCursorPos(originalCursor.x,originalCursor.y);
    gui->destroy(plugin); DestroyWindow(parent);
    plugin->destroy(plugin); entry->deinit(); FreeLibrary(library);
    std::cout << "Focused routing modes and frequency-hover state isolation passed\n";
    return 0;
  }
  {
    // Read the native tooltip, not an internal label property: the module title's
    // description must update on state loads and when the module changes.
    HWND tooltip = nullptr;
    std::pair<HWND, HWND*> match{editor, &tooltip};
    EnumThreadWindows(GetCurrentThreadId(), [](HWND candidate, LPARAM data) -> BOOL {
      const auto& target = *reinterpret_cast<std::pair<HWND, HWND*>*>(data);
      wchar_t kind[64]{}; GetClassNameW(candidate, kind, 64);
      if (lstrcmpW(kind, TOOLTIPS_CLASSW)) return TRUE;
      TOOLINFOW info{}; info.cbSize = TTTOOLINFOW_V2_SIZE;
      info.hwnd = target.first; info.uId = reinterpret_cast<UINT_PTR>(target.first);
      if (!SendMessageW(candidate, TTM_GETTOOLINFOW, 0, reinterpret_cast<LPARAM>(&info))) return TRUE;
      *target.second = candidate;
      return FALSE;
    }, reinterpret_cast<LPARAM>(&match));
    assert(tooltip);
    const auto descriptionTooltip = [&](int x = 200, int y = grid_layout::moduleButtonY) {
      InvalidateRect(editor, nullptr, FALSE); UpdateWindow(editor);
      SendMessage(editor, WM_MOUSEMOVE, 0, MAKELPARAM(x, y));
      SendMessage(editor, WM_MOUSEHOVER, 0, 0);
      std::array<wchar_t, 2048> text{};
      TOOLINFOW info{}; info.cbSize = TTTOOLINFOW_V2_SIZE;
      info.hwnd = editor; info.uId = reinterpret_cast<UINT_PTR>(editor); info.lpszText = text.data();
      SendMessageW(tooltip, TTM_GETTEXTW, text.size(), reinterpret_cast<LPARAM>(&info));
      return std::wstring(text.data());
    };
    auto previous = save().config();
    auto description = cleared;
    description["SlotEffects"][0] = static_cast<int>(SlotEffect::DigitalBlack);
    load(description); selectTile(170, 120);
    const auto beforeHover = save().config();
    const auto titleHelp = [](SlotEffect effect) {
      const auto text = effect_presentation::Sound(effect) + std::string(" Click to choose or replace the module.");
      return std::wstring(text.begin(), text.end());
    };
    assert(descriptionTooltip() == titleHelp(SlotEffect::DigitalBlack));
    assert(save().config() == beforeHover);
    description["Pre 1 Enabled"] = 0.; load(description); selectTile(170, 120);
    assert(descriptionTooltip() == titleHelp(SlotEffect::DigitalBlack)); // The enable circle shows bypass.
    description["Pre 1 Enabled"] = 1.;
    description["SlotEffects"][0] = static_cast<int>(SlotEffect::SmoothComp);
    load(description); selectTile(170, 120);
    assert(descriptionTooltip() == titleHelp(SlotEffect::SmoothComp)); // Never the previous module's text.
    if (argc == 4) // Plot help is produced by rendering; the snapshot host shows the editor.
    {
      description["SlotEffects"][0] = static_cast<int>(SlotEffect::Limiter);
      load(description); selectTile(170, 120);
      const auto help = descriptionTooltip(800, grid_layout::plotTop + 30);
      if (help.find(L"limiting curve") == std::wstring::npos) std::wcerr << L"Unexpected plot help: " << help << L'\n';
      assert(help.find(L"limiting curve") != std::wstring::npos);
      description["SlotEffects"][0] = static_cast<int>(SlotEffect::CosmicEcho);
      for (int style : {0, 2})
      {
        description["Pre 1 Cosmic Style"] = style;
        load(description); selectTile(170, 120);
        const auto upper = descriptionTooltip(800, grid_layout::plotTop + 20);
        const auto lower = descriptionTooltip(800, grid_layout::plotBottom - 10);
        assert(upper.find(L"Measured wet impulse") != std::wstring::npos);
        assert(lower.find(style == 0 ? L"16 delay lines" : L"4 delay lines") != std::wstring::npos);
        assert(lower.find(L"Measured wet impulse") == std::wstring::npos);
      }
    }
    SendMessage(editor, WM_MOUSELEAVE, 0, 0);
    load(previous);
    std::cout << "Native tooltips: clipped descriptions, bypass prefix, fitting text and state preservation passed\n";
  }
  {
    auto previous = save().config();
    const std::pair<SlotEffect, int> modes[]{
      {SlotEffect::CosmicEcho, uCosmicStyle}, {SlotEffect::MultiHeadDelay, uTapeHeads},
      {SlotEffect::ConsoleColour, uColourDesk}, {SlotEffect::Distortion, uDistStyle},
      {SlotEffect::PedalDrive, uPedalStyle},
      {SlotEffect::JunoChorus, uJunoMode}, {SlotEffect::SynthFilter, uFilterMode},
      {SlotEffect::DigitalDelay, uTempoDivision}, {SlotEffect::NoiseGate, uGateMode},
      {SlotEffect::DigitalBlack, uBlackMode}, {SlotEffect::PedalComp, uPedalCompPickup},
      {SlotEffect::PedalComp, uPedalCompAttack}, {SlotEffect::Chorus, uChorusVoices},
      {SlotEffect::SpringReverb, uSpringTank}, {SlotEffect::GraphicEQ, uGraphicVoicing}};
    for (const auto& [effect, control] : modes)
    {
      auto original = cleared;
      original["SlotEffects"][0] = static_cast<int>(effect);
      const auto defaults = SlotDefaultValues(effect);
      for (int p = 0; p < kUtilityControls; ++p)
        original[std::string("Pre 1 ") + kEffectParameters[p].name] = defaults[17 + p];
      const std::string parameter = std::string("Pre 1 ") + kEffectParameters[control].name;
      original[parameter] = 0.;
      if (control == uTempoDivision) original["Pre 1 Tempo Sync"] = 1.;
      load(original); selectTile(170, 120);
      const auto before = save().config();
      if (effect_presentation::ChoiceDropdown(control))
      {
        host.dirty = host.flushRequested = false;
        selectChoice(control, 0, 0, true, effect);
        assert(save().config() == before && !host.dirty && !host.flushRequested);
        selectChoice(control, 0, 0, false, effect);
        assert(save().config() == before && !host.dirty && !host.flushRequested);
        int current = 0;
        for (int choice = static_cast<int>(EffectChoices(control).size()) - 1; choice >= 0; --choice)
        {
          selectChoice(control, choice, current, false, effect);
          auto expected = before; expected[parameter] = static_cast<double>(choice);
          assert(save().config() == expected);
          current = choice;
        }
        selectChoice(control, 2, 0, false, effect);
        auto stored = save().config();
        load(stored); selectTile(170, 120);
        selectChoice(control, 0, 2, true, effect);
        assert(save().config() == stored);
      }
      else
      {
        const int position = effect_presentation::Position(effect, control);
        const int x = effect_presentation::HeaderControl(control) ? optionX(control, effect) : grid_layout::KnobX(position);
        const int y = effect_presentation::HeaderControl(control) ? grid_layout::optionsY : grid_layout::ToggleY(position);
        click(x, y); assert(save().config()[parameter] == 1.);
        click(x, y); assert(save().config() == before);
      }
      if (argc == 4) CaptureEditor(editor, std::filesystem::path(argv[3]) / ("mode-" + std::to_string(control) + ".bmp"));
    }
    load(previous);
    {
      // Module presets: the header menu sets this module's controls only. Bypass, the
      // player's pickup and every other slot keep their values.
      auto comp = cleared;
      comp["SlotEffects"][0] = static_cast<int>(SlotEffect::PedalComp);
      comp["Pre 1 Pedal Comp Pickup"] = 1.;
      comp["Pre 1 Pedal Comp Sustain"] = 12.;
      comp["Pre 1 Pedal Comp Level"] = 5.;
      comp["Pre 1 Enabled"] = 0.;
      comp["Pre 2 Boost Gain"] = 5.;
      // User presets live under the first preset folder; earlier tests may have cleared them.
      const auto previousFolders = userFolders();
      const auto presetRoot = std::filesystem::temp_directory_path()
                              / ("nam-grid-module-presets-" + std::to_string(GetTickCount64()));
      std::filesystem::create_directories(presetRoot);
      {
        auto settings = previousFolders;
        settings.presetFolders = pathTexts({presetRoot});
        assert(nam_library::WriteSettings(settingsBackup.file, settings));
      }
      load(comp); selectTile(170, 120); // Loading a state re-reads the folder settings.
      const int presetX = grid_layout::moduleBrowseX - 26 - 52;
      const auto presetKey = [&](int key, int modifiers = 0) {
        BYTE previous[256]{}, pressed[256]{};
        assert(GetKeyboardState(previous));
        if (modifiers & 1) pressed[VK_SHIFT] = 0x80;
        if (modifiers & 2) pressed[VK_CONTROL] = 0x80;
        if (modifiers & 4) pressed[VK_MENU] = 0x80;
        assert(SetKeyboardState(pressed));
        SendMessage(editor, WM_KEYDOWN, key, static_cast<LPARAM>(MapVirtualKey(key, MAPVK_VK_TO_VSC)) << 16);
        SendMessage(editor, WM_KEYUP, key, 0);
        assert(SetKeyboardState(previous));
      };
      const auto saveNamedPreset = [&](const wchar_t* name, bool cancel = false) {
        win_edit_menu::SelectNamedItem(editor, presetX, grid_layout::moduleButtonY, L"Save current as...");
        pumpFor(30);
        assert(GetFocus() == editor); // Type through actual focus, as a DAW user does.
        // Deletion in this field edits the name; it must never clear the module.
        presetKey('A', 2); presetKey(VK_BACK); presetKey(VK_DELETE);
        for (const wchar_t c : std::wstring(name))
        {
          const auto key = VkKeyScanW(c); assert(key != -1);
          presetKey(LOBYTE(key), HIBYTE(key));
        }
        presetKey(cancel ? VK_ESCAPE : VK_RETURN);
        pumpFor(50);
      };
      win_edit_menu::SelectNamedItem(editor, presetX, grid_layout::moduleButtonY, L"Snappy clean picking");
      pumpFor(50);
      auto chosen = save().config();
      assert(chosen["Pre 1 Pedal Comp Sustain"] == 60. && chosen["Pre 1 Pedal Comp Attack"] == 0.);
      assert(chosen["Pre 1 Pedal Comp Focus"] == -30. && chosen["Pre 1 Pedal Comp Level"] == 0.);
      assert(chosen["Pre 1 Pedal Comp Blend"] == 100.);
      assert(chosen["Pre 1 Pedal Comp Pickup"] == 1. && chosen["Pre 1 Enabled"] == 0. && chosen["Pre 2 Boost Gain"] == 5.);
      win_edit_menu::SelectNamedItem(editor, presetX, grid_layout::moduleButtonY, L"Acoustic strumming");
      pumpFor(50);
      chosen = save().config();
      assert(chosen["Pre 1 Pedal Comp Pickup"] == 2. && chosen["Pre 1 Pedal Comp Blend"] == 85.);
      // User presets: Save current as... writes one JSON file under the first preset folder's
      // Module Presets; the menu lists it after the factory presets and applies it.
      const auto userFile = module_presets::UserFolder(presetRoot, SlotEffect::PedalComp) / "Smoke squash.json";
      const auto beforeName = save().config();
      saveNamedPreset(L"Cancelled", true);
      assert(!std::filesystem::exists(userFile.parent_path() / "Cancelled.json") && save().config() == beforeName);
      saveNamedPreset(L"Smoke squash");
      pumpFor(50);
      assert(std::filesystem::exists(userFile) && save().config() == beforeName);
      // Delete/Backspace clear only the selected slot, with no modal page open.
      for (const int key : {VK_DELETE, VK_BACK})
      {
        load(comp); selectTile(170, 120);
        presetKey(key, 2); assert(save().config() == comp); // Modified keys are not shortcuts.
        click(936, 28); presetKey(key); assert(save().config() == comp); // Settings owns the key.
        presetKey(VK_ESCAPE); pumpFor(50);
        presetKey(key); pumpFor(50);
        const auto removed = save().config();
        assert(removed["SlotEffects"][0] == static_cast<int>(SlotEffect::NAM));
        assert(removed["SlotPaths"][0] == "" && removed["Pre 2 Boost Gain"] == 5.);
      }
      load(chosen); selectTile(170, 120);
      auto changed = chosen;
      changed["Pre 1 Pedal Comp Sustain"] = 5.;
      changed["Pre 1 Pedal Comp Pickup"] = 0.;
      load(changed); selectTile(170, 120);
      const auto readUser = [&] { std::ifstream input(userFile); return json::parse(input); };
      const auto originalUser = readUser();
      win_file_dialog::Confirm(false, [&] { saveNamedPreset(L"Smoke squash"); });
      assert(readUser() == originalUser && save().config() == changed);
      win_file_dialog::Confirm(true, [&] { saveNamedPreset(L"Smoke squash"); });
      assert(readUser()["values"]["Pedal Comp Sustain"] == 5. && save().config() == changed);
      load(chosen); selectTile(170, 120);
      win_file_dialog::Confirm(true, [&] { saveNamedPreset(L"Smoke squash"); });
      assert(readUser() == originalUser);
      load(changed); selectTile(170, 120);
      win_edit_menu::SelectNamedItem(editor, presetX, grid_layout::moduleButtonY, L"Smoke squash");
      pumpFor(50);
      chosen = save().config();
      assert(chosen["Pre 1 Pedal Comp Sustain"] == 35. && chosen["Pre 1 Pedal Comp Pickup"] == 2.);
      assert(chosen["Pre 1 Pedal Comp Blend"] == 85. && chosen["Pre 1 Enabled"] == 0.);
      // Smart Gate user presets include the learned profile, independent of slot.
      const auto assertGateRecall = [&](json expected, int slot) {
        const auto actual = save().config();
        // UI recall passes through host normalization: e.g. 7.7 returns as
        // 7.6999999999999993. Allow rounding only in this gate's module controls;
        // profiles, unrelated controls and the rest of the rig must remain exact.
        for (int c = 0; c < static_cast<int>(SlotValues{}.size()); ++c)
          if (module_presets::ModuleControl(SlotEffect::NoisePrint, c))
          {
            const auto key = "Pre " + std::to_string(slot + 1) + " " + module_presets::ControlName(c);
            const double wanted = expected[key].get<double>();
            assert(std::abs(actual[key].get<double>() - wanted) <= 1.e-12 * std::max(1., std::abs(wanted)));
            expected[key] = actual[key];
          }
        if (actual != expected) std::cerr << "Smart Gate recall differences: " << json::diff(expected, actual) << '\n';
        assert(actual == expected);
      };
      auto gate = learnedNoiseState;
      gate["Pre 1 Noise Sensitivity"] = 7.7;
      gate["Pre 1 Enabled"] = 0.;
      gate["Pre 2 Boost Gain"] = 5.;
      load(gate); selectTile(170, 120);
      const auto gateFile = module_presets::UserFolder(presetRoot, SlotEffect::NoisePrint) / "Learned floor.json";
      saveNamedPreset(L"Learned floor");
      pumpFor(50);
      std::ifstream gateInput(gateFile);
      const auto gateDocument = json::parse(gateInput);
      assert(gateDocument["noiseProfile"] == gate["NoisePrintProfiles"][0]);
      auto different = gate;
      different["NoisePrintProfiles"][0]["power"] = std::vector<double>(noise_print::bands, 1.e-5);
      different["Pre 1 Noise Sensitivity"] = 12.;
      load(different); selectTile(170, 120);
      win_edit_menu::SelectNamedItem(editor, presetX, grid_layout::moduleButtonY, L"Learned floor");
      pumpFor(50);
      assertGateRecall(gate, 0);
      // Recalling into another slot restores only that gate's controls and profile.
      auto otherSlot = different;
      otherSlot["SlotEffects"][1] = static_cast<int>(SlotEffect::NoisePrint);
      otherSlot["Pre 2 Enabled"] = 0.;
      load(otherSlot); selectTile(170 + grid_layout::columnPitch, 120);
      win_edit_menu::SelectNamedItem(editor, presetX, grid_layout::moduleButtonY, L"Learned floor");
      pumpFor(50);
      const auto otherValues = module_presets::Apply(SlotEffect::NoisePrint,
        module_presets::LoadUser({presetRoot}, SlotEffect::NoisePrint).front().values,
        SlotDefaultValues(SlotEffect::NoisePrint));
      for (int c = 0; c < static_cast<int>(otherValues.size()); ++c)
        if (module_presets::ModuleControl(SlotEffect::NoisePrint, c))
          otherSlot["Pre 2 " + std::string(module_presets::ControlName(c))] = otherValues[c];
      otherSlot["NoisePrintProfiles"][1] = gate["NoisePrintProfiles"][0];
      assertGateRecall(otherSlot, 1);
      // Version 1 presets keep the current noise profile; explicit null clears it.
      auto legacyGate = gateDocument;
      legacyGate["version"] = 1;
      legacyGate.erase("noiseProfile");
      legacyGate["values"]["Noise Learn"] = 1.;
      rig_preset::WriteAtomically(gateFile.parent_path() / "Legacy floor.json", legacyGate.dump());
      auto untrainedGate = gateDocument;
      untrainedGate["noiseProfile"] = nullptr;
      rig_preset::WriteAtomically(gateFile.parent_path() / "Untrained floor.json", untrainedGate.dump());
      load(different); selectTile(170, 120);
      win_edit_menu::SelectNamedItem(editor, presetX, grid_layout::moduleButtonY, L"Legacy floor");
      pumpFor(50);
      different["Pre 1 Noise Sensitivity"] = 7.7;
      assertGateRecall(different, 0);
      win_edit_menu::SelectNamedItem(editor, presetX, grid_layout::moduleButtonY, L"Untrained floor");
      pumpFor(50);
      different["NoisePrintProfiles"][0] = nullptr;
      assertGateRecall(different, 0);
      assert(nam_library::WriteSettings(settingsBackup.file, previousFolders));
      std::error_code removed;
      std::filesystem::remove_all(presetRoot, removed);
      if (argc == 4) CaptureEditor(editor, std::filesystem::path(argv[3]) / "module-preset.bmp");
    }
    std::cout << "Native mode menus: every choice, checked selection, cancellation, unchanged selection, recall and two-choice toggles passed\n";
    if (std::getenv("NAM_GRID_MODE_SMOKE"))
    {
      gui->destroy(plugin); DestroyWindow(parent);
      plugin->destroy(plugin); entry->deinit(); FreeLibrary(library);
      return 0;
    }
  }
  if(fxVisuals)
  {
    const std::array<const char*,17> names{{"NAM Input","NAM Output","Enabled","Gate Threshold","Gate Release","Gate Reduction",
      "Comp Threshold","Comp Release","Comp Makeup","EQ Bass","EQ Mid","EQ Treble","EQ Low Cut","EQ High Cut","EQ Presence","IR Output","Full Quality"}};
    int reviewed=0;
    for(const auto& group : nam_library::kModuleGroups)
      for(auto effect : group.effects)
      {
        if(const auto* only=std::getenv("NAM_GRID_FX_VISUALS_ONLY"))
          if((","+std::string(only)+",").find(","+std::to_string(static_cast<int>(effect))+",")==std::string::npos) continue;
        auto preview=cleared;
        preview["SlotEffects"][0]=static_cast<int>(effect);
        const auto defaults=SlotDefaultValues(effect);
        for(size_t i=0;i<names.size();++i) preview[std::string("Pre 1 ")+names[i]]=defaults[i];
        for(int p=0;p<kUtilityControls;++p) preview[std::string("Pre 1 ")+kEffectParameters[p].name]=defaults[17+p];
        load(preview); selectTile(170,120);
        const auto audition = [&](const char* suffix) {
          const auto before=save().config();
          assert(plugin->activate(plugin,48000.,1,512)); assert(plugin->start_processing(plugin));
          process.frames_count=512;
          for(int block=0;block<240;++block)
          {
            for(int i=0;i<512;++i)
            {
              const double t=(block*512+i)/48000.;
              input[0][i]=.4*std::sin(2.*3.141592653589793*220.*t)*(.55+.45*std::cos(2.*3.141592653589793*t));
              input[1][i]=.3*std::sin(2.*3.141592653589793*330.*t);
            }
            assert(plugin->process(plugin,&process)!=CLAP_PROCESS_ERROR);
            if(block%8==0) pumpFor(2);
          }
          pumpFor(100);
          if(argc==4) CaptureEditor(editor,std::filesystem::path(argv[3])/("fx-"+std::to_string(static_cast<int>(effect))+"-"+suffix+".bmp"));
          plugin->stop_processing(plugin); plugin->deactivate(plugin);
          assert(save().config()==before); // Viewing/processing must not edit the rig.
        };
        audition("default");
        if(effect==SlotEffect::SmoothComp || effect==SlotEffect::PunchComp || effect==SlotEffect::Limiter
            || effect==SlotEffect::OutboardChannel || effect==SlotEffect::ButterComp)
        {
          auto heavy=preview;
          heavy["Pre 1 Comp Threshold"]=-60.;
          heavy["Pre 1 Comp Makeup"]=0.;
          heavy["Pre 1 Strip Compress"]=100.;
          heavy["Pre 1 Butter Compress"]=100.;
          heavy["Pre 1 Limiter Ceiling"]=-24.;
          load(heavy); selectTile(170,120);
          audition("heavy"); // Dedicated heavy-compression frame for overrange markers.
          load(preview); selectTile(170,120);
        }
        for(bool high : {false,true})
        {
          // CLAP exposes continuous doubles in normalized host units and
          // choices as integer indices. Use the actual host metadata endpoints.
          std::vector<std::string> targets;
          for(size_t i=3;i<16;++i) targets.push_back(std::string("Pre 1 ")+names[i]);
          for(int p=0;p<kUtilityControls;++p)
            if(p!=uNoiseLearn && (UsesUtility(effect,p)
                || ((p==uTempoSync || p==uTempoDivision) && effect_presentation::Tempo(effect))
                || (p==uDelayPingPong && effect_presentation::PingPong(effect))
                || (p==uReverbStereo && effect_presentation::Visible(effect,p))))
              targets.push_back(std::string("Pre 1 ")+kEffectParameters[p].name);
          std::vector<clap_event_param_value> changes;
          for(uint32_t index=0;index<params->count(plugin);++index)
          {
            clap_param_info info{}; assert(params->get_info(plugin,index,&info));
            if(std::find(targets.begin(),targets.end(),std::string(info.name))==targets.end()) continue;
            clap_event_param_value event{};
            event.header.size=sizeof(event); event.header.type=CLAP_EVENT_PARAM_VALUE;
            event.param_id=info.id; event.value=high ? info.max_value : info.min_value;
            event.note_id=event.port_index=event.channel=event.key=-1;
            changes.push_back(event);
          }
          clap_input_events updates{&changes,[](const clap_input_events* list)->uint32_t {
              return static_cast<uint32_t>(static_cast<const std::vector<clap_event_param_value>*>(list->ctx)->size());
            },[](const clap_input_events* list,uint32_t index)->const clap_event_header* {
              return &(*static_cast<const std::vector<clap_event_param_value>*>(list->ctx))[index].header;
            }};
          params->flush(plugin,&updates,&sent); pumpFor(100);
          for(const auto& event : changes)
          {
            double applied=0.; assert(params->get_value(plugin,event.param_id,&applied));
            if(std::abs(applied-event.value)>1.e-6*std::max(1.,std::abs(event.value)))
              std::cerr << "Visual sweep parameter " << event.param_id << ": requested " << event.value << ", applied " << applied << '\n';
            assert(std::abs(applied-event.value)<=1.e-6*std::max(1.,std::abs(event.value)));
          }
          if(argc==4) CaptureEditor(editor,std::filesystem::path(argv[3])/("fx-"+std::to_string(static_cast<int>(effect))+(high ? "-max.bmp" : "-min.bmp")));
        }
        std::cout << "FX visualization rendered: " << SlotEffectName(effect) << '\n';
        ++reviewed;
      }
    gui->destroy(plugin); DestroyWindow(parent);
    plugin->destroy(plugin); entry->deinit(); FreeLibrary(library);
    assert(reviewed>0);
    std::cout << "Native FX visualization catalog: " << reviewed << " modules at defaults/min/max passed\n";
    return 0;
  }
  if (browserOnly)
  {
    click(910, 28); click(138, 88); click(936, 28);
    pumpFor(250); // Exercise configured sources even when the user's startup rescan is off.
  }
  if (!browserOnly)
  {
    load(learnedNoiseState);
    win_edit_menu::SelectFirstDestination(editor, 0, 0);
    pumpFor(50);
    const auto noiseCopy = save().config();
    assert(noiseCopy["SlotEffects"][1] == static_cast<int>(SlotEffect::NoisePrint));
    assert(noiseCopy["NoisePrintProfiles"][1] == learnedNoiseState["NoisePrintProfiles"][0]);
    assert(noiseCopy["Pre 2 Noise Gate Threshold"] == learnedNoiseState["Pre 1 Noise Gate Threshold"]);
    selectTile(170, 120);
    SendMessage(editor, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(grid_layout::KnobX(5), grid_layout::KnobGestureY(5)));
    SendMessage(editor, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(grid_layout::KnobX(5), grid_layout::KnobGestureY(5) - 25));
    SendMessage(editor, WM_LBUTTONUP, 0, MAKELPARAM(grid_layout::KnobX(5), grid_layout::KnobGestureY(5) - 25));
    pumpFor(100);
    assert(save().config()["Pre 1 Noise Gate Threshold"] > learnedNoiseState["Pre 1 Noise Gate Threshold"]);
    click(grid_layout::KnobX(0), grid_layout::ToggleY(0)); // Learn is an actual toggle in the fixed bottom panel.
    double learningValue = 0.;
    assert(params->get_value(plugin, learnInfo.id, &learningValue));
    assert(learningValue == 1.);
    assert(save().config()["Pre 1 Noise Learn"] == 0.);
    click(grid_layout::KnobX(0), grid_layout::ToggleY(0));
    assert(params->get_value(plugin, learnInfo.id, &learningValue) && learningValue == 0.);
    load(browserState);
    // Exercise the actual native selection callback, parameter commit and prepared publication.
    for (int source : {0, 1})
      for (int mode : {0, 1, 2}) // Copy, Move, Swap
        for (double gain : {-30., 30.})
        {
          const int destination = 1 - source;
          const char* sourceGain = source == 0 ? "Pre 1 NAM Input" : "Pre 2 NAM Input";
          const char* destinationGain = destination == 0 ? "Pre 1 NAM Input" : "Pre 2 NAM Input";
          auto editState = browserState;
          editState["SlotPaths"][0] = libraryFirst.string();
          editState["SlotPaths"][1] = librarySecond.string();
          editState[sourceGain] = gain;
          const double previousGain = gain > 0. ? -35. : 35.;
          editState[destinationGain] = previousGain;
          load(editState);
          assert(plugin->activate(plugin, 48000., 1, 512));
          assert(plugin->start_processing(plugin));
          process.frames_count = 512;
          for (int i = 0; i < 512; ++i)
            input[0][i] = input[1][i] = .1 * std::sin(i * .03);
          assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
          win_edit_menu::SelectFirstDestination(editor, source, mode);
          pumpFor(50);
          const auto edited = save().config();
          assert(std::abs(edited[destinationGain].get<double>() - gain) < 1.e-9);
          assert(edited["SlotPaths"][destination] == editState["SlotPaths"][source]);
          assert(edited["SlotHashes"][destination] == editState["SlotHashes"][source]);
          if (mode == 0)
          {
            assert(edited["SlotPaths"][source] == editState["SlotPaths"][source]);
            assert(edited[sourceGain] == gain);
          }
          else if (mode == 1)
          {
            assert(edited["SlotPaths"][source] == "");
            assert(edited["SlotHashes"][source] == "");
            assert(edited[sourceGain] == 0.);
          }
          else
          {
            assert(edited["SlotPaths"][source] == editState["SlotPaths"][destination]);
            assert(edited["SlotHashes"][source] == editState["SlotHashes"][destination]);
            assert(edited[sourceGain] == previousGain);
          }
          for (int block = 0; block < 8; ++block)
          {
            assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
            for (int i = 0; i < 512; ++i)
              assert(std::isfinite(output[0][i]) && std::isfinite(output[1][i]));
          }
          assert(save().config() == edited);
          plugin->stop_processing(plugin);
          plugin->deactivate(plugin);
        }
    load(browserState);
    std::cout << "Slot edit menus passed; checking module menus\n";
    click(910, 28); click(138, 88); click(936, 28); // Rescan all from library settings.
    pumpFor(250);
    // Recall follows the saved content even when the old filename disappeared or
    // now points at another model. Both loader types resolve through the index.
    for (bool ir : {false, true})
    {
      auto relocated = browserState;
      const auto canonical = ir ? libraryIR : librarySecond;
      const auto hash = nam_library::ContentHash(canonical);
      relocated["SlotEffects"][0] = static_cast<int>(ir ? SlotEffect::CabinetIR : SlotEffect::NAM);
      relocated["SlotPaths"][0] = (fixtures / (ir ? "moved.wav" : "moved.nam")).string();
      relocated["SlotHashes"][0] = hash;
      Stream bytes; bytes.string("###nam-grid###"); bytes.string(relocated.dump());
      assert(loadBytes(bytes));
      for(int wait=0;wait<100 && std::filesystem::path(save().config()["SlotPaths"][0].get<std::string>()) != canonical;++wait) pumpFor(20);
      assert(std::filesystem::path(save().config()["SlotPaths"][0].get<std::string>()) == canonical);
      assert(save().config()["SlotHashes"][0] == hash);
      relocated["SlotPaths"][0] = path2.string(); // A different capture occupies the saved path.
      Stream replaced; replaced.string("###nam-grid###"); replaced.string(relocated.dump());
      assert(loadBytes(replaced));
      assert(std::filesystem::path(save().config()["SlotPaths"][0].get<std::string>()) == canonical);
      // A stale index must not prevent loading a verified saved copy outside it.
      relocated["SlotPaths"][0] = (ir ? irFile : path1).string();
      Stream fallback; fallback.string("###nam-grid###"); fallback.string(relocated.dump());
      const auto firstBlock = [&] {
        assert(plugin->activate(plugin,48000.,1,512)); assert(plugin->start_processing(plugin));
        for(int i=0;i<512;++i) input[0][i]=input[1][i]=.1*std::sin(i*.031);
        process.frames_count=512; assert(plugin->process(plugin,&process)!=CLAP_PROCESS_ERROR);
        const auto result=output;
        plugin->stop_processing(plugin);plugin->deactivate(plugin);return result;
      };
      assert(loadBytes(fallback)); const auto expectedAudio=firstBlock();
      const auto parked=canonical.string()+".relink-test-"+std::to_string(GetTickCount64());
      std::filesystem::rename(canonical,parked);
      assert(loadBytes(fallback)); const auto fallbackAudio=firstBlock();
      std::filesystem::rename(parked,canonical);
      assert(std::filesystem::path(save().config()["SlotPaths"][0].get<std::string>()) == (ir ? irFile : path1));
      for(int ch=0;ch<2;++ch) for(int i=0;i<512;++i) assert(std::abs(expectedAudio[ch][i]-fallbackAudio[ch][i])<1.e-10);
    }
    load(browserState);
    // Choosing one of several identical captures keeps that exact file; the content hash is only a
    // fallback, so a pack copy must not be redirected to the curated copy in another folder.
    {
      auto chosen = browserState;
      chosen["SlotEffects"][0] = 0;
      chosen["SlotPaths"][0] = libraryTwin.string();
      chosen["SlotHashes"][0] = nam_library::ContentHash(libraryTwin);
      chosen["SlotPaths"][0] = libraryTwin.string();
      Stream bytes; bytes.string("###nam-grid###"); bytes.string(chosen.dump());
      assert(loadBytes(bytes));
      assert(std::filesystem::path(save().config()["SlotPaths"][0].get<std::string>()) == libraryTwin);
      chosen["SlotHashes"][0] = ""; // An explicit choice carries no remembered hash.
      Stream explicitChoice; explicitChoice.string("###nam-grid###"); explicitChoice.string(chosen.dump());
      assert(loadBytes(explicitChoice));
      assert(std::filesystem::path(save().config()["SlotPaths"][0].get<std::string>()) == libraryTwin);
    }
    load(browserState);
    const auto beforeHashError = save().config();
    auto invalidHashes = beforeHashError; invalidHashes["SlotHashes"][0] = std::string(40, 'z');
    Stream badHashBytes; badHashBytes.string("###nam-grid###"); badHashBytes.string(invalidHashes.dump());
    assert(!loadBytes(badHashBytes)); assert(save().config() == beforeHashError);
    // The actual empty-card title opens on one mouse-down; every built-in command,
    // including the HQ effects, must pass SetSlotEffect's validation as well.
    selectTile(170, 120);
    for (const auto& group : nam_library::kModuleGroups)
      for (const auto effect : group.effects)
      {
        const std::string name = SlotEffectName(effect);
        const std::wstring wideName(name.begin(), name.end());
        win_edit_menu::SelectNamedItem(editor, 220, grid_layout::moduleButtonY, wideName.c_str()); pumpFor(30);
        const auto inserted = save().config();
        assert(inserted["SlotEffects"][0] == static_cast<int>(effect));
        const auto defaults = SlotDefaultValues(effect);
        // Insertion must use this module's defaults even after another module used
        // the same parameter bank, and must notify the plugin's actual parameters.
        for (int i = 0; i < kUtilityControls; ++i)
          assert(std::abs(inserted[std::string("Pre 1 ") + kEffectParameters[i].name].get<double>() - defaults[17 + i]) < 1.e-6);
        if (effect == SlotEffect::SmoothComp) assert(inserted["Pre 1 Comp Threshold"] == -24.);
        if (effect == SlotEffect::CutEQ) assert(inserted["Pre 1 EQ Low Cut"] == 80.);
        win_edit_menu::SelectNamedItem(editor, 170, 105, L"Clear slot", true); pumpFor(30);
        assert(save().config()["SlotEffects"][0] == 0 && save().config()["SlotPaths"][0] == "");
      }
    win_edit_menu::SelectNamedItem(editor, 220, grid_layout::moduleButtonY, L"Clear slot"); pumpFor(30);
    // Empty slots have neither a card toggle nor a lone editor Enabled control.
    {
      const auto empty = save().config();
      click(grid_layout::cardLeft + 176, grid_layout::cardTop + 12);
      assert(save().config() == empty);
    }
    std::cout << "Single-click module selection and right-click Clear slot passed\n";
  }
  auto eqUI = browserState;
  eqUI["SlotEffects"][0] = static_cast<int>(SlotEffect::ToneEQ);
  eqUI["SlotEffects"][1] = static_cast<int>(SlotEffect::CutEQ);
  eqUI["SlotEffects"][2] = static_cast<int>(SlotEffect::CabinetIR);
  eqUI["SlotPaths"][2] = irFile.string();
  load(eqUI);
  selectTile(170, 120); // Select Pre 1 independently of earlier menu tests.
  pumpFor(100);
  const auto dragAt = [&](int x, int y, int distance = 25) {
    SendMessage(editor, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
    SendMessage(editor, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(x, y - distance));
    SendMessage(editor, WM_LBUTTONUP, 0, MAKELPARAM(x, y - distance));
    pumpFor(100);
  };
  const auto keyAtEditor = [&](int key) {
    InvalidateRect(editor, nullptr, FALSE); UpdateWindow(editor);
    BYTE previous[256]{}, neutral[256]{};
    assert(GetKeyboardState(previous)); assert(SetKeyboardState(neutral));
    SendMessage(editor, WM_KEYDOWN, key, static_cast<LPARAM>(MapVirtualKey(key, MAPVK_VK_TO_VSC)) << 16);
    SendMessage(editor, WM_KEYUP, key, 0);
    assert(SetKeyboardState(previous)); pumpFor(20);
  };
  // On-demand inspection must cover the module's controls without changing the
  // rig. Exercise the global meters and every slot output meter, then each way back.
  {
    auto rig = browserState;
    rig["SlotEffects"][0] = static_cast<int>(SlotEffect::Utility);
    const int knobX = grid_layout::KnobX(1), knobY = grid_layout::KnobGestureY(1);
    for (int point = -1; point <= 16; ++point)
    {
      load(rig); selectTile(170, 120);
      const auto before = save().config();
      const int x = point == -1 ? 95 : point == 16 ? grid_layout::width - 71
        : grid_layout::cardLeft + grid_layout::columnPitch * (point % 4) + 194;
      const int y = point == -1 ? 80 : point == 16 ? 390
        : grid_layout::cardTop + grid_layout::rowPitch * (point / 4) + grid_layout::cardHeight / 2;
      click(x, y); dragAt(knobX, knobY);
      assert(save().config() == before); // The measurement panel owns the lower editor.
      if (argc == 4 && point == -1) CaptureEditor(editor, std::filesystem::path(argv[3]) / "signal-monitor-input.bmp");
      switch ((point + 1) % 5)
      {
        case 0: keyAtEditor(VK_ESCAPE); break;
        case 1: click(x, y); break;
        case 2: click(520, 500); break;
        case 3: selectTile(170, 120); break;
        case 4:
          click((grid_layout::tunerLeft + grid_layout::tunerRight) / 2, grid_layout::headerY);
          assert(save().config()["Tuner"] == 1.);
          click((grid_layout::tunerLeft + grid_layout::tunerRight) / 2, grid_layout::headerY);
          break;
      }
      dragAt(knobX, knobY);
      assert(save().config()["Pre 1 Utility Width"] != before["Pre 1 Utility Width"]);
    }
    load(browserState); selectTile(170, 120);
    std::cout << "On-demand monitor meters, state isolation and panel exits passed\n";
  }
  const auto typeBrowserSearch = [&](const std::string& query) {
    if (IsWindowVisible(editor)) assert(GetFocus() == editor);
    for (const char ch : query) keyAtEditor(ch == ' ' ? VK_SPACE : ch);
    pumpFor(150); // Results must update before Enter commits the search editor.
    InvalidateRect(editor, nullptr, FALSE); UpdateWindow(editor);
  };
  const auto enterBrowserSearch = [&](const std::string& query) {
    click(200, 75);
    typeBrowserSearch(query);
  };
  const auto chooseBrowserItem = [&](const std::string& query) {
    enterBrowserSearch(query); keyAtEditor(VK_RETURN); keyAtEditor(VK_RETURN);
  };
  // Macro names use an unbound text editor: letters must not edit the numeric value.
  {
    const auto before = save().config();
    SendMessage(editor, WM_LBUTTONDBLCLK, MK_LBUTTON, MAKELPARAM(41, 184));
    SendMessage(editor, WM_LBUTTONUP, 0, MAKELPARAM(41, 184)); pumpFor(30);
    BYTE previousKeys[256]{}, entryKeys[256]{};
    assert(GetKeyboardState(previousKeys));
    for (const char c : std::string("Drive"))
    {
      entryKeys[VK_SHIFT] = c == 'D' ? 0x80 : 0;
      assert(SetKeyboardState(entryKeys));
      const int key = std::toupper(static_cast<unsigned char>(c));
      SendMessage(editor, WM_KEYDOWN, key, static_cast<LPARAM>(MapVirtualKey(key, MAPVK_VK_TO_VSC)) << 16);
      SendMessage(editor, WM_KEYUP, key, 0);
    }
    entryKeys[VK_SHIFT] = 0; assert(SetKeyboardState(entryKeys));
    SendMessage(editor, WM_KEYDOWN, VK_RETURN, 0); SendMessage(editor, WM_KEYUP, VK_RETURN, 0);
    assert(SetKeyboardState(previousKeys)); pumpFor(30);
    const auto named = save().config();
    assert(named["MacroNames"][0] == "Drive" && named["Macro 1"] == before["Macro 1"]);
    assert(loadRaw(named));
    assert(save().config() == named);
    assert(loadRaw(before));
  }
  // Tiles and the browser icon share context. Merely opening/searching/navigating
  // must not edit a rig; accepting the selected item targets the originating slot.
  // Add module starts in search: type immediately without clicking the field.
  load(cleared);
  click(170, 120);
  typeBrowserSearch("ZZNOSUCHMODULE"); keyAtEditor(VK_RETURN);
  // Clearing a committed query must leave search ready for immediate typing.
  click(grid_layout::width - 43, 75);
  typeBrowserSearch("CONSOLE EQ"); keyAtEditor(VK_RETURN); keyAtEditor(VK_RETURN);
  assert(save().config()["SlotEffects"][0] == static_cast<int>(SlotEffect::ConsoleEQ));
  auto browserCurrent = browserState;
  browserCurrent["SlotPaths"][0] = libraryTwin.string();
  browserCurrent["SlotPaths"][0] = libraryTwin.string();
  load(browserCurrent);
  const auto beforeBrowse = save().config();
  click(170, 120);
  if (argc == 4) CaptureEditor(editor, std::filesystem::path(argv[3]) / "browser-nam-current.bmp");
  assert(save().config() == beforeBrowse);
  keyAtEditor(VK_RETURN); // The exact duplicate path remains selected.
  assert(std::filesystem::path(save().config()["SlotPaths"][0].get<std::string>()) == libraryTwin);
  click(grid_layout::moduleBrowseX, grid_layout::moduleButtonY);
  typeBrowserSearch("DRIVE"); // The module browser icon focuses search too.
  assert(save().config()["SlotPaths"][0] == beforeBrowse["SlotPaths"][0]);
  if (argc == 4) CaptureEditor(editor, std::filesystem::path(argv[3]) / "browser-nam-search.bmp");
  keyAtEditor(VK_RETURN); keyAtEditor(VK_RETURN);
  assert(std::filesystem::path(save().config()["SlotPaths"][0].get<std::string>()) == librarySecond);
  click(grid_layout::moduleBrowseX, grid_layout::moduleButtonY);
  click(560, 28); // Cell context allows IR, but offers no complete rig presets.
  click(680, 28); // A hidden preset tab must not accept input.
  typeBrowserSearch("CABINET"); keyAtEditor(VK_RETURN); keyAtEditor(VK_RETURN); // Type immediately after switching tabs.
  assert(save().config()["SlotEffects"][0] == static_cast<int>(SlotEffect::CabinetIR));
  assert(std::filesystem::path(save().config()["SlotPaths"][0].get<std::string>()) == libraryIR);
  click(grid_layout::moduleBrowseX, grid_layout::moduleButtonY);
  if (argc == 4) CaptureEditor(editor, std::filesystem::path(argv[3]) / "browser-ir-current.bmp");
  keyAtEditor(VK_ESCAPE);
  // An external file's folder is browsable without adding it to the user's library.
  auto externalFile = browserState;
  externalFile["SlotPaths"][0] = path2.string();
  load(externalFile); click(170, 120); pumpFor(250);
  keyAtEditor(VK_RETURN);
  assert(std::filesystem::path(save().config()["SlotPaths"][0].get<std::string>()) == path2);
  assert(userFolders().namFolders == pathTexts({libraryFolder}) && userFolders().irFolders == pathTexts({irLibrary}));
  auto moduleBrowser = eqUI;
  moduleBrowser["Pre 1 EQ Bass"] = 6.;
  load(moduleBrowser); click(170, 120);
  if (argc == 4) CaptureEditor(editor, std::filesystem::path(argv[3]) / "browser-fx-current.bmp");
  keyAtEditor(VK_RETURN);
  assert(save().config()["Pre 1 EQ Bass"] == 6.); // Reselecting the FX retains edits.
  click(grid_layout::moduleBrowseX, grid_layout::moduleButtonY); chooseBrowserItem("CONSOLE EQ");
  assert(save().config()["SlotEffects"][0] == static_cast<int>(SlotEffect::ConsoleEQ));
  if (argc == 4) CaptureEditor(editor, std::filesystem::path(argv[3]) / "module-header-readable.bmp");
  const auto liveValue = [&](const char* name) {
    clap_param_info info{};
    for (uint32_t i = 0; i < params->count(plugin); ++i)
    {
      assert(params->get_info(plugin, i, &info));
      if (std::strcmp(info.name, name) != 0) continue;
      double value = 0.; char text[64]{};
      assert(params->get_value(plugin, info.id, &value));
      assert(params->value_to_text(plugin, info.id, value, text, sizeof(text)));
      return std::stod(text);
    }
    assert(false); return 0.;
  };
  const auto rawValue = [&](const char* name) {
    clap_param_info info{};
    for (uint32_t i = 0; i < params->count(plugin); ++i)
    {
      assert(params->get_info(plugin, i, &info));
      if (std::strcmp(info.name, name) != 0) continue;
      double value = 0.;
      assert(params->get_value(plugin, info.id, &value));
      return value;
    }
    assert(false); return 0.;
  };
  // Auditions use the real lower editor, while autosave retains the original slot.
  for (const auto kind : {nam_library::FileKind::Module, nam_library::FileKind::NAM, nam_library::FileKind::IR})
  {
    auto original = moduleBrowser;
    const bool nam = kind == nam_library::FileKind::NAM, ir = kind == nam_library::FileKind::IR;
    if (nam || ir)
    {
      original["SlotEffects"][0] = static_cast<int>(ir ? SlotEffect::CabinetIR : SlotEffect::NAM);
      original["SlotPaths"][0] = (ir ? libraryIR : libraryTwin).string();
    }
    const char* parameter = nam ? "Pre 1 NAM Input" : ir ? "Pre 1 IR Output" : "Pre 1 EQ Bass";
    const int knob = ir ? effect_presentation::kOutputPosition : 0; // IR Output sits below the first column.
    load(original); selectTile(170, 120);
    const auto committed = save().config();
    const auto before = liveValue(parameter);
    click(grid_layout::moduleBrowseX, grid_layout::moduleButtonY);
    host.dirty = host.flushRequested = false;
    dragAt(grid_layout::KnobX(knob), grid_layout::KnobGestureY(knob));
    const auto audition = liveValue(parameter);
    assert(std::abs(audition - before) > .01);
    assert(save().config() == committed);
    assert(!host.dirty && !host.flushRequested);
    keyAtEditor(VK_ESCAPE);
    assert(std::abs(liveValue(parameter) - before) < .01 && save().config() == committed);
    click(grid_layout::moduleBrowseX, grid_layout::moduleButtonY);
    dragAt(grid_layout::KnobX(knob), grid_layout::KnobGestureY(knob));
    const auto accepted = liveValue(parameter);
    keyAtEditor(VK_RETURN);
    assert(host.dirty);
    assert(std::abs(save().config()[parameter].get<double>() - accepted) < .11);
    if (nam)
    {
      click(grid_layout::moduleBrowseX, grid_layout::moduleButtonY);
      enterBrowserSearch("DRIVE"); keyAtEditor(VK_RETURN);
      dragAt(grid_layout::KnobX(0), grid_layout::KnobGestureY(0));
      const auto gain = liveValue(parameter);
      assert(save().config()["SlotPaths"][0] == committed["SlotPaths"][0]);
      keyAtEditor(VK_RETURN);
      assert(std::filesystem::path(save().config()["SlotPaths"][0].get<std::string>()) == librarySecond);
      assert(std::abs(save().config()[parameter].get<double>() - gain) < .11);
    }
  }
  auto previewMode = moduleBrowser;
  previewMode["SlotEffects"][0] = static_cast<int>(SlotEffect::CosmicEcho);
  previewMode["Pre 1 Cosmic Style"] = 0.;
  load(previewMode); selectTile(170, 120);
  click(grid_layout::moduleBrowseX, grid_layout::moduleButtonY);
  host.dirty = host.flushRequested = false;
  selectChoice(uCosmicStyle, 1, 0); // A mode edit remains part of the audition.
  assert(rawValue("Pre 1 Cosmic Style") == 1.);
  assert(save().config() == previewMode);
  assert(!host.dirty && !host.flushRequested);
  keyAtEditor(VK_ESCAPE);
  assert(rawValue("Pre 1 Cosmic Style") == 0.);
  assert(save().config() == previewMode);
  assert(!host.dirty && !host.flushRequested);
  click(grid_layout::moduleBrowseX, grid_layout::moduleButtonY);
  selectChoice(uCosmicStyle, 1, 0);
  keyAtEditor(VK_RETURN); // Accepting commits the mode edit and tells the host once.
  assert(host.dirty);
  assert(rawValue("Pre 1 Cosmic Style") == 1. && save().config()["Pre 1 Cosmic Style"] == 1.);
  load(moduleBrowser); selectTile(170, 120);
  click(grid_layout::moduleBrowseX, grid_layout::moduleButtonY);
  dragAt(grid_layout::KnobX(0), grid_layout::KnobGestureY(0));
  const auto editedBass = liveValue("Pre 1 EQ Bass");
  enterBrowserSearch("CONSOLE EQ"); keyAtEditor(VK_RETURN);
  click(grid_layout::width - 43, 75); // The Modules search cross follows the window width.
  enterBrowserSearch("GUITAR EQ"); keyAtEditor(VK_RETURN);
  assert(std::abs(liveValue("Pre 1 EQ Bass") - editedBass) < .01);
  keyAtEditor(VK_ESCAPE);
  assert(save().config()["Pre 1 EQ Bass"] == 6.);
  // A quick-menu choice while the browser auditions is final: it closes the browser, so
  // Escape (or Cancel) can no longer restore the original afterwards.
  click(grid_layout::moduleBrowseX, grid_layout::moduleButtonY);
  enterBrowserSearch("CONSOLE EQ"); keyAtEditor(VK_RETURN);
  win_edit_menu::SelectNamedItem(editor, 200, grid_layout::moduleButtonY, L"Clear slot"); pumpFor(50);
  keyAtEditor(VK_ESCAPE);
  assert(save().config()["SlotEffects"][0] == static_cast<int>(SlotEffect::NAM) && save().config()["SlotPaths"][0] == "");
  load(moduleBrowser); selectTile(170, 120);
  // Cancel top right, where every full page closes, restores the original like Escape.
  click(grid_layout::moduleBrowseX, grid_layout::moduleButtonY);
  enterBrowserSearch("CONSOLE EQ"); keyAtEditor(VK_RETURN);
  assert(save().config()["SlotEffects"][0] != static_cast<int>(SlotEffect::ConsoleEQ)); // Auditioned, not saved.
  click(936, 28);
  assert(save().config()["Pre 1 EQ Bass"] == 6.);
  dragAt(grid_layout::KnobX(0), grid_layout::KnobGestureY(0)); // The browser closed: the editor takes gestures again.
  assert(save().config()["Pre 1 EQ Bass"] != 6.);
  load(moduleBrowser); selectTile(170, 120);
  click(grid_layout::moduleBrowseX, grid_layout::moduleButtonY);
  enterBrowserSearch("CONSOLE EQ"); keyAtEditor(VK_RETURN);
  SendMessage(editor, WM_LBUTTONDBLCLK, MK_LBUTTON, MAKELPARAM(400, 144));
  SendMessage(editor, WM_LBUTTONUP, 0, MAKELPARAM(400, 144)); pumpFor(100);
  assert(save().config()["SlotEffects"][0] == static_cast<int>(SlotEffect::ConsoleEQ));
  load(moduleBrowser); selectTile(170, 120);
  const auto beforeLibraryAudition = save().config();
  click(910, 28); // Browse from settings must audition the selected slot too.
  click(808, 28);
  click(310, 28); // Modules tab
  enterBrowserSearch("CONSOLE EQ"); keyAtEditor(VK_RETURN);
  dragAt(grid_layout::KnobX(0), grid_layout::KnobGestureY(0));
  assert(save().config() == beforeLibraryAudition);
  click(680, 28); // Switching to complete rigs cancels the slot audition.
  assert(save().config() == beforeLibraryAudition);
  assert(std::abs(liveValue("Pre 1 EQ Bass") - 6.) < .01);
  keyAtEditor(VK_ESCAPE);
  std::cout << "FX/NAM/IR live parameter audition, autosave isolation, cancellation and acceptance passed\n";
  load(eqUI); selectTile(170, 120);
  std::cout << "Context browser tile/icon entry, exact current selection, live search and NAM/IR/FX targeting passed\n";
  {
    // Drag from a tile's ID row: plain drag swaps or moves, Shift replaces, Alt duplicates.
    const auto header = [](int slot) { return std::pair{88 + 212 * (slot % 4) + 60, grid_layout::cardTop + grid_layout::rowPitch * (slot / 4) + 14}; };
    const auto dragTile = [&](int from, int to, WPARAM keys = 0, bool alt = false) {
      BYTE previous[256]{}, state[256]{};
      assert(GetKeyboardState(previous)); std::copy(std::begin(previous), std::end(previous), state);
      // Use the requested gesture even if the user is typing while this host runs.
      state[VK_MENU] = alt ? 0x80 : 0;
      state[VK_SHIFT] = (keys & MK_SHIFT) ? 0x80 : 0;
      state[VK_CONTROL] = (keys & MK_CONTROL) ? 0x80 : 0;
      assert(SetKeyboardState(state));
      const auto [x0, y0] = header(from); const auto [x1, y1] = header(to);
      SendMessage(editor, WM_LBUTTONDOWN, MK_LBUTTON | keys, MAKELPARAM(x0, y0));
      SendMessage(editor, WM_MOUSEMOVE, MK_LBUTTON | keys, MAKELPARAM((x0 + x1) / 2, (y0 + y1) / 2));
      SendMessage(editor, WM_MOUSEMOVE, MK_LBUTTON | keys, MAKELPARAM(x1, y1));
      SendMessage(editor, WM_LBUTTONUP, keys, MAKELPARAM(x1, y1));
      assert(SetKeyboardState(previous));
      pumpFor(150);
    };
    const auto effects = [&] {
      const auto config = save().config();
      std::array<int, 4> result{};
      for (int slot = 0; slot < 4; ++slot) result[slot] = config["SlotEffects"][slot];
      return result;
    };
    const int tone = static_cast<int>(SlotEffect::ToneEQ), cut = static_cast<int>(SlotEffect::CutEQ);
    const int ir = static_cast<int>(SlotEffect::CabinetIR), none = static_cast<int>(SlotEffect::NAM);
    assert((effects() == std::array<int, 4>{tone, cut, ir, none}));
    dragTile(0, 1); // Occupied target: swap.
    if (effects() != std::array<int, 4>{cut, tone, ir, none})
    {
      const auto actual = effects();
      std::cerr << "Drag swap produced " << actual[0] << ',' << actual[1] << ',' << actual[2] << ',' << actual[3] << '\n';
    }
    assert((effects() == std::array<int, 4>{cut, tone, ir, none}));
    dragTile(1, 3); // Empty target: move.
    assert((effects() == std::array<int, 4>{cut, none, ir, tone}));
    dragTile(0, 2, MK_SHIFT); // Shift replaces the target and empties the source.
    assert((effects() == std::array<int, 4>{none, none, cut, tone}));
    assert(save().config()["SlotPaths"][2] == "");
    dragTile(2, 0, 0, true); // Alt duplicates.
    assert((effects() == std::array<int, 4>{cut, none, cut, tone}));
    dragTile(1, 3); // An empty tile cannot be dragged.
    assert((effects() == std::array<int, 4>{cut, none, cut, tone}));
    load(eqUI); selectTile(170, 120);
    std::cout << "Tile drag swap, move, Shift replace and Alt duplicate passed\n";
  }
  const auto resetDial = [&](int x, int y) {
    SendMessage(editor, WM_LBUTTONDBLCLK, MK_LBUTTON, MAKELPARAM(x, y));
    SendMessage(editor, WM_LBUTTONUP, 0, MAKELPARAM(x, y)); pumpFor(30);
  };
  // Recall edited Cloud settings; reset returns to Cloud rather than the
  // Room/shared-bank value. Subsequent insertion discards the prior edits.
  auto cloud = browserState;
  cloud["SlotEffects"][0] = static_cast<int>(SlotEffect::CloudReverb);
  cloud["SlotPaths"][0] = "";
  cloud["Pre 1 Reverb Decay"] = 12.;
  load(cloud); selectTile(170, 120); pumpFor(60);
  resetDial(grid_layout::KnobX(0), grid_layout::KnobGestureY(0));
  assert(std::abs(save().config()["Pre 1 Reverb Decay"].get<double>() - 6.) < 1.e-6);
  win_edit_menu::SelectNamedItem(editor, 220, grid_layout::moduleButtonY, L"Room"); pumpFor(30);
  assert(std::abs(save().config()["Pre 1 Reverb Decay"].get<double>() - .8) < 1.e-6);
  assert(save().config()["Pre 1 Reverb Size"] == 80.);
  load(eqUI); selectTile(170, 120); pumpFor(60);
  // Numeric text is below each dial; entry and reset must retain its binding.
  InvalidateRect(editor, nullptr, FALSE); UpdateWindow(editor);
  click(grid_layout::KnobX(0), grid_layout::KnobValueY(0));
  SendMessage(editor, WM_LBUTTONDBLCLK, MK_LBUTTON, MAKELPARAM(grid_layout::KnobX(0), grid_layout::KnobValueY(0)));
  SendMessage(editor, WM_LBUTTONUP, 0, MAKELPARAM(grid_layout::KnobX(0), grid_layout::KnobValueY(0)));
  keyAtEditor('3'); keyAtEditor(VK_RETURN);
  assert(save().config()["Pre 1 EQ Bass"] == 3.);
  resetDial(grid_layout::KnobX(0), grid_layout::KnobGestureY(0));
  assert(save().config()["Pre 1 EQ Bass"] == 0.);
  {
    auto previous = save().config();
    auto cosmic = previous;
    cosmic["SlotEffects"][0] = static_cast<int>(SlotEffect::CosmicEcho);
    load(cosmic); selectTile(170, 120);
    InvalidateRect(editor, nullptr, FALSE); UpdateWindow(editor);
    click(grid_layout::KnobX(0), grid_layout::KnobValueY(0));
    keyAtEditor('2'); keyAtEditor('5'); keyAtEditor('0'); keyAtEditor(VK_RETURN);
    // Time uses a power curve, so the host's normalized round trip need not be bit-exact.
    const double enteredTime = save().config()["Pre 1 Cosmic Time"].get<double>();
    if (std::abs(enteredTime - 250.) >= 1.e-6) std::cerr << "Typed Cosmic Time became " << enteredTime << " ms\n";
    assert(std::abs(enteredTime - 250.) < 1.e-6);
    load(previous); selectTile(170, 120);
    std::cout << "Cosmic Echo: direct numeric entry of 250 ms passed\n";
  }
  for (const auto& control : std::array<std::tuple<int, int, int, const char*, double>, 3>{{
         {37, 130, 100, "Input", 0.}, {967, 298, 268, "A/B Blend", 50.}, {967, 442, 412, "Output", 0.}}})
  {
    const auto [x, valueY, dialY, name, initial] = control;
    click(x, valueY); keyAtEditor('6'); keyAtEditor(VK_RETURN);
    assert(save().config()[name] == 6.);
    resetDial(x, dialY); assert(save().config()[name] == initial);
  }
  std::cout << "Editable values below module/IN/OUT/A-B dials and double-click defaults passed\n";
  const auto beforeGains = save().config();
  dragAt(38, 105); dragAt(967, 272); dragAt(967, 416);
  assert(save().config()["Input"] != beforeGains["Input"]);
  assert(save().config()["A/B Blend"] != beforeGains["A/B Blend"]);
  assert(save().config()["Output"] != beforeGains["Output"]);
  assert(save().config()["Pre 2 NAM Input"] == beforeGains["Pre 2 NAM Input"]); // IN is global, not the Pre 2 NAM input.
  auto rigToSave = browserState;
  rigToSave["SlotEffects"][0] = static_cast<int>(SlotEffect::Rotary);
  rigToSave["SlotEffects"][2] = static_cast<int>(SlotEffect::NoisePrint);
  rigToSave["SlotEffects"][3] = static_cast<int>(SlotEffect::LoFiTape);
  rigToSave["Pre 4 LoFi Speed"] = 3.75; rigToSave["Pre 4 LoFi Wow"] = 35.;
  rigToSave["Pre 4 LoFi Wear"] = 80.; rigToSave["Pre 4 LoFi Noise"] = 0.;
  rigToSave["SlotPaths"][2] = "";
  rigToSave["NoisePrintProfiles"][2] = learnedNoiseState["NoisePrintProfiles"][0];
  rigToSave["Pre 3 Noise Gate Threshold"] = -42.;
  rigToSave["Pre 3 Noise Learn"] = 1.;
  rigToSave["SlotPaths"][1] = path2.string();
  rigToSave["Input"] = -3.; rigToSave["Output"] = -8.; rigToSave["Lane A Level"] = 1.5;
  rigToSave["Pre 1 Freeze Hold"] = 1.;
  load(rigToSave);
  const auto presetDirectory = fixtures / ("presets-" + std::to_string(GetTickCount64()));
  std::filesystem::create_directories(presetDirectory);
  const auto presetFile = presetDirectory / "Clean rig.ngrid";
  const auto presetMenu = [&](const wchar_t* name) { win_edit_menu::SelectNamedItem(editor, 470, 28, name, true); pumpFor(150); };
  std::cout << "Checking native preset dialogs and folder browser\n";
  win_file_dialog::Choose(presetFile, [&] { presetMenu(L"Save preset as..."); });
  std::cout << "Native preset save passed\n";
  assert(std::filesystem::exists(presetFile));
  const auto savedRig = rig_preset::Read(presetFile);
  assert(savedRig["Input"] == -3. && savedRig["Pre 1 Freeze Hold"] == 0.);
  assert(savedRig["Pre 3 Noise Learn"] == 0.);
  assert(savedRig["SlotEffects"][3] == static_cast<int>(SlotEffect::LoFiTape));
  assert(savedRig["Pre 4 LoFi Speed"] == 3.75 && savedRig["Pre 4 LoFi Wear"] == 80.);
  assert(savedRig["NoisePrintProfiles"][2] == learnedNoiseState["NoisePrintProfiles"][0]);
  assert(!savedRig.contains("NamFolders") && !savedRig.contains("IrFolders") && !savedRig.contains("PresetFolders") && !savedRig.contains("Tuner Reference") && !savedRig.contains("Tuner"));
  auto anotherRig = cleared;
  anotherRig["Tuner Reference"] = 442.; anotherRig["Tuner Mute"] = 1.;
  load(anotherRig);
  host.dirty = false; host.rescanned = 0;
  win_file_dialog::Choose(presetFile, [&] { presetMenu(L"Load preset file..."); });
  std::cout << "Native preset load passed\n";
  const auto restoredRig = save().config();
  assert(restoredRig["SlotEffects"] == savedRig["SlotEffects"] && restoredRig["SlotPaths"] == savedRig["SlotPaths"]);
  assert(restoredRig["SlotHashes"] == savedRig["SlotHashes"]);
  assert(restoredRig["Input"] == -3. && restoredRig["Output"] == -8. && restoredRig["Lane A Level"] == 1.5);
  assert(restoredRig["NoisePrintProfiles"] == savedRig["NoisePrintProfiles"]);
  for (int p = uLoFiSpeed; p <= uLoFiOutput; ++p)
  {
    const std::string name = std::string("Pre 4 ") + kEffectParameters[p].name;
    assert(restoredRig[name] == savedRig[name]);
  }
  assert(restoredRig["Pre 3 Noise Learn"] == 0. && restoredRig["Pre 3 Noise Gate Threshold"] == -42.);
  assert(userFolders().irFolders == pathTexts({irLibrary}) && restoredRig["Tuner Reference"] == 442.);
  assert(restoredRig["Pre 1 Freeze Hold"] == 0. && restoredRig["PresetName"] == presetFile.stem().string());
  assert(host.dirty && (host.rescanned & CLAP_PARAM_RESCAN_VALUES));
  win_file_dialog::Choose({}, [&] { presetMenu(L"Load preset file..."); }, true);
  std::cout << "Native preset cancel passed\n";
  assert(save().config() == restoredRig); // Cancel leaves the complete rig unchanged.
  auto secondRig = savedRig; secondRig["Output"] = -10.;
  rig_preset::Write(presetDirectory / "Second rig.ngrid", secondRig);
  // Preset folders are added in Library & settings; the browser links there.
  click(470, 28);
  click(430, 28); // Preset context shows no tabs, so no NAM tab here.
  click(100, 688); // Manage folders opens settings.
  win_file_dialog::Choose(presetDirectory, [&] { click(912, 138); }, false, true);
  pumpFor(250);
  assert(userFolders().presetFolders == pathTexts({presetDirectory})); // Saved for this user, not the project.
  click(936, 28); click(470, 28);
  if (argc == 4) CaptureEditor(editor, std::filesystem::path(argv[3]) / "browser-presets.bmp");
  chooseBrowserItem("SECOND RIG");
  assert(save().config()["Output"] == -10. && userFolders().presetFolders.size() == 1);
  click(kPreviousX, grid_layout::headerY);
  click(kNextX, grid_layout::headerY);
  assert(save().config()["PresetName"] == "Second rig" && save().config()["Output"] == -10.);
  click(kPreviousX, grid_layout::headerY);
  if (save().config()["PresetName"] != "Clean rig" || save().config()["Output"] != -8.)
    std::cerr << "Previous preset: " << save().config()["PresetName"] << ", output=" << save().config()["Output"] << '\n';
  assert(save().config()["PresetName"] == "Clean rig" && save().config()["Output"] == -8.);
  click(470, 28); enterBrowserSearch("SECOND RIG"); keyAtEditor(VK_RETURN); click(932, 688); // Load button.
  assert(save().config()["PresetName"] == "Second rig");
  click(kPreviousX, grid_layout::headerY);
  if (argc == 4) CaptureEditor(editor, std::filesystem::path(argv[3]) / "preset-gains.bmp");
  std::cout << "Preset folder selection and previous/next passed\n";
  // Rig preset switches while audio plays: Short fade dips briefly, Crossfade never drops
  // out, and neither clicks. Audio runs on its own thread at real-time pace.
  const auto measureSwitch = [&](bool crossfade) {
    constexpr int block = 256, rate = 48000;
    std::vector<double> recorded(rate * 3, 0.);
    std::atomic<bool> running{true};
    std::atomic<size_t> written{0};
    std::array<std::array<double, block>, 2> switchIn{}, switchOut{};
    double* switchIns[2]{switchIn[0].data(), switchIn[1].data()};
    double* switchOuts[2]{switchOut[0].data(), switchOut[1].data()};
    clap_audio_buffer switchInBuffer{nullptr, switchIns, 2, 0, 0}, switchOutBuffer{nullptr, switchOuts, 2, 0, 0};
    clap_process switchProcess{};
    switchProcess.audio_inputs = &switchInBuffer; switchProcess.audio_outputs = &switchOutBuffer;
    switchProcess.audio_inputs_count = switchProcess.audio_outputs_count = 1;
    switchProcess.in_events = &events; switchProcess.out_events = &sent;
    switchProcess.frames_count = block;
    assert(plugin->activate(plugin, rate, 1, block));
    std::thread audio([&] {
      assert(plugin->start_processing(plugin));
      auto next = std::chrono::steady_clock::now();
      for (size_t at = 0; running && at + block <= recorded.size(); at += block)
      {
        for (int i = 0; i < block; ++i)
          switchIn[0][i] = switchIn[1][i] = .1 * std::sin(2. * 3.141592653589793 * 220. * static_cast<double>(at + i) / rate);
        assert(plugin->process(plugin, &switchProcess) != CLAP_PROCESS_ERROR);
        std::copy_n(switchOut[0].data(), block, recorded.data() + at);
        written = at + block;
        next += std::chrono::microseconds(1000000 * block / rate);
        std::this_thread::sleep_until(next);
      }
      plugin->stop_processing(plugin);
    });
    pumpFor(500);
    const auto before = save().config()["PresetName"];
    const size_t switchAt = written;
    click(kPreviousX, grid_layout::headerY); // Previous preset while audio plays.
    pumpFor(700);
    running = false;
    audio.join();
    plugin->deactivate(plugin);
    assert(save().config()["PresetName"] != before);
    const size_t end = written, settle = rate / 5, tail = rate / 4, window = rate / 400; // 2.5 ms
    assert(switchAt > settle + window && end > switchAt + tail);
    const auto rms = [&](size_t from) {
      double energy = 0.;
      for (size_t i = from; i < from + window; ++i) energy += recorded[i] * recorded[i];
      return std::sqrt(energy / window);
    };
    const auto median = [&](size_t from, size_t to) {
      std::vector<double> levels;
      for (size_t at = from; at + window <= to; at += window) levels.push_back(rms(at));
      std::nth_element(levels.begin(), levels.begin() + levels.size() / 2, levels.end());
      return levels[levels.size() / 2];
    };
    const double reference = std::min(median(settle, switchAt), median(end - tail, end));
    assert(reference > 1e-4);
    int quiet = 0;
    for (size_t at = switchAt; at + window <= end - tail; at += window) quiet += rms(at) < .25 * reference;
    const auto maxStep = [&](size_t from, size_t to) {
      double step = 0.;
      for (size_t i = from + 1; i < to; ++i) step = std::max(step, std::abs(recorded[i] - recorded[i - 1]));
      return step;
    };
    const double steady = std::max(maxStep(settle, switchAt), maxStep(end - tail, end));
    const double during = maxStep(switchAt, end - tail);
    std::cout << (crossfade ? "Crossfade" : "Short fade") << " preset switch: " << quiet * 2.5 << " ms below -12 dB, "
              << "largest step " << during << " vs steady " << steady << "\n";
    assert(during <= 2. * steady + 1e-6); // No click from a discontinuity.
    if (crossfade) assert(quiet == 0);
    else assert(quiet >= 1 && quiet * 2.5 <= 60.);
  };
  // Two clearly audible rigs in their own folder, so the arrows switch between them.
  const auto switchDirectory = presetDirectory / "switch";
  std::filesystem::create_directories(switchDirectory);
  auto switchA = cleared, switchB = cleared;
  switchB["SlotEffects"][0] = static_cast<int>(SlotEffect::TubeBoost); switchB["Output"] = -6.;
  rig_preset::Write(switchDirectory / "Switch A.ngrid", switchA);
  rig_preset::Write(switchDirectory / "Switch B.ngrid", switchB);
  click(470, 28); click(218, 688); // Rescan, beside Manage folders, picks up the new folder.
  pumpFor(500);
  chooseBrowserItem("SWITCH A");
  assert(save().config()["PresetName"] == "Switch A");
  measureSwitch(false);
  click(910, 28); click(861, 88); click(936, 28); // Library & settings: Preset switch -> Crossfade.
  measureSwitch(true);
  click(470, 28); click(100, 144); chooseBrowserItem("CLEAN RIG"); // All folders, then search.
  assert(save().config()["PresetName"] == "Clean rig");
  std::cout << "Preset switch short fade and crossfade passed\n";
  // Shift-click on header Save writes over the loaded preset file without a dialog.
  auto quickEdit = save().config(); quickEdit["Output"] = -5.;
  load(quickEdit);
  shiftClick(kSaveX, grid_layout::headerY);
  assert(rig_preset::Read(presetFile)["Output"] == -5. && save().config()["PresetName"] == "Clean rig");
  std::cout << "Header quick save passed\n";
  // Deleting the selected (current) preset removes only its file, after confirmation.
  const auto beforeDelete = save().config();
  click(470, 28);
  win_file_dialog::Confirm(false, [&] { click(775, 688); });
  assert(std::filesystem::exists(presetFile));
  win_file_dialog::Confirm(true, [&] { click(775, 688); });
  assert(!std::filesystem::exists(presetFile) && std::filesystem::exists(presetDirectory / "Second rig.ngrid"));
  assert(save().config() == beforeDelete);
  keyAtEditor(VK_ESCAPE);
  std::cout << "Preset delete passed\n";
  // Without an existing file, header Save falls back to Save as.
  const auto savedAgain = presetDirectory / "Saved again.ngrid";
  win_file_dialog::Choose(savedAgain, [&] { click(kSaveX, grid_layout::headerY); });
  assert(std::filesystem::exists(savedAgain) && save().config()["PresetName"] == "Saved again");
  // With a loaded file, a plain click is still Save as: overwriting needs Shift/Alt or the right-click menu.
  const auto savedCopy = presetDirectory / "Saved copy.ngrid";
  win_file_dialog::Choose(savedCopy, [&] { click(kSaveX, grid_layout::headerY); });
  assert(std::filesystem::exists(savedCopy) && save().config()["PresetName"] == "Saved copy");
  // New asks first, then starts an empty grid and keeps library folders and the preset folder.
  const auto beforeNew = save().config();
  win_file_dialog::Confirm(false, [&] { click(kNewX, grid_layout::headerY); });
  assert(save().config() == beforeNew);
  win_file_dialog::Confirm(true, [&] { click(kNewX, grid_layout::headerY); });
  const auto fresh = save().config();
  assert(fresh["PresetName"] == "Untitled" && fresh["PresetDirectory"] == beforeNew["PresetDirectory"]);
  assert(userFolders().presetFolders.size() == 1 && userFolders().namFolders == pathTexts({libraryFolder}));
  assert(fresh["Output"] == 0. && fresh["Input"] == 0.);
  for (int slot = 0; slot < 16; ++slot)
    assert(fresh["SlotEffects"][slot] == static_cast<int>(SlotEffect::NAM) && fresh["SlotPaths"][slot] == "");
  std::cout << "Header Save as fallback and New passed\n";
  if (browserOnly)
  {
    if (argc == 4)
    {
      // Representative dense screens for typography/layout review; no audio device.
      {
        // Long capture names wrap to two lines on the tile before an ellipsis.
        const auto longName = fixtures / "Fender Super Reverb 1977 - Clean Channel Bright Switch On.nam";
        const auto longToken = fixtures / "Marshall_JCM800_2203_Lead_Channel_Gain7_Presence6_SM57.nam";
        std::filesystem::copy_file(path2, longName, std::filesystem::copy_options::overwrite_existing);
        std::filesystem::copy_file(path2, longToken, std::filesystem::copy_options::overwrite_existing);
        auto names = browserState;
        names["SlotPaths"][4] = longName.string(); names["SlotPaths"][5] = longToken.string();
        names["SlotEffects"][6] = static_cast<int>(SlotEffect::ButterComp);
        load(names); selectTile(170, 224); pumpFor(250);
        CaptureEditor(editor, std::filesystem::path(argv[3]) / "grid-long-names.bmp");
        selectTile(594, 224); pumpFor(250);
        CaptureEditor(editor, std::filesystem::path(argv[3]) / "butter-comp.bmp");
      }
      for (const auto [effect, name] : std::array<std::pair<SlotEffect, const char*>, 11>{{
             {SlotEffect::ConsoleEQ, "console-eq"}, {SlotEffect::DigitalDelay, "delay"},
             {SlotEffect::CosmicEcho, "cosmic"}, {SlotEffect::Chorus, "chorus"},
             {SlotEffect::LoFiTape, "lofi"}, {SlotEffect::OutboardChannel, "outboard"},
             {SlotEffect::Octaver, "octaver"}, {SlotEffect::Utility, "utility"}, {SlotEffect::Bitcrusher, "bitcrusher"},
             {SlotEffect::Wah, "wah"}, {SlotEffect::MultiHeadDelay, "multi-head"}}})
      {
        auto preview = browserState;
        preview["SlotEffects"][0] = static_cast<int>(effect);
        preview["SlotPaths"][0] = "";
        load(preview); selectTile(170, 120);
        CaptureEditor(editor, std::filesystem::path(argv[3]) / (std::string("readable-") + name + ".bmp"));
      }
      click(910, 28);
      CaptureEditor(editor, std::filesystem::path(argv[3]) / "readable-settings.bmp");
      click(936, 28); click(grid_layout::width - 36, 356);
      CaptureEditor(editor, std::filesystem::path(argv[3]) / "readable-routing.bmp");
      click((grid_layout::parameterLeft + 100 + grid_layout::parameterRight - 304) / 2, grid_layout::moduleButtonY);
      assert(save().config()["Lane Mode"] == grid_performance::Sequential);
      CaptureEditor(editor, std::filesystem::path(argv[3]) / "readable-routing-sequential.bmp");
      click((grid_layout::parameterLeft + 100 + grid_layout::parameterRight - 304) / 2, grid_layout::moduleButtonY);
      assert(save().config()["Lane Mode"] == grid_performance::Parallel);
      click(grid_layout::parameterRight - 40, grid_layout::moduleButtonY); click(kModulationX, grid_layout::headerY);
      CaptureEditor(editor, std::filesystem::path(argv[3]) / "readable-modulation.bmp");
      click(936, 28); click(kTunerX, grid_layout::headerY);
      CaptureEditor(editor, std::filesystem::path(argv[3]) / "readable-tuner.bmp");
      keyAtEditor(VK_ESCAPE);
    }
    gui->destroy(plugin); DestroyWindow(parent);
    plugin->destroy(plugin); entry->deinit(); FreeLibrary(library);
    std::cout << "Focused native browser smoke passed\n";
    return 0;
  }
  load(eqUI); selectTile(170, 120);
  dragAt(grid_layout::KnobX(0), grid_layout::KnobGestureY(0));
  assert(save().config()["Pre 1 EQ Bass"] != 0.);
  selectTile(390, 120); // Select IN2 Cut EQ; only its controls may receive gestures.
  dragAt(grid_layout::KnobX(0), grid_layout::KnobGestureY(0));
  assert(save().config()["Pre 2 EQ Low Cut"] != 0.);
  selectTile(610, 120); // Select IN3 IR loader; its Output sits below the first column.
  dragAt(grid_layout::KnobX(4), grid_layout::KnobGestureY(4));
  assert(save().config()["Pre 3 IR Output"] != -18.);
  {
    auto original = save().config();
    if (argc == 4) CaptureEditor(editor, std::filesystem::path(argv[3]) / "ir-controls-cabinet.bmp");
    for (int p : {uIRInput, uIRMix, uIRPredelay, uIRLowCut, uIRHighCut})
    {
      const auto key = std::string("Pre 3 ") + kEffectParameters[p].name;
      const double before = save().config()[key];
      const int position = effect_presentation::Position(SlotEffect::CabinetIR, p);
      dragAt(grid_layout::KnobX(position), grid_layout::KnobGestureY(position),
             before == kEffectParameters[p].maximum ? -25 : 25);
      assert(save().config()[key] != before);
    }
    // The length selector reloads the same resource on idle.
    host.dirty = false;
    click(optionX(uIRLength), grid_layout::optionsY);
    assert(save().config()["Pre 3 IR Length"] == 1. && host.dirty);
    pumpFor(200);
    if (argc == 4) CaptureEditor(editor, std::filesystem::path(argv[3]) / "ir-controls-long.bmp");
    const auto recalledIR = save();
    assert(loadBytes(recalledIR));
    assert(save().config()["Pre 3 IR Length"] == 1.);
    selectTile(610, 120);
    click(optionX(uIRLength), grid_layout::optionsY);
    assert(save().config()["Pre 3 IR Length"] == 0.);
    load(original);
  }
  auto utilities = browserState;
  utilities["SlotEffects"][0] = static_cast<int>(SlotEffect::ParametricEQ);
  utilities["SlotEffects"][1] = static_cast<int>(SlotEffect::Boost);
  utilities["Pre 1 EQ High Cut"] = 10000.;
  load(utilities);
  selectTile(170, 120);
  for (int position : {0, 1, 2, 4, 6}) // Frequency, Gain, Q, then Low cut and High cut side by side.
    dragAt(grid_layout::KnobX(position), grid_layout::KnobGestureY(position));
  auto editedUtility = save().config();
  assert(editedUtility["Pre 1 Parametric Frequency"] != utilities["Pre 1 Parametric Frequency"]);
  assert(editedUtility["Pre 1 Parametric Gain"] != utilities["Pre 1 Parametric Gain"]);
  assert(editedUtility["Pre 1 Parametric Q"] != utilities["Pre 1 Parametric Q"]);
  assert(editedUtility["Pre 1 EQ Low Cut"] != utilities["Pre 1 EQ Low Cut"]);
  assert(editedUtility["Pre 1 EQ High Cut"] != utilities["Pre 1 EQ High Cut"]);
  if (argc == 4)
    CaptureEditor(editor, std::filesystem::path(argv[3]) / "parametric-eq.bmp");
  selectTile(390, 120);
  dragAt(grid_layout::KnobX(0), grid_layout::KnobGestureY(0));
  assert(save().config()["Pre 2 Boost Gain"] != utilities["Pre 2 Boost Gain"]);
  dragAt(grid_layout::KnobX(1), grid_layout::KnobGestureY(1));
  dragAt(grid_layout::KnobX(2), grid_layout::KnobGestureY(2));
  assert(save().config()["Pre 2 Boost Tilt"] != utilities["Pre 2 Boost Tilt"]);
  assert(save().config()["Pre 2 EQ Low Cut"] != utilities["Pre 2 EQ Low Cut"]);
  assert(save().config()["Pre 2 NAM Input"] == utilities["Pre 2 NAM Input"]);
  if (argc == 4)
    CaptureEditor(editor, std::filesystem::path(argv[3]) / "boost.bmp");
  // Every visible new knob must update its slot's automated parameter, with no
  // hidden controls stealing gestures when modules are restored into an open UI.
  for (const auto& group : nam_library::kModuleGroups)
    for (const auto effect : group.effects)
  {
    // Smart Gate's transient Learn control has a dedicated native test above.
    if (effect == SlotEffect::NoisePrint) continue;
    const int id = static_cast<int>(effect);
    auto effectControls = browserState;
    effectControls["SlotEffects"][0] = id;
    load(effectControls);
    selectTile(170, 120);
    for (int p = 0; p < kUtilityControls; ++p)
      if (UsesUtility(effect, p) && !effect_presentation::DisplayControl(p)) // Display drags: tested below.
      {
        // Shelf gain is conditional: expose it before testing its gesture.
        if (effect == SlotEffect::ParametricEQ && p == uParamShelfGain)
        {
          click(optionX(uParamLowMode), grid_layout::optionsY);
          assert(save().config()["Pre 1 Parametric Low Mode"] == 1.);
        }
        const auto& descriptor = kEffectParameters[p];
        const std::string name = std::string("Pre 1 ") + descriptor.name;
        const auto before = save().config();
        if (IsAlgorithmMode(p))
        {
          host.dirty = false;
          algorithmAutomation.clear();
        }
        // Drag down at an upper limit (Tape Mix defaults to 100%).
        const int position = effect_presentation::Position(effect, p);
        const int x = grid_layout::KnobX(position);
        if (!EffectChoices(p).empty())
        {
          const int current = static_cast<int>(before[name].get<double>());
          const int next = current >= descriptor.maximum ? static_cast<int>(descriptor.minimum) : current + 1;
          if (effect_presentation::ChoiceDropdown(p)) selectChoice(p, next, current, false, effect);
          else click(effect_presentation::HeaderControl(p) ? optionX(p, effect) : x,
                     effect_presentation::HeaderControl(p) ? grid_layout::optionsY : grid_layout::ToggleY(position));
        }
        else if (EffectChoices(p).empty() && descriptor.minimum == 0. && descriptor.maximum == 1. && descriptor.step == 1.)
          click(effect_presentation::HeaderControl(p) ? optionX(p, effect) : x,
                effect_presentation::HeaderControl(p) ? grid_layout::optionsY : grid_layout::ToggleY(position));
        else
        {
          const int distance = descriptor.step == 1. && descriptor.maximum - descriptor.minimum <= 4. ? 200 : 25;
          const int delta = before[name].get<double>() >= descriptor.maximum ? distance : -distance;
          SendMessage(editor, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, grid_layout::KnobGestureY(position)));
          SendMessage(editor, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(x, grid_layout::KnobGestureY(position) + delta));
          SendMessage(editor, WM_LBUTTONUP, 0, MAKELPARAM(x, grid_layout::KnobGestureY(position) + delta));
        }
        pumpFor(30);
        if (save().config()[name] == before[name]) std::cerr << "Unchanged UI parameter: " << name << " effect=" << id << "\n";
        assert(save().config()[name] != before[name]);
        if (IsAlgorithmMode(p))
        {
          params->flush(plugin, &events, &sent);
          assert(host.dirty); // Saved-state notification replaces automation.
          assert(algorithmAutomation.empty());
        }
      }
    toggleHeader();
    assert(save().config()["Pre 1 Enabled"] == 0.);
    if (argc == 4)
      CaptureEditor(editor, std::filesystem::path(argv[3]) / (std::string("effect-") + std::to_string(id) + ".bmp"));
  }
  {
    auto category = browserState;
    category["SlotEffects"][0] = static_cast<int>(SlotEffect::DigitalDelay);
    load(category); selectTile(170, 120);
    click(grid_layout::moduleBrowseX, grid_layout::moduleButtonY); chooseBrowserItem("TAPE ECHO"); pumpFor(30);
    assert(save().config()["SlotEffects"][0] == static_cast<int>(SlotEffect::TapeDelay));
    click(grid_layout::moduleBrowseX, grid_layout::moduleButtonY); chooseBrowserItem("DUAL DELAY"); pumpFor(30);
    assert(save().config()["SlotEffects"][0] == static_cast<int>(SlotEffect::DualDelay));
  }
  // Multiband Dynamics: dragging a split line moves that split; dragging elsewhere moves
  // Threshold relative to where the drag started. Both are ordinary saved parameters.
  {
    auto bands = browserState;
    bands["SlotEffects"][0] = static_cast<int>(SlotEffect::Multiband);
    load(bands); selectTile(170, 120);
    const double left = grid_layout::visualizationLeft + 12., right = grid_layout::visualizationRight - 12.;
    const auto x = [&](double hz) { return static_cast<int>(std::lround(left + (right - left) * std::log(hz / 20.) / std::log(1000.))); };
    const int middle = (grid_layout::plotTop + grid_layout::plotBottom) / 2;
    const auto drag = [&](int x0, int y0, int x1, int y1) {
      SendMessage(editor, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x0, y0));
      SendMessage(editor, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(x1, y1));
      SendMessage(editor, WM_LBUTTONUP, 0, MAKELPARAM(x1, y1));
      pumpFor(30);
    };
    const auto before = save().config();
    const double split = before["Pre 1 Multiband Low Split"].get<double>();
    drag(x(split), middle, x(split * 2.), middle);
    const auto moved = save().config();
    assert(moved["Pre 1 Multiband Low Split"].get<double>() > split * 1.5);
    assert(moved["Pre 1 Multiband Threshold"] == before["Pre 1 Multiband Threshold"]);
    assert(moved["Pre 1 Multiband High Split"] == before["Pre 1 Multiband High Split"]);
    drag(x(700.), middle, x(700.), middle - 20);
    const auto raised = save().config();
    assert(raised["Pre 1 Multiband Threshold"].get<double>() > before["Pre 1 Multiband Threshold"].get<double>());
    assert(raised["Pre 1 Multiband Low Split"] == moved["Pre 1 Multiband Low Split"]);
  }
  // The larger top-right grid toggle changes bypass without replacing the module.
  {
    const auto before = save().config();
    click(grid_layout::cardLeft + 176, grid_layout::cardTop + 12);
    assert(save().config()["Pre 1 Enabled"] == 1. - before["Pre 1 Enabled"].get<double>());
    assert(save().config()["SlotEffects"] == before["SlotEffects"]);
    click(grid_layout::cardLeft + 176, grid_layout::cardTop + 12);
    assert(save().config()["Pre 1 Enabled"] == before["Pre 1 Enabled"]);
  }
  // Capture actual transport history in the compact editor, with no synthetic UI animation.
  {
    auto tapeUI = browserState;
    tapeUI["SlotEffects"][0] = static_cast<int>(SlotEffect::LoFiTape);
    tapeUI["Pre 1 LoFi Wow"] = 75.; tapeUI["Pre 1 LoFi Flutter"] = 65.;
    tapeUI["Pre 1 LoFi Wear"] = 60.; tapeUI["Pre 1 LoFi Noise"] = 10.;
    load(tapeUI); selectTile(170, 120);
    assert(plugin->activate(plugin, 48000., 1, 512));
    assert(plugin->start_processing(plugin));
    process.frames_count = 512;
    for (int block = 0; block < 300; ++block)
    {
      for (int i = 0; i < 512; ++i)
        input[0][i] = input[1][i] = .2 * std::sin(6.283185307179586 * 220. * (block * 512 + i) / 48000.);
      assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
      if (block % 4 == 0) pumpFor(5);
    }
    pumpFor(100);
    assert(save().config() == tapeUI);
    if (argc == 4) CaptureEditor(editor, std::filesystem::path(argv[3]) / "lofi-tape-transport.bmp");
    plugin->stop_processing(plugin); plugin->deactivate(plugin);
  }
  for (int effect : {static_cast<int>(SlotEffect::DigitalDelay), static_cast<int>(SlotEffect::RoomReverb),
                     static_cast<int>(SlotEffect::SpringReverb), static_cast<int>(SlotEffect::DualDelay)})
  {
    const auto module = static_cast<SlotEffect>(effect);
    for (int tank = 0; tank < (module == SlotEffect::SpringReverb ? 2 : 1); ++tank)
    {
      auto switchState = browserState;
      switchState["SlotEffects"][0] = effect;
      if (module == SlotEffect::SpringReverb) switchState["Pre 1 Spring Tank"] = tank;
      load(switchState);
      selectTile(170, 120);
      const int control = effect_presentation::PingPong(module) ? uDelayPingPong : uReverbStereo;
      const std::string name = std::string("Pre 1 ") + kEffectParameters[control].name;
      for (int toggle = 0; toggle < 2; ++toggle)
      {
        const auto before = save().config();
        click(optionX(control, module), grid_layout::optionsY);
        auto expected = before;
        expected[name] = 1. - before[name].get<double>();
        assert(save().config() == expected); // Stereo wet must not switch Spring's Tank.
      }
    }
  }
  {
    auto reverse = browserState; reverse["SlotEffects"][0] = static_cast<int>(SlotEffect::ReverseDelay);
    load(reverse); selectTile(170, 120);
    const auto before = save().config();
    click(optionX(uGateMode), grid_layout::optionsY); // No ineffective Ping-pong control here.
    assert(save().config() == before);
    auto vibe = browserState; vibe["SlotEffects"][0] = static_cast<int>(SlotEffect::UniVibe);
    vibe["Pre 1 Vibe Vibrato Mode"] = 1.; vibe["Pre 1 Mod Mix"] = 50.;
    const int mix = effect_presentation::Position(SlotEffect::UniVibe, uModMix);
    load(vibe); selectTile(170, 120); dragAt(grid_layout::KnobX(mix), grid_layout::KnobGestureY(mix));
    assert(save().config()["Pre 1 Mod Mix"] == 50.); // Fully wet mode hides Mix.
    vibe["Pre 1 Vibe Vibrato Mode"] = 0.; load(vibe); selectTile(170, 120);
    dragAt(grid_layout::KnobX(mix), grid_layout::KnobGestureY(mix));
    assert(save().config()["Pre 1 Mod Mix"] != 50.);
  }
  const auto packedPath = fixtures / "packed-a2.nam";
  std::ofstream(packedPath) << fixture::packed().dump();
  // All voices expose Character in the eighth available dial grid, with a
  // centered reset. Header and grid circles share the original Enabled binding.
  {
    auto previous = save().config();
    for (int style = 0; style < effect_modes::pedalCount; ++style)
    {
      auto pedal = cleared;
      pedal["SlotEffects"][0] = static_cast<int>(SlotEffect::PedalDrive);
      pedal["Pre 1 Pedal Style"] = style;
      load(pedal); selectTile(170, 120);
      dragAt(grid_layout::KnobX(3), grid_layout::KnobGestureY(3));
      assert(save().config()["Pre 1 Pedal Character"].get<double>() > 0.);
      assert(save().config()["Pre 1 Pedal Body"] == pedal["Pre 1 Pedal Body"]);
      resetDial(grid_layout::KnobX(3), grid_layout::KnobGestureY(3));
      assert(save().config()["Pre 1 Pedal Character"] == 0.);
      toggleHeader();
      assert(save().config()["Pre 1 Enabled"] == 0.);
      click(grid_layout::cardLeft + 176, 80);
      assert(save().config()["Pre 1 Enabled"] == 1.);
      assert(save().config()["Pre 1 Pedal Character"] == 0.);
      if (argc == 4) CaptureEditor(editor, std::filesystem::path(argv[3]) / ("pedal-character-" + std::to_string(style) + ".bmp"));
    }
    load(previous);
  }
  auto qualityState = browserState;
  qualityState["SlotPaths"][0] = packedPath.string();
  qualityState["Pre 1 Full Quality"] = 1.;
  qualityState["Pre 1 EQ High Cut"] = 10000.;
  load(qualityState);
  pumpFor(100);
  assert(plugin->activate(plugin, 48000., 1, 512));
  assert(plugin->start_processing(plugin));
  process.frames_count = 512;
  selectTile(170, 120); // Select IN1.
  const auto automate = [&](const char* name, double value) {
    clap_param_info info{};
    bool found = false;
    for (uint32_t i = 0; i < params->count(plugin); ++i)
    {
      params->get_info(plugin, i, &info);
      if (std::strcmp(info.name, name) == 0)
      {
        found = true;
        break;
      }
    }
    assert(found);
    clap_event_param_value change{};
    change.header.size = sizeof(change);
    change.header.type = CLAP_EVENT_PARAM_VALUE;
    change.param_id = info.id;
    change.value = value;
    change.note_id = change.port_index = change.channel = change.key = -1;
    clap_input_events updates{&change, [](const clap_input_events*) -> uint32_t { return 1; },
                              [](const clap_input_events* e, uint32_t) -> const clap_event_header* {
                                return &static_cast<clap_event_param_value*>(e->ctx)->header;
                              }};
    params->flush(plugin, &updates, &sent);
    pumpFor(100);
  };
  automate("Pre 1 Full Quality", 0.); // Preparation happens on the idle/control thread.
  assert(save().config()["Pre 1 Full Quality"] == 0.);
  click(grid_layout::qualityLeft + 58, grid_layout::optionsY);
  assert(save().config()["Pre 1 Full Quality"] == 1.);
  click(grid_layout::qualityLeft + 58, grid_layout::optionsY);
  assert(save().config()["Pre 1 Full Quality"] == 0.);
  for (int i = 0; i < 30; ++i)
    assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
  dragAt(grid_layout::KnobX(0), grid_layout::KnobGestureY(0));
  dragAt(grid_layout::KnobX(1), grid_layout::KnobGestureY(1));
  dragAt(grid_layout::KnobX(2), grid_layout::KnobGestureY(2));
  dragAt(grid_layout::KnobX(3), grid_layout::KnobGestureY(3));
  // Output sits below Input; Low cut and High cut follow.
  dragAt(grid_layout::KnobX(4), grid_layout::KnobGestureY(4));
  assert(save().config()["Pre 1 NAM Output"] != qualityState["Pre 1 NAM Output"]);
  assert(save().config()["Pre 1 EQ Low Cut"] == qualityState["Pre 1 EQ Low Cut"]);
  dragAt(grid_layout::KnobX(5), grid_layout::KnobGestureY(5));
  assert(save().config()["Pre 1 EQ Low Cut"] != qualityState["Pre 1 EQ Low Cut"]);
  assert(save().config()["Pre 1 EQ High Cut"] == qualityState["Pre 1 EQ High Cut"]);
  dragAt(grid_layout::KnobX(6), grid_layout::KnobGestureY(6));
  const auto namControls = save().config();
  assert(namControls["Pre 1 NAM Input"] != qualityState["Pre 1 NAM Input"]);
  assert(namControls["Pre 1 EQ Bass"] != qualityState["Pre 1 EQ Bass"]);
  assert(namControls["Pre 1 EQ Mid"] != qualityState["Pre 1 EQ Mid"]);
  assert(namControls["Pre 1 EQ Treble"] != qualityState["Pre 1 EQ Treble"]);
  assert(namControls["Pre 1 NAM Output"] != qualityState["Pre 1 NAM Output"]);
  assert(namControls["Pre 1 EQ Low Cut"] != qualityState["Pre 1 EQ Low Cut"]);
  assert(namControls["Pre 1 EQ High Cut"] != qualityState["Pre 1 EQ High Cut"]);
  toggleHeader();
  assert(save().config()["Pre 1 Enabled"] == 0.);
  toggleHeader();
  assert(save().config()["Pre 1 Enabled"] == 1.);
  if (argc == 4)
    CaptureEditor(editor, std::filesystem::path(argv[3]) / "nam-eight-controls.bmp");
  auto liteSaved = save();
  automate("Pre 1 Full Quality", 1.);
  assert(save().config()["Pre 1 Full Quality"] == 1.);
  for (int i = 0; i < 30; ++i)
    assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
  plugin->stop_processing(plugin);
  plugin->deactivate(plugin);
  assert(loadBytes(liteSaved));
  assert(save().config()["Pre 1 Full Quality"] == 0.);
  assert(plugin->activate(plugin, 48000., 1, 512));
  assert(plugin->start_processing(plugin));
  assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
  plugin->stop_processing(plugin);
  plugin->deactivate(plugin);
  load(browserState);
  pumpFor(100);
  // Restore built-ins into an open editor: hidden NAM knobs must not intercept their controls.
  auto effectUI = browserState;
  effectUI["SlotEffects"][0] = static_cast<int>(SlotEffect::NoiseGate);
  effectUI["SlotEffects"][1] = static_cast<int>(SlotEffect::SmoothComp);
  load(effectUI);
  pumpFor(100);
  selectTile(170, 120);
  dragAt(grid_layout::KnobX(0), grid_layout::KnobGestureY(0)); // Selected IN1 gate threshold.
  assert(save().config()["Pre 1 Gate Threshold"] != effectUI["Pre 1 Gate Threshold"]);
  assert(save().config()["Pre 1 NAM Input"] == effectUI["Pre 1 NAM Input"]);
  click(optionX(uGateMode), grid_layout::optionsY); // Gate Mode sits in the header with the other mode selectors.
  assert(save().config()["Pre 1 Gate Mode"] == 1.);
  auto gateModes = save();
  assert(loadBytes(gateModes));
  assert(save().config()["Pre 1 Gate Mode"] == 1.);
  click(optionX(uGateMode), grid_layout::optionsY);
  assert(save().config()["Pre 1 Gate Mode"] == 0.);
  if (argc == 4)
    CaptureEditor(editor, std::filesystem::path(argv[3]) / "gate.bmp");
  selectTile(390, 120);
  dragAt(grid_layout::KnobX(2), grid_layout::KnobGestureY(2)); // Selected IN2 compressor makeup.
  assert(save().config()["Pre 2 Comp Makeup"] != effectUI["Pre 2 Comp Makeup"]);
  assert(save().config()["Pre 2 NAM Output"] == effectUI["Pre 2 NAM Output"]);
  // Card dot bypass and editor-header circle must share the same automated parameter.
  click(grid_layout::cardLeft + grid_layout::columnPitch + 176, grid_layout::cardTop + 16);
  assert(save().config()["Pre 2 Enabled"] == 0.);
  click(grid_layout::cardLeft + grid_layout::columnPitch + 176, grid_layout::cardTop + 16);
  assert(save().config()["Pre 2 Enabled"] == 1.);
  // FX navigation stays in the current menu category and wraps in both directions.
  auto navigation = browserState;
  navigation["SlotEffects"][0] = static_cast<int>(SlotEffect::JunoChorus);
  load(navigation); selectTile(170, 120);
  click(grid_layout::moduleNextX, grid_layout::moduleButtonY);
  assert(save().config()["SlotEffects"][0] == static_cast<int>(SlotEffect::Flanger));
  click(grid_layout::modulePreviousX, grid_layout::moduleButtonY);
  assert(save().config()["SlotEffects"][0] == static_cast<int>(SlotEffect::JunoChorus));
  // NAM/IR arrows use physical siblings, including outside configured roots.
  const auto neighbor = libraryFirst.parent_path() / "neighbor.nam";
  auto neighborModel = a2; neighborModel["metadata"]["name"] = "Unique sibling";
  std::ofstream(neighbor) << neighborModel.dump();
  const auto neighborIR = libraryIR.parent_path() / "neighbor.wav";
  std::filesystem::copy_file(libraryIR, neighborIR, std::filesystem::copy_options::overwrite_existing);
  { std::fstream wav(neighborIR, std::ios::in | std::ios::out | std::ios::binary); wav.seekp(-2, std::ios::end); wav.put(1); wav.put(0); }
  navigation = browserState;
  navigation["SlotPaths"][0] = libraryFirst.string();
  navigation["SlotEffects"][1] = static_cast<int>(SlotEffect::CabinetIR); navigation["SlotPaths"][1] = libraryIR.string();
  load(navigation); selectTile(170, 120);
  click(grid_layout::moduleNextX, grid_layout::moduleButtonY);
  assert(std::filesystem::path(save().config()["SlotPaths"][0].get<std::string>()) == neighbor);
  assert(save().config()["SlotEffects"][0] == 0);
  click(grid_layout::moduleNextX, grid_layout::moduleButtonY); // Wrap without entering another folder or IR category.
  assert(std::filesystem::path(save().config()["SlotPaths"][0].get<std::string>()) == libraryFirst);
  click(grid_layout::modulePreviousX, grid_layout::moduleButtonY);
  assert(std::filesystem::path(save().config()["SlotPaths"][0].get<std::string>()) == neighbor);
  click(grid_layout::moduleBrowseX, grid_layout::moduleButtonY); chooseBrowserItem("AMP"); pumpFor(30);
  assert(std::filesystem::path(save().config()["SlotPaths"][0].get<std::string>()) == libraryFirst);
  selectTile(390, 120); click(grid_layout::moduleNextX, grid_layout::moduleButtonY);
  assert(std::filesystem::path(save().config()["SlotPaths"][1].get<std::string>()) == neighborIR);
  assert(save().config()["SlotEffects"][1] == static_cast<int>(SlotEffect::CabinetIR));
  click(grid_layout::modulePreviousX, grid_layout::moduleButtonY);
  assert(std::filesystem::path(save().config()["SlotPaths"][1].get<std::string>()) == libraryIR);
  if(argc==4) CaptureEditor(editor,std::filesystem::path(argv[3])/"ir.bmp");
  // Empty file slots cannot cycle into another category.
  selectTile(170, 224); click(grid_layout::moduleNextX, grid_layout::moduleButtonY);
  assert(save().config()["SlotEffects"][4] == 0 && save().config()["SlotPaths"][4] == "");
  // The settings page is opaque and intercepts clicks; grid/editor controls cannot receive them.
  selectTile(170, 120);
  if (argc == 4)
    CaptureEditor(editor, std::filesystem::path(argv[3]) / "grid.bmp");
  const auto beforeSettings = save().config();
  click(910, 36);
  click(510, 88); // Global visualizations off.
  assert(save().config() == beforeSettings); // Editor preference is outside the rig.
  click(510, 88); // Restore preference for subsequent tests/previews.
  if (argc == 4)
    CaptureEditor(editor, std::filesystem::path(argv[3]) / "settings.bmp");
  dragAt(grid_layout::KnobX(0), grid_layout::KnobGestureY(0)); // NAM Input underneath; the settings reference dial starts below this point.
  assert(save().config() == beforeSettings);
  click(936, 28);
  click(910, 36);
  // Escape closes settings so the selected module becomes editable again.
  SendMessage(editor, WM_MOUSEMOVE, 0, MAKELPARAM(936, 28));
  SendMessage(editor, WM_KEYDOWN, VK_ESCAPE, static_cast<LPARAM>(MapVirtualKey(VK_ESCAPE, MAPVK_VK_TO_VSC)) << 16);
  pumpFor(100);
  dragAt(grid_layout::KnobX(0), grid_layout::KnobGestureY(0));
  assert(save().config()["Pre 1 NAM Input"] != beforeSettings["Pre 1 NAM Input"]);
  if (argc == 4)
  {
    auto colors = browserState;
    const char* categories[] = {"amp_head", "cabinet", "amp_cab", "pedal"};
    for (int i = 0; i < 4; ++i)
    {
      auto model = a2;
      model["metadata"]["gear_type"] = categories[i];
      const auto path = fixtures / (std::string(categories[i]) + ".nam");
      std::ofstream(path) << model.dump();
      colors["SlotPaths"][i] = path.string();
    }
    colors["SlotEffects"][4] = static_cast<int>(SlotEffect::NoiseGate);
    colors["SlotEffects"][5] = static_cast<int>(SlotEffect::SmoothComp);
    colors["SlotEffects"][6] = static_cast<int>(SlotEffect::ToneEQ);
    colors["SlotEffects"][7] = static_cast<int>(SlotEffect::CutEQ);
    colors["SlotEffects"][8] = static_cast<int>(SlotEffect::ParametricEQ);
    colors["SlotEffects"][9] = static_cast<int>(SlotEffect::Boost);
    colors["SlotEffects"][10] = static_cast<int>(SlotEffect::DigitalDelay);
    colors["SlotEffects"][11] = static_cast<int>(SlotEffect::TapeDelay);
    colors["SlotEffects"][12] = static_cast<int>(SlotEffect::RoomReverb);
    colors["SlotEffects"][13] = static_cast<int>(SlotEffect::PlateReverb);
    colors["SlotEffects"][14] = static_cast<int>(SlotEffect::Chorus);
    colors["SlotEffects"][15] = static_cast<int>(SlotEffect::Tape);
    load(colors);
    pumpFor(100);
    selectTile(610, 120);
    CaptureEditor(editor, std::filesystem::path(argv[3]) / "node-colors.bmp");
    colors["SlotEffects"][0] = static_cast<int>(SlotEffect::TubeBoost);
    colors["SlotPaths"][0] = "";
    colors["SlotPaths"][0] = "";
    load(colors);
    selectTile(170, 120);
    pumpFor(300);
    CaptureEditor(editor, std::filesystem::path(argv[3]) / "tube-boost.bmp");
    for (int effect : {static_cast<int>(SlotEffect::SpringReverb), static_cast<int>(SlotEffect::Wah), static_cast<int>(SlotEffect::Chorus), static_cast<int>(SlotEffect::RoomReverb), static_cast<int>(SlotEffect::DigitalDelay)})
    {
      auto preview = browserState;
      preview["SlotEffects"][0] = effect;
      if (effect == static_cast<int>(SlotEffect::DigitalDelay))
        preview["Pre 1 Delay Ping-Pong"] = 1.;
      load(preview);
      selectTile(170, 120);
      CaptureEditor(
        editor, std::filesystem::path(argv[3]) / (std::string("stereo-guitar-") + std::to_string(effect) + ".bmp"));
    }
    auto utilityPreview = browserState;
    utilityPreview["SlotEffects"][0] = static_cast<int>(SlotEffect::Utility);
    load(utilityPreview);
    selectTile(170, 120);
    CaptureEditor(editor, std::filesystem::path(argv[3]) / "utility.bmp");
    auto syncPreview = browserState;
    syncPreview["SlotEffects"][0] = static_cast<int>(SlotEffect::TapeDelay);
    syncPreview["Pre 1 Tempo Sync"] = 1.;
    load(syncPreview);
    selectTile(170, 120);
    CaptureEditor(editor, std::filesystem::path(argv[3]) / "tempo-sync.bmp");
  }
  load(cleared);
  click(kTunerX, grid_layout::headerY); // Tuner beside Settings.
  assert(save().config()["Tuner"] == 1.);
  dragAt(391, 601); // The Reference knob below Mute output, at module knob size.
  assert(save().config()["Tuner Reference"] != 440.);
  assert(plugin->activate(plugin, 48000., 1, 512));
  assert(plugin->start_processing(plugin));
  process.frames_count = 512;
  for (int b = 0; b < 30; ++b)
  {
    for (int i = 0; i < 512; ++i)
      input[0][i] = input[1][i] = .1 * std::sin(6.283185307179586 * 110. * (b * 512 + i) / 48000.);
    assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
    pumpFor(2);
  }
  for (int i = 0; i < 512; ++i)
    assert(output[0][i] == 0. && output[1][i] == 0.);
  if (argc == 4)
    CaptureEditor(editor, std::filesystem::path(argv[3]) / "tuner.bmp");
  SendMessage(editor, WM_KEYDOWN, VK_ESCAPE, static_cast<LPARAM>(MapVirtualKey(VK_ESCAPE, MAPVK_VK_TO_VSC)) << 16);
  pumpFor(50);
  assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
  assert(std::abs(output[0][400]) > .001);
  // The Tuner button works like a module selector: click it again, or select a slot, to leave.
  click(kTunerX, grid_layout::headerY);
  assert(save().config()["Tuner"] == 1.);
  click(391, 533); // Mute output, at the top of the tuner's control column.
  assert(save().config()["Tuner Mute"] == 0.);
  click(391, 533);
  assert(save().config()["Tuner Mute"] == 1.);
  click(kTunerX, grid_layout::headerY);
  assert(save().config()["Tuner"] == 0.);
  click(kTunerX, grid_layout::headerY);
  selectTile(390, 120);
  assert(save().config()["Tuner"] == 0.);
  plugin->stop_processing(plugin);
  plugin->deactivate(plugin);
  // Exercise the real performance overlay and scene transactions without replacing models.
  load(cleared);
  click(grid_layout::width - 36, 356); // Routing, below the A / B knob
  dragAt(grid_layout::KnobX(1), grid_layout::KnobGestureY(1));
  const double laneBalance = save().config()["Lane A Balance"];
  assert(laneBalance != 0.);
  click(grid_layout::width - 36, 328); // Open Routing stays open while mode changes.
  assert(save().config()["Lane Mode"] == grid_performance::Sequential);
  dragAt(grid_layout::KnobX(1), grid_layout::KnobGestureY(1));
  assert(save().config()["Lane A Balance"] == laneBalance); // Inactive in Sequential.
  if (argc == 4) CaptureEditor(editor, std::filesystem::path(argv[3]) / "routing-sequential.bmp");
  if (const char* captures = std::getenv("NAM_GRID_ROUTING_SCREENSHOTS"))
    CaptureEditor(editor, std::filesystem::path(captures) / "routing-sequential.bmp");
  click((grid_layout::parameterLeft + 100 + grid_layout::parameterRight - 304) / 2, grid_layout::moduleButtonY);
  assert(save().config()["Lane Mode"] == grid_performance::Parallel);
  assert(save().config()["Lane A Balance"] == laneBalance);
  if (argc == 4) CaptureEditor(editor, std::filesystem::path(argv[3]) / "routing-parallel.bmp");
  if (const char* captures = std::getenv("NAM_GRID_ROUTING_SCREENSHOTS"))
    CaptureEditor(editor, std::filesystem::path(captures) / "routing-parallel.bmp");
  checkLaneButtons();
  dragAt(grid_layout::KnobX(0), grid_layout::KnobGestureY(0)); // Lane A level
  const double sceneGain = save().config()["Lane A Level"];
  assert(sceneGain != 0.);
  click(grid_layout::SceneX(0), grid_layout::headerY); // An empty scene recalls nothing.
  assert(save().config()["Lane A Level"] == sceneGain);
  win_edit_menu::SelectNamedItem(editor, grid_layout::SceneX(0), grid_layout::headerY, L"Store in scene 1", true); pumpFor(30);
  const auto sceneState = save().config();
  assert(sceneState["PerformanceSceneDeltas"][0].is_object() && sceneState["PerformanceSceneDeltas"][0].empty());
  assert(sceneState["Lane A Level"] == sceneGain);
  assert(!sceneState["PerformanceSceneDeltas"][0].contains("Pre 1 Noise Learn"));
  assert(!sceneState["PerformanceSceneDeltas"][0].contains("Tuner"));
  automate("Lane A Level", 0.); // -60 dB in the normalized CLAP API
  assert(save().config()["Lane A Level"] == -60.);
  click(grid_layout::SceneX(0), grid_layout::headerY); // Recall scene 1
  assert(save().config()["Lane A Level"] == sceneGain);
  const auto storedScene = save();
  load(cleared);
  assert(loadBytes(storedScene));
  assert(save().config()["PerformanceSceneDeltas"] == sceneState["PerformanceSceneDeltas"]);
  automate("Lane A Level", 0.);
  automate("Output", .8);
  click(grid_layout::SceneX(0), grid_layout::headerY);
  assert(save().config()["Lane A Level"] == sceneGain);
  assert(save().config()["Output"] == sceneState["Output"]);

  auto malformedScene = save().config();
  malformedScene["PerformanceSceneDeltas"][0]["Not a real parameter"] = 1.;
  Stream malformedSceneBytes; malformedSceneBytes.string("###nam-grid###"); malformedSceneBytes.string(malformedScene.dump());
  assert(!loadBytes(malformedSceneBytes));
  assert(save().config()["PerformanceSceneDeltas"] == sceneState["PerformanceSceneDeltas"]);
  malformedScene["PerformanceSceneDeltas"][0].erase("Not a real parameter");
  malformedScene["PerformanceSceneDeltas"][0]["Pre 1 Noise Learn"] = 1.;
  Stream learnSceneBytes;
  learnSceneBytes.string("###nam-grid###");
  learnSceneBytes.string(malformedScene.dump());
  assert(!loadBytes(learnSceneBytes));
  assert(save().config()["PerformanceSceneDeltas"] == sceneState["PerformanceSceneDeltas"]);
  {
    // Three legacy snapshots matching a high-precision rig must compact before
    // export. The headless case separately covers fully divergent scene values.
    auto large = sceneState;
    for (uint32_t i = 0; i < params->count(plugin); ++i)
    {
      clap_param_info info{};
      assert(params->get_info(plugin, i, &info));
      if (info.flags & CLAP_PARAM_IS_STEPPED) continue;
      const double value = large[info.name].get<double>();
      const double offset = 1.2345678901234567e-7 * std::max(1., std::abs(value));
      large[info.name] = value + (info.default_value < .5 ? offset : -offset);
    }
    for (auto& scene : large["PerformanceScenes"])
    {
      scene = json::object();
      for (uint32_t i = 0; i < params->count(plugin); ++i)
      {
        clap_param_info info{}; assert(params->get_info(plugin, i, &info));
        if (performance_scenes::IsParameter(info.id)) scene[info.name] = large[info.name];
      }
    }
    large["schema"] = 22; large.erase("PerformanceSceneDeltas");
    Stream largeBytes;
    largeBytes.string("###nam-grid###");
    largeBytes.string(large.dump());
    assert(loadBytes(largeBytes));
    auto saved = save();
    assert(saved.bytes.size() < largeBytes.bytes.size() / 2
           && saved.bytes.size() < rig_preset::kMaximumBytes / 4);
    const auto presetFile = settingsBackup.home / "worst-case-size.ngrid";
    rig_preset::Write(presetFile, saved.config());
    assert(std::filesystem::file_size(presetFile) < rig_preset::kMaximumBytes / 4);
    assert(rig_preset::Read(presetFile)["PerformanceSceneDeltas"] == saved.config()["PerformanceSceneDeltas"]);
    std::filesystem::remove(presetFile);
    assert(loadBytes(storedScene));
    assert(save().config()["PerformanceSceneDeltas"] == sceneState["PerformanceSceneDeltas"]);
  }
  win_edit_menu::SelectNamedItem(editor, grid_layout::SceneX(1), grid_layout::headerY, L"Store in scene 2", true); pumpFor(30);
  win_edit_menu::SelectNamedItem(editor, grid_layout::SceneX(1), grid_layout::headerY, L"Clear scene 2", true); pumpFor(30);
  assert(save().config()["PerformanceScenes"][1].empty());
  assert(save().config()["PerformanceSceneDeltas"][1].is_null());
  click(kModulationX, grid_layout::headerY); // Modulation
  {
    const auto before = save().config();
    click(131, 84); // LFO 2
    dragAt(185, 355);
    assert(save().config()["LFO 2 Rate"] != before["LFO 2 Rate"]);
    assert(save().config()["LFO Rate"] == before["LFO Rate"]);
    click(274, 84); // Env 2
    dragAt(185, 355);
    assert(save().config()["Envelope 2 Sensitivity"] != before["Envelope 2 Sensitivity"]);
    assert(save().config()["Envelope Sensitivity"] == before["Envelope Sensitivity"]);
    click(416, 84); // Seq 2
    dragAt(410, 230);
    assert(save().config()["Sequencer 2 Step 8"] != before["Sequencer 2 Step 8"]);
    assert(save().config()["Sequencer Step 8"] == before["Sequencer Step 8"]);
    if (argc == 4) CaptureEditor(editor, std::filesystem::path(argv[3]) / "modulation-seq2.bmp");
    click(60, 84); // LFO 1
  }
  {
    using namespace grid_performance;
    auto before = save().config();
    auto named = before;
    named["MacroNames"][0] = "Test";
    load(named);
    std::vector<std::string> labels;
    for (int source : sourceMenuOrder)
      labels.push_back(SourceLabel(source, source >= SourceMacro1 && source <= SourceMacro4
                                             ? named["MacroNames"][source - SourceMacro1].get<std::string>() : ""));
    std::vector<const char*> choices;
    for (const auto& label : labels) choices.push_back(label.c_str());
    const auto position = [&](int source) {
      return static_cast<int>(std::find(sourceMenuOrder.begin(), sourceMenuOrder.end(), source) - sourceMenuOrder.begin());
    };
    win_edit_menu::SelectChoice(editor, 550, 141, choices, position(SourceLFO2), position(named["Mod 1 Source"].get<int>()));
    pumpFor(30);
    assert(save().config()["Mod 1 Source"] == SourceLFO2);
    win_edit_menu::SelectChoice(editor, 936, 141, choices, position(SourceEnv2), position(named["Mod 1 Via"].get<int>()));
    pumpFor(30);
    assert(save().config()["Mod 1 Via"] == SourceEnv2); // Compact second-bank label must draw safely.
    win_edit_menu::SelectChoice(editor, 550, 141, choices, position(SourceMacro1), position(SourceLFO2));
    pumpFor(30);
    assert(save().config()["Mod 1 Source"] == SourceMacro1);
    const bool sync = save().config()["LFO Sync"].get<double>() >= .5;
    click(291, 350);
    assert((save().config()["LFO Sync"].get<double>() >= .5) != sync);
    load(before);
  }
  dragAt(862, 141); // Matrix row 1 depth
  assert(save().config()["Mod 1 Depth"] > 0.);
  click(936, 28); // Close overlay
  // Right-click a knob to modulate it: the routing panel (still open) has Lane A level, which
  // takes the first free row.
  win_edit_menu::SelectNamedItem(editor, grid_layout::KnobX(0), grid_layout::KnobGestureY(0), L"Modulate with Macro 1", true);
  pumpFor(30);
  {
    const auto state = save().config();
    assert(state["Mod 1 Source"] == grid_performance::SourceMacro1 && state["Mod 1 Slot"] == grid_performance::gridSlot);
    assert(state["Mod 1 Destination"] == grid_performance::GridLaneA && state["Mod 1 Depth"] == 25.);
  }
  // The routing page's saved Split Slope choice: steep, then back to the released gentle split.
  {
    click(grid_layout::parameterLeft + 88, grid_layout::optionsY);
    assert(save().config()["Frequency Split"] == 1.);
    const int slopeX = grid_layout::parameterRight - 210, slopeY = grid_layout::moduleButtonY;
    assert(save().config()["Split Slope"] == 0.);
    click(slopeX, slopeY);
    pumpFor(30);
    assert(save().config()["Split Slope"] == 1.);
    click(slopeX, slopeY);
    pumpFor(30);
    assert(save().config()["Split Slope"] == 0.);
    click(grid_layout::parameterLeft + 88, grid_layout::optionsY);
  }
  keyAtEditor(VK_ESCAPE); // Closes the routing panel.
  // Wah's Auto button creates the envelope row that makes it an auto-wah, and only exists on Wah.
  {
    const int autoX = (grid_layout::modeLeft + grid_layout::parameterRight) / 2;
    const auto autoRow = [&] {
      const auto state = save().config();
      for (int row = 1; row <= grid_performance::rows; ++row)
      {
        const auto key = [&](const char* field) { return "Mod " + std::to_string(row) + " " + field; };
        if (state[key("Source")] == grid_performance::SourceEnv1 && state[key("Slot")] == 0)
          return std::pair{state[key("Destination")].get<double>(), state[key("Depth")].get<double>()};
      }
      return std::pair{-1., 0.};
    };
    auto wah = cleared;
    wah["SlotEffects"][0] = static_cast<int>(SlotEffect::Wah);
    load(wah);
    selectTile(170, 120);
    pumpFor(100);
    assert(autoRow().first < 0.);
    const auto parked = save().config()["Pre 1 Wah Position"];
    click(autoX, grid_layout::optionsY);
    assert(autoRow().first == parameter_layout::standardControls + uWahPosition && autoRow().second == 25.);
    assert(save().config()["Pre 1 Wah Position"] == parked); // Quick setup adds a row, it never turns knobs.
    assert(save().config()["SlotEffects"][0] == static_cast<int>(SlotEffect::Wah));
    // The button belongs to the Wah header: another module leaves the matrix untouched.
    auto chorus = cleared;
    chorus["SlotEffects"][0] = static_cast<int>(SlotEffect::Chorus);
    load(chorus);
    selectTile(170, 120);
    pumpFor(100);
    const auto matrix = [&](const json& state) {
      json rows = json::array();
      for (int row = 1; row <= grid_performance::rows; ++row)
        for (const char* field : {"Source", "Slot", "Destination", "Depth", "Via"})
          rows.push_back(state["Mod " + std::to_string(row) + " " + field]);
      return rows;
    };
    const auto before = matrix(save().config());
    click(autoX, grid_layout::optionsY);
    assert(matrix(save().config()) == before);
    assert(save().config()["SlotEffects"][0] == static_cast<int>(SlotEffect::Chorus));
    load(cleared);
  }
  // A shorter lane's new conversion delay requires a restart even when max(A,B) is unchanged.
  auto unequal = cleared; // The library still holds libraryFolder.
  unequal["SlotPaths"][4] = libraryFirst.string();
  unequal["SlotPaths"][5] = libraryFirst.string();
  load(unequal);
  click(910, 28); click(138, 88); click(936, 28); pumpFor(250);
  assert(plugin->activate(plugin, 44100., 1, 512));
  assert(plugin->start_processing(plugin));
  process.frames_count = 512;
  assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
  host.restart = false;
  auto* latency = static_cast<const clap_plugin_latency*>(plugin->get_extension(plugin, CLAP_EXT_LATENCY));
  assert(latency->get(plugin) == 54);
  selectTile(170, 328);
  win_edit_menu::SelectNamedItem(editor,200,grid_layout::moduleButtonY,L"amp"); // B1 gets one model (27 samples), while A stays 54 samples.
  pumpFor(30);
  if (host.callback)
  {
    host.callback = false;
    plugin->on_main_thread(plugin);
  }
  assert(host.restart && latency->get(plugin) == 54);
  plugin->stop_processing(plugin);
  plugin->deactivate(plugin);
  assert(plugin->activate(plugin, 44100., 1, 512));
  assert(plugin->start_processing(plugin));
  assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
  plugin->stop_processing(plugin);
  plugin->deactivate(plugin);
  load(cleared);
  click(kTunerX, grid_layout::headerY);
  gui->destroy(plugin);
  assert(save().config()["Tuner"] == 1.); // Closing the editor must not cancel host automation.
  assert(plugin->activate(plugin, 48000., 1, 512));
  assert(plugin->start_processing(plugin));
  for (int i = 0; i < 512; ++i)
    input[0][i] = input[1][i] = .1 * std::sin(i * .05);
  assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
  assert(output[0][400] == 0. && output[1][400] == 0.);
  automate("Tuner", 0.); // Headless host/MIDI automation restores output.
  assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
  assert(std::abs(output[0][400]) > .001);
  automate("Tuner", 1.);
  assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
  assert(output[0][400] == 0. && output[1][400] == 0.);
  automate("Tuner Mute", 0.);
  assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
  assert(std::abs(output[0][400]) > .001);
  automate("Tuner", 0.);
  plugin->stop_processing(plugin);
  plugin->deactivate(plugin);
  DestroyWindow(parent);
#endif
  plugin->destroy(plugin);
  entry->deinit();
#ifdef _WIN32
  FreeLibrary(library);
  std::cout << "CLAP factory, model files, state, missing/invalid files, reset, teardown and native editor passed\n";
#else
  dlclose(library);
  std::cout << "CLAP factory, model files, state, missing/invalid files, reset and teardown passed\n";
#endif
}
