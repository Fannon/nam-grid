#include "IsolatedUserHome.h"
#include "ClapTestHost.h"
#include "ClapHostContractTests.h"
#include "../NeuralAmpModeler/ParameterLayout.h"
#include "../NeuralAmpModeler/ModuleCatalog.h"
#include <set>
#include "ModuleControlMetadata.h"
#include "../NeuralAmpModeler/PerformanceSceneState.h"

// Simulate future growth with hypothetical segment tables: appending controls to
// the last segment, then a global segment, then more slot controls, must never
// move an existing ID or change which slot/control owns it.
namespace future_layout
{
using parameter_layout::Segment;
inline constexpr int g = parameter_layout::globalControls, s = parameter_layout::slotControls;
inline constexpr auto current = std::to_array(parameter_layout::segments);
inline constexpr auto grown = [] { auto table = current; table.back().slotControls += 3; return table; }();
inline constexpr auto extended = [] {
  std::array<Segment, grown.size() + 2> table{};
  std::copy(grown.begin(), grown.end(), table.begin());
  table[grown.size()] = {2, 0};
  table[grown.size() + 1] = {0, 4};
  return table;
}();
constexpr bool Preserves(std::span<const Segment> before, std::span<const Segment> after, int globals, int controls)
{
  for (int i = 0; i < globals; ++i)
    if (parameter_layout::GlobalID(after, i) != parameter_layout::GlobalID(before, i))
      return false;
  for (int control = 0; control < controls; ++control)
    for (int slot = 0; slot < parameter_layout::slots; ++slot)
    {
      const int id = parameter_layout::SlotID(before, slot, control);
      if (id < 0 || parameter_layout::SlotID(after, slot, control) != id
          || parameter_layout::SlotOf(after, id) != slot || parameter_layout::ControlOf(after, id) != control)
        return false;
    }
  return true;
}
// The exhaustive ID comparison runs in main(): it exceeds Clang's constexpr step limit.
// New IDs are unique, contiguous and start where the previous table ended.
static_assert(parameter_layout::SlotID(grown, 0, s) == parameter_layout::Total(current));
static_assert(parameter_layout::GlobalID(extended, g) == parameter_layout::Total(grown));
static_assert(parameter_layout::SlotID(extended, 0, s + 3) == parameter_layout::Total(grown) + 2);
static_assert(parameter_layout::SlotID(extended, 15, s + 6) == parameter_layout::Total(extended) - 1);
static_assert(parameter_layout::SlotOf(extended, parameter_layout::GlobalID(extended, g + 1)) == -1);
static_assert(parameter_layout::Total(current) == kNumParams);
} // namespace future_layout

// Serial state extensions: Juno's appended mode, Trails global, then Time Change slots.
static_assert(PerformanceParam(grid_performance::Trails) == 4476);
static_assert(SlotUtilityParam(0, uDelayTimeChange) == 4477);
static_assert(SlotUtilityParam(15, uDelayTimeChange) == 4492);
static_assert(grid_performance::parameters[grid_performance::Trails].introducedIn == 11);
static_assert(kEffectParameters[uDelayTimeChange].introducedIn == 12);
// Pedal Comp's six controls (4493-4588), then Chorus Voices.
static_assert(SlotUtilityParam(0, uChorusVoices) == 4589);
static_assert(SlotUtilityParam(15, uChorusVoices) == 4604);
static_assert(kEffectParameters[uChorusVoices].introducedIn == 14);
static_assert(SlotUtilityParam(0, uSpringTank) == 4605);
static_assert(SlotUtilityParam(15, uSpringTank) == 4620);
static_assert(kEffectParameters[uSpringTank].introducedIn == 15);
static_assert(SlotUtilityParam(0, uDynamicEQFrequency) == 4621);
static_assert(SlotUtilityParam(15, uDynamicEQMix) == 4748);
static_assert(kEffectParameters[uDynamicEQFrequency].introducedIn == 16 && kEffectParameters[uDynamicEQMix].introducedIn == 16);
static_assert(SlotUtilityParam(0, uTransientShaperAttack) == 4749);
static_assert(SlotUtilityParam(15, uTransientShaperMix) == 4844);
static_assert(kEffectParameters[uTransientShaperAttack].introducedIn == 17 && kEffectParameters[uTransientShaperMix].introducedIn == 17);
static_assert(SlotUtilityParam(0, uHarmonicCrossover) == 4845);
static_assert(SlotUtilityParam(15, uHarmonicCrossover) == 4860);
static_assert(kEffectParameters[uHarmonicCrossover].introducedIn == 18);
static_assert(SlotUtilityParam(0, uMultibandDepth) == 4861);
static_assert(SlotUtilityParam(15, uMultibandThreshold) == 5036);
static_assert(kEffectParameters[uMultibandDepth].introducedIn == 19 && kEffectParameters[uMultibandThreshold].introducedIn == 19);
static_assert(SlotUtilityParam(0, uGraphicBand1) == 5037);
static_assert(SlotUtilityParam(15, uGraphicVoicing) == 5180);
static_assert(kEffectParameters[uGraphicBand1].introducedIn == 20 && kEffectParameters[uGraphicVoicing].introducedIn == 20);

static_assert(static_cast<int>(SlotEffect::Reserved63) == 63);
static_assert(static_cast<int>(SlotEffect::PitchShift) == 64);
static_assert(SlotUtilityParam(0, uPitchInterval) == 5181);
static_assert(SlotUtilityParam(15, uPitchMix) == 5260);
static_assert(kEffectParameters[uPitchInterval].introducedIn == 22 && kEffectParameters[uPitchMix].introducedIn == 22);
// Split Slope (schema 24) appends one state-only global after every previous ID.
static_assert(PerformanceParam(grid_performance::CrossoverSlope) == 5261);
static_assert(grid_performance::parameters[grid_performance::CrossoverSlope].introducedIn == 24);
static_assert(static_cast<int>(grid_performance::SplitSlopeLinkwitzRiley) == 1);

int main(int argc, char** argv)
{
  IsolatedUserHome userHome("nam-grid-parameter-contract"); // Never the user's settings or library.
  assert(argc == 3 || argc == 4);
  assert(future_layout::Preserves(future_layout::current, future_layout::grown, future_layout::g, future_layout::s));
  assert(future_layout::Preserves(future_layout::grown, future_layout::extended, future_layout::g, future_layout::s + 3));
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
  Host host;
  auto* plugin = factory->create_plugin(factory, &host.host, factory->get_plugin_descriptor(factory, 0)->id);
  assert(plugin && plugin->init(plugin));
  const auto* params = static_cast<const clap_plugin_params*>(plugin->get_extension(plugin, CLAP_EXT_PARAMS));
  const auto* state = static_cast<const clap_plugin_state*>(plugin->get_extension(plugin, CLAP_EXT_STATE));
  assert(params && state);
  auto save = [&] {
    Stream data;
    clap_ostream stream{&data, Stream::write};
    assert(state->save(plugin, &stream));
    return data.config();
  };
  auto load = [&](const json& config) {
    Stream data;
    data.string("###nam-grid###");
    data.string(config.dump());
    clap_istream stream{&data, Stream::read};
    return state->load(plugin, &stream);
  };
  auto metadata = [&](int id) {
    clap_param_info info{};
    assert(params->get_info(plugin, id, &info));
    assert(info.id == id);
    // Text conversions capture the normalized value -> physical-unit mapping,
    // including power/log curves. CLAP doubles themselves have a 0..1 range.
    json text = json::array();
    for (double position : {0., .125, .25, .5, .75, .875, 1.})
    {
      char label[128]{};
      double value = info.min_value + position * (info.max_value - info.min_value);
      if (info.flags & CLAP_PARAM_IS_STEPPED)
        value = std::round(value);
      assert(params->value_to_text(plugin, id, value, label, sizeof(label)));
      text.push_back(label);
    }
    return json{{"id", id},
                {"name", info.name},
                {"minimum", info.min_value},
                {"maximum", info.max_value},
                {"default", info.default_value},
                {"flags", info.flags},
                {"stepped", bool(info.flags & CLAP_PARAM_IS_STEPPED)},
                {"mapping", text}};
  };
  if (argc == 3 && std::string(argv[2]) == "--module-controls")
  {
    std::cout << ModuleControlMetadata(plugin, params).dump() << '\n';
    plugin->destroy(plugin); entry->deinit();
#ifdef _WIN32
    FreeLibrary(library);
#else
    dlclose(library);
#endif
    return 0;
  }
  TestClapHostContract(plugin, host);
  const auto* descriptor = factory->get_plugin_descriptor(factory, 0);
  const std::filesystem::path fixture = argv[2];
  if (argc == 4)
  {
    assert(std::string(argv[3]) == "--record");
    json snapshot{{"baseline", "nam-grid 0.1.0 host layout; algorithm selectors are state only"},
                  {"plugin", {{"clap_id", descriptor->id}, {"vendor", descriptor->vendor}, {"name", descriptor->name}}},
                  {"state_schema", kEffectStateSchema},
                  {"slot", json::array()},
                  {"global", json::array()},
                  {"algorithms", json::object()},
                  {"modules", json::object()}};
    for (int control = 0; control < parameter_layout::slotControls; ++control)
    {
      snapshot["slot"].push_back(metadata(SlotParam(0, control)));
      const int p = control - parameter_layout::standardControls;
      if (p >= 0 && IsAlgorithmMode(p))
        for (const auto& mode : AlgorithmIdentities(p))
          snapshot["algorithms"][kEffectParameters[p].name][mode.key] = mode.value;
    }
    for (int index = 0; index < parameter_layout::globalControls; ++index)
      snapshot["global"].push_back(metadata(parameter_layout::GlobalID(index)));
    for (int effect = 0; effect <= static_cast<int>(SlotEffect::Last); ++effect)
      if (IsValidSlotEffect(static_cast<SlotEffect>(effect)))
        snapshot["modules"][SlotEffectName(static_cast<SlotEffect>(effect))] = effect;
    std::ofstream(fixture) << snapshot.dump(2) << '\n';
  }
  json snapshot;
  std::ifstream(fixture) >> snapshot;
  assert(snapshot["plugin"]["clap_id"] == descriptor->id && snapshot["plugin"]["vendor"] == descriptor->vendor
         && snapshot["plugin"]["name"] == descriptor->name);
  assert(params->count(plugin) == kNumParams);
  assert(snapshot["slot"].size() <= parameter_layout::slotControls);
  for (int slot = 0; slot < 16; ++slot)
    for (int control = 0; control < snapshot["slot"].size(); ++control)
    {
      const int id = SlotParam(slot, control);
      auto expected = snapshot["slot"][control];
      // Slot 0's name begins "Pre 1 "; every slot shares the remaining control name.
      expected["id"] = expected["id"].get<int>() + slot;
      expected["name"] = GridSlotName(slot) + " " + expected["name"].get<std::string>().substr(6);
      auto actual = metadata(id);
      const int p = control - parameter_layout::standardControls;
      if (p >= 0 && IsAlgorithmMode(p))
      {
        // Algorithm menus may grow. Their integer identities remain frozen, and
        // unlike musical controls they must never acquire automation flags.
        assert(!(actual["flags"].get<int>() & CLAP_PARAM_IS_AUTOMATABLE));
        assert(actual["maximum"].get<double>() >= expected["maximum"].get<double>());
        actual.erase("maximum");
        expected.erase("maximum");
        actual.erase("mapping");
        expected.erase("mapping");
      }
      if (actual != expected)
        std::cerr << "Parameter contract changed: " << expected << "\nActual: " << actual << '\n';
      assert(actual == expected);
    }
  for (auto expected : snapshot["global"])
  {
    const int id = expected["id"].get<int>();
    auto actual = metadata(id);
    if (IsStateOnlyParam(id))
    {
      // Matrix routing lists may grow like algorithm menus, and never become automatable.
      assert(!(actual["flags"].get<int>() & CLAP_PARAM_IS_AUTOMATABLE));
      assert(actual["maximum"].get<double>() >= expected["maximum"].get<double>());
      for (auto* entry : {&actual, &expected}) { entry->erase("maximum"); entry->erase("mapping"); }
    }
    if (actual != expected)
      std::cerr << "Parameter contract changed: " << expected << "\nActual: " << actual << '\n';
    assert(actual == expected);
  }
  // New globals live after all pre-existing slot IDs; ownership and metadata must
  // follow the segment map rather than assuming globals are one contiguous bank.
  for (int p = grid_performance::LFO2Shape; p < grid_performance::Count; ++p)
  {
    const int id = PerformanceParam(p);
    assert(id >= parameter_layout::SlotID(15, parameter_layout::standardControls + uParamLowMode) + 1);
    assert(PerformanceIndex(id) == p && SlotControlOwner(id) == -1);
    const auto actual = metadata(id);
    assert(actual["name"] == grid_performance::parameters[p].name);
    assert(bool(actual["flags"].get<int>() & CLAP_PARAM_IS_AUTOMATABLE) == !grid_performance::StateOnly(p));
  }
  for (int p = 0; p < kUtilityControls; ++p)
    if (IsAlgorithmMode(p))
    {
      const auto& key = kEffectParameters[p].name;
      const auto saved = snapshot["algorithms"].find(key);
      if (saved == snapshot["algorithms"].end())
        continue; // Newly appended family.
      for (const auto& [identity, value] : saved->items())
      {
        const auto modes = AlgorithmIdentities(p);
        assert(std::any_of(
          modes.begin(), modes.end(), [&](const auto& mode) { return identity == mode.key && value == mode.value; }));
      }
    }
  // Module IDs are permanent; display names may change only by re-recording a reviewed baseline.
  for (const auto& [name, id] : snapshot["modules"].items())
    assert(IsValidSlotEffect(static_cast<SlotEffect>(id.get<int>()))
           && name == SlotEffectName(static_cast<SlotEffect>(id.get<int>())));
  std::set<int> moduleIDs;
  for (const auto& group : nam_library::kModuleGroups)
    for (auto effect : group.effects)
      assert(moduleIDs.insert(static_cast<int>(effect)).second && IsValidSlotEffect(effect));

  // All selectors, all current choices and all slots must survive state recall.
  // Scenes retain raw algorithm identities too; no normalized values are saved.
  const auto base = save();
  // Released schemas and both actual predecessor layouts lack Time Change. Loading
  // one after Crossfade must restore Glide while retaining Juno-10 and Trails-11 data.
  for (int schema = kInitialEffectStateSchema; schema <= 11; ++schema)
  {
    auto expected = base;
    if (schema >= 10)
      for (const int slot : {0, 15})
      {
        expected["SlotEffects"][slot] = static_cast<int>(SlotEffect::JunoChorus);
        expected["SlotPaths"][slot] = expected["SlotHashes"][slot] = "";
        const auto key = GridSlotName(slot) + " Juno Mode";
        expected[key] = expected["PerformanceScenes"][0][key] = 3.;
      }
    if (schema >= 11) expected["Trails"] = expected["PerformanceScenes"][0]["Trails"] = 1.;
    auto legacy = expected;
    legacy["schema"] = schema;
    const auto omit = [&](const std::string& key) {
      legacy.erase(key);
      for (auto& scene : legacy["PerformanceScenes"]) scene.erase(key);
    };
    for (int p = 0; p < grid_performance::Count; ++p)
      if (grid_performance::parameters[p].introducedIn > schema) omit(grid_performance::parameters[p].name);
    for (int p = 0; p < kUtilityControls; ++p)
      if (kEffectParameters[p].introducedIn > schema)
        for (int slot = 0; slot < 16; ++slot) omit(GridSlotName(slot) + " " + kEffectParameters[p].name);
    auto crossfade = base;
    for (int slot = 0; slot < 16; ++slot)
      crossfade[GridSlotName(slot) + " " + kEffectParameters[uDelayTimeChange].name] = 1.;
    assert(load(crossfade));
    assert(load(legacy));
    assert(save() == expected);
  }
  assert(load(base));
  // Future controls must explicitly declare their introduction version so this
  // original baseline preset gains defaults without losing any existing values.
  auto historical = base;
  historical["schema"] = snapshot["state_schema"];
  for (int p = 0; p < grid_performance::Count; ++p)
    if (grid_performance::parameters[p].introducedIn > snapshot["state_schema"].get<int>())
      historical.erase(grid_performance::parameters[p].name);
  for (int p = static_cast<int>(snapshot["slot"].size()) - parameter_layout::standardControls;
       p < kUtilityControls; ++p)
    for (int slot = 0; slot < 16; ++slot)
      historical.erase(GridSlotName(slot) + " " + kEffectParameters[p].name);
  assert(load(historical));
  assert(save() == base);
  for (int p = 0; p < kUtilityControls; ++p)
    if (IsAlgorithmMode(p))
      for (const auto& mode : AlgorithmIdentities(p))
      {
        auto config = base;
        for (int slot = 0; slot < 16; ++slot)
        {
          const auto name = GridSlotName(slot) + " " + kEffectParameters[p].name;
          config[name] = mode.value;
          config["PerformanceScenes"][0][name] = mode.value;
        }
        assert(load(config));
        assert(save() == config);
      }
  // Schema 7 rigs receive only schema 8 defaults; existing IR and older
  // controls remain exactly as saved. New controls also survive scene recall.
  auto schema7 = base;
  schema7["schema"] = 7;
  auto extendedControls = base;
  for (int p = uToneMidFrequency; p < kUtilityControls; ++p)
    for (int slot = 0; slot < 16; ++slot)
    {
      const auto key = GridSlotName(slot) + " " + kEffectParameters[p].name;
      schema7.erase(key);
      for (auto& scene : schema7["PerformanceScenes"]) scene.erase(key);
      extendedControls[key] = kEffectParameters[p].maximum;
      extendedControls["PerformanceScenes"][0][key] = kEffectParameters[p].maximum;
    }
  assert(load(schema7)); assert(save() == base);
  assert(load(extendedControls)); assert(save() == extendedControls);
  auto incomplete = extendedControls;
  incomplete.erase("Post 4 LoFi Drive");
  assert(!load(incomplete)); assert(save() == extendedControls);
  assert(load(base));
  for (int p = 0; p < kUtilityControls; ++p)
    if (IsAlgorithmMode(p))
      for (double invalid : {-1., .5, kEffectParameters[p].maximum + 1.})
      {
        const auto name = GridSlotName(15) + " " + kEffectParameters[p].name;
        auto bad = base;
        bad[name] = invalid;
        assert(!load(bad));
        assert(save() == base);
        bad = base;
        bad["PerformanceScenes"][0][name] = invalid;
        assert(!load(bad));
        assert(save() == base);
      }
  // The unreleased monitor module was removed; reject it before mutating a rig.
  for (int slot : {0, 15})
  {
    auto bad = base;
    bad["SlotEffects"][slot] = 63;
    assert(!load(bad));
    assert(save() == base);
  }
  // A real predecessor state with full snapshots must re-save as deltas without
  // changing its scene values. This fails on the old, full-copy serializer.
  auto sceneBaseline = performance_scenes::Baseline(base, [&](int i) { return metadata(i)["name"].get<std::string>(); });
  auto fullScenes = base;
  fullScenes["schema"] = 22; fullScenes.erase("PerformanceSceneDeltas");
  fullScenes["PerformanceScenes"] = json::array({sceneBaseline, sceneBaseline, sceneBaseline});
  assert(load(fullScenes));
  auto compactScenes = save();
  assert(compactScenes.dump().size() < fullScenes.dump().size() / 2);
  assert(compactScenes["PerformanceSceneDeltas"][0].is_object() && compactScenes["PerformanceSceneDeltas"][0].empty());
  auto expandedScenes = compactScenes;
  assert(performance_scenes::Expand(expandedScenes, sceneBaseline));
  assert(expandedScenes["PerformanceScenes"] == fullScenes["PerformanceScenes"]);
  auto invalidDelta = compactScenes;
  invalidDelta["PerformanceSceneDeltas"][0]["Output"] = "invalid";
  assert(!load(invalidDelta)); assert(save() == compactScenes);
  assert(load(base));
  plugin->destroy(plugin);
  entry->deinit();
#ifdef _WIN32
  FreeLibrary(library);
#else
  dlclose(library);
#endif
  std::cout << "Parameter compatibility: frozen 0.1.0 IDs/mappings, append-only growth, all algorithms/modules, "
               "preset/scene recall and atomic rejection passed\n";
}
