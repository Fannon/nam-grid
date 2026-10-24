#pragma once
// Modulation matrix through the real plugin: conversion of 0.1 fixed assignments,
// generic and routing destinations, and state-only selectors.

template<class Save, class Load, class LoadRaw>
void TestModulationMatrixClap(const clap_plugin* plugin, json cleared, Save save, Load load, LoadRaw loadRaw)
{
  using namespace grid_performance;
  const auto* params = static_cast<const clap_plugin_params*>(plugin->get_extension(plugin, CLAP_EXT_PARAMS));
  std::array<double, 512> l{}, r{}, outL{}, outR{};
  double* inputs[]{l.data(), r.data()}, *outputs[]{outL.data(), outR.data()};
  clap_audio_buffer in{}; in.data64 = inputs; in.channel_count = 2;
  clap_audio_buffer out{}; out.data64 = outputs; out.channel_count = 2;
  clap_input_events empty{nullptr, [](const clap_input_events*) -> uint32_t { return 0; },
    [](const clap_input_events*, uint32_t) -> const clap_event_header* { return nullptr; }};
  clap_output_events sent{nullptr, [](const clap_output_events*, const clap_event_header*) { return true; }};
  clap_process process{}; process.audio_inputs_count = process.audio_outputs_count = 1;
  process.audio_inputs = &in; process.audio_outputs = &out; process.in_events = &empty; process.out_events = &sent;
  // Uneven host blocks: the 16-sample control clock must not depend on them.
  const auto render = [&] {
    std::vector<double> result;
    assert(plugin->activate(plugin, 48000., 1, 512)); assert(plugin->start_processing(plugin));
    int position = 0;
    for (int block = 0; block < 48; ++block)
    {
      process.frames_count = std::array<int, 4>{512, 127, 1, 256}[block % 4];
      for (uint32_t i = 0; i < process.frames_count; ++i, ++position)
      { l[i] = .2 * std::sin(position * .031); r[i] = .15 * std::sin(position * .017); }
      assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
      for (uint32_t i = 0; i < process.frames_count; ++i) { result.push_back(outL[i]); result.push_back(outR[i]); }
    }
    plugin->stop_processing(plugin); plugin->deactivate(plugin);
    return result;
  };
  const auto difference = [](const std::vector<double>& a, const std::vector<double>& b) {
    double largest = 0.;
    for (size_t i = 0; i < a.size(); ++i) largest = std::max(largest, std::abs(a[i] - b[i]));
    return largest;
  };
  const auto rms = [](const std::vector<double>& a) {
    double sum = 0.; for (double x : a) sum += x * x; return std::sqrt(sum / a.size());
  };
  // A 0.1-era state: no matrix controls, schema below the matrix.
  const auto older = [](json config) {
    config["schema"] = 2;
    for (const auto& p : parameters) if (p.introducedIn >= 3) config.erase(p.name);
    return config;
  };
  const auto row = [](int index, int field) { return std::string(parameters[RowParam(index, field)].name); };

  // Each fixed target sounds the same after conversion to a matrix row.
  struct Case { SlotEffect effect; int target; double depth; double tolerance; };
  for (const auto& test : {Case{SlotEffect::Utility, 3, 60., 1.e-12}, Case{SlotEffect::SynthFilter, 2, -45., 1.e-9},
                           Case{SlotEffect::RingMod, 8, 70., 1.e-9},
                           Case{SlotEffect::AirHall, 5, 50., 1.e-12}})
  {
    auto config = cleared;
    config["SlotEffects"][4] = static_cast<int>(test.effect);
    config["LFO Slot"] = 4.; config["LFO Target"] = test.target; config["LFO Depth"] = test.depth; config["LFO Rate"] = 3.;
    config["Parallel Blend"] = 0.;
    load(config);
    const auto fixed = render();
    auto converted = older(config);
    load(converted);
    const auto state = save().config();
    assert(state["schema"] == kEffectStateSchema && state["LFO Target"] == 0.);
    assert(state[row(0, RowSource)] == SourceLFO && state[row(0, RowSlot)] == 4.);
    assert(state[row(1, RowSource)] == SourceNone);
    const auto matrix = render();
    if (difference(fixed, matrix) > test.tolerance)
      std::cerr << "Converted target " << test.target << " differs by " << difference(fixed, matrix) << '\n';
    assert(difference(fixed, matrix) <= test.tolerance);
    config["LFO Depth"] = 0.;
    load(config);
    assert(difference(render(), fixed) > 1.e-4); // The case modulates audibly.
  }
  // Older scenes convert too, and recall all eight rows.
  {
    auto config = cleared;
    config["Macro One Slot"] = 7.; config["Macro One Target"] = 7.; config["Macro One Depth"] = 40.;
    auto scene = json::object();
    for (const auto& p : parameters) if (p.introducedIn < 3) scene[p.name] = config[p.name];
    config["PerformanceScenes"][1] = scene;
    load(config = older(config));
    const auto state = save().config();
    for (const auto& values : {state, state["PerformanceScenes"][1]})
    {
      assert(values["Macro One Target"] == 0. && values[row(0, RowSource)] == SourceMacro1);
      assert(values[row(0, RowSlot)] == gridSlot && values[row(0, RowDestination)] == GridBlend && values[row(0, RowDepth)] == 40.);
      assert(values.contains(row(7, RowVia)));
    }
    assert(state["PerformanceScenes"][0].empty());
  }
  // Any modulatable slot control, and the lane levels: Macro 1 turns Utility down.
  {
    auto config = cleared;
    config["SlotEffects"][0] = static_cast<int>(SlotEffect::Utility);
    load(config);
    const auto dry = render();
    config["Macro One"] = 100.;
    config[row(2, RowSource)] = SourceMacro1; config[row(2, RowSlot)] = 0.;
    config[row(2, RowDestination)] = parameter_layout::standardControls + uUtilityLevel; config[row(2, RowDepth)] = -25.;
    load(config);
    assert(save().config() == config);
    const auto quieter = render();
    assert(rms(quieter) < rms(dry) * .5);
    // A second row on the same control adds.
    config[row(5, RowSource)] = SourceMacro1; config[row(5, RowSlot)] = 0.;
    config[row(5, RowDestination)] = parameter_layout::standardControls + uUtilityLevel; config[row(5, RowDepth)] = 25.;
    load(config);
    assert(difference(render(), dry) < 1.e-9);
    // Routing destination: lane A's level, with the blend fully on A.
    config = cleared; config["Parallel Blend"] = 0.; config["Macro One"] = 100.;
    config[row(0, RowSource)] = SourceMacro1; config[row(0, RowSlot)] = gridSlot;
    config[row(0, RowDestination)] = GridLaneA; config[row(0, RowDepth)] = -50.;
    load(config);
    assert(rms(render()) < rms(dry) * .1);
    // Via scales the row: Macro 2 at zero silences it.
    config[row(0, RowVia)] = SourceMacro2; config["Macro Two"] = 0.;
    load(config);
    assert(difference(render(), dry) < 1.e-9);
  }
  // Selectors are integer state, never host automation; Depth remains automatable.
  {
    for (const auto& [name, value] : {std::pair{row(0, RowSource), 1.5}, std::pair{row(0, RowSlot), 17.},
                                     std::pair{row(3, RowVia), -1.}, std::pair{row(4, RowDestination), 2.25}})
    {
      auto bad = cleared; bad[name] = value;
      assert(!loadRaw(bad));
    }
    for (uint32_t i = 0; i < params->count(plugin); ++i)
    {
      clap_param_info info{};
      assert(params->get_info(plugin, i, &info));
      const std::string name = info.name;
      if (name.rfind("Mod ", 0) != 0 && name != "LFO Shape" && name != "Macro Three" && name != "Macro Four") continue;
      const bool depth = name.size() > 6 && name.substr(name.size() - 6) == " Depth";
      const bool automatable = info.flags & CLAP_PARAM_IS_AUTOMATABLE;
      assert(automatable == (depth || name.rfind("Mod ", 0) != 0));
    }
  }
  load(cleared);
  std::cout << "Modulation matrix CLAP: exact 0.1 conversion, scenes, generic and routing destinations, Via and state-only selectors passed\n";
}
