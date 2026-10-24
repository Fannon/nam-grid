#pragma once

template<class Save, class Load>
void TestRoadmapClap(const clap_plugin* plugin, json cleared, Save save, Load load)
{
  const auto* tail = static_cast<const clap_plugin_tail*>(plugin->get_extension(plugin, CLAP_EXT_TAIL));
  const auto* params = static_cast<const clap_plugin_params*>(plugin->get_extension(plugin, CLAP_EXT_PARAMS));
  std::array<double, 512> l{}, r{}, outL{}, outR{};
  double* inputs[]{l.data(), r.data()}, *outputs[]{outL.data(), outR.data()};
  clap_audio_buffer in{}; in.data64 = inputs; in.channel_count = 2;
  clap_audio_buffer out{}; out.data64 = outputs; out.channel_count = 2;
  clap_input_events empty{nullptr, [](const clap_input_events*) -> uint32_t { return 0; },
    [](const clap_input_events*, uint32_t) -> const clap_event_header* { return nullptr; }};
  clap_output_events sent{nullptr, [](const clap_output_events*, const clap_event_header*) { return true; }};
  clap_process process{}; process.frames_count = 512; process.audio_inputs_count = process.audio_outputs_count = 1;
  process.audio_inputs = &in; process.audio_outputs = &out; process.in_events = &empty; process.out_events = &sent;
  for (int effect = static_cast<int>(SlotEffect::Rotary); effect <= static_cast<int>(SlotEffect::Distortion); ++effect)
  {
    auto config = cleared;
    config["SlotEffects"][0] = effect;
    config["Pre 1 Utility Fine Delay"] = 1.25;
    config["Pre 1 Rotary Fast Rate"] = 7.3;
    config["Pre 1 Octaver 2 Oct Down"] = 25.;
    config["Pre 1 Doubler Detune"] = 14.;
    config["Pre 1 Dual Delay Ratio"] = 1.75;
    config["Pre 1 Shimmer Pitch"] = 7.; config["Pre 1 Juno Mode"] = 2.; config["Pre 1 Shifter Frequency"] = 250.;
    if (effect == static_cast<int>(SlotEffect::Distortion))
    {
      config["Pre 1 Distortion Style"] = 3.; config["Pre 1 Distortion Drive"] = 24.;
      config["Pre 1 Distortion High Cut"] = 3500.; config["Pre 1 Distortion Bias"] = 30.;
      config["Pre 1 Distortion Mix"] = 75.; config["Pre 1 Distortion Output"] = -9.;
    }
    config["Lane A Level"] = -2.; config["Lane Timing"] = .25;
    config["Mod 1 Source"] = grid_performance::SourceLFO; config["Mod 1 Slot"] = 0.;
    config["Mod 1 Destination"] = parameter_layout::standardControls + uUtilityBalance; config["Mod 1 Depth"] = 20.;
    load(config); assert(save().config() == config);
    for (double rate : {44100., 48000., 96000.})
    {
      assert(plugin->activate(plugin, rate, 1, 512)); assert(plugin->start_processing(plugin));
      for (int block = 0; block < 16; ++block)
      {
        process.frames_count = std::array<int, 4>{1, 127, 512, 64}[block % 4];
        for (int i = 0; i < process.frames_count; ++i)
        { l[i] = .15 * std::sin((block * 512 + i) * .03); r[i] = .1 * std::cos((block * 512 + i) * .05); }
        assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
        for (int i = 0; i < process.frames_count; ++i) assert(std::isfinite(outL[i]) && std::isfinite(outR[i]));
      }
      plugin->stop_processing(plugin); plugin->deactivate(plugin);
    }
  }
  const auto set = [&](const char* name, bool enabled) {
    clap_param_info info{}; bool found = false;
    for (uint32_t i = 0; i < params->count(plugin); ++i)
      if (params->get_info(plugin, i, &info) && std::strcmp(info.name, name) == 0) { found = true; break; }
    assert(found);
    clap_event_param_value value{}; value.header.size = sizeof(value); value.header.type = CLAP_EVENT_PARAM_VALUE;
    value.param_id = info.id; value.value = enabled ? 1. : 0.; value.note_id = value.port_index = value.channel = value.key = -1;
    clap_input_events events{&value, [](const clap_input_events*) -> uint32_t { return 1; },
      [](const clap_input_events* e, uint32_t) -> const clap_event_header* { return &static_cast<clap_event_param_value*>(e->ctx)->header; }};
    params->flush(plugin, &events, &sent);
    assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
  };
  for (auto effect : {SlotEffect::Freeze, SlotEffect::ShimmerReverb})
    for (int slot : {0, 15})
    {
      const std::string label = slot == 0 ? "Pre 1" : "Post 4";
      const auto hold = label + (effect == SlotEffect::Freeze ? " Freeze Hold" : " Shimmer Freeze");
      const auto unusedHold = label + (effect == SlotEffect::Freeze ? " Shimmer Freeze" : " Freeze Hold");
      auto config = cleared; config["SlotEffects"][slot] = static_cast<int>(effect);
      config[hold] = 1.; // Held states must also report infinity immediately after activation.
      load(config); assert(plugin->activate(plugin, 48000., 1, 512));
      assert(tail->get(plugin) == std::numeric_limits<uint32_t>::max());
      assert(plugin->start_processing(plugin));
      set(hold.c_str(), false); assert(tail->get(plugin) != std::numeric_limits<uint32_t>::max());
      set(unusedHold.c_str(), true); assert(tail->get(plugin) != std::numeric_limits<uint32_t>::max());
      set(hold.c_str(), true); assert(tail->get(plugin) == std::numeric_limits<uint32_t>::max());
      const auto active = slot == 0 ? std::string("Pre 1 Enabled") : label + " Enabled";
      set(active.c_str(), false); assert(tail->get(plugin) != std::numeric_limits<uint32_t>::max());
      set(active.c_str(), true); assert(tail->get(plugin) == std::numeric_limits<uint32_t>::max());
      plugin->stop_processing(plugin); plugin->deactivate(plugin);
    }
  load(cleared);
  std::cout << "Roadmap CLAP: module recall, routing/modulation and Freeze/Shimmer tails passed\n";
}
