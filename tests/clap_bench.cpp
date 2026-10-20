#include "ClapTestHost.h"

struct Instance
{
  Host host;
  const clap_plugin* plugin;
  const clap_plugin_state* state;
  Instance(const clap_plugin_factory* factory)
  {
    plugin = factory->create_plugin(factory, &host.host, factory->get_plugin_descriptor(factory, 0)->id);
    assert(plugin && plugin->init(plugin));
    state = static_cast<const clap_plugin_state*>(plugin->get_extension(plugin, CLAP_EXT_STATE));
  }
  ~Instance() { plugin->destroy(plugin); }
  void Load(const std::string& drive, const std::string& amp, bool original)
  {
    Stream stream;
    if (original)
    {
      stream.string("###NeuralAmpModeler###");
      stream.string("0.8.0");
      stream.string(amp);
      stream.string("");
      // Original parameter order; gate/EQ/IR/calibration/normalization disabled, unity gain.
      for (double v : {0., -80., 5., 5., 5., 0., 0., 0., 0., 0., 12., 0., 1.})
      {
        auto* p = reinterpret_cast<uint8_t*>(&v);
        stream.bytes.insert(stream.bytes.end(), p, p + sizeof(v));
      }
    }
    else
    {
      Stream saved;
      clap_ostream out{&saved, Stream::write};
      assert(state->save(plugin, &out));
      auto config = saved.config();
      config["DrivePath"] = drive;
      config["NAMPath"] = amp;
      config["NoiseGateActive"] = 0.;
      config["ToneStack"] = 0.;
      config["IRToggle"] = 0.;
      config["OutputMode"] = 0.;
      stream.string("###NAMDual-v1###");
      stream.string(config.dump());
    }
    clap_istream in{&stream, Stream::read};
    assert(state->load(plugin, &in));
  }
};

int main(int argc, char** argv)
{
  // Run separately for the original plugin (1 or 2 instances), and the dual prototype (1 instance).
  assert(argc == 6);
  const bool original = std::string(argv[2]) == "original";
  const int count = std::stoi(argv[3]);
  assert(count == 1 || count == 2);
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
  {
    Instance one(factory), two(factory);
    // Upstream's loader reads host sample rate/block size, so establish those before recall.
    assert(one.plugin->activate(one.plugin, 48000., 128, 128));
    assert(two.plugin->activate(two.plugin, 48000., 128, 128));
    one.Load(original ? "" : argv[4], original && count == 2 ? argv[4] : argv[5], original);
    if (count == 2)
      two.Load("", argv[5], original);
    one.plugin->deactivate(one.plugin);
    two.plugin->deactivate(two.plugin);
    std::array<double, 128> input{}, middle{}, output{};
    double *ins[] = {input.data(), input.data()}, *mids[] = {middle.data(), middle.data()},
           *outs[] = {output.data(), output.data()};
    clap_audio_buffer in{nullptr, ins, 2, 0, 0}, mid{nullptr, mids, 2, 0, 0}, out{nullptr, outs, 2, 0, 0};
    clap_input_events events{nullptr, [](const clap_input_events*) -> uint32_t { return 0; },
                             [](const clap_input_events*, uint32_t) -> const clap_event_header* { return nullptr; }};
    clap_output_events sent{nullptr, [](const clap_output_events*, const clap_event_header*) { return true; }};
    clap_process process{};
    process.frames_count = 128;
    process.audio_inputs_count = process.audio_outputs_count = 1;
    process.in_events = &events;
    process.out_events = &sent;
    for (double rate : {48000., 44100., 96000.})
    {
      assert(one.plugin->activate(one.plugin, rate, 128, 128));
      assert(one.plugin->start_processing(one.plugin));
      if (count == 2)
      {
        assert(two.plugin->activate(two.plugin, rate, 128, 128));
        assert(two.plugin->start_processing(two.plugin));
      }
      for (int i = 0; i < 128; ++i)
        input[i] = .1 * std::sin(i * .03);
      double fastest = 1e9;
      for (int run = 0; run < 4; ++run)
      {
        auto start = std::chrono::steady_clock::now();
        for (int block = 0; block < 1000; ++block)
        {
          process.audio_inputs = &in;
          process.audio_outputs = count == 2 ? &mid : &out;
          assert(one.plugin->process(one.plugin, &process) != CLAP_PROCESS_ERROR);
          if (count == 2)
          {
            process.audio_inputs = &mid;
            process.audio_outputs = &out;
            assert(two.plugin->process(two.plugin, &process) != CLAP_PROCESS_ERROR);
          }
        }
        auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        if (run > 0)
          fastest = std::min(fastest, seconds);
      }
      auto latency = [](Instance& i) {
        return static_cast<const clap_plugin_latency*>(i.plugin->get_extension(i.plugin, CLAP_EXT_LATENCY))
          ->get(i.plugin);
      };
      std::cout << (original ? "original" : "dual") << "," << count << "," << rate << "," << fastest * 1000. << ","
                << (fastest / (128000. / rate) * 100.) << "," << latency(one) + (count == 2 ? latency(two) : 0) << "\n";
      one.plugin->stop_processing(one.plugin);
      one.plugin->deactivate(one.plugin);
      if (count == 2)
      {
        two.plugin->stop_processing(two.plugin);
        two.plugin->deactivate(two.plugin);
      }
    }
  }
  entry->deinit();
#ifdef _WIN32
  FreeLibrary(library);
#else
  dlclose(library);
#endif
}
