// On-demand host for the actual plugin; no audio device, editor or listening test.
#include "ClapTestHost.h"
#include "A2Fixture.h"
#include "../NeuralAmpModeler/EffectPresentation.h"
#include "../NeuralAmpModeler/ModuleCatalog.h"
#include "../NeuralAmpModeler/SlotDefaults.h"
#include <algorithm>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <stdexcept>
#include <thread>

using Clock = std::chrono::steady_clock;
static void Check(bool ok, const std::string& message)
{
  if (!ok)
    throw std::runtime_error(message);
}
static json Read(const char* path)
{
  std::ifstream in(path);
  Check(in.good(), std::string("Cannot read ") + path);
  return json::parse(in);
}

static json Case(const std::string& id, const std::string& name, SlotEffect effect,
                 const json& controls = json::object())
{
  std::string mode;
  for (auto it = controls.begin(); it != controls.end(); ++it)
  {
    std::string value = it.value().dump();
    for (int p = 0; p < kUtilityControls; ++p)
      if (it.key() == kEffectParameters[p].name)
      {
        const auto choices = EffectChoices(p);
        if (!choices.empty())
          value = choices[it.value().get<int>()];
      }
    if (!mode.empty())
      mode += "; ";
    mode += it.key() + "=" + value;
  }
  // Continuous controls let the render audit automate each module's knobs.
  json knobs = json::array();
  for (int p = 0; p < kUtilityControls; ++p)
  {
    const auto& descriptor = kEffectParameters[p];
    const bool binary = descriptor.minimum == 0. && descriptor.maximum == 1. && descriptor.step == 1.;
    if (effect != SlotEffect::NAM && effect_presentation::Visible(effect, p) && EffectChoices(p).empty() && !binary)
      knobs.push_back({{"name", descriptor.name}, {"minimum", descriptor.minimum}, {"maximum", descriptor.maximum}});
  }
  return {{"id", id},
          {"module", name},
          {"effect", static_cast<int>(effect)},
          {"controls", controls},
          {"knobs", knobs},
          {"mode", mode.empty() ? "insertion defaults" : mode}};
}

static json Catalog()
{
  json cases = json::array();
  cases.push_back(Case("empty", "Empty grid", SlotEffect::NAM));
  for (const auto& group : nam_library::kModuleGroups)
    for (auto effect : group.effects)
    {
      std::vector<json> variants{json::object()};
      for (int p = 0; p < kUtilityControls; ++p)
      {
        // Presentation adds shared sync/stereo switches to UsesUtility().
        if (p == uTempoDivision || !effect_presentation::Visible(effect, p))
          continue;
        const auto& descriptor = kEffectParameters[p];
        const auto choices = EffectChoices(p);
        const int count = !choices.empty() ? static_cast<int>(choices.size())
                          : descriptor.minimum == 0. && descriptor.maximum == 1. && descriptor.step == 1. ? 2
                                                                                                          : 0;
        if (!count)
          continue;
        std::vector<json> expanded;
        for (const auto& variant : variants)
          for (int choice = 0; choice < count; ++choice)
          {
            auto next = variant;
            next[descriptor.name] = choice;
            expanded.push_back(std::move(next));
          }
        variants = std::move(expanded);
      }
      // Resolution is continuous but switches internal rate branches. Sample its
      // extremes as well as the insertion default, independently of other knobs.
      int resolution = effect == SlotEffect::AirPlate        ? uAirPlateResolution
                       : effect == SlotEffect::AirHall       ? uAirHallResolution
                       : effect == SlotEffect::GalacticSpace ? uSpaceResolution
                                                             : -1;
      if (resolution >= 0)
      {
        const auto original = variants;
        for (double value : {kEffectParameters[resolution].minimum, kEffectParameters[resolution].maximum})
          for (auto variant : original)
          {
            variant[kEffectParameters[resolution].name] = value;
            variants.push_back(std::move(variant));
          }
      }
      // All rhythmic choices, with the other mode controls at insertion defaults.
      // The algorithm/toggle combinations above already include Sync on/off.
      if (effect_presentation::Tempo(effect))
        for (int division = 0; division < grid_tempo::names.size(); ++division)
          if (division != static_cast<int>(kEffectParameters[uTempoDivision].initial))
            variants.push_back({{"Tempo Sync", 1}, {"Tempo Division", division}});
      for (int i = 0; i < variants.size(); ++i)
        cases.push_back(Case("fx-" + std::to_string(static_cast<int>(effect)) + "-" + std::to_string(i),
                             SlotEffectName(effect), effect, variants[i]));
      if (effect == SlotEffect::NoisePrint)
      {
        auto untrained = Case("noise-untrained", SlotEffectName(effect), effect, {{"Noise Learn", 0}});
        untrained["untrained"] = true;
        untrained["mode"] = "untrained";
        cases.push_back(std::move(untrained));
      }
    }
  return cases;
}

static void WriteFixtures(const std::filesystem::path& folder)
{
  std::filesystem::create_directories(folder);
  std::ofstream(folder / "a2-small.nam") << fixture::model(3, 123).dump();
  std::ofstream(folder / "a2-large.nam") << fixture::model(8, 456).dump();
  std::ofstream(folder / "slimmable.nam") << fixture::packed().dump();
  // The cabinet loader accepts mono IRs; processing still has stereo input/output.
  for (int frames : {2048, 16384})
  {
    std::ofstream out(folder / ("ir-" + std::to_string(frames) + ".wav"), std::ios::binary);
    auto word = [&](uint32_t value, int bytes) {
      for (int i = 0; i < bytes; ++i)
        out.put(static_cast<char>((value >> (8 * i)) & 255));
    };
    out.write("RIFF", 4);
    word(36 + frames * 2, 4);
    out.write("WAVEfmt ", 8);
    word(16, 4);
    word(1, 2);
    word(1, 2);
    word(48000, 4);
    word(96000, 4);
    word(2, 2);
    word(16, 2);
    out.write("data", 4);
    word(frames * 2, 4);
    uint32_t seed = 123;
    for (int i = 0; i < frames; ++i)
    {
      seed = 1664525 * seed + 1013904223;
      const double noise = static_cast<double>(seed) / 4294967296. * 2. - 1.;
      const double value = i == 0 ? .6 : .06 * noise * std::exp(-8. * i / frames);
      word(static_cast<uint16_t>(static_cast<int16_t>(value * 32767.)), 2);
    }
    Check(out.good(), "Cannot write IR fixture");
  }
}

class Library
{
public:
#ifdef _WIN32
  HMODULE handle = nullptr;
#else
  void* handle = nullptr;
#endif
  const clap_plugin_entry* entry = nullptr;
  const clap_plugin_factory* factory = nullptr;
  explicit Library(const char* path)
  {
#ifdef _WIN32
    handle = LoadLibraryW(std::filesystem::path(path).c_str());
    if (handle)
      entry = reinterpret_cast<const clap_plugin_entry*>(GetProcAddress(handle, "clap_entry"));
#else
    handle = dlopen(path, RTLD_NOW);
    if (handle)
      entry = reinterpret_cast<const clap_plugin_entry*>(dlsym(handle, "clap_entry"));
#endif
    Check(entry && entry->init(path), "Cannot load/initialize CLAP plugin");
    factory = static_cast<const clap_plugin_factory*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    Check(factory && factory->get_plugin_count(factory) == 1, "Expected one CLAP plugin");
  }
  ~Library()
  {
    if (entry)
      entry->deinit();
#ifdef _WIN32
    if (handle)
      FreeLibrary(handle);
#else
    if (handle)
      dlclose(handle);
#endif
  }
};

class Instance
{
public:
  Host host;
  const clap_plugin* plugin = nullptr;
  const clap_plugin_state* state = nullptr;
  bool active = false, started = false;
  explicit Instance(const Library& library)
  {
    plugin = library.factory->create_plugin(
      library.factory, &host.host, library.factory->get_plugin_descriptor(library.factory, 0)->id);
    Check(plugin && plugin->init(plugin), "Cannot create plugin instance");
    state = static_cast<const clap_plugin_state*>(plugin->get_extension(plugin, CLAP_EXT_STATE));
    Check(state != nullptr, "Missing state extension");
  }
  ~Instance()
  {
    if (started)
      plugin->stop_processing(plugin);
    if (active)
      plugin->deactivate(plugin);
    if (plugin)
      plugin->destroy(plugin);
  }
  void Activate(double rate, int buffer)
  {
    Check(plugin->activate(plugin, rate, 1, buffer), "Activation failed");
    active = true;
  }
  void Deactivate()
  {
    plugin->deactivate(plugin);
    active = false;
  }
  json Save()
  {
    Stream bytes;
    clap_ostream stream{&bytes, Stream::write};
    Check(state->save(plugin, &stream), "State save failed");
    return bytes.config();
  }
  void Load(const json& config)
  {
    Stream bytes;
    bytes.string("###nam-grid###");
    bytes.string(config.dump());
    clap_istream stream{&bytes, Stream::read};
    Check(state->load(plugin, &stream), "State load failed");
  }
};

static json Configure(json config, const json& scenario)
{
  const auto effect = static_cast<SlotEffect>(scenario.at("effect").get<int>());
  config["CaptureFolders"] = json::array();
  config["Tuner"] = 0.;
  config["Tuner Mute"] = 0.;
  config["Pre 2 In"] = config["Output"] = 0.;
  config["SlotPaths"] = std::vector<std::string>(16, "");
  config["SlotHashes"] = std::vector<std::string>(16, "");
  config["SlotEffects"] = std::vector<int>(16, 0);
  config["SlotEffects"][0] = static_cast<int>(effect);
  const auto defaults = SlotDefaultValues(effect);
  const std::array<const char*, 17> names{
    "Pre 1 In",          "Pre 1 Trim",           "Pre 1 Enabled",      "Pre 1 Gate Threshold", "Pre 1 Gate Release",
    "Pre 1 Gate Reduction", "Pre 1 Comp Threshold", "Pre 1 Comp Release", "Pre 1 Comp Makeup",    "Pre 1 EQ Bass",
    "Pre 1 EQ Mid",         "Pre 1 EQ Treble",      "Pre 1 EQ High Pass", "Pre 1 EQ Low Pass",    "Pre 1 EQ Presence",
    "Pre 1 IR Level",       "Pre 1 Full Quality"};
  for (int p = 0; p < names.size(); ++p)
    config[names[p]] = defaults[p];
  for (int p = 0; p < kUtilityControls; ++p)
    config[std::string("Pre 1 ") + kEffectParameters[p].name] = defaults[17 + p];
  for (auto it = scenario.at("controls").begin(); it != scenario.at("controls").end(); ++it)
    config["Pre 1 " + it.key()] = it.value();
  if (scenario.contains("asset"))
    config["SlotPaths"][0] = scenario["asset"];
  if (effect == SlotEffect::NoisePrint && !scenario.value("untrained", false))
    config["NoisePrintProfiles"][0] = {{"version", 1}, {"power", std::vector<double>(noise_print::bands, 1.e-6)}};
  return config;
}

static double Percentile(std::vector<double> values, double percentile)
{
  Check(!values.empty(), "No measured callbacks");
  const auto index = static_cast<size_t>(std::ceil(percentile * values.size())) - 1;
  std::nth_element(values.begin(), values.begin() + index, values.end());
  return values[index];
}
static json Statistics(const std::vector<double>& times, double deadline)
{
  const double mean = std::accumulate(times.begin(), times.end(), 0.) / times.size();
  const double worst = *std::max_element(times.begin(), times.end());
  return {{"blocks", times.size()},
          {"mean_us", mean},
          {"p95_us", Percentile(times, .95)},
          {"p99_us", Percentile(times, .99)},
          {"max_us", worst},
          {"cpu_percent", 100. * mean / deadline},
          {"max_budget_percent", 100. * worst / deadline},
          {"deadline_misses", std::count_if(times.begin(), times.end(), [&](double t) { return t > deadline; })}};
}

static json Measure(const Library& library, const json& scenario, const json& settings, int rate, int buffer,
                    int repeat)
{
  Instance instance(library);
  // Upstream loaders need the host rate and block limit before preparing assets.
  instance.Activate(rate, buffer);
  instance.Deactivate();
  auto config = Configure(instance.Save(), scenario);
  // Capture needs live input history before Hold is engaged.
  const std::string latch = config.value("Pre 1 Freeze Hold", 0.) >= .5      ? "Freeze Hold"
                            : config.value("Pre 1 Shimmer Freeze", 0.) >= .5 ? "Shimmer Freeze"
                                                                             : "";
  const bool hold = !latch.empty();
  const bool learn = config.value("Pre 1 Noise Learn", 0.) >= .5;
  if (hold)
    config["Pre 1 " + latch] = 0.;
  // Learning is intentionally transient and is disabled by preset recall.
  if (learn)
    config["Pre 1 Noise Learn"] = 0.;
  const auto loadStart = Clock::now();
  instance.Load(config);
  const double loadMs = std::chrono::duration<double, std::milli>(Clock::now() - loadStart).count();
  const auto recalled = instance.Save();
  Check(recalled.at("SlotEffects")[0] == scenario.at("effect"), "Module selection not recalled");
  for (auto it = scenario.at("controls").begin(); it != scenario.at("controls").end(); ++it)
    if (it.key() != "Freeze Hold" && it.key() != "Shimmer Freeze" && it.key() != "Noise Learn")
      Check(recalled.at("Pre 1 " + it.key()) == it.value(), "Mode not recalled: " + it.key());
  if (scenario.contains("asset"))
    Check(recalled.at("SlotHashes")[0].is_string() && !recalled.at("SlotHashes")[0].get<std::string>().empty(),
          "Asset failed to load (plugin recalled a missing/invalid file)");
  const auto activationStart = Clock::now();
  instance.Activate(rate, buffer);
  const double activationMs = std::chrono::duration<double, std::milli>(Clock::now() - activationStart).count();
  Check(instance.plugin->start_processing(instance.plugin), "Start processing failed");
  instance.started = true;
  std::array<std::vector<double>, 4> audio;
  for (auto& channel : audio)
    channel.resize(buffer);
  double *inputs[]{audio[0].data(), audio[1].data()}, *outputs[]{audio[2].data(), audio[3].data()};
  clap_audio_buffer input{nullptr, inputs, 2, 0, 0}, output{nullptr, outputs, 2, 0, 0};
  clap_event_param_value trigger{};
  const clap_plugin_params* triggerParams = nullptr;
  trigger.header.size = sizeof(trigger);
  trigger.header.type = CLAP_EVENT_PARAM_VALUE;
  trigger.note_id = trigger.port_index = trigger.channel = trigger.key = -1;
  trigger.value = 1.;
  if (hold || learn)
  {
    const auto* params =
      static_cast<const clap_plugin_params*>(instance.plugin->get_extension(instance.plugin, CLAP_EXT_PARAMS));
    triggerParams = params;
    Check(params != nullptr, "Missing parameters");
    bool found = false;
    for (uint32_t i = 0; i < params->count(instance.plugin); ++i)
    {
      clap_param_info info{};
      Check(params->get_info(instance.plugin, i, &info), "Cannot inspect parameter");
      if (std::string(info.name) == "Pre 1 " + (hold ? latch : std::string("Noise Learn")))
      {
        trigger.param_id = info.id;
        found = true;
        break;
      }
    }
    Check(found, "Missing trigger parameter");
  }
  // Optional on-demand numerical render audit. No events or file output in normal timings.
  std::map<int, clap_event_param_value> automation;
  for (const auto& change : settings.value("parameter_events", json::array()))
  {
    const auto* params =
      static_cast<const clap_plugin_params*>(instance.plugin->get_extension(instance.plugin, CLAP_EXT_PARAMS));
    Check(params != nullptr && !hold && !learn, "Audit automation cannot share a capture trigger");
    triggerParams = params;
    auto value = trigger;
    bool found = false;
    for (uint32_t index = 0; index < params->count(instance.plugin); ++index)
    {
      clap_param_info info{};
      Check(params->get_info(instance.plugin, index, &info), "Cannot inspect audit parameter");
      if (std::string(info.name) == change.at("name").get<std::string>())
      {
        value.param_id = info.id;
        // Enumerations (e.g. modulation targets) convert from their display text.
        const auto& requested = change.at("value");
        const auto text =
          requested.is_string() ? requested.get<std::string>() : std::to_string(requested.get<double>());
        Check(
          params->text_to_value(instance.plugin, info.id, text.c_str(), &value.value), "Cannot convert audit value");
        found = true;
        break;
      }
    }
    Check(found, "Unknown audit parameter");
    Check(automation.emplace(change.at("block").get<int>(), value).second, "Only one audit event per callback");
  }
  std::ofstream render;
  if (settings.contains("render_path"))
  {
    render.open(settings.at("render_path").get<std::string>(), std::ios::binary);
    Check(render.good(), "Cannot open numerical render output");
  }
  clap_input_events events{nullptr, [](const clap_input_events* e) -> uint32_t { return e->ctx ? 1 : 0; },
                           [](const clap_input_events* e, uint32_t) -> const clap_event_header* {
                             return static_cast<clap_event_header*>(e->ctx);
                           }};
  clap_output_events sent{nullptr, [](const clap_output_events*, const clap_event_header*) { return true; }};
  clap_event_transport transport{};
  transport.header.size = sizeof(transport);
  transport.header.type = CLAP_EVENT_TRANSPORT;
  transport.flags = CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_IS_PLAYING;
  transport.tempo = 120.;
  clap_process process{};
  process.frames_count = buffer;
  process.audio_inputs = &input;
  process.audio_outputs = &output;
  process.audio_inputs_count = process.audio_outputs_count = 1;
  process.in_events = &events;
  process.out_events = &sent;
  process.transport = &transport;
  const double deadline = 1.e6 * buffer / rate;
  const int warmBlocks =
    std::max(1, static_cast<int>(std::ceil(settings.at("warmup_seconds").get<double>() * rate / buffer)));
  const int signalBlocks =
    std::max(1, static_cast<int>(std::ceil(settings.at("seconds").get<double>() * rate / buffer * .75)));
  const int silenceBlocks =
    std::max(1, static_cast<int>(std::ceil(settings.at("seconds").get<double>() * rate / buffer * .25)));
  std::vector<double> warm, signal, silence, late;
  warm.reserve(warmBlocks);
  signal.reserve(signalBlocks);
  silence.reserve(silenceBlocks);
  const bool paced = settings.value("paced", false);
  if (paced)
    late.reserve(signalBlocks + silenceBlocks);
  double peak = 0.;
  uint64_t nonfinite = 0;
  int errors = 0;
  auto next = Clock::now();
  int64_t position = 0;
  for (int block = 0; block < warmBlocks + signalBlocks + silenceBlocks; ++block)
  {
    const bool quiet = block >= warmBlocks + signalBlocks;
    for (int i = 0; i < buffer; ++i)
    {
      const double t = static_cast<double>(position + i) / rate;
      const double envelope = .4 + .6 * std::pow(std::sin(3.141592653589793 * .8 * t), 2.);
      audio[0][i] =
        quiet
          ? 0.
          : envelope * (.12 * std::sin(6.283185307179586 * 110. * t) + .035 * std::sin(6.283185307179586 * 1760. * t));
      audio[1][i] = quiet ? 0.
                          : envelope
                              * (.1 * std::sin(6.283185307179586 * 164.813778 * t + .3)
                                 + .04 * std::sin(6.283185307179586 * 2637. * t));
      // Poison outputs so partial writes cannot pass as stable audio.
      audio[2][i] = audio[3][i] = std::numeric_limits<double>::quiet_NaN();
    }
    events.ctx = ((hold && block == warmBlocks) || (learn && block == 0)) ? &trigger.header : nullptr;
    if (auto change = automation.find(block); change != automation.end())
      events.ctx = &change->second.header;
    process.steady_time = position;
    if (paced)
      std::this_thread::sleep_until(next);
    const auto start = Clock::now();
    const auto status = instance.plugin->process(instance.plugin, &process);
    const auto end = Clock::now();
    if (events.ctx)
    {
      double value = 0.;
      const auto* applied = static_cast<const clap_event_param_value*>(events.ctx);
      Check(triggerParams->get_value(instance.plugin, applied->param_id, &value)
              && std::abs(value - applied->value) < 1.e-12,
            "Trigger/audit event not applied");
    }
    const double elapsed = std::chrono::duration<double, std::micro>(end - start).count();
    (block < warmBlocks ? warm : quiet ? silence : signal).push_back(elapsed);
    if (paced)
    {
      next +=
        std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(static_cast<double>(buffer) / rate));
      if (block >= warmBlocks)
        late.push_back(std::max(0., std::chrono::duration<double, std::micro>(end - next).count()));
    }
    if (status == CLAP_PROCESS_ERROR)
      ++errors;
    for (int ch = 2; ch < 4; ++ch)
      for (double value : audio[ch])
        if (!std::isfinite(value))
          ++nonfinite;
        else
          peak = std::max(peak, std::abs(value));
    if (render.is_open())
    {
      // Two channel-major blocks, native-endian doubles; identical layout in both audits.
      render.write(reinterpret_cast<const char*>(audio[2].data()), buffer * sizeof(double));
      render.write(reinterpret_cast<const char*>(audio[3].data()), buffer * sizeof(double));
      Check(render.good(), "Numerical render write failed");
    }
    position += buffer;
  }
  std::vector<double> all = signal;
  all.insert(all.end(), silence.begin(), silence.end());
  const auto* latency =
    static_cast<const clap_plugin_latency*>(instance.plugin->get_extension(instance.plugin, CLAP_EXT_LATENCY));
  json result{{"rate", rate},
              {"buffer", buffer},
              {"repeat", repeat},
              {"deadline_us", deadline},
              {"load_ms", loadMs},
              {"activation_ms", activationMs},
              {"latency_samples", latency ? latency->get(instance.plugin) : 0},
              {"first_callback_us", warm.front()},
              {"warmup", Statistics(warm, deadline)},
              {"signal", Statistics(signal, deadline)},
              {"silence", Statistics(silence, deadline)},
              {"timing", Statistics(all, deadline)},
              {"nonfinite_samples", nonfinite},
              {"process_errors", errors},
              {"output_peak", peak},
              {"paced", paced}};
  if (paced)
    result["paced_completion"] = {
      {"late_blocks", std::count_if(late.begin(), late.end(), [](double t) { return t > 0.; })},
      {"max_lateness_us", *std::max_element(late.begin(), late.end())}};
  return result;
}

int main(int argc, char** argv)
try
{
  std::cout << std::unitbuf;
  if (argc == 2 && std::string(argv[1]) == "--catalog")
  {
    std::cout << Catalog().dump() << '\n';
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--modules")
  {
    // Module name -> permanent ID, so scripts never hard-code renumberable IDs.
    json modules = json::object();
    for (int effect = 0; effect <= static_cast<int>(SlotEffect::Last); ++effect)
      modules[SlotEffectName(static_cast<SlotEffect>(effect))] = effect;
    std::cout << modules.dump() << '\n';
    return 0;
  }
  if (argc == 3 && std::string(argv[1]) == "--fixtures")
  {
    WriteFixtures(argv[2]);
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--self-test")
  {
    const auto stats = Statistics({1., 2., 3., 4., 10.}, 5.);
    Check(stats["deadline_misses"] == 1 && stats["p95_us"] == 10. && stats["mean_us"] == 4., "Statistics regression");
    const auto cases = Catalog();
    for (const auto& [effect, count] : std::array<std::pair<SlotEffect, int>, 5>{{{SlotEffect::Distortion, 6},
                                                                                  {SlotEffect::CosmicEcho, 6},
                                                                                  {SlotEffect::Freeze, 2},
                                                                                  {SlotEffect::JunoChorus, 3},
                                                                                  {SlotEffect::NoisePrint, 3}}})
      Check(std::count_if(
              cases.begin(), cases.end(), [&](const json& c) { return c["effect"] == static_cast<int>(effect); })
              == count,
            "Mode coverage regression");
    for (int effect = 1; effect <= static_cast<int>(SlotEffect::Last); ++effect)
      if (IsValidSlotEffect(static_cast<SlotEffect>(effect))
          && effect != static_cast<int>(SlotEffect::CabinetIR))
        Check(
          std::any_of(cases.begin(), cases.end(), [&](const json& c) { return c["effect"] == effect; }), "Missing FX");
    std::cout << "Catalog coverage and timing statistics passed: " << cases.size() << " cases\n";
    return 0;
  }
  Check(argc == 5 && std::string(argv[1]) == "--run",
        "Usage: nam-fx-performance --catalog | --fixtures DIR | --self-test | --run PLUGIN CASE.json SETTINGS.json");
#ifndef NDEBUG
  throw std::runtime_error("Build nam-fx-performance in Release for CPU measurements");
#endif
  const auto scenario = Read(argv[3]), settings = Read(argv[4]);
  Check(settings.at("seconds").get<double>() > 0. && settings.at("warmup_seconds").get<double>() > 0.
          && settings.at("repeats").get<int>() > 0,
        "Invalid duration/repeats");
  Library library(argv[2]);
  for (int rate : settings.at("rates"))
    for (int buffer : settings.at("buffers"))
      for (int repeat = 0; repeat < settings.at("repeats").get<int>(); ++repeat)
      {
        Check(rate >= 8000 && rate <= 384000 && buffer > 0 && buffer <= 8192, "Invalid rate/buffer");
        try
        {
          std::cout << Measure(library, scenario, settings, rate, buffer, repeat).dump() << '\n';
        }
        catch (const std::exception& error)
        {
          std::cout << json{{"rate", rate}, {"buffer", buffer}, {"repeat", repeat}, {"error", error.what()}}.dump()
                    << '\n';
        }
      }
  return 0;
}
catch (const std::exception& error)
{
  std::cerr << error.what() << '\n';
  return 1;
}
