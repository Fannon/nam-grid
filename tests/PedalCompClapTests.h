#pragma once
#include "../NeuralAmpModeler/SlotPedalComp.h"

template <class Save, class Load>
void TestPedalCompClap(const clap_plugin* plugin, const json& cleared, Save save, Load load)
{
  const auto* latency = static_cast<const clap_plugin_latency*>(plugin->get_extension(plugin, CLAP_EXT_LATENCY));
  assert(latency);
  auto rig = cleared;
  rig["SlotEffects"][0] = static_cast<int>(SlotEffect::PedalComp);
  rig["Pre 1 Pedal Comp Sustain"] = 80.;
  rig["Pre 1 Pedal Comp Attack"] = 0.;
  rig["Pre 1 Pedal Comp Focus"] = -40.;
  rig["Pre 1 Pedal Comp Blend"] = 70.;
  rig["Pre 1 Pedal Comp Level"] = -2.;
  rig["Pre 1 Pedal Comp Pickup"] = 2.;
  load(rig);
  assert(save().config() == rig);
  assert(plugin->activate(plugin, 48000., 1, 512));
  assert(latency->get(plugin) == 0u); // No lookahead.
  assert(plugin->start_processing(plugin));
  std::array<double, 512> l{}, r{}, outL{}, outR{};
  double *inputs[]{l.data(), r.data()}, *outputs[]{outL.data(), outR.data()};
  clap_audio_buffer input{};
  input.data64 = inputs;
  input.channel_count = 2;
  clap_audio_buffer output{};
  output.data64 = outputs;
  output.channel_count = 2;
  clap_input_events empty{nullptr, [](const clap_input_events*) -> uint32_t { return 0; },
                          [](const clap_input_events*, uint32_t) -> const clap_event_header* { return nullptr; }};
  clap_output_events sent{nullptr, [](const clap_output_events*, const clap_event_header*) { return true; }};
  clap_process process{};
  process.audio_inputs_count = process.audio_outputs_count = 1;
  process.audio_inputs = &input;
  process.audio_outputs = &output;
  process.in_events = &empty;
  process.out_events = &sent;
  double energy = 0.;
  for (int block = 0; block < 48; ++block)
  {
    process.frames_count = std::array<int, 4>{1, 3, 127, 512}[block % 4];
    for (int i = 0; i < process.frames_count; ++i)
    {
      l[i] = .3 * std::sin((block * 512 + i) * .071);
      r[i] = .5 * l[i];
    }
    assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
    for (int i = 0; i < process.frames_count; ++i)
    {
      assert(std::isfinite(outL[i]) && std::abs(outL[i]) < 8.);
      // The linked detector applies one gain to both channels.
      assert(std::abs(outR[i] - .5 * outL[i]) < 1.e-9);
      energy += outL[i] * outL[i];
    }
  }
  assert(energy > .01);
  plugin->stop_processing(plugin);
  plugin->deactivate(plugin);
  // States saved before Pedal Comp existed gain its controls at their defaults.
  auto historical = cleared;
  historical["schema"] = 12;
  for (int p = uPedalCompSustain; p <= uPedalCompPickup; ++p)
    for (int slot = 0; slot < 16; ++slot)
      historical.erase(GridSlotName(slot) + " " + kEffectParameters[p].name);
  load(historical);
  const auto upgraded = save().config();
  assert(upgraded["schema"] == kEffectStateSchema);
  for (int p = uPedalCompSustain; p <= uPedalCompPickup; ++p)
    for (int slot : {0, 15})
      assert(upgraded[GridSlotName(slot) + " " + kEffectParameters[p].name] == kEffectParameters[p].initial);
  auto reset = cleared;
  load(reset);
}
