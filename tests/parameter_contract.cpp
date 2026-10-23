#include "ClapTestHost.h"
#include "../NeuralAmpModeler/ParameterLayout.h"
#include "../NeuralAmpModeler/ModuleCatalog.h"
#include <set>

// Simulate several future additions without adding selectable dummy controls.
static_assert(parameter_layout::UtilityID(15, 200) == 3546);
static_assert(parameter_layout::UtilityID(0, 201) == 3547);
static_assert(parameter_layout::UtilityID(15, 201) == 3562);
static_assert(parameter_layout::UtilityID(0, 202) == 3563);
static_assert(
  [] {
    constexpr int futureCount = 205;
    for (int slot = 0; slot < 16; ++slot)
      for (int control = 0; control < futureCount; ++control)
      {
        const int id = parameter_layout::UtilityID(slot, control);
        if (parameter_layout::UtilitySlot(id, futureCount) != slot
            || parameter_layout::UtilityControl(id, futureCount) != control)
          return false;
        if (id != 331 + control * 16 + slot)
          return false;
        if (control >= 201 && id <= 3546)
          return false;
      }
    return true;
  }(),
  "Growing the effect bank must preserve every old ID and ownership");

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
    data.string("###NAMGrid-v1###");
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
  const std::filesystem::path fixture = argv[2];
  if (argc == 4)
  {
    assert(std::string(argv[3]) == "--record");
    json snapshot{{"baseline", "Stable layout v1; algorithm selectors are state only"},
                  {"state_schema", kEffectStateSchema},
                  {"utility", json::array()},
                  {"global", json::array()},
                  {"algorithms", json::object()},
                  {"modules", json::object()}};
    for (int p = 0; p < kUtilityControls; ++p)
    {
      snapshot["utility"].push_back(metadata(SlotUtilityParam(0, p)));
      if (IsAlgorithmMode(p))
        for (const auto& mode : AlgorithmIdentities(p))
          snapshot["algorithms"][kEffectParameters[p].name][mode.key] = mode.value;
    }
    for (int id = 0; id < parameter_layout::utilityBase; ++id)
      if (parameter_layout::UtilityControl(id) < 0)
        snapshot["global"].push_back(metadata(id));
    for (const auto& group : nam_library::kModuleGroups)
      for (auto effect : group.effects)
        snapshot["modules"][SlotEffectName(effect)] = static_cast<int>(effect);
    // File modules and the legacy gate alias are not separate catalog entries.
    snapshot["modules"]["NAM"] = static_cast<int>(SlotEffect::NAM);
    snapshot["modules"]["Cabinet IR"] = static_cast<int>(SlotEffect::CabinetIR);
    snapshot["legacy_tight_gate"] = static_cast<int>(SlotEffect::TightGate);
    std::ofstream(fixture) << snapshot.dump(2) << '\n';
  }
  json snapshot;
  std::ifstream(fixture) >> snapshot;
  assert(params->count(plugin) == kNumParams);
  for (int slot = 0; slot < 16; ++slot)
    for (int p = 0; p < snapshot["utility"].size(); ++p)
    {
      const int id = SlotUtilityParam(slot, p);
      auto expected = snapshot["utility"][p];
      expected["id"] = snapshot["utility"][p]["id"].get<int>() + slot;
      expected["name"] = GridSlotName(slot) + " " + expected["name"].get<std::string>().substr(6);
      auto actual = metadata(id);
      if (IsAlgorithmMode(p))
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
  for (const auto& expected : snapshot["global"])
    assert(metadata(expected["id"].get<int>()) == expected);
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
  std::set<int> moduleIDs;
  for (const auto& group : nam_library::kModuleGroups)
    for (auto effect : group.effects)
    {
      assert(moduleIDs.insert(static_cast<int>(effect)).second);
      const auto previous = snapshot["modules"].find(SlotEffectName(effect));
      if (previous != snapshot["modules"].end())
        assert(*previous == static_cast<int>(effect));
    }
  for (const auto& [name, id] : snapshot["modules"].items())
    assert(name == SlotEffectName(static_cast<SlotEffect>(id.get<int>())));
  assert(snapshot["legacy_tight_gate"] == static_cast<int>(SlotEffect::TightGate));

  // All selectors, all current choices and all slots must survive state recall.
  // Scenes retain raw algorithm identities too; no normalized values are saved.
  const auto base = save();
  // Future controls must explicitly declare their introduction version so this
  // original baseline preset gains defaults without losing any existing values.
  auto historical = base;
  historical["schema"] = snapshot["state_schema"];
  for (int p = static_cast<int>(snapshot["utility"].size()); p < kUtilityControls; ++p)
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
  plugin->destroy(plugin);
  entry->deinit();
#ifdef _WIN32
  FreeLibrary(library);
#else
  dlclose(library);
#endif
  std::cout << "Parameter compatibility: stable v1 IDs/mappings, append-only growth, all algorithms/modules, "
               "preset/scene recall and atomic rejection passed\n";
}
