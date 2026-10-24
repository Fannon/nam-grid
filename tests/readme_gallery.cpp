// On-demand README gallery: renders an example rig in the real CLAP editor and
// captures each view. Windows only; see docs/screenshots/README.md.
//   nam-readme-gallery <nam-grid.clap> <repository root> <output directory>
#include "ClapTestHost.h"
#include "WinEditorCapture.h"
#include "WinFileDialogDriver.h"
#include "../NeuralAmpModeler/SlotDefaults.h"
#include "../NeuralAmpModeler/CaptureLibrary.h"
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

int main(int argc, char** argv)
{
  assert(argc == 4);
  const std::filesystem::path repository = argv[2];
  win_file_dialog::IsolatedUserHome settingsBackup("nam-grid-gallery"); // Never touches the user's preferences.
  {
    nam_library::Settings settings;
    settings.rescanOnStartup = settings.showVisualizations = true;
    assert(nam_library::WriteSettings(settingsBackup.file, settings));
  }
  const auto demo = std::filesystem::path("C:/Users/Public/Documents/NAM Grid demo").make_preferred();
  const auto captures = demo / "Captures", irs = demo / "IRs", presets = demo / "Presets";
  const auto amp1 = captures / "Example WaveNet amp.nam", amp2 = captures / "Example LSTM amp.nam";
  const auto cab = irs / "Example 4x12 cabinet.wav";
  const auto out = std::filesystem::absolute(argv[3]);
  std::filesystem::create_directories(out);
  std::filesystem::create_directories(captures); std::filesystem::create_directories(irs);
  for (const auto& [source, target] : {std::pair{"wavenet.nam", amp1}, std::pair{"lstm.nam", amp2}})
    std::filesystem::copy_file(repository / "NeuralAmpModelerCore" / "example_models" / source, target,
                               std::filesystem::copy_options::overwrite_existing);
  WriteExampleCabinet(cab);

  const auto path = std::filesystem::absolute(argv[1]);
  auto library = LoadLibraryW(path.c_str()); assert(library);
  auto* entry = reinterpret_cast<const clap_plugin_entry*>(GetProcAddress(library, "clap_entry"));
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
  const char* standard[]{"In","Trim","Enabled","Gate Threshold","Gate Release","Gate Reduction",
    "Comp Threshold","Comp Release","Comp Makeup","EQ Bass","EQ Mid","EQ Treble","EQ High Pass",
    "EQ Low Pass","EQ Presence","IR Level","Full Quality"};
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
  config["Pre 3 In"] = 6.; config["Pre 3 Trim"] = 12.; config["B 1 In"] = 3.; config["B 1 Trim"] = 10.;
  config["Pre 4 IR Level"] = -12.; config["B 2 IR Level"] = -18.;
  config["Post 1 Parametric Frequency"] = 2400.; config["Post 1 Parametric Gain"] = 4.; config["Post 1 Parametric Q"] = 1.2;
  config["NamFolders"] = {captures.string()};
  config["IrFolders"] = {irs.string()};
  config["PresetFolders"] = {presets.string()};
  config["PresetName"] = "Ambient pedalboard"; config["PresetDirectory"] = presets.string();
  config["Global Input"] = 0.; config["Output"] = -3.; config["Parallel Blend"] = 40.;
  // Matrix rows: source (1 LFO, 2 Envelope, 4 Macro 1), slot, slot control (17 shared + module control), depth.
  const auto modulate = [&](int row, int source, int slot, int control, double depth) {
    const auto name = "Mod " + std::to_string(row) + " ";
    config[name + "Source"] = source; config[name + "Slot"] = slot;
    config[name + "Destination"] = control; config[name + "Depth"] = depth;
  };
  config["LFO Rate"] = .4; config["Macro One"] = 40.;
  modulate(1, 1, 5, 17 + uDelayMix, 35.);
  modulate(2, 2, 6, 17 + uReverbMix, 30.);
  modulate(3, 4, 2, 1, 15.); // Pre 3 capture output (Trim)

  // Example rig presets for the preset browser, written like Save preset as.
  auto rigOf = [&](json rig, const char* name) {
    for (const char* key : {"PresetDirectory", "NamFolders", "IrFolders", "PresetFolders", "Tuner", "Tuner Reference", "Tuner Mute"})
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
    if (std::string(name) == "Clean chorus") rig["Parallel Blend"] = 0.;
    if (std::string(name) == "Lead with echo") rig["A 2 Delay Mix"] = 35.;
    std::ofstream(presets / (std::string(name) + ".ngrid")) << rigOf(rig, name).dump(2);
  }

  Stream bytes; bytes.string("###nam-grid###"); bytes.string(config.dump());
  clap_istream input{&bytes, Stream::read}; assert(state->load(plugin, &input));
  auto* gui = static_cast<const clap_plugin_gui*>(plugin->get_extension(plugin, CLAP_EXT_GUI));
  assert(gui->create(plugin, CLAP_WINDOW_API_WIN32, false));
  auto parent = CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE, L"STATIC", L"NAM Grid gallery capture", WS_POPUP,
    0, 0, 1000, 720, nullptr, nullptr, GetModuleHandle(nullptr), nullptr); assert(parent);
  clap_window window{}; window.api = CLAP_WINDOW_API_WIN32; window.win32 = parent;
  assert(gui->set_parent(plugin, &window));
  SetWindowPos(parent, HWND_TOP, 0, 0, 1000, 720, SWP_NOACTIVATE|SWP_SHOWWINDOW);
  auto editor = GetWindow(parent, GW_CHILD); assert(editor);

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
  const auto click = [&](int x, int y) {
    SendMessage(editor, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
    SendMessage(editor, WM_LBUTTONUP, 0, MAKELPARAM(x, y)); play(300);
  };
  const auto tile = [&](int slot) { return std::pair{88 + 212 * (slot % 4) + 94, 64 + 104 * (slot / 4) + 40}; };
  const auto select = [&](int slot) { const auto [x, y] = tile(slot); click(x, y - 26); }; // The ID row only selects.
  const auto shot = [&](const char* name) { CaptureEditor(editor, out / (std::string(name) + ".bmp")); };

  play(2500); // Library scan and model preparation
  select(10); play(3000); shot("grid");
  select(2); play(4000); shot("nam");
  select(3); play(1000); shot("ir");
  select(5); play(3000); shot("delay");
  select(12); play(3000); shot("eq");
  { const auto [x, y] = tile(4); click(x, y); } play(800); shot("browser-modules"); click(794, 448); // Cancel
  click(470, 28); play(1500); shot("browser-presets"); click(853, 688); // Close
  click(919, 28); play(800); shot("settings"); click(915, 44);
  click(225, 28); play(2000); shot("control"); click(936, 28);
  sustained = true; play(500); click(810, 28); play(3000); shot("tuner"); click(810, 28);
  plugin->stop_processing(plugin); plugin->deactivate(plugin);
  gui->destroy(plugin); DestroyWindow(parent); plugin->destroy(plugin); entry->deinit(); FreeLibrary(library);
  std::cout << "Captured gallery into " << out.string() << "\n";
}
