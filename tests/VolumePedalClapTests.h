#pragma once

// A Volume Pedal follows MIDI CCs sent to the plugin (any channel), and Position takes
// over again when it changes: the last pedal or knob move wins.
template <typename Load>
void TestVolumePedalMidiClap(const clap_plugin* plugin, json cleared, Load load)
{
  auto* params = static_cast<const clap_plugin_params*>(plugin->get_extension(plugin, CLAP_EXT_PARAMS));
  cleared["Lane Mode"] = grid_performance::Sequential; // One chain, so the pedal sets the whole output.
  auto config = cleared;
  config["SlotEffects"][0] = static_cast<int>(SlotEffect::VolumePedal);
  config[std::string("Pre 1 ") + kEffectParameters[uVolumeSmoothing].name] = 1.;
  std::array<double, 512> left{}, right{}, outLeft{}, outRight{};
  double* in[]{left.data(), right.data()}, *out[]{outLeft.data(), outRight.data()};
  clap_audio_buffer input{nullptr, in, 2, 0, 0}, result{nullptr, out, 2, 0, 0};
  std::vector<const clap_event_header*> queued;
  clap_input_events events{&queued,
    [](const clap_input_events* e) { return static_cast<uint32_t>(static_cast<std::vector<const clap_event_header*>*>(e->ctx)->size()); },
    [](const clap_input_events* e, uint32_t i) { return (*static_cast<std::vector<const clap_event_header*>*>(e->ctx))[i]; }};
  clap_output_events sent{nullptr, [](const clap_output_events*, const clap_event_header*) { return true; }};
  clap_process process{}; process.audio_inputs_count = process.audio_outputs_count = 1;
  process.audio_inputs = &input; process.audio_outputs = &result; process.in_events = &events; process.out_events = &sent;
  // Gain (dB) of the last block after four blocks with `first` events on the first block.
  int position = 0;
  double reference = 0.; // The rig's own gain without the pedal.
  const auto render = [&](std::vector<const clap_event_header*> first) {
    double inEnergy = 0., outEnergy = 0.;
    for (int block = 0; block < 4; ++block)
    {
      queued = block == 0 ? first : std::vector<const clap_event_header*>{};
      for (int i = 0; i < 512; ++i) left[i] = right[i] = .2 * std::sin((position + i) * .05);
      process.frames_count = 512;
      assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
      if (block == 3)
        for (int i = 0; i < 512; ++i) { inEnergy += left[i] * left[i]; outEnergy += outLeft[i] * outLeft[i]; }
      position += 512;
    }
    queued.clear();
    return 10. * std::log10(std::max(1.e-30, outEnergy) / inEnergy) - reference;
  };
  load(cleared);
  assert(plugin->activate(plugin, 48000., 1, 512)); assert(plugin->start_processing(plugin));
  reference = render({});
  plugin->stop_processing(plugin); plugin->deactivate(plugin);
  load(config);
  assert(plugin->activate(plugin, 48000., 1, 512)); assert(plugin->start_processing(plugin));
  const auto cc = [](int status, int number, int value) {
    clap_event_midi midi{};
    midi.header.size = sizeof(midi); midi.header.type = CLAP_EVENT_MIDI; midi.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    midi.port_index = 0; midi.data[0] = static_cast<uint8_t>(status); midi.data[1] = static_cast<uint8_t>(number);
    midi.data[2] = static_cast<uint8_t>(value);
    return midi;
  };
  const auto positionEvent = [&](const char* text) {
    clap_event_param_value change{};
    change.header.size = sizeof(change); change.header.type = CLAP_EVENT_PARAM_VALUE;
    change.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    change.param_id = static_cast<clap_id>(SlotUtilityParam(0, uVolumePosition));
    assert(params->text_to_value(plugin, change.param_id, text, &change.value));
    change.note_id = -1; change.port_index = change.channel = change.key = -1;
    return change;
  };
  assert(std::abs(render({})) < .01); // Insertion: Position 100%, Toe 0 dB.
  // CC 11 on channel 4 moves the pedal: heel mutes, half travel is the audio taper.
  auto heel = cc(0xB3, 11, 0), half = cc(0xB0, 11, 64), other = cc(0xB0, 7, 0);
  assert(render({&heel.header}) < -50.); // The rig after the pedal adds a faint floor of its own.
  const double expected = 20. * std::log10(std::pow(64. / 127., 2.));
  assert(std::abs(render({&half.header}) - expected) < .05);
  assert(std::abs(render({&other.header}) - expected) < .05); // Another CC is ignored.
  // Moving Position takes over from the pedal, until the next CC.
  auto knob = positionEvent("100");
  auto knobLow = positionEvent("50");
  assert(std::abs(render({&knobLow.header}) - 20. * std::log10(.25)) < .05);
  assert(render({&heel.header}) < -50.); // The rig after the pedal adds a faint floor of its own.
  assert(std::abs(render({&knob.header}) - 0.) < .01);
  plugin->stop_processing(plugin); plugin->deactivate(plugin);
  // MIDI CC Off ignores controllers.
  config[std::string("Pre 1 ") + kEffectParameters[uVolumeCC].name] = 0.;
  load(config);
  assert(plugin->activate(plugin, 48000., 1, 512)); assert(plugin->start_processing(plugin));
  assert(std::abs(render({&heel.header})) < .01);
  plugin->stop_processing(plugin); plugin->deactivate(plugin);
}
