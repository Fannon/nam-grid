#pragma once
// Modulation matrix through the real plugin: host automation of depths, generic and
// routing destinations, retrigger and state-only selectors.

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
  // MIDI note-ons at chosen absolute frames, delivered in whichever host block holds them.
  clap_event_note note{};
  note.header.size = sizeof(note); note.header.type = CLAP_EVENT_NOTE_ON; note.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
  note.note_id = -1; note.port_index = 0; note.channel = 0; note.key = 60; note.velocity = 1.;
  std::vector<clap_event_note> blockNotes;
  clap_input_events notes{&blockNotes,
    [](const clap_input_events* e) -> uint32_t { return static_cast<uint32_t>(static_cast<std::vector<clap_event_note>*>(e->ctx)->size()); },
    [](const clap_input_events* e, uint32_t i) -> const clap_event_header* {
      return &(*static_cast<std::vector<clap_event_note>*>(e->ctx))[i].header; }};
  // Uneven host blocks by default: the 16-sample control clock must not depend on them.
  const auto render = [&](std::vector<int> noteFrames = {}, std::vector<int> sizes = {512, 127, 1, 256}) {
    std::vector<double> result;
    assert(plugin->activate(plugin, 48000., 1, 512)); assert(plugin->start_processing(plugin));
    constexpr int total = 12 * (512 + 127 + 1 + 256);
    for (int position = 0, block = 0; position < total; ++block)
    {
      process.frames_count = std::min(sizes[block % sizes.size()], total - position);
      blockNotes.clear();
      for (const int frame : noteFrames)
        if (frame >= position && frame < position + static_cast<int>(process.frames_count))
        {
          note.header.time = static_cast<uint32_t>(frame - position);
          blockNotes.push_back(note);
        }
      process.in_events = blockNotes.empty() ? &empty : &notes;
      for (uint32_t i = 0; i < process.frames_count; ++i)
      { l[i] = .2 * std::sin((position + i) * .031); r[i] = .15 * std::sin((position + i) * .017); }
      assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
      for (uint32_t i = 0; i < process.frames_count; ++i) { result.push_back(outL[i]); result.push_back(outR[i]); }
      position += static_cast<int>(process.frames_count);
    }
    plugin->stop_processing(plugin); plugin->deactivate(plugin);
    process.in_events = &empty;
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
  const auto row = [](int index, int field) { return std::string(parameters[RowParam(index, field)].name); };

  // Old rigs acquire bank 2 defaults even after editing it. Current states require
  // every new field, and both bank settings and routing survive scene/state recall.
  {
    auto configured = cleared;
    for (int p = LFO2Shape; p < Count; ++p) configured[parameters[p].name] = parameters[p].maximum;
    configured[row(0, RowSource)] = SourceLFO2;
    configured[row(0, RowVia)] = SourceEnv2;
    configured[row(1, RowSource)] = SourceSeq2;
    configured["PerformanceScenes"][0] = {{"LFO 2 Rate", 3.}, {"Envelope 2 Listen At", 16.},
                                         {"Sequencer 2 Step 8", 13.}, {row(0, RowSource), SourceSeq2}};
    load(configured);
    assert(save().config() == configured);
    for (int schema = kInitialEffectStateSchema; schema < kEffectStateSchema; ++schema)
    {
      auto old = cleared;
      old["schema"] = schema;
      old["LFO Rate"] = 1.7; old["Envelope Attack"] = 21.; old["Sequencer Step 3"] = 63.;
      old["Macro 4"] = 72.; old[row(0, RowSource)] = SourceMacro4;
      old["PerformanceScenes"][0] = {{"LFO Rate", 2.3}, {row(0, RowSource), SourceMacro2}};
      for (int p = 0; p < Count; ++p)
        if (parameters[p].introducedIn > schema) old.erase(parameters[p].name);
      for (int p = 0; p < kUtilityControls; ++p)
        if (kEffectParameters[p].introducedIn > schema)
          for (int slot = 0; slot < 16; ++slot) old.erase(GridSlotName(slot) + " " + kEffectParameters[p].name);
      load(old);
      const auto upgraded = save().config();
      for (int p = LFO2Shape; p < Count; ++p) assert(upgraded[parameters[p].name] == parameters[p].initial);
      assert(upgraded["LFO Rate"] == 1.7 && upgraded["Envelope Attack"] == 21. && upgraded["Sequencer Step 3"] == 63.);
      assert(upgraded["Macro 4"] == 72. && upgraded[row(0, RowSource)] == SourceMacro4);
      assert(upgraded["PerformanceScenes"] == old["PerformanceScenes"]);
    }
    load(configured);
    for (int p = LFO2Shape; p < Count; ++p)
    {
      auto bad = configured; bad.erase(parameters[p].name);
      assert(!loadRaw(bad) && save().config() == configured);
    }
    const std::pair<const char*, double> invalidValues[]{{"LFO 2 Shape", .5}, {"Envelope 2 Listen At", 17.},
                                                            {"Sequencer 2 Retrigger", 3.}};
    for (const auto& [name, value] : invalidValues)
    {
      auto bad = configured; bad[name] = value;
      assert(!loadRaw(bad) && save().config() == configured);
      bad = configured; bad["PerformanceScenes"][0][name] = value;
      assert(!loadRaw(bad) && save().config() == configured);
    }
    const int invalidSources[]{SourceCount, -1};
    for (int source : invalidSources)
    {
      auto bad = configured; bad[row(0, RowSource)] = source;
      assert(!loadRaw(bad) && save().config() == configured);
      bad = configured; bad[row(0, RowVia)] = source;
      assert(!loadRaw(bad) && save().config() == configured);
    }
  }

  // Bank 2 follows the same timeline across uneven host blocks. Giving it bank
  // 1's settings and destination reproduces bank 1's output for every source type.
  const std::pair<int, int> sourcePairs[]{{SourceLFO1, SourceLFO2}, {SourceEnv1, SourceEnv2},
                                          {SourceSeq1, SourceSeq2}};
  for (const auto [first, second] : sourcePairs)
  {
    auto config = cleared;
    config["SlotEffects"][0] = static_cast<int>(SlotEffect::Utility);
    config["LFO Rate"] = config["LFO 2 Rate"] = 2.;
    config["LFO Shape"] = config["LFO 2 Shape"] = 3.;
    config["LFO Retrigger"] = config["LFO 2 Retrigger"] = RetriggerMidi;
    config["LFO One Shot"] = config["LFO 2 One Shot"] = 1.;
    config["Sequencer Retrigger"] = config["Sequencer 2 Retrigger"] = RetriggerMidi;
    config[row(0, RowSource)] = first; config[row(0, RowSlot)] = 0.;
    config[row(0, RowDestination)] = parameter_layout::standardControls + uUtilityLevel;
    config[row(0, RowDepth)] = -25.;
    load(config);
    const auto expected = render({0, 256, 1900});
    config[row(0, RowSource)] = second;
    load(config);
    const auto actual = render({0, 256, 1900});
    assert(difference(expected, actual) < 1.e-12);
    assert(difference(actual, render({0, 256, 1900}, {37, 500})) < 1.e-12);
    if (second != SourceEnv2)
    {
      config[row(0, RowSource)] = SourceNone;
      load(config);
      assert(difference(actual, render()) > 1.e-3);
    }
  }

  // Both taps run simultaneously: Env 1 hears the quiet first slot; Env 2 hears
  // the restored signal after the next slot. Swapping their routes changes audio.
  {
    auto config = cleared;
    for (int slot : {0, 12}) config["SlotEffects"][slot] = static_cast<int>(SlotEffect::Utility);
    config["SlotEffects"][1] = static_cast<int>(SlotEffect::Boost);
    config["Pre 1 Utility Output"] = -12.; config["Pre 2 Boost Gain"] = 18.;
    config["Envelope Sensitivity"] = 12.; config["Envelope 2 Sensitivity"] = 12.;
    config["Envelope Listen At"] = 1.; config["Envelope 2 Listen At"] = 2.;
    config[row(0, RowSource)] = SourceEnv1; config[row(0, RowSlot)] = 12.;
    config[row(0, RowDestination)] = parameter_layout::standardControls + uUtilityLevel;
    config[row(0, RowDepth)] = -50.; config[row(0, RowVia)] = SourceEnv2;
    load(config);
    const auto quiet = render();
    config[row(0, RowSource)] = SourceEnv2; config[row(0, RowVia)] = SourceNone;
    load(config);
    const auto loud = render();
    assert(rms(loud) < .4 * rms(quiet));
    assert(difference(loud, render({}, {37, 500})) < 1.e-12);
  }

  // A row's depth follows host automation like any other automatable parameter.
  {
    auto config = cleared;
    config["SlotEffects"][4] = static_cast<int>(SlotEffect::Utility);
    config["Macro 1"] = 100.; config["A/B Blend"] = 0.;
    config[row(0, RowSource)] = SourceMacro1; config[row(0, RowSlot)] = 4.;
    config[row(0, RowDestination)] = parameter_layout::standardControls + uUtilityBalance; config[row(0, RowDepth)] = 20.;
    load(config);
    clap_param_info info{};
    for (uint32_t i = 0; i < params->count(plugin); ++i)
      if (params->get_info(plugin, i, &info) && std::strcmp(info.name, row(0, RowDepth).c_str()) == 0) break;
    clap_event_param_value event{};
    event.header.size = sizeof(event); event.header.type = CLAP_EVENT_PARAM_VALUE; event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    event.param_id = info.id; event.value = .75; // +50% in the normalized CLAP range
    event.note_id = event.port_index = event.channel = event.key = -1;
    clap_input_events automation{&event, [](const clap_input_events*) -> uint32_t { return 1; },
      [](const clap_input_events* e, uint32_t) -> const clap_event_header* { return &static_cast<clap_event_param_value*>(e->ctx)->header; }};
    clap_output_events ignored{nullptr, [](const clap_output_events*, const clap_event_header*) { return true; }};
    params->flush(plugin, &automation, &ignored);
    const auto automated = render();
    assert(save().config()[row(0, RowDepth)] == 50.);
    config[row(0, RowDepth)] = 50.;
    load(config);
    assert(difference(automated, render()) == 0.);
  }
  // Any modulatable slot control, and the lane levels: Macro 1 turns Utility down.
  {
    auto config = cleared;
    config["SlotEffects"][0] = static_cast<int>(SlotEffect::Utility);
    load(config);
    const auto dry = render();
    config["Macro 1"] = 100.;
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
    config = cleared; config["A/B Blend"] = 0.; config["Macro 1"] = 100.;
    config[row(0, RowSource)] = SourceMacro1; config[row(0, RowSlot)] = gridSlot;
    config[row(0, RowDestination)] = GridLaneA; config[row(0, RowDepth)] = -50.;
    load(config);
    assert(rms(render()) < rms(dry) * .1);
    // Via scales the row: Macro 2 at zero silences it.
    config[row(0, RowVia)] = SourceMacro2; config["Macro 2"] = 0.;
    load(config);
    assert(difference(render(), dry) < 1.e-9);
  }
  // The follower listens after a chosen slot: here after a -40 dB Utility, it hardly ducks.
  {
    auto config = cleared;
    for (int slot : {0, 1, 12}) config["SlotEffects"][slot] = static_cast<int>(SlotEffect::Utility);
    config["Pre 1 Utility Output"] = -40.; config["Pre 2 Utility Output"] = 40.;
    load(config);
    const auto dry = render();
    config[row(0, RowSource)] = SourceEnvelope; config[row(0, RowSlot)] = 12.;
    config[row(0, RowDestination)] = parameter_layout::standardControls + uUtilityLevel; config[row(0, RowDepth)] = -50.;
    config["Envelope Sensitivity"] = 12.;
    load(config);
    const auto input = render();
    config["Envelope Listen At"] = 1.; // After Pre 1
    load(config);
    const auto tapped = render();
    assert(rms(input) < .3 * rms(dry) && rms(tapped) > .9 * rms(dry));
  }
  // MIDI: the plugin takes notes; a note-on restarts a one-shot LFO at its frame.
  {
    const auto* ports = static_cast<const clap_plugin_note_ports*>(plugin->get_extension(plugin, CLAP_EXT_NOTE_PORTS));
    assert(ports && ports->count(plugin, true) == 1 && ports->count(plugin, false) == 0);
    auto config = cleared;
    config["SlotEffects"][0] = static_cast<int>(SlotEffect::Utility);
    config["LFO Shape"] = 3.; config["LFO One Shot"] = 1.; config["LFO Retrigger"] = RetriggerMidi; config["LFO Rate"] = 2.;
    config[row(0, RowSource)] = SourceLFO; config[row(0, RowSlot)] = 0.;
    config[row(0, RowDestination)] = parameter_layout::standardControls + uUtilityLevel; config[row(0, RowDepth)] = -40.;
    load(config);
    const int at = 2 * (512 + 127 + 1 + 256) + 100; // Inside a 512-frame block.
    const auto idle = render(), played = render({at});
    for (size_t i = 0; i < 2 * static_cast<size_t>(at); ++i) assert(played[i] == idle[i]);
    assert(difference(played, idle) > 1.e-3);
    // Every note-on counts, also several in one host block: the result follows the
    // timeline, not the host's block size.
    config["LFO Rate"] = 20.;
    load(config);
    const auto both = render({0, 256}, {512});
    assert(difference(both, render({0}, {512})) > 1.e-3);
    assert(difference(both, render({0, 256}, {256})) < 1.e-12 && difference(both, render({0, 256}, {37, 500})) < 1.e-12);
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
      const bool selector = name == "LFO Shape" || name == "Envelope Listen At" || name == "LFO Retrigger" || name == "Sequencer Retrigger"
                            || name == "LFO 2 Shape" || name == "Envelope 2 Listen At" || name == "LFO 2 Retrigger" || name == "Sequencer 2 Retrigger";
      const bool control = name == "Macro 3" || name == "Macro 4" || name == "Envelope Threshold"
                           || name == "Envelope Curve" || name == "LFO One Shot";
      if (name.rfind("Mod ", 0) != 0 && !selector && !control) continue;
      const bool depth = name.size() > 6 && name.substr(name.size() - 6) == " Depth";
      const bool automatable = info.flags & CLAP_PARAM_IS_AUTOMATABLE;
      assert(automatable == (depth || control));
    }
  }
  load(cleared);
  std::cout << "Modulation matrix CLAP: depth automation, generic and routing destinations, Via, follower tap, MIDI retrigger and state-only selectors passed\n";
}
