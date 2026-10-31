// On-demand README gallery: renders an example rig in the real CLAP editor and
// captures each view, then every built-in module's editor panel for the module
// guides (modules/<name>.bmp). Windows writes BMP from the OpenGL surface; macOS writes PNG
// from the Metal layer. See docs/screenshots/README.md.
//   nam-readme-gallery <nam-grid.clap> <repository root> <output directory> [--module <slug>]
#include "ClapTestHost.h"
#ifdef _WIN32
  #include "WinEditorCapture.h"
  #include "WinFileDialogDriver.h"
#else
  #include "IsolatedUserHome.h"
  #include "MacEditorCapture.h"
  #include <dlfcn.h>
#endif
#include "../NeuralAmpModeler/SlotDefaults.h"
#include "../NeuralAmpModeler/ModulePresets.h"
#include "../NeuralAmpModeler/CaptureLibrary.h"
#include "../NeuralAmpModeler/RigPreset.h"
#include "../NeuralAmpModeler/ModuleCatalog.h"
#include "../NeuralAmpModeler/GridLayout.h"
#include <cctype>
#include <random>
#include <thread>

// Synthetic 4x12-style cabinet: decaying noise through RBJ high-pass, low
// resonance, presence peak and two low-pass sections; 24-bit mono at 48 kHz.
static void WriteExampleCabinet(const std::filesystem::path& path)
{
  constexpr double rate = 48000., pi = 3.141592653589793;
  std::vector<double> x(4096);
  std::mt19937 random(7);
  std::normal_distribution<double> noise;
  for (size_t i = 0; i < x.size(); ++i) x[i] = noise(random) * std::exp(-double(i) / (rate * .004));
  x[0] += 4.;
  struct Section { int kind; double frequency, q, gain; };
  for (const auto& s : {Section{0, 75, .7, 0}, Section{2, 115, 1.2, 6}, Section{2, 2600, 1.4, 5}, Section{1, 5200, .8, 0}, Section{1, 7000, .7, 0}})
  {
    const double w = 2 * pi * s.frequency / rate, c = std::cos(w), a = std::sin(w) / (2 * s.q), A = std::pow(10., s.gain / 40.);
    double b0, b1, b2, a0, a1, a2;
    if (s.kind == 0) { b0 = (1 + c) / 2; b1 = -(1 + c); b2 = b0; a0 = 1 + a; a1 = -2 * c; a2 = 1 - a; }
    else if (s.kind == 1) { b0 = (1 - c) / 2; b1 = 1 - c; b2 = b0; a0 = 1 + a; a1 = -2 * c; a2 = 1 - a; }
    else { b0 = 1 + a * A; b1 = -2 * c; b2 = 1 - a * A; a0 = 1 + a / A; a1 = -2 * c; a2 = 1 - a / A; }
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    for (auto& v : x)
    {
      const double y = (b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2) / a0;
      x2 = x1; x1 = v; y2 = y1; y1 = y; v = y;
    }
  }
  double peak = 0; for (double v : x) peak = std::max(peak, std::abs(v));
  std::ofstream out(path, std::ios::binary);
  const auto u32 = [&](uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); };
  const auto u16 = [&](uint16_t v) { out.write(reinterpret_cast<const char*>(&v), 2); };
  const uint32_t bytes = static_cast<uint32_t>(x.size() * 3);
  out.write("RIFF", 4); u32(36 + bytes); out.write("WAVEfmt ", 8); u32(16); u16(1); u16(1);
  u32(48000); u32(48000 * 3); u16(3); u16(24); out.write("data", 4); u32(bytes);
  for (double v : x) { const int32_t s = static_cast<int32_t>(v / peak * .9 * 8388607.); out.write(reinterpret_cast<const char*>(&s), 3); }
}

static std::string ModuleSlug(const char* name)
{
  std::string slug;
  for (const unsigned char c : std::string(name))
    if (std::isalnum(c)) slug += static_cast<char>(std::tolower(c));
    else if (!slug.empty() && slug.back() != '-') slug += '-';
  while (!slug.empty() && slug.back() == '-') slug.pop_back();
  return slug;
}

static int Run(int argc, char** argv)
{
  const char* usage = "Usage: nam-readme-gallery <nam-grid.clap> <repository root> <output directory> [--module <slug>]\n"
                      "macOS regressions: append --preset-save-smoke or --browser-text-smoke instead.\n";
  if (argc == 2 && std::string(argv[1]) == "--help") { std::cout << usage; return 0; }
  const bool browserTextSmoke = argc == 5 && std::string(argv[4]) == "--browser-text-smoke";
  const bool presetSmoke = argc == 5 && std::string(argv[4]) == "--preset-save-smoke";
#ifdef _WIN32
  if (presetSmoke || browserTextSmoke) throw std::runtime_error("Native smoke modes use the macOS editor driver.");
#endif
  if (argc != 4 && !presetSmoke && !browserTextSmoke && !(argc == 6 && std::string(argv[4]) == "--module"))
    throw std::runtime_error(usage);
  const std::string filter = browserTextSmoke ? "browser-text" : presetSmoke ? "bitcrusher" : argc == 6 ? ModuleSlug(argv[5]) : "";
  if (argc == 6)
  {
    bool found = false;
    for (const auto& group : nam_library::kModuleGroups)
      for (const auto effect : group.effects) found |= ModuleSlug(SlotEffectName(effect)) == filter;
    if (!found || filter.empty()) throw std::runtime_error("Unknown module: " + std::string(argv[5]));
  }
  const std::filesystem::path repository = argv[2];
  IsolatedUserHome settingsBackup("nam-grid-gallery"); // Never touches the user's preferences.
  const auto demo = settingsBackup.home / "Demo Library";
  const auto captures = demo / "Captures", irs = demo / "IRs", presets = demo / "Presets";
  {
    // Library folders are per-user settings; the plugin reads them at startup and on every state load.
    nam_library::Settings settings;
    settings.rescanOnStartup = !presetSmoke;
    settings.showVisualizations = true;
    settings.namFolders = {nam_library::PathText(captures)};
    settings.irFolders = {nam_library::PathText(irs)};
    settings.presetFolders = {nam_library::PathText(presets)};
    assert(nam_library::WriteSettings(settingsBackup.file, settings));
  }
  const auto amp1 = captures / (browserTextSmoke
    ? "Very long capture filename with several words and amp settings - bright switch on channel one gain six presence seven bass four middle five treble six cabinet microphone and room details.nam"
    : "Example WaveNet amp.nam"), amp2 = captures / "Example LSTM amp.nam";
  const auto cab = irs / "Example 4x12 cabinet.wav";
  const auto out = std::filesystem::absolute(argv[3]);
  std::filesystem::create_directories(out);
  std::filesystem::create_directories(captures); std::filesystem::create_directories(irs);
  for (const auto& [source, target] : {std::pair{"wavenet.nam", amp1}, std::pair{"lstm.nam", amp2}})
    std::filesystem::copy_file(repository / "NeuralAmpModelerCore" / "example_models" / source, target,
                               std::filesystem::copy_options::overwrite_existing);
  if (browserTextSmoke)
  {
    std::ifstream input(amp1);
    auto model = json::parse(input);
    model["metadata"]["gear_make"] = std::string(600, 'M');
    model["metadata"]["gear_model"] = std::string(600, 'T');
    std::ofstream(amp1) << model.dump();
  }
  WriteExampleCabinet(cab);
  // The bundled default capture and Clean Default rig (resources/defaults) for the main image.
  const auto defaults = repository / "NeuralAmpModeler" / "resources" / "defaults";
  const auto cleanCapture = captures / "riffsmith-clean.nam";
  std::filesystem::copy_file(defaults / "riffsmith-clean.nam", cleanCapture, std::filesystem::copy_options::overwrite_existing);

  const auto path = std::filesystem::absolute(argv[1]);
#ifdef _WIN32
  auto library = LoadLibraryW(path.c_str()); assert(library);
  auto* entry = reinterpret_cast<const clap_plugin_entry*>(GetProcAddress(library, "clap_entry"));
#else
  auto library = dlopen(path.c_str(), RTLD_NOW);
  if (!library) throw std::runtime_error(std::string("Cannot load the plugin: ") + dlerror());
  auto* entry = reinterpret_cast<const clap_plugin_entry*>(dlsym(library, "clap_entry"));
#endif
  assert(entry && entry->init(path.string().c_str()));
  auto* factory = static_cast<const clap_plugin_factory*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
  Host host;
  auto* plugin = factory->create_plugin(factory, &host.host, factory->get_plugin_descriptor(factory, 0)->id);
  assert(plugin && plugin->init(plugin));
  auto* state = static_cast<const clap_plugin_state*>(plugin->get_extension(plugin, CLAP_EXT_STATE));
  Stream saved; clap_ostream output{&saved, Stream::write}; assert(state->save(plugin, &output));
  auto config = saved.config();

  const SlotEffect effects[]{SlotEffect::NoiseGate, SlotEffect::TubeBoost, SlotEffect::NAM, SlotEffect::CabinetIR,
    SlotEffect::JunoChorus, SlotEffect::TapeDelay, SlotEffect::RoomReverb, SlotEffect::NAM,
    SlotEffect::NAM, SlotEffect::CabinetIR, SlotEffect::CosmicEcho, SlotEffect::NAM,
    SlotEffect::ParametricEQ, SlotEffect::Tape, SlotEffect::Limiter, SlotEffect::NAM};
  const char* standard[]{"NAM Input","NAM Output","Enabled","Gate Threshold","Gate Release","Gate Reduction",
    "Comp Threshold","Comp Release","Comp Makeup","EQ Bass","EQ Mid","EQ Treble","EQ Low Cut",
    "EQ High Cut","EQ Presence","IR Output","Full Quality"};
  for (int slot = 0; slot < 16; ++slot)
  {
    const auto prefix = std::string(slot<4 ? "Pre " : slot<8 ? "A " : slot<12 ? "B " : "Post ")+std::to_string(slot%4+1)+" ";
    config["SlotEffects"][slot] = static_cast<int>(effects[slot]); config["SlotPaths"][slot] = ""; config["SlotHashes"][slot] = "";
    const auto defaults = SlotDefaultValues(effects[slot]);
    for (int i = 0; i < 17; ++i) config[prefix+standard[i]] = defaults[i];
    for (int i = 0; i < kUtilityControls; ++i) config[prefix+kEffectParameters[i].name] = defaults[17+i];
  }
  config["SlotPaths"][2] = amp1.string(); config["SlotPaths"][3] = cab.string();
  config["SlotPaths"][8] = amp2.string(); config["SlotPaths"][9] = cab.string();
  config["Pre 3 NAM Input"] = 6.; config["Pre 3 NAM Output"] = 12.; config["B 1 NAM Input"] = 3.; config["B 1 NAM Output"] = 10.;
  config["Pre 4 IR Output"] = -12.; config["B 2 IR Output"] = -18.;
  config["Post 1 Parametric Frequency"] = 2400.; config["Post 1 Parametric Gain"] = 4.; config["Post 1 Parametric Q"] = 1.2;
  config["Post 1 EQ Low Cut"] = 180.; config["Post 1 EQ High Cut"] = 4200.;
  config["Post 1 Parametric Low Q"] = 1.8; config["Post 1 Parametric High Q"] = 2.5;
  config["Post 1 Parametric Shelf Gain"] = 6.;
  config["PresetName"] = "Ambient pedalboard"; config["PresetDirectory"] = presets.string();
  config["Input"] = 0.; config["Output"] = -3.; config["A/B Blend"] = 40.;
  // Matrix rows: source (1 LFO, 2 Envelope, 4 Macro 1), slot, slot control (17 shared + module control), depth.
  const auto modulate = [&](int row, int source, int slot, int control, double depth) {
    const auto name = "Mod " + std::to_string(row) + " ";
    config[name + "Source"] = source; config[name + "Slot"] = slot;
    config[name + "Destination"] = control; config[name + "Depth"] = depth;
  };
  config["LFO Rate"] = .4; config["Macro 1"] = 40.;
  modulate(1, 1, 5, 17 + uDelayMix, 35.);
  modulate(2, 2, 6, 17 + uReverbMix, 30.);
  modulate(3, 4, 2, 1, 15.); // Pre 3 capture output (Trim)

  // Example rig presets for the preset browser, written like Save preset as.
  auto rigOf = [&](json rig, const char* name) {
    for (const char* key : {"PresetDirectory", "Tuner", "Tuner Reference", "Tuner Mute"})
      rig.erase(key);
    rig["PresetName"] = name;
    return json{{"format", "nam-grid-preset"}, {"version", 1}, {"rig", rig}};
  };
  std::filesystem::create_directories(presets);
  for (const auto& old : std::filesystem::directory_iterator(presets))
    if (old.path().extension() == ".ngrid") std::filesystem::remove(old.path());
  for (const char* name : {"Ambient pedalboard", "Clean chorus", "Crunch rhythm", "Dual amp lanes", "Lead with echo"})
  {
    auto rig = config;
    if (std::string(name) == "Clean chorus") rig["A/B Blend"] = 0.;
    if (std::string(name) == "Lead with echo") rig["A 2 Delay Mix"] = 35.;
    std::ofstream(presets / (std::string(name) + ".ngrid")) << rigOf(rig, name).dump(2);
  }

  Stream bytes; bytes.string("###nam-grid###"); bytes.string(config.dump());
  clap_istream input{&bytes, Stream::read}; assert(state->load(plugin, &input));
  auto* gui = static_cast<const clap_plugin_gui*>(plugin->get_extension(plugin, CLAP_EXT_GUI));
#ifdef _WIN32
  assert(gui->create(plugin, CLAP_WINDOW_API_WIN32, false));
  auto parent = CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE, L"STATIC", L"NAM Grid gallery capture", WS_POPUP,
    0, 0, grid_layout::width, grid_layout::height, nullptr, nullptr, GetModuleHandle(nullptr), nullptr); assert(parent);
  clap_window window{}; window.api = CLAP_WINDOW_API_WIN32; window.win32 = parent;
  assert(gui->set_parent(plugin, &window));
  SetWindowPos(parent, HWND_TOP, 0, 0, grid_layout::width, grid_layout::height, SWP_NOACTIVATE|SWP_SHOWWINDOW);
  auto editor = GetWindow(parent, GW_CHILD); assert(editor);
  const auto mouse = [&](int x, int y, bool click) {
    if (!click) { SendMessage(editor, WM_MOUSEMOVE, 0, MAKELPARAM(x, y)); return; }
    SendMessage(editor, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
    SendMessage(editor, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
  };
  const auto capture = [&](const std::string& name, int top) { CaptureEditor(editor, out / (name + ".bmp"), top); };
#else
  auto* editor = OpenMacEditor(plugin, gui, grid_layout::width, grid_layout::height);
  const auto mouse = [&](int x, int y, bool click) { MacEditorMouse(editor, x, y, click); };
  const auto capture = [&](const std::string& name, int top) { CaptureMacEditor(editor, out / (name + ".png"), top); };
  const auto pump = [] { PumpMacEditor(); };
#endif

  assert(plugin->activate(plugin, 48000, 1, 512)); assert(plugin->start_processing(plugin));
  std::array<double,512> inL{}, inR{}, outL{}, outR{};
  double* ins[]{inL.data(), inR.data()}, *outs[]{outL.data(), outR.data()};
  clap_audio_buffer audioIn{nullptr, ins, 2, 0, 0}, audioOut{nullptr, outs, 2, 0, 0};
  clap_input_events events{nullptr, [](const clap_input_events*)->uint32_t{ return 0; },
    [](const clap_input_events*, uint32_t)->const clap_event_header*{ return nullptr; }};
  clap_output_events sent{nullptr, [](const clap_output_events*, const clap_event_header*){ return true; }};
  clap_process process{}; process.frames_count = 512; process.audio_inputs = &audioIn; process.audio_outputs = &audioOut;
  process.audio_inputs_count = process.audio_outputs_count = 1; process.in_events = &events; process.out_events = &sent;
  long long block = 0;
  bool sustained = false; // Steady note for the tuner; otherwise repeated plucks.
  // Process real-time-paced audio while pumping the editor, so meters and plots fill naturally.
  const auto play = [&](int ms) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (std::chrono::steady_clock::now() < until)
    {
      // Behave like a DAW when a module changes latency or prepares new DSP.
      // Ignoring these requests can leave a new editor showing the previous processor's audio.
      if (host.callback) { host.callback = false; plugin->on_main_thread(plugin); }
      if (host.restart)
      {
        host.restart = false;
        plugin->stop_processing(plugin); plugin->deactivate(plugin);
        assert(plugin->activate(plugin, 48000, 1, 512)); assert(plugin->start_processing(plugin));
      }
      for (int i = 0; i < 512; ++i)
      {
        const double t = (block * 512 + i) / 48000.;
        inL[i] = inR[i] = sustained ? .04 * std::sin(2 * 3.141592653589793 * 110.6 * t)
                                    : .09 * (std::sin(t * 691.15) + .35 * std::sin(t * 1382.3)) * std::exp(-std::fmod(t, .8) * 2.);
      }
      plugin->process(plugin, &process); ++block;
      pump(); std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  };
  const auto click = [&](int x, int y) { mouse(x, y, true); play(300); };
  const auto tile = [&](int slot) { return std::pair{grid_layout::cardLeft + grid_layout::columnPitch * (slot % 4) + 94, 64 + 104 * (slot / 4) + 40}; };
  const auto select = [&](int slot) { const auto [x, y] = tile(slot); click(x, y - 26); }; // The ID row only selects.
  // Park the pointer on an empty spot first, so no button shows its hover state.
  const auto shot = [&](const std::string& name, int top = 0) {
    mouse(630, 470, false); play(100);
    capture(name, top);
  };

  play(2500); // Library scan and model preparation

#ifdef __APPLE__
  if (browserTextSmoke)
  {
    select(2); click(grid_layout::moduleBrowseX, grid_layout::moduleButtonY); play(1500);
    click(218, 448); play(2500); // Refresh the generated metadata fixture.
    click(100, 144); // All folders, independent of the selected capture's context folder.
    click(100, 75); MacEditorKey(editor, 0, "a", true);
    for (const char c : std::string("Very long capture")) { const char key[]{c, 0}; MacEditorKey(editor, 0, key); }
    MacEditorKey(editor, 36, "\r"); play(300);
    click(400, 144); play(500);
    shot("browser-long-details");
    plugin->stop_processing(plugin); plugin->deactivate(plugin);
    CloseMacEditor(editor, plugin, gui); plugin->destroy(plugin); entry->deinit(); dlclose(library);
    std::cout << "Captured long browser title, metadata and path into " << out.string() << "\n";
    return 0;
  }
#endif

  if (filter.empty())
  {
    // The NAM and Cabinet IR module-guide panes come from the same frames.
    std::filesystem::create_directories(out / "modules");
    select(2); play(4000); shot("nam"); capture("modules/nam", 476);
    select(3); play(1000); shot("ir"); capture("modules/cabinet-ir", 476);
    select(5); play(3000); shot("delay");
    select(12); play(3000); shot("eq");
    click(grid_layout::modeLeft + 82, grid_layout::optionsY); play(1000); shot("eq-shelf");
    click(grid_layout::modeLeft + 82, grid_layout::optionsY);
    { const auto [x, y] = tile(4); click(x, y); } play(800); shot("browser-modules"); click(794, 448); // Cancel
    click(470, 28); play(1500); shot("browser-presets"); click(853, 688); // Close
    click(919, 28); play(800); shot("settings"); click(936, 28);
    click((grid_layout::modulationLeft + grid_layout::modulationRight) / 2, grid_layout::headerY); play(2000); shot("control"); click(936, 28);
    constexpr int tunerX = (grid_layout::tunerLeft + grid_layout::tunerRight) / 2;
    sustained = true; play(500); click(tunerX, grid_layout::headerY); play(3000); shot("tuner"); click(tunerX, grid_layout::headerY);
    sustained = false;
  }
  // Every built-in module with its defaults in the empty A4 slot, cropped to the editor panel.
  std::filesystem::create_directories(out / "modules");
  for (const auto& group : nam_library::kModuleGroups)
    for (const auto effect : group.effects)
    {
      const auto slug = ModuleSlug(SlotEffectName(effect));
      if (!filter.empty() && slug != filter) continue;
      auto rig = config;
      const auto defaults = SlotDefaultValues(effect);
      rig["SlotEffects"][7] = static_cast<int>(effect);
      for (int i = 0; i < 17; ++i) rig[std::string("A 4 ") + standard[i]] = defaults[i];
      for (int i = 0; i < kUtilityControls; ++i) rig[std::string("A 4 ") + kEffectParameters[i].name] = defaults[17 + i];
      if (presetSmoke) rig[std::string("A 4 ") + kEffectParameters[uCrushBits].name] = 11.;
      Stream moduleBytes; moduleBytes.string("###nam-grid###"); moduleBytes.string(rig.dump());
      clap_istream moduleInput{&moduleBytes, Stream::read}; assert(state->load(plugin, &moduleInput));
      select(7); play(1800);
#ifdef __APPLE__
      if (presetSmoke)
      {
        const auto savedState = [&] {
          Stream bytes; clap_ostream output{&bytes, Stream::write};
          assert(state->save(plugin, &output)); return bytes.config();
        };
        const auto before = savedState();
        const int presetX = grid_layout::moduleBrowseX - 26 - 52;
        const auto beginName = [&] {
          MacEditorSelectMenuItem(editor, presetX, grid_layout::moduleButtonY, "Save current as...");
          play(300);
          assert(!MacEditorHasSheet(editor)); // The old native Save panel fails here.
        };
        const auto type = [&](const char* name) {
          MacEditorKey(editor, 0, "a", true);
          for (const char* p = name; *p; ++p) { const char character[]{*p, 0}; MacEditorKey(editor, 0, character); }
        };
        beginName(); type("Cancelled"); MacEditorKey(editor, 53, "\x1b"); play(100);
        const auto folder = module_presets::UserFolder(presets, effect);
        assert(!std::filesystem::exists(folder / "Cancelled.json") && savedState() == before);
        beginName(); type("Smoke crunch"); shot("preset-name-entry", 476);
        MacEditorKey(editor, 36, "\r"); play(200);
        const auto file = folder / "Smoke crunch.json";
        assert(std::filesystem::is_regular_file(file) && savedState() == before);
        const auto users = module_presets::LoadUser({presets}, effect);
        assert(users.size() == 1 && users[0].name == "Smoke crunch");
        // It is in the very next menu, before any rescan or settings refresh.
        MacEditorSelectMenuItem(editor, presetX, grid_layout::moduleButtonY, "Smoke crunch", 1);
        play(100); assert(savedState() == before);
        shot("preset-name-saved", 476);
        std::cout << "Module preset name-only entry, Escape, module folder and immediate menu recall passed\n";
      }
#endif
      shot("modules/" + slug, 476);
    }
  // The README main image: the bundled Clean Default rig with A1's capture selected.
  if (filter.empty())
  {
    auto clean = rig_preset::Read(defaults / "Clean Default.ngrid");
    for (auto& path : clean["SlotPaths"])
      if (path == "riffsmith-clean.nam") path = cleanCapture.string();
    clean["Tuner"] = 0.; clean["Tuner Reference"] = 440.; clean["Tuner Mute"] = 1.;
    Stream cleanBytes; cleanBytes.string("###nam-grid###"); cleanBytes.string(clean.dump());
    clap_istream cleanInput{&cleanBytes, Stream::read}; assert(state->load(plugin, &cleanInput));
    play(2500); // Model preparation
    select(4); play(4000); shot("grid");
  }
  plugin->stop_processing(plugin); plugin->deactivate(plugin);
#ifdef _WIN32
  gui->destroy(plugin); DestroyWindow(parent); plugin->destroy(plugin); entry->deinit(); FreeLibrary(library);
#else
  CloseMacEditor(editor, plugin, gui); plugin->destroy(plugin); entry->deinit(); dlclose(library);
#endif
  std::cout << "Captured gallery into " << out.string() << "\n";
  return 0;
}

int main(int argc, char** argv)
{
  try { return Run(argc, argv); }
  catch (const std::exception& error)
  {
    std::cerr << "Gallery capture failed: " << error.what() << '\n';
    return 1;
  }
}
