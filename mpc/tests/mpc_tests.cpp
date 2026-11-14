// NAM Grid MPC plugins: layout checks and plugin behaviour through the VST2 entry point.
// Built and run by `python mpc/build.py --test`. Uses a temporary library folder
// (NAM_GRID_MPC_LIBRARY) and the Amp's assembled plugin folder (MPC_PLUGIN_FOLDER).
#include "../plugins/amp/Amp.cpp"
#include "../plugins/cosmic-echo/CosmicEcho.cpp"
#include "../plugins/delay/Delay.cpp"
#include "../plugins/pedals/Pedals.cpp"
#include "../plugins/lofi/LoFi.cpp"
#include "../plugins/mod/Mod.cpp"
#include "../plugins/console/Console.cpp"
#include "Contract.h"

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <functional>
#include <random>
#include <set>
#include <tuple>
#if defined(__linux__)
  #include <sys/stat.h>
#endif

namespace
{
using namespace mpc_grid;
int gFailures = 0;
std::atomic<bool> gSawLoading{false};
#define CHECK(condition)                                                                   \
  do                                                                                       \
  {                                                                                        \
    if (!(condition))                                                                      \
    {                                                                                      \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);           \
      ++gFailures;                                                                         \
    }                                                                                      \
  } while (0)

VstIntPtr Host(AEffect* effect, VstInt32 opcode, VstInt32, VstIntPtr, void*, float)
{
  if (effect && opcode == audioMasterUpdateDisplay)
  {
    auto& plugin = *static_cast<GridPlugin*>(mpc::detail::self(effect));
    for (int i = 0; i < plugin.numParams(); ++i)
      if (plugin.param(i).id == "amp_info1" && plugin.displayText(i) == "Loading...") gSawLoading.store(true);
  }
  return opcode == audioMasterVersion ? 2400 : 0;
}

struct Instance
{
  AEffect* e = nullptr;
  explicit Instance(const PluginSpec& spec)
  {
    e = CreateEffect(spec, &Host);
    assert(e);
    Call(effOpen);
    Call(effSetSampleRate, 0, 0, nullptr, 44100.f);
    Call(effSetBlockSize, 0, 128);
    Call(effMainsChanged, 0, 1);
  }
  ~Instance()
  {
    Call(effMainsChanged, 0, 0);
    Call(effClose);
  }
  VstIntPtr Call(VstInt32 op, VstInt32 index = 0, VstIntPtr value = 0, void* ptr = nullptr, float opt = 0.f)
  {
    return e->dispatcher(e, op, index, value, ptr, opt);
  }
  int Count() const { return e->numParams; }
  // Parameters by their permanent id.
  int Index(const std::string& id)
  {
    auto& plugin = *static_cast<GridPlugin*>(mpc::detail::self(e));
    for (int i = 0; i < plugin.numParams(); ++i)
      if (plugin.param(i).id == id) return i;
    std::fprintf(stderr, "no parameter %s\n", id.c_str());
    ++gFailures;
    return 0;
  }
  std::string Name(const std::string& id) { return Name(Index(id)); }
  std::string Name(int index)
  {
    char buffer[64]{};
    Call(effGetParamName, index, 0, buffer);
    return buffer;
  }
  std::string Display(const std::string& id)
  {
    char buffer[64]{};
    Call(effGetParamDisplay, Index(id), 0, buffer);
    return buffer;
  }
  void Set(const std::string& id, float value) { e->setParameter(e, Index(id), value); }
  float Get(const std::string& id) { return e->getParameter(e, Index(id)); }
  // Selects a menu entry by its screen name.
  void Choose(const std::string& id, const std::string& name)
  {
    auto& plugin = *static_cast<GridPlugin*>(mpc::detail::self(e));
    const auto& p = plugin.param(Index(id));
    for (size_t i = 0; i < p.choices.size(); ++i)
      if (p.choices[i] == name)
      {
        Set(id, p.toNormalised(static_cast<float>(i)));
        return;
      }
    std::fprintf(stderr, "no entry %s in %s\n", name.c_str(), id.c_str());
    ++gFailures;
  }
  // The knob or option whose current name is `name`, or "".
  std::string Find(const std::string& section, const std::string& name)
  {
    for (const char* kind : {"k", "o"})
      for (int n = 1; n <= kKnobs; ++n)
      {
        const auto id = section + "_" + kind + std::to_string(n);
        auto& plugin = *static_cast<GridPlugin*>(mpc::detail::self(e));
        bool exists = false;
        for (int i = 0; i < plugin.numParams(); ++i) exists = exists || plugin.param(i).id == id;
        if (exists && Name(id) == name) return id;
      }
    return "";
  }
  std::vector<uint8_t> Chunk()
  {
    void* data = nullptr;
    const auto size = Call(effGetChunk, 0, 0, &data);
    const auto* bytes = static_cast<const uint8_t*>(data);
    return {bytes, bytes + size};
  }
  bool Load(std::vector<uint8_t> chunk) { return Call(effSetChunk, 0, static_cast<VstIntPtr>(chunk.size()), chunk.data()) == 1; }
  void Process(std::vector<float>& left, std::vector<float>& right, int block = 128)
  {
    for (size_t start = 0; start < left.size(); start += block)
    {
      const int n = static_cast<int>(std::min<size_t>(block, left.size() - start));
      float* channels[2]{left.data() + start, right.data() + start};
      e->processReplacing(e, channels, channels, n);
    }
  }
};

// The extra state (after the parameter section) as JSON, and a chunk with edited extra state.
size_t ExtraOffset(const std::vector<uint8_t>& chunk)
{
  auto u32 = [&](size_t at) { uint32_t v = 0; for (int i = 0; i < 4; ++i) v |= uint32_t(chunk[at + i]) << (8 * i); return v; };
  size_t pos = 12;
  const uint32_t n = u32(8);
  for (uint32_t k = 0; k < n; ++k) pos += 1 + chunk[pos] + 8;
  return pos;
}
nlohmann::json Extra(const std::vector<uint8_t>& chunk)
{
  const size_t pos = ExtraOffset(chunk) + 4;
  return nlohmann::json::parse(chunk.begin() + pos, chunk.end());
}
std::vector<uint8_t> WithExtra(std::vector<uint8_t> chunk, const nlohmann::json& extra)
{
  chunk.resize(ExtraOffset(chunk));
  const auto text = extra.dump();
  for (int i = 0; i < 4; ++i) chunk.push_back(static_cast<uint8_t>(text.size() >> (8 * i)));
  chunk.insert(chunk.end(), text.begin(), text.end());
  return chunk;
}

bool WaitFor(const std::function<bool()>& condition, double seconds = 5.)
{
  const auto end = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
  while (std::chrono::steady_clock::now() < end)
  {
    if (condition()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return condition();
}
void Settle() { WaitFor([] { return false; }, .25); }

// mkdir -p without std::filesystem, which cannot stat on some hosts in 32-bit builds.
bool MakeFolders(const std::string& path)
{
#if defined(__linux__)
  for (size_t i = 1; i <= path.size(); ++i)
    if (i == path.size() || path[i] == '/')
      if (::mkdir(path.substr(0, i).c_str(), 0755) != 0 && errno != EEXIST) return false;
  return true;
#else
  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::u8path(path), ec);
  return !ec;
#endif
}
void WriteFile(const std::string& path, const std::string& bytes)
{
  MakeFolders(FolderOf(path));
  std::ofstream(std::filesystem::u8path(path), std::ios::binary) << bytes;
}
std::string Wav(int frames)
{
  std::string out;
  auto u32 = [&](uint32_t v) { out.append(reinterpret_cast<const char*>(&v), 4); };
  auto u16 = [&](uint16_t v) { out.append(reinterpret_cast<const char*>(&v), 2); };
  out += "RIFF"; u32(36 + frames * 2); out += "WAVEfmt "; u32(16); u16(1); u16(1); u32(48000); u32(96000);
  u16(2); u16(16); out += "data"; u32(frames * 2);
  for (int i = 0; i < frames; ++i) u16(static_cast<uint16_t>(static_cast<int16_t>(i == 0 ? 16000 : 8000. * std::exp(-i / 200.))));
  return out;
}

const std::vector<const PluginSpec*>& Specs()
{
  static const std::vector<const PluginSpec*> specs{&mpc_plugins::AmpSpec(), &mpc_plugins::PedalsSpec(),
                                                    &mpc_plugins::DelaySpec(), &mpc_plugins::CosmicEchoSpec(),
                                                    &mpc_plugins::LoFiSpec(), &mpc_plugins::ModSpec(),
                                                    &mpc_plugins::ConsoleSpec()};
  return specs;
}

// ----- layout ------------------------------------------------------------------------
void TestLayouts()
{
  std::set<uint32_t> uids;
  for (const auto* spec : Specs())
  {
    CHECK(uids.insert(spec->uid).second);
    std::set<std::string> sectionIds;
    for (const auto& section : spec->sections)
    {
      CHECK(sectionIds.insert(section.id).second);
      CHECK(section.defaultChoice >= 0 && section.defaultChoice < static_cast<int>(section.choices.size()));
      for (const auto& choice : section.choices)
        for (bool sync : {false, true})
        {
          auto values = ChoiceDefaults(choice);
          values[standardControls + uTempoSync] = sync ? 1. : 0.;
          const auto layout = MakeLayout(choice, values, true);
          std::set<int> mapped;
          for (int c : layout.knobs)
            if (c >= 0) CHECK(mapped.insert(c).second);
          for (int c : layout.options)
            if (c >= 0) CHECK(mapped.insert(c).second);
          // Every control the desktop editor shows for the module is reachable on the MPC.
          if (choice.effect != SlotEffect::NAM)
            for (int p = 0; p < kUtilityControls; ++p)
              if (effect_presentation::Visible(choice.effect, p, sync) && !effect_presentation::DisplayControl(p)
                  && standardControls + p != choice.styleControl && !mapped.count(standardControls + p))
              {
                std::fprintf(stderr, "%s / %s: %s is not mapped\n", spec->name, choice.name, kEffectParameters[p].name);
                ++gFailures;
              }
          for (int c : mapped)
          {
            CHECK(!ControlLabel(choice, c, values).empty());
            const double v = values[c];
            CHECK(std::abs(FromNormalized(c, ToNormalized(c, v)) - v) <= 1.e-6 * (1. + std::abs(v)));
          }
          if (choice.styleControl >= 0) CHECK(std::lround(values[choice.styleControl]) == choice.style);
        }
    }
  }
  // The plugin-wide Output is only offered where no module has its own: never twice.
  for (const auto* spec : Specs())
  {
    bool own = false;
    for (const auto& section : spec->sections)
      for (const auto& choice : section.choices)
      {
        const auto values = ChoiceDefaults(choice);
        for (int c : MakeLayout(choice, values, true).knobs)
          own = own || (c >= 0 && (ControlLabel(choice, c, values) == "Output" || ControlLabel(choice, c, values) == "Level"));
      }
    const bool expected = std::string_view(spec->name) == "NG Cosmic" || std::string_view(spec->name) == "NG Mod";
    CHECK(own != expected);
  }
  // Curves match the desktop host parameters.
  const int time = standardControls + uCosmicTime; // power 2
  CHECK(std::abs(FromNormalized(time, .5) - (20. + .25 * (8000. - 20.))) < 1.e-9);
  const int cut = standardControls + uCosmicTone; // exponential
  CHECK(std::abs(FromNormalized(cut, .5) - std::sqrt(200. * 16000.)) < 1.e-6);
  CHECK(FormatValue(standardControls + uNoiseGateThreshold, -100.) == "Off");
  CHECK(FormatValue(cut, 6000.) == "6.00 kHz");
  CHECK(FormatValue(parameter_layout::kSlotEQHighPass, 0.) == "Off");
  CHECK(FormatValue(parameter_layout::kSlotEQLowPass, 20000.) == "Off");
  // Library folders group files and keep each folder together.
  std::vector<LibraryEntry> entries{{"user", "A/a1.nam", "", "a1"}, {"user", "A/a2.nam", "", "a2"},
                                    {"user", "A/B/b.nam", "", "b"}, {"factory", "f.nam", "", "f"}};
  const auto folders = LibraryFolders(entries);
  CHECK(folders.size() == 3 && folders[0].display == "A" && folders[0].count == 2 && folders[2].display == "Factory");
  CHECK(FolderDisplay("user", "Packs/Some Pack/Amps") == "Some Pack/Amps");
}

// Existing arrays are prefixes: new parameters and choices may append. Formerly unused
// knob/option places may acquire controls without changing the meaning of occupied places.
bool ContractContains(const nlohmann::json& actual, const nlohmann::json& expected)
{
  if (expected.is_object())
  {
    if (!actual.is_object()) return false;
    for (auto it = expected.begin(); it != expected.end(); ++it)
      if (!actual.contains(it.key()) || !ContractContains(actual[it.key()], it.value())) return false;
    return true;
  }
  if (expected.is_array())
  {
    if (!actual.is_array() || actual.size() < expected.size()) return false;
    for (size_t i = 0; i < expected.size(); ++i)
      if (!ContractContains(actual[i], expected[i])) return false;
    return true;
  }
  if (expected.is_string() && expected.get<std::string>().empty()) return actual.is_string();
  return actual == expected;
}

std::vector<uint8_t> LegacyChunk(const nlohmann::json& state)
{
  std::vector<uint8_t> chunk{'M', 'P', 'C', 'P'};
  const auto u32 = [&](uint32_t value) { for (int i = 0; i < 4; ++i) chunk.push_back(static_cast<uint8_t>(value >> (8 * i))); };
  const auto f32 = [&](float value) { uint32_t bits; std::memcpy(&bits, &value, 4); u32(bits); };
  u32(2);
  u32(static_cast<uint32_t>(state["parameters"].size()));
  for (const auto& p : state["parameters"])
  {
    const auto id = p["id"].get<std::string>();
    chunk.push_back(static_cast<uint8_t>(id.size()));
    chunk.insert(chunk.end(), id.begin(), id.end());
    f32(p["plain"].get<float>());
    f32(p["normalized"].get<float>());
  }
  const auto extra = state["extra"].dump();
  u32(static_cast<uint32_t>(extra.size()));
  chunk.insert(chunk.end(), extra.begin(), extra.end());
  return chunk;
}

void TestCompatibility(const std::string& fixtures)
{
  std::ifstream input(std::filesystem::u8path(fixtures + "/contract-v1.json"));
  nlohmann::json baseline;
  if (!(input >> baseline)) { CHECK(false); return; }
  for (const auto* spec : Specs())
  {
    Instance instance(*spec);
    auto& plugin = *static_cast<GridPlugin*>(mpc::detail::self(instance.e));
    const auto& frozen = baseline.at(spec->stateFormat);
    CHECK(ContractContains(DescribeContract(plugin), frozen["contract"]));
    CHECK(instance.Load(LegacyChunk(frozen["state"])));
    auto expectedState = frozen["state"]["extra"];
    auto actualState = Extra(instance.Chunk());
    CHECK(actualState["schema"] >= expectedState["schema"]);
    expectedState.erase("schema"); // New saves may use an additive schema while old values retain their meaning.
    CHECK(ContractContains(actualState, expectedState));
    if (std::string(spec->stateFormat) == "nam-grid-amp")
    {
      std::ifstream displayInput(std::filesystem::u8path(fixtures + "/display-parameters-v1.json"));
      nlohmann::json display;
      displayInput >> display;
      const int first = display["firstIndex"].get<int>();
      CHECK(plugin.numParams() >= first + static_cast<int>(display["ids"].size()));
      for (size_t i = 0; i < display["ids"].size() && first + static_cast<int>(i) < plugin.numParams(); ++i)
      {
        const auto& p = plugin.param(first + static_cast<int>(i));
        CHECK(p.id == display["ids"][i].get<std::string>() && !p.automatable);
        CHECK(p.kind == mpc::Param::Kind::Float && p.min == 0.f && p.max == 1.f && p.def == 0.f);
      }
    }
    // An appended module cannot change the existing option assignments.
    for (const auto& section : spec->sections)
      for (const auto& choice : section.choices)
      {
        auto expanded = section;
        expanded.choices.insert(expanded.choices.end(), 20, choice);
        CHECK(SectionOptionOrder(expanded) == SectionOptionOrder(section));
      }
  }
  // Ensure the comparator itself catches parameter remapping and reordered option meaning.
  auto changed = baseline.begin().value()["contract"];
  changed["parameters"][0]["id"] = "wrong-id";
  CHECK(!ContractContains(changed, baseline.begin().value()["contract"]));
}

void TestLibraryDetails(const std::string& library)
{
  {
    Instance check(mpc_plugins::AmpSpec());
    auto& plugin = *static_cast<GridPlugin*>(mpc::detail::self(check.e));
    bool found = false;
    for (int i = 0; i < plugin.numParams(); ++i) found = found || plugin.param(i).id == "amp_info1";
    CHECK(found);
    if (!found) return;
  }
  std::string capture;
  CHECK(ReadWholeFile(library + "/Captures/A/a1.nam", capture));
  // Long names must survive the SDK's 31-byte display limit, including UTF-8 boundaries.
  std::string name(5, 'W');
  for (int i = 0; i < 80; ++i) name += "\xc3\xa4";
  name += " guitar capture.nam";
  WriteFile(library + "/Captures/A/" + name, capture);
  auto metadataFree = nlohmann::json::parse(capture);
  metadataFree["metadata"].erase("loudness");
  WriteFile(library + "/Captures/A/no-level.nam", metadataFree.dump());
  Instance a(mpc_plugins::AmpSpec());
  auto chunk = a.Chunk();
  auto extra = Extra(chunk);
  extra["sections"]["amp"]["file"] = {{"root", "user"}, {"path", "A/" + name}};
  gSawLoading.store(false);
  CHECK(a.Load(WithExtra(chunk, extra)));
  CHECK(WaitFor([&] { return a.Display("amp_info1") == "Ready"; }));
  CHECK(gSawLoading.load());
  std::string fullName;
  for (int line = 3; line <= 12; ++line) fullName += a.Display("amp_info" + std::to_string(line));
  CHECK(fullName == name);
  CHECK(a.Display("amp_info13") == "A");
  CHECK(a.Display("normalize_info").find("to -18 dB") != std::string::npos);
  a.Set("normalize", 0.f);
  CHECK(a.Display("normalize_info") == "Off");
  extra["sections"]["amp"]["file"]["path"] = "A/no-level.nam";
  CHECK(a.Load(WithExtra(chunk, extra)));
  CHECK(WaitFor([&] { return a.Display("amp_info1") == "Ready"; }));
  CHECK(a.Display("normalize_info") == "On: no loudness metadata");
  extra["sections"]["amp"]["file"]["path"] = "Broken/broken.nam";
  CHECK(a.Load(WithExtra(chunk, extra)));
  CHECK(WaitFor([&] { return a.Display("amp_info1") == "Load failed - dry"; }));
  CHECK(a.Display("amp_info2") == "Invalid capture JSON");
  CHECK(a.Display("amp_info3") == "broken.nam");
  CHECK(a.Display("normalize_info") == "Unavailable: capture not loaded");
  WriteFile(library + "/Captures/Broken/broken.nam", capture);
  a.Set("rescan", 1.f);
  CHECK(WaitFor([&] { return a.Display("amp_info1") == "Ready"; }));
  CHECK(a.Display("amp_info2").empty());
  auto& plugin = *static_cast<GridPlugin*>(mpc::detail::self(a.e));
  for (int i = 0; i < plugin.numParams(); ++i)
    if (plugin.param(i).id.find("_info") != std::string::npos) CHECK(!plugin.param(i).automatable);
}

// ----- NAM Grid Amp --------------------------------------------------------------------
void TestAmpGates()
{
  Instance a(mpc_plugins::AmpSpec());
  CHECK(a.Name("gate_type") == "Gate");
  CHECK(a.Display("gate_type") == "Off"); // No gate until one is chosen.
  CHECK(a.Name("gate_k1").empty());
  a.Choose("gate_type", "AI Gate");
  CHECK(a.Name("gate_k1") == "Reduction" && a.Name("gate_k2") == "Sensitivity");
  CHECK(a.Name("gate_k5") == "Gate threshold" && a.Name("gate_k6") == "Gate release");
  CHECK(a.Name("gate_k8").empty()); // AI Gate leaves places empty: hidden by the skin.
  // The standard gate shows the slot's shared gate controls first.
  a.Choose("gate_type", "Noise Gate");
  CHECK(a.Name("gate_k1") == "Threshold" && a.Display("gate_k1") == "-55.0 dB");
  CHECK(a.Name("gate_o1") == "Mode");
  a.Choose("gate_type", "Off");
  CHECK(a.Name("gate_k1").empty());

  // Smart Gate: learn, try another gate, come back: the profile is kept and saved.
  a.Choose("gate_type", "Smart Gate");
  // Options keep fixed places across gates: Noise Gate's Mode first, then Learn and Tracking.
  CHECK(a.Name("gate_o1").empty() && a.Name("gate_o2") == "Learn" && a.Name("gate_o3") == "Tracking");
  CHECK(a.Name("gate_k1") == "Sensitivity" && a.Name("gate_k5") == "Gate threshold" && a.Name("gate_k8") == "Mix");
  Settle();
  std::vector<float> l(44100 * 2), r(44100 * 2);
  std::mt19937 rng(3);
  std::normal_distribution<float> noise(0.f, .01f);
  for (size_t i = 0; i < l.size(); ++i) l[i] = r[i] = noise(rng);
  a.Set("gate_o2", 1.f);
  a.Process(l, r);
  a.Set("gate_o2", 0.f);
  std::vector<float> l2(4096), r2(4096);
  a.Process(l2, r2);
  const auto learned = Extra(a.Chunk())["sections"]["gate"];
  CHECK(learned.contains("noise"));
  CHECK(learned["values"]["Noise Learn"].get<double>() == 0.);
  a.Choose("gate_type", "AI Gate");
  Settle();
  a.Choose("gate_type", "Smart Gate");
  Settle();
  CHECK(Extra(a.Chunk())["sections"]["gate"]["noise"] == learned["noise"]);
  // And it survives a project reload.
  Instance b(mpc_plugins::AmpSpec());
  CHECK(b.Load(a.Chunk()));
  CHECK(b.Display("gate_type") == "Smart Gate");
  CHECK(Extra(b.Chunk())["sections"]["gate"]["noise"] == learned["noise"]);
  // A second project loaded into a used instance must replace the old audio snapshot too.
  a.Process(l2, r2);
  auto replacement = Extra(a.Chunk());
  replacement["sections"]["gate"]["noise"] = std::vector<double>(noise_print::Profile{}.power.size(), .0001);
  CHECK(a.Load(WithExtra(a.Chunk(), replacement)));
  a.Process(l2, r2);
  Settle();
  CHECK(Extra(a.Chunk())["sections"]["gate"]["noise"] == replacement["sections"]["gate"]["noise"]);
}

void TestAmpParameterState()
{
  Instance a(mpc_plugins::AmpSpec());
  a.Set("amp_file", 1.f); // Save immediately, before the worker resolves the File edit.
  const auto chunk = a.Chunk();
  CHECK(Extra(chunk)["sections"]["amp"]["file"]["path"] == "A/a2.nam");
  CHECK(WaitFor([&] { return a.Display("amp_file") == "a2"; }));
  auto parameters = a.Chunk();
  parameters.resize(ExtraOffset(parameters));
  parameters.insert(parameters.end(), 4, 0);
  Instance b(mpc_plugins::AmpSpec());
  CHECK(b.Load(parameters));
  CHECK(WaitFor([&] { return b.Display("amp_file") == "a2"; }));
  // The latest turn wins: changing Folder after File starts at the new folder's first file.
  a.Set("amp_folder", .5f);
  a.Set("amp_file", 1.f);
  a.Set("amp_folder", 0.f);
  CHECK(Extra(a.Chunk())["sections"]["amp"]["file"]["path"] == "A/a1.nam");
}

void TestAmpLibrary(const std::string& library)
{
  // Captures: A/a1, A/a2, B/b1 (user), then riffsmith-clean (factory). IRs: cab, Room/room.
  Instance a(mpc_plugins::AmpSpec());
  CHECK(a.Name("amp_folder") == "Folder" && a.Name("amp_file") == "Capture");
  CHECK(a.Display("amp_folder") == "A" && a.Display("amp_file") == "a1");
  CHECK(a.Name("amp_k1") == "Input" && a.Name("amp_k5") == "Output");
  CHECK(a.Name("cab_k1") == "Input" && a.Name("cab_k2") == "Output"); // One column, as on the AMP page.
  a.Set("amp_file", 1.f); // last file in A
  CHECK(WaitFor([&] { return a.Display("amp_file") == "a2"; }));
  a.Set("amp_folder", .5f); // B starts at its first file
  CHECK(WaitFor([&] { return a.Display("amp_file") == "b1"; }));
  CHECK(a.Display("amp_folder") == "B");
  a.Set("amp_folder", 1.f);
  CHECK(WaitFor([&] { return a.Display("amp_folder") == "Factory" && a.Display("amp_file") == "riffsmith-clean"; }));
  a.Set("amp_folder", .5f);
  CHECK(WaitFor([&] { return a.Display("amp_file") == "b1"; }));
  auto extra = Extra(a.Chunk());
  CHECK(extra["sections"]["amp"]["file"]["root"] == "user" && extra["sections"]["amp"]["file"]["path"] == "B/b1.nam");
  // New files never change which capture is used.
  std::string capture;
  CHECK(ReadWholeFile(library + "/Captures/A/a1.nam", capture));
  WriteFile(library + "/Captures/A/a0 first.nam", capture);
  WriteFile(library + "/Captures/AA/x.nam", capture);
  a.Set("rescan", 1.f);
  CHECK(WaitFor([&] { return a.Get("rescan") < .5f; }));
  CHECK(a.Display("amp_file") == "b1" && a.Display("amp_folder") == "B");
  CHECK(std::abs(a.Get("amp_folder") - 2.f / 3.f) < 1.e-6); // A, AA, B, Factory
  // Cabinet IRs, switched off by default (many captures include a cabinet).
  CHECK(a.Name("cab_file") == "IR" && a.Get("cab_on") < .5f);
  a.Set("cab_on", 1.f);
  CHECK(WaitFor([&] { return a.Display("cab_file") == "cab"; }));
  CHECK(a.Display("cab_folder") == "Main");
  // A missing file says so, keeps its reference and plays dry.
  extra = Extra(a.Chunk());
  extra["sections"]["amp"]["file"]["path"] = "Gone/missing.nam";
  Instance b(mpc_plugins::AmpSpec());
  CHECK(b.Load(WithExtra(a.Chunk(), extra)));
  CHECK(WaitFor([&] { return b.Display("amp_file") == "Missing: missing"; }));
  CHECK(b.Display("amp_folder") == "Gone");
  CHECK(Extra(b.Chunk())["sections"]["amp"]["file"]["path"] == "Gone/missing.nam");
  // Corrupt captures fail to load without crashing.
  WriteFile(library + "/Captures/Broken/broken.nam", "{ not json");
  b.Set("rescan", 1.f);
  CHECK(WaitFor([&] { return b.Get("rescan") < .5f; }));
  b.Set("amp_folder", 3.f / 4.f); // A, AA, B, Broken, Factory
  CHECK(WaitFor([&] { return b.Display("amp_file") == "Load failed"; }));
  std::vector<float> l(4096, .1f), r(4096, .1f);
  b.Process(l, r);
  CHECK(std::isfinite(l.back()));
}

void TestAmpState()
{
  std::vector<uint8_t> chunk;
  {
    Instance a(mpc_plugins::AmpSpec());
    a.Choose("gate_type", "Noise Gate");
    a.Set("gate_k1", .3f);
    a.Set("amp_k2", .8f); // Bass
    a.Set("cab_on", 1.f);
    a.Set("out", .25f);
    WaitFor([&] { return a.Display("amp_file") == "a1"; });
    chunk = a.Chunk();
  }
  Instance b(mpc_plugins::AmpSpec());
  CHECK(b.Load(chunk));
  CHECK(Extra(b.Chunk()) == Extra(chunk));
  CHECK(b.Display("gate_type") == "Noise Gate");
  CHECK(std::abs(b.Get("gate_k1") - .3f) < 1.e-5f && std::abs(b.Get("amp_k2") - .8f) < 1.e-5f);
  CHECK(b.Get("cab_on") > .5f && std::abs(b.Get("out") - .25f) < 1.e-6f);
  // Unknown modules (from newer versions) fall back; out-of-range values are clamped.
  auto j = Extra(chunk);
  j["sections"]["gate"]["module"] = "future-gate";
  j["sections"]["amp"]["values"]["EQ Bass"] = 1.e9;
  Instance c(mpc_plugins::AmpSpec());
  CHECK(c.Load(WithExtra(chunk, j)));
  CHECK(c.Display("gate_type") == "Off");
  CHECK(Extra(c.Chunk())["sections"]["amp"]["values"]["EQ Bass"].get<double>() == 12.);
}

void TestStateValidation()
{
  Instance source(mpc_plugins::DelaySpec());
  source.Choose("delay_type", "Tape");
  source.Set("delay_k1", .82f);
  const auto sourceChunk = source.Chunk();
  Instance target(mpc_plugins::DelaySpec());
  target.Set("delay_k1", .27f);
  const auto original = target.Chunk();
  const auto reject = [&](const std::vector<uint8_t>& chunk) {
    CHECK(!target.Load(chunk));
    CHECK(target.Chunk() == original); // Rejection must leave parameters and named state intact.
  };
  reject({});
  reject(std::vector<uint8_t>(sourceChunk.begin(), sourceChunk.begin() + 20)); // Truncated after header.
  auto invalid = Extra(sourceChunk);
  invalid["format"] = 17;
  try { reject(WithExtra(sourceChunk, invalid)); }
  catch (...) { CHECK(false); } // No exception may escape the VST dispatcher.
  invalid = Extra(sourceChunk);
  invalid["schema"] = kStateSchema + 1;
  reject(WithExtra(sourceChunk, invalid));
  invalid = Extra(sourceChunk);
  invalid["format"] = "nam-grid-mod";
  reject(WithExtra(sourceChunk, invalid));
  invalid = Extra(sourceChunk);
  invalid["sections"]["delay"]["file"] = {{"root", 42}, {"path", "test.nam"}};
  try { reject(WithExtra(sourceChunk, invalid)); }
  catch (...) { CHECK(false); }

  // A valid parameter-only chunk keeps the incoming knobs and the tempo-dependent layout.
  source.Set("delay_o1", 1.f);
  source.Set("delay_k1", .8f);
  source.Set("delay_k2", .73f);
  const auto savedDivision = source.Display("delay_k1");
  auto parameters = source.Chunk();
  parameters.resize(ExtraOffset(parameters));
  parameters.insert(parameters.end(), 4, 0); // No plugin-specific extra state.
  CHECK(target.Load(parameters));
  CHECK(target.Display("delay_type") == "Tape");
  CHECK(target.Name("delay_k1") == "Division" && target.Display("delay_k1") == savedDivision);
  CHECK(std::abs(target.Get("delay_k2") - .73f) < 1.e-6f);

  // Loading another project clears remembered module settings from the old project.
  Instance reused(mpc_plugins::DelaySpec());
  reused.Choose("delay_type", "Digital");
  reused.Set("delay_k2", .95f);
  reused.Choose("delay_type", "Analog");
  Instance fresh(mpc_plugins::DelaySpec());
  CHECK(reused.Load(fresh.Chunk()));
  reused.Choose("delay_type", "Digital");
  fresh.Choose("delay_type", "Digital");
  CHECK(reused.Display("delay_k2") == fresh.Display("delay_k2"));
}

// NG Amp: other drives, Lite by default, mono processing, DC blocker and Normalize.
void SetEnv(const char* name, const std::string& value)
{
#if defined(_WIN32)
  _putenv_s(name, value.c_str());
#else
  if (value.empty()) unsetenv(name); else setenv(name, value.c_str(), 1);
#endif
}
double Rms(const std::vector<float>& x, size_t from)
{
  double sum = 0.;
  for (size_t i = from; i < x.size(); ++i) sum += double(x[i]) * x[i];
  return std::sqrt(sum / double(x.size() - from));
}
void TestAmpDrivesAndLevels(const std::string& library)
{
  // A "NAM Grid" library on another drive appears after the plugin drive's folders.
  const auto drive = FolderOf(FolderOf(library)) + "/MPC_SD";
  std::string capture;
  CHECK(ReadWholeFile(library + "/Captures/A/a1.nam", capture));
  WriteFile(drive + "/NAM Grid/Captures/Amps/sd1.nam", capture);
  SetEnv("NAM_GRID_MPC_DRIVES", drive);
  {
    Instance a(mpc_plugins::AmpSpec());
    a.Set("rescan", 1.f);
    CHECK(WaitFor([&] { return a.Get("rescan") < .5f; }));
    // Folders: user A, AA, B, Broken, then MPC_SD: Amps, then Factory.
    a.Set("amp_folder", 4.f / 5.f);
    CHECK(WaitFor([&] { return a.Display("amp_file") == "sd1"; }));
    CHECK(a.Display("amp_folder") == "MPC_SD: Amps");
    const auto file = Extra(a.Chunk())["sections"]["amp"]["file"];
    CHECK(file["root"] == "drive:MPC_SD" && file["path"] == "Amps/sd1.nam");
  }
  SetEnv("NAM_GRID_MPC_DRIVES", "");

  // The factory capture: slimmable (Quality shows, starting at Lite) with loudness metadata.
  // folder: the Folder value; signal(i) -> (left, right).
  const auto run = [&](float folder, bool normalize, const std::function<std::pair<float, float>(size_t)>& signal) {
    Instance a(mpc_plugins::AmpSpec());
    a.Set("amp_folder", folder);
    WaitFor([&] { return a.Display("amp_file") == "riffsmith-clean" || a.Display("amp_file") == "Load failed"; });
    a.Set("normalize", normalize ? 1.f : 0.f);
    Settle();
    std::vector<float> l(44100 * 2), r(44100 * 2);
    for (size_t i = 0; i < l.size(); ++i) std::tie(l[i], r[i]) = signal(i);
    a.Process(l, r);
    return std::make_tuple(l, r, a.Name("amp_o1"), a.Display("amp_o1"));
  };
  const auto tone = [](size_t i) { const float x = static_cast<float>(.2 * std::sin(.03 * double(i))); return std::make_pair(x, x); };
  // Folders: A, AA, B, Broken (a capture that fails to load), Factory.
  const auto [plain, plainR, quality, qualityValue] = run(1.f, false, tone);
  const auto [norm, normR, q2, q2Value] = run(1.f, true, tone);
  CHECK(quality == "Quality" && qualityValue == "Lite");
  // Normalize lifts this capture (loudness -19.1 dB) to -18 dB: exactly the level difference.
  const double gain = 20. * std::log10(Rms(norm, 44100) / Rms(plain, 44100));
  CHECK(std::abs(gain - (kNormalizedLoudness - -19.137987390292942)) < .01);
  // The DC blocker removes the capture's small offset: silence in, silence out.
  const auto [idle, idleR, q3, q3Value] = run(1.f, false, [](size_t) { return std::make_pair(0.f, 0.f); });
  double mean = 0.;
  for (size_t i = 44100; i < idle.size(); ++i) mean += idle[i];
  mean /= double(idle.size() - 44100);
  CHECK(std::abs(mean) < 1.e-6);
  // Mono: with no capture playing, a stereo input still leaves as one channel on both sides.
  const auto [dryL, dryR, q4, q4Value] = run(.75f, false, [](size_t i) {
    return std::make_pair(static_cast<float>(.2 * std::sin(.03 * double(i))), static_cast<float>(.2 * std::sin(.05 * double(i))));
  });
  CHECK(dryL == dryR && Rms(dryL, 44100) > .01);
}

// ----- NAM Grid Pedals ------------------------------------------------------------------
void TestPedals()
{
  Instance a(mpc_plugins::PedalsSpec());
  CHECK(a.Name("drive_type") == "Voice" && a.Display("drive_type") == "Screamer");
  CHECK(a.Get("comp_on") < .5f);
  CHECK(a.Name("comp_k1") == "Sustain" && a.Name("comp_k5") == "Level");
  // Every voice has its gain and output in the first Q-Link column.
  for (const auto& [voice, gain, output] : std::vector<std::tuple<std::string, std::string, std::string>>{
         {"Clean Boost", "Gain", "Tilt"}, {"Tube Boost", "Drive", "Output"}, {"Screamer", "Drive", "Output"},
         {"Vintage Fuzz", "Drive", "Output"}, {"Metal Stack", "Gain", "Level"}})
  {
    a.Choose("drive_type", voice);
    CHECK(a.Name("drive_k1") == gain && a.Name("drive_k2") == output);
  }
  a.Choose("drive_type", "Screamer");
  a.Set("drive_k1", .9f);
  // Another voice of the same pedal keeps the other settings, like a voice switch.
  a.Choose("drive_type", "Vintage Fuzz");
  CHECK(std::abs(a.Get("drive_k1") - .9f) < 1.e-5f);
  CHECK(Extra(a.Chunk())["sections"]["drive"]["values"]["Pedal Style"].get<double>()
        == static_cast<double>(effect_modes::PedalStyle::VintageFuzz));
  // None switches the pedal off; choosing the pedal again returns to its settings.
  a.Choose("drive_type", "None");
  CHECK(a.Name("drive_k1").empty());
  a.Choose("drive_type", "Vintage Fuzz");
  CHECK(std::abs(a.Get("drive_k1") - .9f) < 1.e-5f);
  // The other pedal starts from its own settings, and its voice is restored with a project.
  a.Choose("drive_type", "Metal Stack");
  CHECK(a.Name("drive_k1") == "Gain");
  Instance b(mpc_plugins::PedalsSpec());
  CHECK(b.Load(a.Chunk()));
  CHECK(b.Display("drive_type") == "Metal Stack");
  // None and the comp off: the sound passes unchanged.
  b.Choose("drive_type", "None");
  Settle();
  std::vector<float> warm(44100), warmR(44100);
  b.Process(warm, warmR);
  std::vector<float> left(44100), right(44100);
  std::mt19937 rng(7);
  std::uniform_real_distribution<float> d(-.5f, .5f);
  for (size_t i = 0; i < left.size(); ++i) left[i] = right[i] = d(rng);
  const auto input = left;
  b.Process(left, right);
  double error = 0.;
  for (size_t i = 0; i < left.size(); ++i) error = std::max(error, double(std::abs(left[i] - input[i])));
  CHECK(error < 1.e-6);
  // A compressed clean boost from the programs.
  char name[64]{};
  CHECK(b.Call(effGetProgramNameIndexed, 2, 0, name) == 1 && std::string(name) == "Compressed Clean Boost");
  b.Call(effSetProgram, 0, 2);
  CHECK(b.Display("drive_type") == "Clean Boost" && b.Get("comp_on") > .5f);
}

// ----- NAM Grid Delay and Cosmic Echo ----------------------------------------------------
void TestDelay()
{
  Instance a(mpc_plugins::DelaySpec());
  CHECK(a.Name("delay_type") == "Type" && a.Display("delay_type") == "Analog");
  CHECK(a.Name("delay_k1") == "Time" && a.Display("delay_k1") == "320 ms");
  CHECK(a.Name("delay_o1") == "Tempo sync");
  a.Set("delay_o1", 1.f);
  CHECK(a.Name("delay_k1") == "Division");
  a.Choose("delay_type", "Multi-Head");
  CHECK(a.Name("delay_k1") == "Time" && a.Display("delay_k1") == "500 ms");
  CHECK(!a.Find("delay", "Heads").empty());
  char name[64]{};
  CHECK(a.Call(effGetProgramNameIndexed, 2, 0, name) == 1 && std::string(name) == "Tape");
  a.Call(effSetProgram, 0, 2);
  CHECK(a.Display("delay_type") == "Tape");
  std::vector<float> l(8192, 0.f), r(8192, 0.f);
  l[0] = r[0] = 1.f;
  a.Process(l, r);
  CHECK(std::all_of(l.begin(), l.end(), [](float x) { return std::isfinite(x); }));
}

void TestCosmicEcho()
{
  Instance a(mpc_plugins::CosmicEchoSpec());
  CHECK(a.Name("echo_type") == "Style" && a.Display("echo_type") == "Aurora Hall");
  CHECK(a.Name("echo_k7") == "Mix" && a.Display("echo_k7") == "20.0 %");
  a.Choose("echo_type", "Gemini");
  CHECK(a.Display("echo_k7") == "20.0 %"); // Styles share the settings.
  CHECK(Extra(a.Chunk())["sections"]["echo"]["values"]["Cosmic Style"].get<double>() == 0.);
  char name[64]{};
  CHECK(a.Call(effGetProgramNameIndexed, 0, 0, name) == 1 && std::string(name) == "Aurora Hall");
  CHECK(a.e->numPrograms == effect_modes::cosmicCount);
}
// Lo-Fi, Mod and Console: one section with a Type menu each.
void TestTypePlugin(const PluginSpec& spec, const std::string& section, const std::vector<std::string>& firstKnobs)
{
  Instance a(spec);
  const auto& choices = spec.sections[0].choices;
  CHECK(a.Name(section + "_type") == "Type" && a.Display(section + "_type") == choices[0].name);
  CHECK(a.e->numPrograms == static_cast<int>(choices.size()));
  for (size_t c = 0; c < choices.size(); ++c)
  {
    a.Choose(section + "_type", choices[c].name);
    if (a.Name(section + "_k1") != firstKnobs[c])
      std::fprintf(stderr, "%s: first knob is \"%s\", expected \"%s\"\n", choices[c].name, a.Name(section + "_k1").c_str(),
                   firstKnobs[c].c_str());
    CHECK(a.Name(section + "_k1") == firstKnobs[c]);
    Settle();
    std::vector<float> l(8192), r(8192);
    for (size_t i = 0; i < l.size(); ++i) l[i] = r[i] = static_cast<float>(.3 * std::sin(.05 * i));
    a.Process(l, r);
    bool finite = true, sound = false;
    for (size_t i = 0; i < l.size(); ++i)
    {
      finite = finite && std::isfinite(l[i]) && std::isfinite(r[i]);
      sound = sound || std::abs(l[i]) > 1.e-3f;
    }
    CHECK(finite && sound);
  }
  Instance b(spec);
  CHECK(b.Load(a.Chunk()));
  CHECK(b.Display(section + "_type") == choices.back().name);
  CHECK(Extra(b.Chunk()) == Extra(a.Chunk()));
}

void TestNewPlugins()
{
  TestTypePlugin(mpc_plugins::LoFiSpec(), "lofi", {"Speed", "Drive", "Bits"});
  TestTypePlugin(mpc_plugins::ModSpec(), "mod", {"Brightness", "Rate", "Slow rate", "Rate"});
  TestTypePlugin(mpc_plugins::ConsoleSpec(), "console", {"Input", "Input", "Compress"});
  Instance mod(mpc_plugins::ModSpec());
  mod.Choose("mod_type", "Uni-Vibe");
  const auto sync = mod.Find("mod", "Tempo sync");
  CHECK(!sync.empty() && !mod.Find("mod", "Vibrato mode").empty());
  mod.Set(sync, 1.f);
  CHECK(mod.Name("mod_k1") == "Division");
  Instance console(mpc_plugins::ConsoleSpec());
  CHECK(console.Name("console_o1") == "Desk" && console.Display("console_o1") == "Dark");
}
} // namespace

int main(int argc, char** argv)
{
  if (argc < 2)
  {
    std::fprintf(stderr, "usage: mpc-tests <scratch folder prefix>\n");
    return 2;
  }
  // A fresh folder per run: 32-bit std::filesystem cannot always remove an old one.
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  const std::string library = std::string(argv[1]) + "-" + std::to_string(stamp) + "/NAM Grid";
  // stdio only: 32-bit std::filesystem cannot stat files on some host file systems.
  const auto factory = mpc::pluginFolder() + "/Content/Captures/riffsmith-clean.nam";
  std::string capture;
  if (!ReadWholeFile(factory, capture))
  {
    std::fprintf(stderr, "factory capture missing: %s (set MPC_PLUGIN_FOLDER)\n", factory.c_str());
    return 2;
  }
  for (const char* name : {"A/a1.nam", "A/a2.nam", "B/b1.nam"}) WriteFile(library + "/Captures/" + name, capture);
  WriteFile(library + "/IRs/cab.wav", Wav(2048));
  WriteFile(library + "/IRs/Room/room.wav", Wav(4096));
#if defined(_WIN32)
  _putenv_s("NAM_GRID_MPC_LIBRARY", library.c_str());
#else
  setenv("NAM_GRID_MPC_LIBRARY", library.c_str(), 1);
#endif
  TestLayouts();
  TestCompatibility(argc >= 3 ? argv[2] : "mpc/tests/fixtures");
  TestAmpGates();
  TestAmpParameterState();
  TestAmpLibrary(library);
  TestAmpState();
  TestStateValidation();
  TestAmpDrivesAndLevels(library);
  TestPedals();
  TestDelay();
  TestCosmicEcho();
  TestNewPlugins();
  TestLibraryDetails(library);
  std::error_code ec;
  std::filesystem::remove_all(std::filesystem::u8path(FolderOf(library)), ec);
  if (gFailures) std::fprintf(stderr, "%d check(s) failed\n", gFailures);
  else std::printf("mpc-tests: all checks passed\n");
  return gFailures ? 1 : 0;
}
