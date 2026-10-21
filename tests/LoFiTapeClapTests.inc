// Schema 14 -> 15 preserves existing controls while initializing new tape knobs.
{
  auto prior = cleared;
  prior["schema"] = 14;
  prior["Post 4 Boost Gain"] = 13.;
  for (int slot = 0; slot < 16; ++slot)
  {
    const auto label = std::string(slot < 4 ? "Pre " : slot < 8 ? "A " : slot < 12 ? "B " : "Post ")
                       + std::to_string(slot % 4 + 1);
    for (int p = uLoFiSpeed; p <= uLoFiOutput; ++p) prior.erase(label + " " + kEffectParameters[p].name);
  }
  load(prior);
  const auto migrated = save().config();
  assert(migrated["schema"] == 15 && migrated["Post 4 Boost Gain"] == 13.);
  for (const char* slot : {"Pre 1", "A 4", "B 4", "Post 4"})
    for (int p = uLoFiSpeed; p <= uLoFiOutput; ++p)
      assert(migrated[std::string(slot) + " " + kEffectParameters[p].name] == kEffectParameters[p].initial);

  auto tapeState = cleared;
  tapeState["SlotEffects"][0] = static_cast<int>(SlotEffect::LoFiTape);
  for (int p = uLoFiSpeed; p <= uLoFiOutput; ++p)
    tapeState[std::string("Pre 1 ") + kEffectParameters[p].name] = kEffectParameters[p].maximum;
  load(tapeState);
  assert(save().config() == tapeState);
  Stream rejected;
  auto oldTape = tapeState; oldTape["schema"] = 14;
  rejected.string("###NAMGrid-v1###"); rejected.string(oldTape.dump());
  assert(!loadBytes(rejected) && save().config() == tapeState);
  auto malformed = tapeState; malformed.erase("Pre 1 LoFi Wear");
  Stream missing; missing.string("###NAMGrid-v1###"); missing.string(malformed.dump());
  assert(!loadBytes(missing) && save().config() == tapeState);

  // IDs of ALL predecessor parameters are unchanged. New tape controls are appended.
  for (uint32_t index = 0; index < params->count(plugin); ++index)
  {
    clap_param_info info{}; assert(params->get_info(plugin, index, &info));
    if (std::strcmp(info.name, "Pre 1 LoFi Speed") == 0)
      assert(info.id == 286 + 16 * kSchema14UtilityControls + grid_performance::Count);
    if (std::strcmp(info.name, "Post 4 Noise Gate Threshold") == 0)
      assert(info.id == 284 + 15 * kSchema14UtilityControls + uNoiseGateThreshold);
    if (std::strcmp(info.name, "Tuner Reference") == 0)
      assert(info.id == 284 + 16 * kSchema14UtilityControls);
  }
  // Real CLAP automation reaches all seven controls and survives state recall.
  assert(plugin->activate(plugin, 48000., 1, 512));
  assert(plugin->start_processing(plugin));
  process.frames_count = 128;
  for (int p = uLoFiSpeed; p <= uLoFiOutput; ++p)
  {
    const std::string name = std::string("Pre 1 ") + kEffectParameters[p].name;
    clap_param_info target{}; bool found = false;
    for (uint32_t index = 0; index < params->count(plugin); ++index)
    {
      clap_param_info info{}; assert(params->get_info(plugin, index, &info));
      if (name == info.name) { target = info; found = true; break; }
    }
    assert(found);
    clap_event_param_value value{};
    value.header.size = sizeof(value); value.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    value.header.type = CLAP_EVENT_PARAM_VALUE;
    value.param_id = target.id; value.value = kEffectParameters[p].initial;
    value.note_id = value.port_index = value.channel = value.key = -1;
    clap_input_events update{&value, [](const clap_input_events*) -> uint32_t { return 1; },
                             [](const clap_input_events* e, uint32_t) -> const clap_event_header* {
                               return &static_cast<clap_event_param_value*>(e->ctx)->header;
                             }};
    process.in_events = &update;
    input[0].fill(.05); input[1].fill(-.02);
    assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
    for (int ch = 0; ch < 2; ++ch) for (int frame = 0; frame < 128; ++frame) assert(std::isfinite(output[ch][frame]));
    assert(std::abs(save().config()[name].get<double>() - kEffectParameters[p].initial) < 1.e-8);
    process.in_events = &events;
  }
  plugin->stop_processing(plugin); plugin->deactivate(plugin);
  const auto automated = save();
  load(base); assert(loadBytes(automated));
  assert(save().config() == automated.config());
  load(base);
}
