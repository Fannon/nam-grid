#pragma once

// Actual DSP must see host automation after its slot snapshot has been cached.
template <class Load>
void TestCachedControlsClap(const clap_plugin* plugin, json cleared, Load load,
                            const std::filesystem::path& fixtures)
{
  const auto path = fixtures / "cache-identity.nam";
  std::ofstream(path) << json{{"version", "0.7.0"},
                              {"architecture", "Linear"},
                              {"config", {{"receptive_field", 1}, {"bias", true}}},
                              {"weights", {1., 0.}},
                              {"sample_rate", 48000.}};
  const auto* params = static_cast<const clap_plugin_params*>(plugin->get_extension(plugin, CLAP_EXT_PARAMS));
  assert(params);
  std::array<double, 64> inputL{}, inputR{}, outputL{}, outputR{};
  double *inputs[]{inputL.data(), inputR.data()}, *outputs[]{outputL.data(), outputR.data()};
  clap_audio_buffer input{nullptr, inputs, 2, 0, 0}, output{nullptr, outputs, 2, 0, 0};
  clap_input_events empty{nullptr, [](const clap_input_events*) -> uint32_t { return 0; },
                          [](const clap_input_events*, uint32_t) -> const clap_event_header* { return nullptr; }};
  clap_output_events sent{nullptr, [](const clap_output_events*, const clap_event_header*) { return true; }};
  clap_process process{};
  process.frames_count = 64;
  process.audio_inputs = &input;
  process.audio_outputs = &output;
  process.audio_inputs_count = process.audio_outputs_count = 1;
  process.in_events = &empty;
  process.out_events = &sent;
  const auto automate = [&](const std::string& name, double value) {
    clap_param_info info{};
    bool found = false;
    for (uint32_t i = 0; i < params->count(plugin); ++i)
      if (params->get_info(plugin, i, &info) && name == info.name)
      {
        found = true;
        break;
      }
    assert(found);
    clap_event_param_value event{};
    event.header.size = sizeof(event);
    event.header.type = CLAP_EVENT_PARAM_VALUE;
    event.param_id = info.id;
    // iPlug2 exposes double CLAP parameters in normalized host units.
    assert(params->text_to_value(plugin, info.id, std::to_string(value).c_str(), &event.value));
    event.note_id = event.port_index = event.channel = event.key = -1;
    clap_input_events update{&event, [](const clap_input_events*) -> uint32_t { return 1; },
                             [](const clap_input_events* e, uint32_t) -> const clap_event_header* {
                               return &static_cast<clap_event_param_value*>(e->ctx)->header;
                             }};
    params->flush(plugin, &update, &sent);
  };
  const auto level = [&](int blocks) {
    double energy = 0.;
    for (int block = 0; block < blocks; ++block)
    {
      for (int i = 0; i < 64; ++i)
      {
        inputL[i] = .1 * std::sin(6.283185307179586 * i / 64.);
        inputR[i] = -.7 * inputL[i];
      }
      assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
      for (int i = 0; i < 64; ++i)
      {
        assert(std::isfinite(outputL[i]) && std::isfinite(outputR[i]));
        energy += outputL[i] * outputL[i];
      }
    }
    return std::sqrt(energy / (blocks * 64));
  };
  for (auto effect : {SlotEffect::NAM, SlotEffect::Utility})
    for (int slot = 0; slot < 16; ++slot)
    {
      const auto prefix = std::string(slot < 4    ? "Pre "
                                      : slot < 8  ? "A "
                                      : slot < 12 ? "B "
                                                  : "Post ")
                          + std::to_string(slot % 4 + 1);
      auto config = cleared;
      config["Output"] = config["Input"] = config["Pre 2 NAM Input"] = config["Tuner"] = 0.;
      config["A/B Blend"] = slot >= 8 && slot < 12 ? 100. : 0.;
      config["SlotEffects"][slot] = static_cast<int>(effect);
      config[prefix + " Utility Output"] = 0.;
      const std::vector<std::string> names = effect == SlotEffect::Utility
                                               ? std::vector<std::string>{prefix + " Utility Output"}
                                               : std::vector<std::string>{prefix + " NAM Input", prefix + " NAM Output"};
      for (const auto& name : names)
        config[name] = 0.;
      config[prefix + " Enabled"] = 1.;
      if (effect == SlotEffect::NAM)
      {
        config["SlotPaths"][slot] = path.string();
      }
      load(config);
      assert(plugin->activate(plugin, 48000., 1, 64));
      assert(plugin->start_processing(plugin));
      level(128);
      const double reference = level(32);
      assert(reference > .001);
      for (const auto& name : names)
      {
        automate(name, -6.);
        level(128);
        const double ratio = level(32) / reference;
        if (std::abs(ratio - std::pow(10., -.3)) >= .002)
          std::cerr << "Cached automation mismatch: effect=" << static_cast<int>(effect) << " slot=" << slot
                    << " parameter=" << name << " ratio=" << ratio << " reference=" << reference << "\n";
        assert(std::abs(ratio - std::pow(10., -.3)) < .002);
        automate(name, 0.);
        level(128);
        assert(std::abs(level(32) / reference - 1.) < .002);
      }
      plugin->stop_processing(plugin);
      plugin->deactivate(plugin);
    }
  load(cleared);
  std::cout << "Cached controls: live input/trim/utility automation reaches DSP in all 16 slots\n";
}
