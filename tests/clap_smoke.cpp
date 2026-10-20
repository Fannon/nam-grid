#include "A2Fixture.h"
#include "ClapTestHost.h"
#include <thread>
#include "WinEditorCapture.h"
#include "../NeuralAmpModeler/EffectParameters.h"
int main(int argc, char** argv)
{
  assert(argc == 3 || argc == 4);
#ifdef _WIN32
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
    Stream s;
    s.string("###NAMGrid-v1###");
    s.string(config.dump());
    assert(loadBytes(std::move(s)));
  };
  auto base = save().config();
  assert(params->count(plugin) == 284 + 16 * kUtilityControls);
  assert(base["NoiseGateActive"] == 0. && base["ToneStack"] == 0. && base["IRToggle"] == 0.);
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
  for (const json& folders : {json(123), json::array({"valid", 123}), json::array({std::string("bad\0path", 8)}),
                              json(std::vector<std::string>(33, "folder"))})
  {
    auto invalidFolders = base;
    invalidFolders["CaptureFolders"] = folders;
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
  clap_output_events sent{nullptr, [](const clap_output_events*, const clap_event_header*) { return true; }};
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
  dynamics["A 1 Gate Decay"] = 75.;
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
    assert(tail && tail->get(plugin) > static_cast<uint32_t>(rate * 100.));
    assert(tail->get(plugin) != std::numeric_limits<uint32_t>::max());

    for (int b = 0; b < 40; ++b)
    {
      process.frames_count = std::array<int, 6>{1, 3, 64, 127, 256, 512}[b % 6];
      for (uint32_t i = 0; i < process.frames_count; ++i)
        input[0][i] = input[1][i] = .2 * std::sin((b * 512 + i) * .03);
      assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
      for (uint32_t i = 0; i < process.frames_count; ++i)
        assert(std::isfinite(output[0][i]) && output[0][i] == output[1][i]);
    }
    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);
  }
  load(base);
#ifdef _WIN32
  // Create native editor attached to a hidden parent, without launching a visible application.
  const auto libraryFolder = fixtures / "library";
  const auto libraryFirst = libraryFolder / "Amps" / "Nested" / "amp.nam";
  const auto librarySecond = libraryFolder / "Pedals" / "Nested" / "drive.nam";
  std::filesystem::create_directories(libraryFirst.parent_path());
  std::filesystem::create_directories(librarySecond.parent_path());
  std::filesystem::copy_file(path2, libraryFirst, std::filesystem::copy_options::overwrite_existing);
  std::filesystem::copy_file(path1, librarySecond, std::filesystem::copy_options::overwrite_existing);
  const auto irLibrary = fixtures / "ir-library";
  std::filesystem::create_directories(irLibrary);
  const auto libraryIR = irLibrary / "cabinet.wav";
  std::filesystem::copy_file(irFile, libraryIR, std::filesystem::copy_options::overwrite_existing);
  auto browserState = cleared;
  browserState["CaptureFolder"] = libraryFolder.string();
  browserState["CaptureFolders"] = {libraryFolder.string(), irLibrary.string()};
  load(browserState);
  auto* gui = static_cast<const clap_plugin_gui*>(plugin->get_extension(plugin, CLAP_EXT_GUI));
  assert(gui);
  assert(gui->create(plugin, CLAP_WINDOW_API_WIN32, false));
  auto parent =
    CreateWindowExW(0, L"STATIC", L"NAM test", 0, 0, 0, 1000, 800, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
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
    SetWindowPos(parent, HWND_TOP, 0, 0, 1000, 800, SWP_NOACTIVATE | SWP_SHOWWINDOW);
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
  auto click = [&](int x, int y) {
    SendMessage(editor, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
    SendMessage(editor, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
    pumpFor(250);
  };
  click(890, 90); // Manual rescan works even if startup scanning is disabled in user preferences.
  pumpFor(250);
  auto eqUI = browserState;
  eqUI["SlotEffects"][0] = 5;
  eqUI["SlotEffects"][1] = 6;
  eqUI["SlotEffects"][2] = 7;
  eqUI["SlotPaths"][2] = irFile.string();
  load(eqUI);
  pumpFor(100);
  const auto dragAt = [&](int x, int y) {
    SendMessage(editor, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
    SendMessage(editor, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(x, y - 25));
    SendMessage(editor, WM_LBUTTONUP, 0, MAKELPARAM(x, y - 25));
    pumpFor(100);
  };
  dragAt(105, 700);
  assert(save().config()["Pre 1 EQ Bass"] != 0.);
  click(390, 160); // Select IN2 Cut EQ; only its controls may receive gestures.
  dragAt(105, 700);
  assert(save().config()["Pre 2 EQ High Pass"] != 0.);
  click(610, 160); // Select IN3 IR loader.
  dragAt(105, 700);
  assert(save().config()["Pre 3 IR Level"] != -18.);
  auto utilities = browserState;
  utilities["SlotEffects"][0] = 8;
  utilities["SlotEffects"][1] = 9;
  utilities["Pre 1 EQ Low Pass"] = 10000.;
  load(utilities);
  click(170, 160);
  for (int x : {105, 257, 409, 561, 713})
    dragAt(x, 700);
  auto editedUtility = save().config();
  assert(editedUtility["Pre 1 Parametric Frequency"] != utilities["Pre 1 Parametric Frequency"]);
  assert(editedUtility["Pre 1 Parametric Gain"] != utilities["Pre 1 Parametric Gain"]);
  assert(editedUtility["Pre 1 Parametric Q"] != utilities["Pre 1 Parametric Q"]);
  assert(editedUtility["Pre 1 EQ High Pass"] != utilities["Pre 1 EQ High Pass"]);
  assert(editedUtility["Pre 1 EQ Low Pass"] != utilities["Pre 1 EQ Low Pass"]);
  if (argc == 4)
    CaptureEditor(editor, std::filesystem::path(argv[3]) / "parametric-eq.bmp");
  click(390, 160);
  dragAt(105, 700);
  assert(save().config()["Pre 2 Boost Gain"] != utilities["Pre 2 Boost Gain"]);
  dragAt(257, 700); dragAt(409, 700);
  assert(save().config()["Pre 2 Boost Tilt"] != utilities["Pre 2 Boost Tilt"]);
  assert(save().config()["Pre 2 EQ High Pass"] != utilities["Pre 2 EQ High Pass"]);
  assert(save().config()["Input"] == utilities["Input"]);
  if (argc == 4)
    CaptureEditor(editor, std::filesystem::path(argv[3]) / "boost.bmp");
  // Every visible new knob must update its slot's automated parameter, with no
  // hidden controls stealing gestures when modules are restored into an open UI.
  for (int id = 10; id <= 23; ++id)
  {
    auto effectControls = browserState;
    effectControls["SlotEffects"][0] = id;
    load(effectControls);
    click(170, 160);
    const auto effect = static_cast<SlotEffect>(id);
    for (int p = 0; p < kUtilityControls; ++p)
      if (UsesUtility(effect, p))
      {
        const auto& descriptor = kEffectParameters[p];
        const std::string name = std::string("Pre 1 ") + descriptor.name;
        const auto before = save().config();
        // Drag down at an upper limit (Tape Mix defaults to 100%).
        const int x = 105 + 152 * descriptor.position;
        const int delta = before[name].get<double>() >= descriptor.maximum ? 25 : -25;
        SendMessage(editor, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, 700));
        SendMessage(editor, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(x, 700 + delta));
        SendMessage(editor, WM_LBUTTONUP, 0, MAKELPARAM(x, 700 + delta));
        pumpFor(30);
        assert(save().config()[name] != before[name]);
      }
    click(865, 707);
    assert(save().config()["Drive Enabled"] == 0.);
    if (argc == 4)
      CaptureEditor(editor, std::filesystem::path(argv[3]) / (std::string("effect-") + std::to_string(id) + ".bmp"));
  }
  const auto packedPath = fixtures / "packed-a2.nam";
  std::ofstream(packedPath) << fixture::packed().dump();
  auto qualityState = browserState;
  qualityState["DrivePath"] = packedPath.string();
  qualityState["Pre 1 Full Quality"] = 1.;
  load(qualityState);
  pumpFor(100);
  assert(plugin->activate(plugin, 48000., 1, 512));
  assert(plugin->start_processing(plugin));
  process.frames_count = 512;
  click(170, 160); // Select IN1.
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
  for (int i = 0; i < 30; ++i)
    assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
  dragAt(105, 700);
  dragAt(257, 700);
  dragAt(409, 700);
  dragAt(561, 700);
  dragAt(713, 700);
  const auto namControls = save().config();
  assert(namControls["Drive Input"] != qualityState["Drive Input"]);
  assert(namControls["Pre 1 EQ Bass"] != qualityState["Pre 1 EQ Bass"]);
  assert(namControls["Pre 1 EQ Mid"] != qualityState["Pre 1 EQ Mid"]);
  assert(namControls["Pre 1 EQ Treble"] != qualityState["Pre 1 EQ Treble"]);
  assert(namControls["Drive Trim"] != qualityState["Drive Trim"]);
  click(865, 707);
  assert(save().config()["Drive Enabled"] == 0.);
  click(865, 707);
  assert(save().config()["Drive Enabled"] == 1.);
  if (argc == 4)
    CaptureEditor(editor, std::filesystem::path(argv[3]) / "nam-six-controls.bmp");
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
  click(170, 160);
  dragAt(105, 700); // Selected IN1 gate threshold.
  assert(save().config()["Pre 1 Gate Threshold"] != effectUI["Pre 1 Gate Threshold"]);
  assert(save().config()["Drive Input"] == effectUI["Drive Input"]);
  if (argc == 4)
    CaptureEditor(editor, std::filesystem::path(argv[3]) / "gate.bmp");
  click(390, 160);
  dragAt(409, 700); // Selected IN2 compressor makeup.
  assert(save().config()["Pre 2 Comp Makeup"] != effectUI["Pre 2 Comp Makeup"]);
  assert(save().config()["Amp Trim"] == effectUI["Amp Trim"]);
  // Card dot bypass and bottom editor toggle must share the same automated parameter.
  click(499, 132);
  assert(save().config()["Amp Enabled"] == 0.);
  click(499, 132);
  assert(save().config()["Amp Enabled"] == 1.);
  // Next buttons traverse the recursive library, rather than only the current file's siblings.
  click(490, 621); // IN2 next.
  assert(host.dirty);
  assert(save().config()["SlotEffects"][1] == 0);
  assert(std::filesystem::path(save().config()["NAMPath"].get<std::string>()) == libraryFirst);
  click(490, 621);
  assert(std::filesystem::path(save().config()["NAMPath"].get<std::string>()) == librarySecond);
  click(490, 621); // WAV from the second source becomes an IR loader automatically.
  assert(save().config()["SlotEffects"][1] == 7);
  assert(std::filesystem::path(save().config()["SlotPaths"][1].get<std::string>()) == libraryIR);
  if (argc == 4)
    CaptureEditor(editor, std::filesystem::path(argv[3]) / "ir.bmp");
  click(490, 621); // Cycling back switches cleanly to NAM again.
  assert(save().config()["SlotEffects"][1] == 0);
  assert(std::filesystem::path(save().config()["NAMPath"].get<std::string>()) == libraryFirst);
  click(170, 160);
  click(490, 621); // IN1 next.
  assert(save().config()["SlotEffects"][0] == 0);
  assert(std::filesystem::path(save().config()["DrivePath"].get<std::string>()) == libraryFirst);
  click(200, 90);
  for (int key : std::array<int, 4>{'A', 'M', 'P', VK_RETURN})
    SendMessage(editor, WM_KEYDOWN, key, static_cast<LPARAM>(MapVirtualKey(key, MAPVK_VK_TO_VSC)) << 16);
  pumpFor(100);
  click(390, 160);
  click(490, 621);
  assert(std::filesystem::path(save().config()["NAMPath"].get<std::string>()) == libraryFirst);
  click(170, 264);
  click(490, 621); // A1 next, with "amp" search active.
  assert(std::filesystem::path(save().config()["SlotPaths"][4].get<std::string>()) == libraryFirst);
  click(170, 368);
  click(490, 621); // B1 next.
  assert(std::filesystem::path(save().config()["SlotPaths"][8].get<std::string>()) == libraryFirst);
  click(830, 472);
  click(490, 621); // OUT4 next.
  assert(std::filesystem::path(save().config()["SlotPaths"][15].get<std::string>()) == libraryFirst);
  const auto filteredState = save().config();
  click(608, 90); // Clear search/category. Visible field and count must synchronize.
  click(490, 621);
  assert(std::filesystem::path(save().config()["SlotPaths"][15].get<std::string>()) == librarySecond);
  click(200, 90);
  for (int key : std::array<int, 5>{'T', 'E', 'S', 'T', VK_RETURN})
    SendMessage(editor, WM_KEYDOWN, key, static_cast<LPARAM>(MapVirtualKey(key, MAPVK_VK_TO_VSC)) << 16);
  pumpFor(100);
  const auto noMatches = save().config();
  click(490, 621);
  assert(save().config() == noMatches);
  if (argc == 4)
    CaptureEditor(editor, std::filesystem::path(argv[3]) / "search-no-matches.bmp");
  click(608, 90);
  click(490, 621);
  assert(save().config()["SlotEffects"][15] == 7); // Third library entry, WAV, recovered after Clear.
  // The settings page is opaque and intercepts clicks; grid/editor controls cannot receive them.
  click(170, 160);
  if (argc == 4)
    CaptureEditor(editor, std::filesystem::path(argv[3]) / "grid.bmp");
  const auto beforeSettings = save().config();
  click(910, 36);
  if (argc == 4)
    CaptureEditor(editor, std::filesystem::path(argv[3]) / "settings.bmp");
  dragAt(713, 700); // NAM output trim underneath; no settings control occupies this point.
  assert(save().config() == beforeSettings);
  click(915, 44);
  click(910, 36);
  // Escape closes settings so the selected module becomes editable again.
  SendMessage(editor, WM_MOUSEMOVE, 0, MAKELPARAM(915, 44));
  SendMessage(editor, WM_KEYDOWN, VK_ESCAPE, static_cast<LPARAM>(MapVirtualKey(VK_ESCAPE, MAPVK_VK_TO_VSC)) << 16);
  pumpFor(100);
  dragAt(105, 700);
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
    click(610, 160);
    CaptureEditor(editor, std::filesystem::path(argv[3]) / "node-colors.bmp");
  }
  // A shorter lane's new conversion delay requires a restart even when max(A,B) is unchanged.
  auto unequal = cleared;
  unequal["CaptureFolder"] = libraryFolder.string();
  unequal["CaptureFolders"] = {libraryFolder.string()};
  unequal["SlotPaths"][4] = libraryFirst.string();
  unequal["SlotPaths"][5] = libraryFirst.string();
  load(unequal);
  assert(plugin->activate(plugin, 44100., 1, 512));
  assert(plugin->start_processing(plugin));
  process.frames_count = 512;
  assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
  host.restart = false;
  auto* latency = static_cast<const clap_plugin_latency*>(plugin->get_extension(plugin, CLAP_EXT_LATENCY));
  assert(latency->get(plugin) == 54);
  click(170, 368);
  click(490, 621); // B1 gets one model (27 samples), while A stays 54 samples.
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
  gui->destroy(plugin);
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
