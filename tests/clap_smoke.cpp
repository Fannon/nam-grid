#include "A2Fixture.h"
#include "ClapTestHost.h"
#include <thread>
#include <cstdlib>
#include "WinEditorCapture.h"
#include "WinEditMenuDriver.h"
#include "../NeuralAmpModeler/EffectParameters.h"
#include "../NeuralAmpModeler/SlotDefaults.h"
#include "../NeuralAmpModeler/ModuleCatalog.h"
#include "../NeuralAmpModeler/GridPerformance.h"
#include "RoadmapClapTests.h"
#include "EffectModeClapTests.h"
#include "PresetGainClapTests.h"
#include "CachedControlsClapTests.h"
#include <cstdlib>
#include "WinFileDialogDriver.h"
#include "../NeuralAmpModeler/GridLayout.h"
#include "../NeuralAmpModeler/EffectPresentation.h"
#include "../NeuralAmpModeler/CaptureLibrary.h"
int main(int argc, char** argv)
{
  std::cout << std::unitbuf;
  assert(argc == 3 || argc == 4);
#ifdef _WIN32
  win_file_dialog::SavedLibrarySettings settingsBackup;
  const bool fxVisuals = std::getenv("NAM_GRID_FX_VISUALS") != nullptr;
  if(fxVisuals)
  {
    auto settings=nam_library::ReadSettings(settingsBackup.file);
    settings.showVisualizations=true;
    assert(nam_library::WriteSettings(settingsBackup.file,settings));
  }
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
  assert(std::strcmp(descriptor->name, "nam-grid") == 0);
  assert(std::strcmp(descriptor->id, "com.StevenAtkinson.NAMGridPrototype") == 0);
  Host host;
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
    // Keep the old path aliases in sync while exercising existing migration scenarios.
    for (int i = 0; i < 2; ++i)
      if (!config.contains("SlotEffects") || config["SlotEffects"][i] == 0)
        config["SlotPaths"][i] = config[i == 0 ? "DrivePath" : "NAMPath"];
    // These fixtures explicitly choose paths. Derive their new identities from
    // those files; raw loadBytes below separately tests saved-hash relocation.
    config["SlotHashes"] = json::array();
    for (int i = 0; i < 16; ++i)
    {
      const int effect = config.contains("SlotEffects") ? config["SlotEffects"][i].get<int>() : 0;
      const auto path = config["SlotPaths"][i].get<std::string>();
      config["SlotHashes"].push_back((effect == 0 || effect == 7) && !path.empty()
        && std::filesystem::is_regular_file(std::filesystem::u8path(path))
          ? nam_library::ContentHash(std::filesystem::u8path(path)) : "");
    }
    Stream s;
    s.string("###NAMGrid-v1###");
    s.string(config.dump());
    assert(loadBytes(std::move(s)));
    // Identity resolution may choose a shallower indexed copy of the same file.
    // Keep round-trip expectations canonical; dedicated tests below assert the
    // chosen path/hash independently. All parameter expectations remain intact.
    const auto canonical = save().config();
    config["SlotPaths"] = canonical["SlotPaths"];
    config["SlotHashes"] = canonical["SlotHashes"];
    config["DrivePath"] = canonical["DrivePath"];
    config["NAMPath"] = canonical["NAMPath"];
  };
  auto base = save().config();
  assert(base["Tuner"] == 0.);
  auto beforeTunerAutomation = base; beforeTunerAutomation["schema"] = 22; beforeTunerAutomation.erase("Tuner");
  Stream beforeTunerBytes; beforeTunerBytes.string("###NAMGrid-v1###"); beforeTunerBytes.string(beforeTunerAutomation.dump());
  assert(loadBytes(beforeTunerBytes)); assert(save().config() == base);
  assert(params->count(plugin) == 287 + 16 * kUtilityControls + grid_performance::Count);
  assert(base["NoiseGateActive"] == 0. && base["ToneStack"] == 0. && base["IRToggle"] == 0.);
  for (const char* name : {"Input", "Drive Input", "Pre 3 In", "Post 4 In"})
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
  base["DrivePath"] = path1.string();
  base["NAMPath"] = path2.string();
  base["NoiseGateActive"] = 0.;
  base["ToneStack"] = 0.;
  base["IRToggle"] = 0.;
  base["Drive Input"] = 2.;
  base["Drive Trim"] = -3.;
  base["Input"] = 1.;
  base["Amp Trim"] = -2.;
  base["Output"] = -6.;
  base["CaptureFolder"] = fixtures.string();
  base["CaptureFolders"] = {fixtures.string()};
  load(base);
  assert(save().config() == base);
  // Version-2 NAM-only grids migrate without changing their signal chain or gains.
  auto gridV2 = base;
  gridV2["schema"] = 2;
  gridV2.erase("SlotEffects");
  for (auto it = gridV2.begin(); it != gridV2.end();)
    if (it.key().find(" Gate ") != std::string::npos || it.key().find(" Comp ") != std::string::npos)
      it = gridV2.erase(it);
    else
      ++it;
  load(gridV2);
  assert(save().config() == base);
  auto gridV3 = base;
  gridV3["schema"] = 3;
  for (auto it = gridV3.begin(); it != gridV3.end();)
    if (it.key().find(" EQ ") != std::string::npos || it.key().find(" IR Level") != std::string::npos
        || it.key().find(" Full Quality") != std::string::npos)
      it = gridV3.erase(it);
    else
      ++it;
  load(gridV3);
  assert(save().config() == base);
  // Older dual-stage states predate library settings; they remain readable.
  auto oldDual = base;
  oldDual.erase("CaptureFolder");
  oldDual.erase("CaptureFolders");
  load(oldDual);
  assert(save().config()["CaptureFolder"] == "");
  load(base);
  auto recalled = save();
  assert(loadBytes(recalled));
  assert(save().config() == base);
  // The previous dual-stage format migrates into IN 1 / IN 2; the other 14 slots stay empty.
  auto dual = base;
  dual["schema"] = 1;
  dual.erase("SlotPaths");
  for (auto it = dual.begin(); it != dual.end();)
    if (it.key().starts_with("Pre ") || it.key().starts_with("Post ") || it.key().starts_with("A ")
        || it.key().starts_with("B ") || it.key() == "Parallel Blend")
      it = dual.erase(it);
    else
      ++it;
  Stream dualBytes;
  dualBytes.string("###NAMDual-v1###");
  dualBytes.string(dual.dump());
  assert(loadBytes(dualBytes));
  const auto dualMigrated = save().config();
  assert(dualMigrated["SlotPaths"][0] == path1.string() && dualMigrated["SlotPaths"][1] == path2.string());
  for (int slot = 2; slot < 16; ++slot)
    assert(dualMigrated["SlotPaths"][slot] == "");
  assert(dualMigrated["Parallel Blend"] == 50.);
  load(base);
  Stream legacy;
  legacy.string("###NeuralAmpModeler###");
  legacy.string("0.8.0");
  legacy.string(path2.string());
  legacy.string("");
  for (const char* name : {"Input", "Threshold", "Bass", "Middle", "Treble", "Output", "NoiseGateActive", "ToneStack",
                           "IRToggle", "CalibrateInput", "InputCalibrationLevel", "OutputMode", "Slim"})
  {
    double value = base[name];
    auto* bytes = reinterpret_cast<uint8_t*>(&value);
    legacy.bytes.insert(legacy.bytes.end(), bytes, bytes + sizeof(value));
  }
  assert(loadBytes(legacy));
  auto migrated = save().config();
  assert(migrated["NAMPath"] == path2.string() && migrated["DrivePath"] == "" && migrated["Drive Input"] == 0.);
  load(base);
  legacy.bytes.pop_back();
  assert(!loadBytes(legacy));
  assert(save().config() == base);
  // Malformed state must leave the previously committed state intact.
  for (const char* path : {"DrivePath", "NAMPath", "IRPath"})
  {
    auto invalidPath = base;
    invalidPath[path] = std::string("bad\0path", 8);
    if (std::strcmp(path, "IRPath") != 0)
      invalidPath["SlotPaths"][std::strcmp(path, "DrivePath") == 0 ? 0 : 1] = invalidPath[path];
    Stream malformed;
    malformed.string("###NAMGrid-v1###"); malformed.string(invalidPath.dump());
    assert(!loadBytes(malformed));
    assert(save().config() == base);
  }
  // The byte strings themselves must not silently truncate at an embedded NUL.
  for (bool corruptHeader : {false, true})
  {
    Stream malformed;
    malformed.string(std::string("###NAMGrid-v1###") + (corruptHeader ? std::string("\0junk", 5) : ""));
    malformed.string(base.dump() + (corruptHeader ? "" : std::string("\0junk", 5)));
    assert(!loadBytes(malformed));
    assert(save().config() == base);
  }
  for (const json& folder : {json(123), json(std::string("bad\0folder", 10))})
  {
    auto invalidFolder = base;
    invalidFolder["CaptureFolder"] = folder;
    Stream malformed;
    malformed.string("###NAMGrid-v1###");
    malformed.string(invalidFolder.dump());
    assert(!loadBytes(malformed));
    assert(save().config() == base);
  }
  for (const char* folderField : {"CaptureFolders", "PresetFolders"})
  for (const json& folders : {json(123), json::array({"valid", 123}), json::array({std::string("bad\0path", 8)}),
                              json(std::vector<std::string>(33, "folder"))})
  {
    auto invalidFolders = base;
    invalidFolders[folderField] = folders;
    Stream malformed;
    malformed.string("###NAMGrid-v1###");
    malformed.string(invalidFolders.dump());
    assert(!loadBytes(malformed));
    assert(save().config() == base);
  }
  auto namEQState = base;
  namEQState["Pre 1 EQ Bass"] = 6.;
  namEQState["Pre 1 EQ Mid"] = -3.;
  load(namEQState);
  assert(save().config() == namEQState);
  auto utilityState = base;
  utilityState["SlotEffects"][0] = 8;
  utilityState["SlotPaths"][0] = "";
  utilityState["DrivePath"] = "";
  utilityState["Pre 1 Parametric Frequency"] = 1200.;
  utilityState["Pre 1 Parametric Gain"] = -5.;
  utilityState["Pre 1 Parametric Q"] = 1.3;
  utilityState["SlotEffects"][15] = 9;
  utilityState["Post 4 Boost Gain"] = 9.;
  load(utilityState);
  assert(save().config() == utilityState);
  auto inheritedProcessing = base;
  inheritedProcessing["NoiseGateActive"] = 1.;
  inheritedProcessing["ToneStack"] = 1.;
  inheritedProcessing["IRToggle"] = 1.;
  load(inheritedProcessing);
  assert(save().config() == inheritedProcessing); // Earlier projects keep their global processing flags.
  auto oneFolderState = base;
  oneFolderState.erase("CaptureFolders");
  load(oneFolderState); // Pre-multiple-folder projects still restore their library root.
  assert(save().config() == base);
  auto multipleFolders = base;
  multipleFolders["CaptureFolders"] = {fixtures.string(), (fixtures / "IRs").string()};
  load(multipleFolders);
  assert(save().config() == multipleFolders);
  load(base);
  Stream bad;
  bad.string("###NAMGrid-v1###");
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
  for (const json& badEffect : {json(-1), json(99), json(1.5), json("gate"), json(18446744073709551615ull)})
  {
    auto malformedEffect = base;
    malformedEffect["SlotEffects"][15] = badEffect;
    Stream invalid;
    invalid.string("###NAMGrid-v1###");
    invalid.string(malformedEffect.dump());
    assert(!loadBytes(invalid));
    assert(save().config() == base);
  }
  // Schemas 4/5 were unused WIP numbers. Reject explicitly without mutation.
  for (int schema : {4, 5, 99}) // Unused schemas must reject without mutating state.
  {
    auto unsupported = base;
    unsupported["schema"] = schema;
    Stream invalid;
    invalid.string("###NAMGrid-v1###");
    invalid.string(unsupported.dump());
    assert(!loadBytes(invalid));
    assert(save().config() == base);
  }
  auto oldGateNames = base;
  auto expectedGates = base;
  for (int slot = 0; slot < 16; ++slot)
  {
    const std::string label = std::string(slot < 4    ? "Pre "
                                          : slot < 8  ? "A "
                                          : slot < 12 ? "B "
                                                      : "Post ")
                              + std::to_string(slot % 4 + 1);
    oldGateNames.erase(label + " Gate Release");
    oldGateNames[label + " Gate Decay"] = 55. + slot;
    expectedGates[label + " Gate Release"] = 55. + slot;
  }
  load(oldGateNames);
  assert(save().config() == expectedGates);
  load(base);
  // The last prototype's schema 6 fills new controls by name despite the new
  // per-slot parameter stride, without dropping later slots' saved values.
  auto prior = base;
  prior["schema"] = 6;
  for (int slot = 0; slot < 16; ++slot)
  {
    const std::string slotName = std::string(slot < 4    ? "Pre "
                                             : slot < 8  ? "A "
                                             : slot < 12 ? "B "
                                                         : "Post ")
                                 + std::to_string(slot % 4 + 1);
    for (int p = 30; p < kUtilityControls; ++p)
      prior.erase(slotName + " " + kEffectParameters[p].name);
  }
  prior["Post 4 Boost Gain"] = 11.;
  load(prior);
  assert(save().config()["Post 4 Boost Gain"] == 11.);
  assert(save().config()["Post 4 Comp Attack"] == 8.);
  assert(save().config()["Post 4 Punch Attack"] == 25.);
  auto schema7 = base;
  schema7["schema"] = 7;
  schema7.erase("Tuner Reference");
  schema7.erase("Tuner Mute");
  for (int slot = 0; slot < 16; ++slot)
  {
    const std::string label = std::string(slot < 4    ? "Pre "
                                          : slot < 8  ? "A "
                                          : slot < 12 ? "B "
                                                      : "Post ")
                              + std::to_string(slot % 4 + 1);
    for (int p = 41; p < kUtilityControls; ++p)
      schema7.erase(label + " " + kEffectParameters[p].name);
  }
  schema7["Post 4 Boost Gain"] = 13.;
  load(schema7);
  assert(save().config()["Post 4 Boost Gain"] == 13.);
  assert(save().config()["Post 4 Tempo Sync"] == 0. && save().config()["Tuner Reference"] == 440.);
  load(base);
  auto schema8 = base;
  schema8["schema"] = 8;
  schema8["Tuner Reference"] = 442.;
  for (int slot = 0; slot < 16; ++slot)
  {
    const std::string label = std::string(slot < 4    ? "Pre "
                                          : slot < 8  ? "A "
                                          : slot < 12 ? "B "
                                                      : "Post ")
                              + std::to_string(slot % 4 + 1);
    for (int p = 48; p < kUtilityControls; ++p)
      schema8.erase(label + " " + kEffectParameters[p].name);
    schema8.erase(label + " Utility Width");
    schema8[label + " Utility Width"] = slot == 15 ? 145. : 100.;
  }
  load(schema8);
  assert(save().config()["Post 4 Utility Width"] == 45. && save().config()["Pre 1 Utility Width"] == 0.);
  assert(save().config()["Tuner Reference"] == 442.);
  load(base);
  // Schema 10 predates the three HQ modules. Remove only their named controls;
  // every earlier control must survive the new per-slot stride.
  auto schema10 = base;
  schema10["schema"] = 10;
  for (int slot = 0; slot < 16; ++slot)
  {
    const std::string label = std::string(slot < 4    ? "Pre "
                                          : slot < 8  ? "A "
                                          : slot < 12 ? "B "
                                                      : "Post ")
                              + std::to_string(slot % 4 + 1);
    for (int p = 103; p < kUtilityControls; ++p)
      schema10.erase(label + " " + kEffectParameters[p].name);
  }
  schema10["Post 4 Boost Gain"] = 14.;
  schema10["Pre 1 Rotary Fast"] = 7.3;
  load(schema10);
  assert(save().config()["Post 4 Boost Gain"] == 14.);
  assert(save().config()["Pre 1 Rotary Fast"] == 7.3);
  assert(save().config()["Pre 1 Shimmer Pitch"] == 12.);
  assert(save().config()["Pre 1 Juno Mode"] == 1.);
  assert(save().config()["Pre 1 Shifter Frequency"] == 0.);
  auto schema10Effect = schema10;
  schema10Effect["SlotEffects"][0] = static_cast<int>(SlotEffect::ShimmerReverb);
  Stream oldEffect;
  oldEffect.string("###NAMGrid-v1###");
  oldEffect.string(schema10Effect.dump());
  assert(!loadBytes(oldEffect));
  load(base);
  // Missing/invalid paths remain available for relinking, while their DSP is cleared.
  auto missing = base;
  missing["DrivePath"] = (fixtures / "missing.nam").string();
  load(missing);
  assert(save().config() == missing);
  std::ofstream(fixtures / "invalid.nam") << "invalid";
  missing["NAMPath"] = (fixtures / "invalid.nam").string();
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
    badGrid.string("###NAMGrid-v1###");
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
      if (std::strcmp(automation.name, "Drive Trim") == 0)
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
    assert(std::abs(save().config()["Drive Trim"].get<double>() + 8.) < 1e-9);
    for (int replacement = 0; replacement < 6; ++replacement)
    {
      auto next = base;
      if (replacement % 2 == 0)
        next["DrivePath"] = "";
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
    fullGrid[label + " In"] = .1 * slot;
    fullGrid[label + " Trim"] = -.2 * slot;
    fullGrid[label + " Enabled"] = slot % 3 == 0 ? 0. : 1.;
  }
  fullGrid["Parallel Blend"] = 37.;
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
  cleared["DrivePath"] = "";
  cleared["NAMPath"] = "";
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
  stereoUtility["SlotEffects"][15] = 25;
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
  syncState["SlotEffects"][0] = 12;
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
  auto irState = cleared;
  irState["IRPath"] = irFile.string();
  irState["IRToggle"] = 1.;
  load(irState);
  assert(save().config() == irState);
  assert(plugin->activate(plugin, 48000., 1, 512));
  assert(plugin->start_processing(plugin));
  assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
  for (int i = 0; i < 512; ++i)
    assert(std::isfinite(output[0][i]));
  plugin->stop_processing(plugin);
  plugin->deactivate(plugin);
  // All four built-ins occupy independently saved slots in every row, including parallel lanes.
  auto dynamics = cleared;
  for (int slot = 0; slot < 16; ++slot)
    dynamics["SlotEffects"][slot] = slot % 4 + 1;
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
  eqIR["SlotEffects"][0] = 7;
  eqIR["SlotPaths"][0] = irFile.string();
  eqIR["Pre 1 IR Level"] = -12.;
  eqIR["SlotEffects"][3] = 5;
  eqIR["Pre 4 EQ Bass"] = 3.;
  eqIR["Pre 4 EQ Mid"] = -4.;
  eqIR["Pre 4 EQ Treble"] = 2.;
  eqIR["SlotEffects"][12] = 6;
  eqIR["Post 1 EQ High Pass"] = 75.;
  eqIR["Post 1 EQ Low Pass"] = 7500.;
  eqIR["Post 1 EQ Presence"] = -2.;
  load(eqIR);
  assert(save().config() == eqIR);
  const auto eqIRBytes = save();
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
    timeEffects["SlotEffects"][slot] = 10 + slot % 14;
  timeEffects["Pre 3 Delay Time"] = 123.;
  timeEffects["Pre 4 Delay Character"] = 67.;
  timeEffects["A 2 Reverb Decay"] = 3.4;
  timeEffects["B 1 Mod Rate"] = 2.3;
  timeEffects["Post 2 Tape Drive"] = 5.;
  timeEffects["SlotEffects"][14] = 24;
  timeEffects["Post 3 Tube Drive"] = 12.;
  timeEffects["SlotEffects"][0] = 26;
  timeEffects["SlotEffects"][1] = 27;
  timeEffects["Pre 1 Spring Dwell"] = 5.;
  timeEffects["Pre 1 Spring Drip"] = 73.;
  timeEffects["Pre 2 Wah Position"] = 28.;
  timeEffects["Pre 2 Wah Resonance"] = 4.;
  timeEffects["Pre 3 Delay Ping Pong"] = 1.;
  timeEffects["B 1 Chorus Spread"] = 85.;
  timeEffects["Post 3 Tube Asymmetry"] = 63.;
  timeEffects["Post 3 Tube Mix"] = 47.;
  timeEffects["A 2 Reverb High Pass"] = 175.;
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
    allDelays["SlotEffects"][slot] = 12;
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
#include "NoisePrintClapTests.inc"
#include "LoFiTapeClapTests.inc"
  load(base);
  TestRoadmapClap(plugin, cleared, save, load);
  TestEffectModeClap(plugin, cleared, save, load);
  TestGlobalInputGain(plugin, cleared, load);
  TestCachedControlsClap(plugin, cleared, load, fixtures);
  auto schema11 = cleared; schema11["schema"] = 11; schema11.erase("Global Input");
  load(schema11);
  assert(save().config()["Global Input"] == 0.);
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
  browserState["CaptureFolder"] = libraryFolder.string();
  browserState["CaptureFolders"] = {libraryFolder.string(), irLibrary.string()};
  browserState["PresetFolders"] = json::array();
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
  const auto selectTile = [&](int x, int y) { click(x, y); click(932, 28); };
  if(fxVisuals)
  {
    const std::array<const char*,17> names{{"In","Trim","Enabled","Gate Threshold","Gate Release","Gate Reduction",
      "Comp Threshold","Comp Release","Comp Makeup","EQ Bass","EQ Mid","EQ Treble","EQ High Pass","EQ Low Pass","EQ Presence","IR Level","Full Quality"}};
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
        if(argc==4) CaptureEditor(editor,std::filesystem::path(argv[3])/("fx-"+std::to_string(static_cast<int>(effect))+"-default.bmp"));
        plugin->stop_processing(plugin); plugin->deactivate(plugin);
        assert(save().config()==before); // Viewing/processing must not edit the rig.
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
    click(910, 28); click(260, 132); click(915, 44);
    pumpFor(250); // Exercise configured sources even when the user's startup rescan is off.
  }
  if (!browserOnly)
  {
    load(learnedNoiseState);
    win_edit_menu::SelectFirstDestination(editor, 0, 0);
    pumpFor(50);
    const auto noiseCopy = save().config();
    assert(noiseCopy["SlotEffects"][1] == 64);
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
          const char* sourceGain = source == 0 ? "Drive Input" : "Input";
          const char* destinationGain = destination == 0 ? "Drive Input" : "Input";
          auto editState = browserState;
          editState["DrivePath"] = libraryFirst.string();
          editState["NAMPath"] = librarySecond.string();
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
    click(910, 28); click(260, 132); click(915, 44); // Rescan all from library settings.
    pumpFor(250);
    // Recall follows the saved content even when the old filename disappeared or
    // now points at another model. Both loader types resolve through the index.
    for (bool ir : {false, true})
    {
      auto relocated = browserState;
      const auto canonical = ir ? libraryIR : librarySecond;
      const auto hash = nam_library::ContentHash(canonical);
      relocated["SlotEffects"][0] = ir ? 7 : 0;
      relocated["SlotPaths"][0] = (fixtures / (ir ? "moved.wav" : "moved.nam")).string();
      relocated["SlotHashes"][0] = hash;
      relocated["DrivePath"] = ir ? "" : relocated["SlotPaths"][0].get<std::string>();
      Stream bytes; bytes.string("###NAMGrid-v1###"); bytes.string(relocated.dump());
      assert(loadBytes(bytes));
      for(int wait=0;wait<100 && std::filesystem::path(save().config()["SlotPaths"][0].get<std::string>()) != canonical;++wait) pumpFor(20);
      assert(std::filesystem::path(save().config()["SlotPaths"][0].get<std::string>()) == canonical);
      assert(save().config()["SlotHashes"][0] == hash);
      relocated["SlotPaths"][0] = path2.string(); // A different capture occupies the saved path.
      relocated["DrivePath"] = ir ? "" : path2.string();
      Stream replaced; replaced.string("###NAMGrid-v1###"); replaced.string(relocated.dump());
      assert(loadBytes(replaced));
      assert(std::filesystem::path(save().config()["SlotPaths"][0].get<std::string>()) == canonical);
      // A stale index must not prevent loading a verified saved copy outside it.
      relocated["SlotPaths"][0] = (ir ? irFile : path1).string();
      relocated["DrivePath"] = ir ? "" : path1.string();
      Stream fallback; fallback.string("###NAMGrid-v1###"); fallback.string(relocated.dump());
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
      chosen["DrivePath"] = libraryTwin.string();
      Stream bytes; bytes.string("###NAMGrid-v1###"); bytes.string(chosen.dump());
      assert(loadBytes(bytes));
      assert(std::filesystem::path(save().config()["SlotPaths"][0].get<std::string>()) == libraryTwin);
      chosen["SlotHashes"][0] = ""; // An explicit choice carries no remembered hash.
      Stream explicitChoice; explicitChoice.string("###NAMGrid-v1###"); explicitChoice.string(chosen.dump());
      assert(loadBytes(explicitChoice));
      assert(std::filesystem::path(save().config()["SlotPaths"][0].get<std::string>()) == libraryTwin);
    }
    load(browserState);
    const auto beforeHashError = save().config();
    auto invalidHashes = beforeHashError; invalidHashes["SlotHashes"][0] = std::string(40, 'z');
    Stream badHashBytes; badHashBytes.string("###NAMGrid-v1###"); badHashBytes.string(invalidHashes.dump());
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
        if (effect == SlotEffect::CutEQ) assert(inserted["Pre 1 EQ High Pass"] == 80.);
        win_edit_menu::SelectNamedItem(editor, 170, 105, L"Clear slot", true); pumpFor(30);
        assert(save().config()["SlotEffects"][0] == 0 && save().config()["SlotPaths"][0] == "");
      }
    win_edit_menu::SelectNamedItem(editor, 220, grid_layout::moduleButtonY, L"Clear slot"); pumpFor(30);
    // Empty slots have neither a card toggle nor a lone editor Enabled control.
    {
      const auto empty = save().config();
      click(263, grid_layout::cardTop + 12);
      click(grid_layout::KnobX(7), grid_layout::ToggleY(7));
      assert(save().config() == empty);
    }
    std::cout << "Single-click module selection and right-click Clear slot passed\n";
  }
  auto eqUI = browserState;
  eqUI["SlotEffects"][0] = 5;
  eqUI["SlotEffects"][1] = 6;
  eqUI["SlotEffects"][2] = 7;
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
    assert(SetKeyboardState(previous)); pumpFor(20);
  };
  const auto enterBrowserSearch = [&](const std::string& query) {
    click(200, 122);
    if (IsWindowVisible(editor)) assert(GetFocus() == editor);
    for (const char ch : query) keyAtEditor(ch == ' ' ? VK_SPACE : ch);
    pumpFor(150); // Results must update before Enter commits the search editor.
  };
  const auto chooseBrowserItem = [&](const std::string& query) {
    enterBrowserSearch(query); keyAtEditor(VK_RETURN); keyAtEditor(VK_RETURN);
  };
  // Tiles and the browser icon share context. Merely opening/searching/navigating
  // must not edit a rig; accepting the selected item targets the originating slot.
  auto browserCurrent = browserState;
  browserCurrent["SlotPaths"][0] = libraryTwin.string();
  browserCurrent["DrivePath"] = libraryTwin.string();
  load(browserCurrent);
  const auto beforeBrowse = save().config();
  click(170, 120);
  if (argc == 4) CaptureEditor(editor, std::filesystem::path(argv[3]) / "browser-nam-current.bmp");
  assert(save().config() == beforeBrowse);
  keyAtEditor(VK_RETURN); // The exact duplicate path remains selected.
  assert(std::filesystem::path(save().config()["SlotPaths"][0].get<std::string>()) == libraryTwin);
  click(356, grid_layout::moduleButtonY);
  enterBrowserSearch("DRIVE");
  assert(save().config()["SlotPaths"][0] == beforeBrowse["SlotPaths"][0]);
  if (argc == 4) CaptureEditor(editor, std::filesystem::path(argv[3]) / "browser-nam-search.bmp");
  keyAtEditor(VK_RETURN); keyAtEditor(VK_RETURN);
  assert(std::filesystem::path(save().config()["SlotPaths"][0].get<std::string>()) == librarySecond);
  click(356, grid_layout::moduleButtonY);
  click(350, 74); // Cell context allows IR, but offers no complete rig presets.
  click(480, 74); // A hidden preset tab must not accept input.
  chooseBrowserItem("CABINET");
  assert(save().config()["SlotEffects"][0] == static_cast<int>(SlotEffect::CabinetIR));
  assert(std::filesystem::path(save().config()["SlotPaths"][0].get<std::string>()) == libraryIR);
  click(356, grid_layout::moduleButtonY);
  if (argc == 4) CaptureEditor(editor, std::filesystem::path(argv[3]) / "browser-ir-current.bmp");
  keyAtEditor(VK_ESCAPE);
  // An external file's folder is browsable without adding it to the user's library.
  auto externalFile = browserState;
  externalFile["SlotPaths"][0] = path2.string(); externalFile["DrivePath"] = path2.string();
  load(externalFile); click(170, 120); pumpFor(250);
  keyAtEditor(VK_RETURN);
  assert(std::filesystem::path(save().config()["SlotPaths"][0].get<std::string>()) == path2);
  assert(save().config()["CaptureFolders"] == browserState["CaptureFolders"]);
  auto moduleBrowser = eqUI;
  moduleBrowser["Pre 1 EQ Bass"] = 6.;
  load(moduleBrowser); click(170, 120);
  if (argc == 4) CaptureEditor(editor, std::filesystem::path(argv[3]) / "browser-fx-current.bmp");
  keyAtEditor(VK_RETURN);
  assert(save().config()["Pre 1 EQ Bass"] == 6.); // Reselecting the FX retains edits.
  click(356, grid_layout::moduleButtonY); chooseBrowserItem("CONSOLE EQ");
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
  // Auditions use the real lower editor, while autosave retains the original slot.
  for (const auto kind : {nam_library::FileKind::Module, nam_library::FileKind::NAM, nam_library::FileKind::IR})
  {
    auto original = moduleBrowser;
    const bool nam = kind == nam_library::FileKind::NAM, ir = kind == nam_library::FileKind::IR;
    if (nam || ir)
    {
      original["SlotEffects"][0] = static_cast<int>(ir ? SlotEffect::CabinetIR : SlotEffect::NAM);
      original["SlotPaths"][0] = (ir ? libraryIR : libraryTwin).string();
      original["DrivePath"] = nam ? libraryTwin.string() : "";
    }
    const char* parameter = nam ? "Drive Input" : ir ? "Pre 1 IR Level" : "Pre 1 EQ Bass";
    load(original); selectTile(170, 120);
    const auto committed = save().config();
    const auto before = liveValue(parameter);
    click(356, grid_layout::moduleButtonY);
    host.dirty = host.flushRequested = false;
    dragAt(grid_layout::KnobX(0), grid_layout::KnobGestureY(0));
    const auto audition = liveValue(parameter);
    assert(std::abs(audition - before) > .01);
    assert(save().config() == committed);
    assert(!host.dirty && !host.flushRequested);
    keyAtEditor(VK_ESCAPE);
    assert(std::abs(liveValue(parameter) - before) < .01 && save().config() == committed);
    click(356, grid_layout::moduleButtonY);
    dragAt(grid_layout::KnobX(0), grid_layout::KnobGestureY(0));
    const auto accepted = liveValue(parameter);
    keyAtEditor(VK_RETURN);
    assert(host.dirty);
    assert(std::abs(save().config()[parameter].get<double>() - accepted) < .11);
    if (nam)
    {
      click(356, grid_layout::moduleButtonY);
      enterBrowserSearch("DRIVE"); keyAtEditor(VK_RETURN);
      dragAt(grid_layout::KnobX(0), grid_layout::KnobGestureY(0));
      const auto gain = liveValue(parameter);
      assert(save().config()["SlotPaths"][0] == committed["SlotPaths"][0]);
      keyAtEditor(VK_RETURN);
      assert(std::filesystem::path(save().config()["SlotPaths"][0].get<std::string>()) == librarySecond);
      assert(std::abs(save().config()[parameter].get<double>() - gain) < .11);
    }
  }
  load(moduleBrowser); selectTile(170, 120);
  click(356, grid_layout::moduleButtonY);
  dragAt(grid_layout::KnobX(0), grid_layout::KnobGestureY(0));
  const auto editedBass = liveValue("Pre 1 EQ Bass");
  enterBrowserSearch("CONSOLE EQ"); keyAtEditor(VK_RETURN);
  click(765, 122); // Clear search before returning to the original FX.
  enterBrowserSearch("GUITAR EQ"); keyAtEditor(VK_RETURN);
  assert(std::abs(liveValue("Pre 1 EQ Bass") - editedBass) < .01);
  keyAtEditor(VK_ESCAPE);
  assert(save().config()["Pre 1 EQ Bass"] == 6.);
  click(356, grid_layout::moduleButtonY);
  enterBrowserSearch("CONSOLE EQ"); keyAtEditor(VK_RETURN);
  SendMessage(editor, WM_LBUTTONDBLCLK, MK_LBUTTON, MAKELPARAM(400, 176));
  SendMessage(editor, WM_LBUTTONUP, 0, MAKELPARAM(400, 176)); pumpFor(100);
  assert(save().config()["SlotEffects"][0] == static_cast<int>(SlotEffect::ConsoleEQ));
  load(moduleBrowser); selectTile(170, 120);
  const auto beforeLibraryAudition = save().config();
  click(910, 28); // Browse from settings must audition the selected slot too.
  click(760, 44);
  click(80, 74);
  enterBrowserSearch("CONSOLE EQ"); keyAtEditor(VK_RETURN);
  dragAt(grid_layout::KnobX(0), grid_layout::KnobGestureY(0));
  assert(save().config() == beforeLibraryAudition);
  click(460, 74); // Switching to complete rigs cancels the slot audition.
  assert(save().config() == beforeLibraryAudition);
  assert(std::abs(liveValue("Pre 1 EQ Bass") - 6.) < .01);
  keyAtEditor(VK_ESCAPE);
  std::cout << "FX/NAM/IR live parameter audition, autosave isolation, cancellation and acceptance passed\n";
  load(eqUI); selectTile(170, 120);
  std::cout << "Context browser tile/icon entry, exact current selection, live search and NAM/IR/FX targeting passed\n";
  const auto resetDial = [&](int x, int y) {
    SendMessage(editor, WM_LBUTTONDBLCLK, MK_LBUTTON, MAKELPARAM(x, y));
    SendMessage(editor, WM_LBUTTONUP, 0, MAKELPARAM(x, y)); pumpFor(30);
  };
  // Recall edited Cloud settings; reset returns to Cloud rather than the
  // Room/shared-bank value. Subsequent insertion discards the prior edits.
  auto cloud = browserState;
  cloud["SlotEffects"][0] = static_cast<int>(SlotEffect::CloudReverb);
  cloud["SlotPaths"][0] = ""; cloud["DrivePath"] = "";
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
  keyAtEditor('3'); keyAtEditor(VK_RETURN);
  assert(save().config()["Pre 1 EQ Bass"] == 3.);
  resetDial(grid_layout::KnobX(0), grid_layout::KnobGestureY(0));
  assert(save().config()["Pre 1 EQ Bass"] == 0.);
  for (const auto& control : std::array<std::tuple<int, int, int, const char*, double>, 3>{{
         {37, 130, 100, "Global Input", 0.}, {967, 298, 268, "Parallel Blend", 50.}, {967, 442, 412, "Output", 0.}}})
  {
    const auto [x, valueY, dialY, name, initial] = control;
    click(x, valueY); keyAtEditor('6'); keyAtEditor(VK_RETURN);
    assert(save().config()[name] == 6.);
    resetDial(x, dialY); assert(save().config()[name] == initial);
  }
  std::cout << "Editable values below module/IN/OUT/A-B dials and double-click defaults passed\n";
  const auto beforeGains = save().config();
  dragAt(38, 105); dragAt(967, 272); dragAt(967, 416);
  assert(save().config()["Global Input"] != beforeGains["Global Input"]);
  assert(save().config()["Parallel Blend"] != beforeGains["Parallel Blend"]);
  assert(save().config()["Output"] != beforeGains["Output"]);
  assert(save().config()["Input"] == beforeGains["Input"]); // IN is global, not the Pre 2 NAM input.
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
  rigToSave["NAMPath"] = path2.string();
  rigToSave["Global Input"] = -3.; rigToSave["Output"] = -8.; rigToSave["Lane A Level"] = 1.5;
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
  assert(savedRig["Global Input"] == -3. && savedRig["Pre 1 Freeze Hold"] == 0.);
  assert(savedRig["Pre 3 Noise Learn"] == 0.);
  assert(savedRig["SlotEffects"][3] == static_cast<int>(SlotEffect::LoFiTape));
  assert(savedRig["Pre 4 LoFi Speed"] == 3.75 && savedRig["Pre 4 LoFi Wear"] == 80.);
  assert(savedRig["NoisePrintProfiles"][2] == learnedNoiseState["NoisePrintProfiles"][0]);
  assert(!savedRig.contains("CaptureFolders") && !savedRig.contains("PresetFolders") && !savedRig.contains("Tuner Reference") && !savedRig.contains("Tuner"));
  auto anotherRig = cleared;
  anotherRig["CaptureFolder"] = irLibrary.string(); anotherRig["CaptureFolders"] = {irLibrary.string()};
  anotherRig["PresetFolders"] = json::array();
  anotherRig["Tuner Reference"] = 442.; anotherRig["Tuner Mute"] = 1.;
  load(anotherRig);
  host.dirty = false; host.rescanned = 0;
  win_file_dialog::Choose(presetFile, [&] { presetMenu(L"Load preset file..."); });
  std::cout << "Native preset load passed\n";
  const auto restoredRig = save().config();
  assert(restoredRig["SlotEffects"] == savedRig["SlotEffects"] && restoredRig["SlotPaths"] == savedRig["SlotPaths"]);
  assert(restoredRig["SlotHashes"] == savedRig["SlotHashes"]);
  assert(restoredRig["Global Input"] == -3. && restoredRig["Output"] == -8. && restoredRig["Lane A Level"] == 1.5);
  assert(restoredRig["NoisePrintProfiles"] == savedRig["NoisePrintProfiles"]);
  for (int p = uLoFiSpeed; p <= uLoFiOutput; ++p)
  {
    const std::string name = std::string("Pre 4 ") + kEffectParameters[p].name;
    assert(restoredRig[name] == savedRig[name]);
  }
  assert(restoredRig["Pre 3 Noise Learn"] == 0. && restoredRig["Pre 3 Noise Gate Threshold"] == -42.);
  assert(restoredRig["CaptureFolders"] == anotherRig["CaptureFolders"] && restoredRig["Tuner Reference"] == 442.);
  assert(restoredRig["Pre 1 Freeze Hold"] == 0. && restoredRig["PresetName"] == presetFile.stem().string());
  assert(host.dirty && (host.rescanned & CLAP_PARAM_RESCAN_VALUES));
  win_file_dialog::Choose({}, [&] { presetMenu(L"Load preset file..."); }, true);
  std::cout << "Native preset cancel passed\n";
  assert(save().config() == restoredRig); // Cancel leaves the complete rig unchanged.
  auto secondRig = savedRig; secondRig["Output"] = -10.;
  rig_preset::Write(presetDirectory / "Second rig.ngrid", secondRig);
  // Add preset roots using the actual directory picker, then browse them recursively.
  click(470, 28);
  click(220, 74); // Preset context has no NAM tab here.
  win_file_dialog::Choose(presetDirectory, [&] { click(615, 74); }, false, true);
  pumpFor(250);
  assert(save().config()["PresetFolders"] == json::array({nam_library::PathText(presetDirectory)}));
  if (argc == 4) CaptureEditor(editor, std::filesystem::path(argv[3]) / "browser-presets.bmp");
  chooseBrowserItem("SECOND RIG");
  assert(save().config()["Output"] == -10. && save().config()["PresetFolders"].size() == 1);
  click(664, 28);
  click(704, 28);
  assert(save().config()["PresetName"] == "Second rig" && save().config()["Output"] == -10.);
  click(664, 28);
  if (save().config()["PresetName"] != "Clean rig" || save().config()["Output"] != -8.)
    std::cerr << "Previous preset: " << save().config()["PresetName"] << ", output=" << save().config()["Output"] << '\n';
  assert(save().config()["PresetName"] == "Clean rig" && save().config()["Output"] == -8.);
  click(470, 28); chooseBrowserItem("SECOND RIG");
  assert(save().config()["PresetName"] == "Second rig");
  click(664, 28);
  if (argc == 4) CaptureEditor(editor, std::filesystem::path(argv[3]) / "preset-gains.bmp");
  std::cout << "Preset folder selection and previous/next passed\n";
  if (browserOnly)
  {
    if (argc == 4)
    {
      // Representative dense screens for typography/layout review; no audio device.
      for (const auto [effect, name] : std::array<std::pair<SlotEffect, const char*>, 8>{{
             {SlotEffect::ConsoleEQ, "console-eq"}, {SlotEffect::DigitalDelay, "delay"},
             {SlotEffect::CosmicEcho, "cosmic"}, {SlotEffect::Chorus, "chorus"},
             {SlotEffect::LoFiTape, "lofi"}, {SlotEffect::OutboardChannel, "outboard"},
             {SlotEffect::Octaver, "octaver"}, {SlotEffect::Utility, "utility"}}})
      {
        auto preview = browserState;
        preview["SlotEffects"][0] = static_cast<int>(effect);
        preview["SlotPaths"][0] = ""; preview["DrivePath"] = "";
        load(preview); selectTile(170, 120);
        CaptureEditor(editor, std::filesystem::path(argv[3]) / (std::string("readable-") + name + ".bmp"));
      }
      click(910, 28);
      CaptureEditor(editor, std::filesystem::path(argv[3]) / "readable-settings.bmp");
      click(915, 44); click(225, 28);
      CaptureEditor(editor, std::filesystem::path(argv[3]) / "readable-routing.bmp");
      click(295, 104);
      CaptureEditor(editor, std::filesystem::path(argv[3]) / "readable-modulation.bmp");
      click(465, 104);
      CaptureEditor(editor, std::filesystem::path(argv[3]) / "readable-scenes.bmp");
      click(915, 44); click(808, 36);
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
  assert(save().config()["Pre 2 EQ High Pass"] != 0.);
  selectTile(610, 120); // Select IN3 IR loader.
  dragAt(grid_layout::KnobX(0), grid_layout::KnobGestureY(0));
  assert(save().config()["Pre 3 IR Level"] != -18.);
  auto utilities = browserState;
  utilities["SlotEffects"][0] = 8;
  utilities["SlotEffects"][1] = 9;
  utilities["Pre 1 EQ Low Pass"] = 10000.;
  load(utilities);
  selectTile(170, 120);
  for (int position = 0; position < 5; ++position)
    dragAt(grid_layout::KnobX(position), grid_layout::KnobGestureY(position));
  auto editedUtility = save().config();
  assert(editedUtility["Pre 1 Parametric Frequency"] != utilities["Pre 1 Parametric Frequency"]);
  assert(editedUtility["Pre 1 Parametric Gain"] != utilities["Pre 1 Parametric Gain"]);
  assert(editedUtility["Pre 1 Parametric Q"] != utilities["Pre 1 Parametric Q"]);
  assert(editedUtility["Pre 1 EQ High Pass"] != utilities["Pre 1 EQ High Pass"]);
  assert(editedUtility["Pre 1 EQ Low Pass"] != utilities["Pre 1 EQ Low Pass"]);
  if (argc == 4)
    CaptureEditor(editor, std::filesystem::path(argv[3]) / "parametric-eq.bmp");
  selectTile(390, 120);
  dragAt(grid_layout::KnobX(0), grid_layout::KnobGestureY(0));
  assert(save().config()["Pre 2 Boost Gain"] != utilities["Pre 2 Boost Gain"]);
  dragAt(grid_layout::KnobX(1), grid_layout::KnobGestureY(1));
  dragAt(grid_layout::KnobX(2), grid_layout::KnobGestureY(2));
  assert(save().config()["Pre 2 Boost Tilt"] != utilities["Pre 2 Boost Tilt"]);
  assert(save().config()["Pre 2 EQ High Pass"] != utilities["Pre 2 EQ High Pass"]);
  assert(save().config()["Input"] == utilities["Input"]);
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
      if (UsesUtility(effect, p))
      {
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
        if (p == uTapeHeads || p == uCosmicStyle || p == uColourDesk || p == uBlackMode) click(558, grid_layout::optionsY);
        else if (EffectChoices(p).empty() && descriptor.minimum == 0. && descriptor.maximum == 1. && descriptor.step == 1.)
          click(x, grid_layout::ToggleY(position));
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
    click(grid_layout::KnobX(7), grid_layout::ToggleY(7));
    assert(save().config()["Drive Enabled"] == 0.);
    if (argc == 4)
      CaptureEditor(editor, std::filesystem::path(argv[3]) / (std::string("effect-") + std::to_string(id) + ".bmp"));
  }
  {
    auto category = browserState;
    category["SlotEffects"][0] = static_cast<int>(SlotEffect::DigitalDelay);
    load(category); selectTile(170, 120);
    click(356, grid_layout::moduleButtonY); chooseBrowserItem("TAPE ECHO"); pumpFor(30);
    assert(save().config()["SlotEffects"][0] == static_cast<int>(SlotEffect::TapeDelay));
    click(356, grid_layout::moduleButtonY); chooseBrowserItem("DUAL DELAY"); pumpFor(30);
    assert(save().config()["SlotEffects"][0] == static_cast<int>(SlotEffect::DualDelay));
  }
  // The larger top-right grid toggle changes bypass without replacing the module.
  {
    const auto before = save().config();
    click(263, grid_layout::cardTop + 12);
    assert(save().config()["Drive Enabled"] == 1. - before["Drive Enabled"].get<double>());
    assert(save().config()["SlotEffects"] == before["SlotEffects"]);
    click(263, grid_layout::cardTop + 12);
    assert(save().config()["Drive Enabled"] == before["Drive Enabled"]);
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
  for (int effect : {12, 15, 26, static_cast<int>(SlotEffect::DualDelay)})
  {
    auto switchState = browserState;
    switchState["SlotEffects"][0] = effect;
    load(switchState);
    selectTile(170, 120);
    const char* name = effect == 12 || effect == static_cast<int>(SlotEffect::DualDelay)
                         ? "Pre 1 Delay Ping Pong" : "Pre 1 Reverb Stereo";
    const double before = save().config()[name].get<double>();
    click(558, grid_layout::optionsY);
    assert(save().config()[name].get<double>() == 1. - before);
  }
  {
    auto reverse = browserState; reverse["SlotEffects"][0] = static_cast<int>(SlotEffect::ReverseDelay);
    load(reverse); selectTile(170, 120);
    const auto before = save().config();
    click(558, grid_layout::optionsY); // No ineffective Ping-pong control here.
    assert(save().config() == before);
    auto vibe = browserState; vibe["SlotEffects"][0] = static_cast<int>(SlotEffect::UniVibe);
    vibe["Pre 1 Vibe Vibrato"] = 1.; vibe["Pre 1 Mod Mix"] = 50.;
    load(vibe); selectTile(170, 120); dragAt(grid_layout::KnobX(4), grid_layout::KnobGestureY(4));
    assert(save().config()["Pre 1 Mod Mix"] == 50.); // Fully wet mode hides Mix.
    vibe["Pre 1 Vibe Vibrato"] = 0.; load(vibe); selectTile(170, 120);
    dragAt(grid_layout::KnobX(4), grid_layout::KnobGestureY(4));
    assert(save().config()["Pre 1 Mod Mix"] != 50.);
  }
  const auto packedPath = fixtures / "packed-a2.nam";
  std::ofstream(packedPath) << fixture::packed().dump();
  auto qualityState = browserState;
  qualityState["DrivePath"] = packedPath.string();
  qualityState["Pre 1 Full Quality"] = 1.;
  qualityState["Pre 1 EQ Low Pass"] = 10000.;
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
  click(530, grid_layout::optionsY);
  assert(save().config()["Pre 1 Full Quality"] == 1.);
  click(530, grid_layout::optionsY);
  assert(save().config()["Pre 1 Full Quality"] == 0.);
  for (int i = 0; i < 30; ++i)
    assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
  dragAt(grid_layout::KnobX(0), grid_layout::KnobGestureY(0));
  dragAt(grid_layout::KnobX(1), grid_layout::KnobGestureY(1));
  dragAt(grid_layout::KnobX(2), grid_layout::KnobGestureY(2));
  dragAt(grid_layout::KnobX(3), grid_layout::KnobGestureY(3));
  dragAt(grid_layout::KnobX(4), grid_layout::KnobGestureY(4));
  dragAt(grid_layout::KnobX(5), grid_layout::KnobGestureY(5));
  dragAt(grid_layout::KnobX(6), grid_layout::KnobGestureY(6));
  const auto namControls = save().config();
  assert(namControls["Drive Input"] != qualityState["Drive Input"]);
  assert(namControls["Pre 1 EQ Bass"] != qualityState["Pre 1 EQ Bass"]);
  assert(namControls["Pre 1 EQ Mid"] != qualityState["Pre 1 EQ Mid"]);
  assert(namControls["Pre 1 EQ Treble"] != qualityState["Pre 1 EQ Treble"]);
  assert(namControls["Drive Trim"] != qualityState["Drive Trim"]);
  assert(namControls["Pre 1 EQ High Pass"] != qualityState["Pre 1 EQ High Pass"]);
  assert(namControls["Pre 1 EQ Low Pass"] != qualityState["Pre 1 EQ Low Pass"]);
  click(grid_layout::KnobX(7), grid_layout::ToggleY(7));
  assert(save().config()["Drive Enabled"] == 0.);
  click(grid_layout::KnobX(7), grid_layout::ToggleY(7));
  assert(save().config()["Drive Enabled"] == 1.);
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
  effectUI["SlotEffects"][0] = 1;
  effectUI["SlotEffects"][1] = 3;
  load(effectUI);
  pumpFor(100);
  selectTile(170, 120);
  dragAt(grid_layout::KnobX(0), grid_layout::KnobGestureY(0)); // Selected IN1 gate threshold.
  assert(save().config()["Pre 1 Gate Threshold"] != effectUI["Pre 1 Gate Threshold"]);
  assert(save().config()["Drive Input"] == effectUI["Drive Input"]);
  dragAt(grid_layout::KnobX(3), grid_layout::KnobGestureY(3), 200);
  assert(save().config()["Pre 1 Gate Mode"] == 1.);
  auto tightGate = save();
  assert(loadBytes(tightGate));
  assert(save().config()["Pre 1 Gate Mode"] == 1.);
  resetDial(grid_layout::KnobX(3), grid_layout::KnobGestureY(3));
  assert(save().config()["Pre 1 Gate Mode"] == 0.);
  if (argc == 4)
    CaptureEditor(editor, std::filesystem::path(argv[3]) / "gate.bmp");
  selectTile(390, 120);
  dragAt(grid_layout::KnobX(2), grid_layout::KnobGestureY(2)); // Selected IN2 compressor makeup.
  assert(save().config()["Pre 2 Comp Makeup"] != effectUI["Pre 2 Comp Makeup"]);
  assert(save().config()["Amp Trim"] == effectUI["Amp Trim"]);
  // Card dot bypass and bottom editor toggle must share the same automated parameter.
  click(471, 80);
  assert(save().config()["Amp Enabled"] == 0.);
  click(471, 80);
  assert(save().config()["Amp Enabled"] == 1.);
  // FX navigation stays in the current menu category and wraps in both directions.
  auto navigation = browserState;
  navigation["SlotEffects"][0] = static_cast<int>(SlotEffect::JunoChorus);
  load(navigation); selectTile(170, 120);
  click(422, grid_layout::moduleButtonY);
  assert(save().config()["SlotEffects"][0] == static_cast<int>(SlotEffect::Flanger));
  click(392, grid_layout::moduleButtonY);
  assert(save().config()["SlotEffects"][0] == static_cast<int>(SlotEffect::JunoChorus));
  // NAM/IR arrows use physical siblings, including outside configured roots.
  const auto neighbor = libraryFirst.parent_path() / "neighbor.nam";
  auto neighborModel = a2; neighborModel["metadata"]["name"] = "Unique sibling";
  std::ofstream(neighbor) << neighborModel.dump();
  const auto neighborIR = libraryIR.parent_path() / "neighbor.wav";
  std::filesystem::copy_file(libraryIR, neighborIR, std::filesystem::copy_options::overwrite_existing);
  { std::fstream wav(neighborIR, std::ios::in | std::ios::out | std::ios::binary); wav.seekp(-2, std::ios::end); wav.put(1); wav.put(0); }
  navigation = browserState;
  navigation["DrivePath"] = libraryFirst.string();
  navigation["SlotEffects"][1] = 7; navigation["SlotPaths"][1] = libraryIR.string();
  load(navigation); selectTile(170, 120);
  click(422, grid_layout::moduleButtonY);
  assert(std::filesystem::path(save().config()["SlotPaths"][0].get<std::string>()) == neighbor);
  assert(save().config()["SlotEffects"][0] == 0);
  click(422, grid_layout::moduleButtonY); // Wrap without entering another folder or IR category.
  assert(std::filesystem::path(save().config()["SlotPaths"][0].get<std::string>()) == libraryFirst);
  click(392, grid_layout::moduleButtonY);
  assert(std::filesystem::path(save().config()["SlotPaths"][0].get<std::string>()) == neighbor);
  click(356, grid_layout::moduleButtonY); chooseBrowserItem("AMP"); pumpFor(30);
  assert(std::filesystem::path(save().config()["SlotPaths"][0].get<std::string>()) == libraryFirst);
  selectTile(390, 120); click(422,grid_layout::moduleButtonY);
  assert(std::filesystem::path(save().config()["SlotPaths"][1].get<std::string>()) == neighborIR);
  assert(save().config()["SlotEffects"][1] == 7);
  click(392,grid_layout::moduleButtonY);
  assert(std::filesystem::path(save().config()["SlotPaths"][1].get<std::string>()) == libraryIR);
  if(argc==4) CaptureEditor(editor,std::filesystem::path(argv[3])/"ir.bmp");
  // Empty file slots cannot cycle into another category.
  selectTile(170, 224); click(422,grid_layout::moduleButtonY);
  assert(save().config()["SlotEffects"][4] == 0 && save().config()["SlotPaths"][4] == "");
  // The settings page is opaque and intercepts clicks; grid/editor controls cannot receive them.
  selectTile(170, 120);
  if (argc == 4)
    CaptureEditor(editor, std::filesystem::path(argv[3]) / "grid.bmp");
  const auto beforeSettings = save().config();
  click(910, 36);
  click(700, 132); // Global visualizations off.
  assert(save().config() == beforeSettings); // Editor preference is outside the rig.
  click(700, 132); // Restore preference for subsequent tests/previews.
  if (argc == 4)
    CaptureEditor(editor, std::filesystem::path(argv[3]) / "settings.bmp");
  dragAt(grid_layout::KnobX(0), grid_layout::KnobGestureY(0)); // NAM Input underneath; the settings reference dial starts below this point.
  assert(save().config() == beforeSettings);
  click(915, 44);
  click(910, 36);
  // Escape closes settings so the selected module becomes editable again.
  SendMessage(editor, WM_MOUSEMOVE, 0, MAKELPARAM(915, 44));
  SendMessage(editor, WM_KEYDOWN, VK_ESCAPE, static_cast<LPARAM>(MapVirtualKey(VK_ESCAPE, MAPVK_VK_TO_VSC)) << 16);
  pumpFor(100);
  dragAt(grid_layout::KnobX(0), grid_layout::KnobGestureY(0));
  assert(save().config()["Drive Input"] != beforeSettings["Drive Input"]);
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
      if (i < 2)
        colors[i == 0 ? "DrivePath" : "NAMPath"] = path.string();
    }
    colors["SlotEffects"][4] = 1;
    colors["SlotEffects"][5] = 3;
    colors["SlotEffects"][6] = 5;
    colors["SlotEffects"][7] = 6;
    colors["SlotEffects"][8] = 8;
    colors["SlotEffects"][9] = 9;
    colors["SlotEffects"][10] = 12;
    colors["SlotEffects"][11] = 14;
    colors["SlotEffects"][12] = 15;
    colors["SlotEffects"][13] = 16;
    colors["SlotEffects"][14] = 18;
    colors["SlotEffects"][15] = 23;
    load(colors);
    pumpFor(100);
    selectTile(610, 120);
    CaptureEditor(editor, std::filesystem::path(argv[3]) / "node-colors.bmp");
    colors["SlotEffects"][0] = 24;
    colors["SlotPaths"][0] = "";
    colors["DrivePath"] = "";
    load(colors);
    selectTile(170, 120);
    pumpFor(300);
    CaptureEditor(editor, std::filesystem::path(argv[3]) / "tube-boost.bmp");
    for (int effect : {26, 27, 18, 15, 12})
    {
      auto preview = browserState;
      preview["SlotEffects"][0] = effect;
      if (effect == 12)
        preview["Pre 1 Delay Ping Pong"] = 1.;
      load(preview);
      selectTile(170, 120);
      CaptureEditor(
        editor, std::filesystem::path(argv[3]) / (std::string("stereo-guitar-") + std::to_string(effect) + ".bmp"));
    }
    auto utilityPreview = browserState;
    utilityPreview["SlotEffects"][0] = 25;
    load(utilityPreview);
    selectTile(170, 120);
    CaptureEditor(editor, std::filesystem::path(argv[3]) / "utility.bmp");
    auto syncPreview = browserState;
    syncPreview["SlotEffects"][0] = 14;
    syncPreview["Pre 1 Tempo Sync"] = 1.;
    load(syncPreview);
    selectTile(170, 120);
    CaptureEditor(editor, std::filesystem::path(argv[3]) / "tempo-sync.bmp");
  }
  load(cleared);
  click(808, 36); // Tuner beside Settings.
  assert(save().config()["Tuner"] == 1.);
  dragAt(278, 579);
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
  plugin->stop_processing(plugin);
  plugin->deactivate(plugin);
  // Exercise the real performance overlay and scene transactions without replacing models.
  load(cleared);
  click(225, 28);
  dragAt(95, 235);
  const double sceneGain = save().config()["Lane A Level"];
  assert(sceneGain != 0.);
  click(465, 104); // Scenes tab
  click(110, 419); // Store scene 1
  const auto sceneState = save().config();
  assert(sceneState["PerformanceScenes"][0]["Lane A Level"] == sceneGain);
  assert(!sceneState["PerformanceScenes"][0].contains("Pre 1 Noise Learn"));
  assert(!sceneState["PerformanceScenes"][0].contains("Tuner"));
  automate("Lane A Level", 0.); // -60 dB in the normalized CLAP API
  assert(save().config()["Lane A Level"] == -60.);
  click(245, 419); // Recall scene 1
  assert(save().config()["Lane A Level"] == sceneGain);
  const auto storedScene = save();
  load(cleared);
  assert(loadBytes(storedScene));
  assert(save().config()["PerformanceScenes"] == sceneState["PerformanceScenes"]);
  auto malformedScene = save().config();
  malformedScene["PerformanceScenes"][0]["Not a real parameter"] = 1.;
  Stream malformedSceneBytes; malformedSceneBytes.string("###NAMGrid-v1###"); malformedSceneBytes.string(malformedScene.dump());
  assert(!loadBytes(malformedSceneBytes));
  assert(save().config()["PerformanceScenes"] == sceneState["PerformanceScenes"]);
  malformedScene["PerformanceScenes"][0].erase("Not a real parameter");
  malformedScene["PerformanceScenes"][0]["Pre 1 Noise Learn"] = 1.;
  Stream learnSceneBytes;
  learnSceneBytes.string("###NAMGrid-v1###");
  learnSceneBytes.string(malformedScene.dump());
  assert(!loadBytes(learnSceneBytes));
  assert(save().config()["PerformanceScenes"] == sceneState["PerformanceScenes"]);
  click(295, 104); // Modulation tab
  dragAt(230, 315);
  assert(save().config()["LFO Depth"] != 0.);
  click(915, 44); // Close overlay
  // A shorter lane's new conversion delay requires a restart even when max(A,B) is unchanged.
  auto unequal = cleared;
  unequal["CaptureFolder"] = libraryFolder.string();
  unequal["CaptureFolders"] = {libraryFolder.string()};
  unequal["SlotPaths"][4] = libraryFirst.string();
  unequal["SlotPaths"][5] = libraryFirst.string();
  load(unequal);
  click(910, 28); click(260, 132); click(915, 44); pumpFor(250);
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
  click(808, 36);
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
#else
  dlclose(library);
#endif
  std::cout << "CLAP factory, model files, state, missing/invalid files, reset, teardown and native editor passed\n";
}
