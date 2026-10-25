#pragma once
#include "../NeuralAmpModeler/DrivePresets.h"
#include "../NeuralAmpModeler/SlotPedalDrive.h"

template <class Save, class Load>
void TestPedalDriveClap(const clap_plugin* plugin, const json& cleared, Save save, Load load)
{
  const auto* latency = static_cast<const clap_plugin_latency*>(plugin->get_extension(plugin, CLAP_EXT_LATENCY));
  assert(latency);
  for (auto rig : drive_presets::Make(cleared))
  {
    load(rig);
    assert(save().config() == rig);
    assert(plugin->activate(plugin, 48000., 1, 512));
    const bool boost = rig["SlotEffects"][0] == static_cast<int>(SlotEffect::Boost);
    assert(latency->get(plugin) == (boost ? 0u : SlotPedalDrive::latency));
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
    for (int block = 0; block < 24; ++block)
    {
      process.frames_count = std::array<int, 4>{1, 3, 127, 512}[block % 4];
      for (int i = 0; i < process.frames_count; ++i)
        l[i] = .1 * std::sin((block * 512 + i) * .071);
      assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
      for (int i = 0; i < process.frames_count; ++i)
      {
        assert(std::isfinite(outL[i]) && std::abs(outL[i]) < 8.);
        assert(std::abs(outR[i]) < 1.e-15);
        energy += outL[i] * outL[i];
      }
    }
    assert(energy > .01);
    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);
  }
  auto historical = cleared;
  historical["schema"] = 2;
  historical["Post 4 Black Hold"] = 19.;
  for (int p = uPedalStyle; p <= uPedalCharacter; ++p)
    for (int slot = 0; slot < 16; ++slot)
      historical.erase(GridSlotName(slot) + " " + kEffectParameters[p].name);
  load(historical);
  const auto upgraded = save().config();
  assert(upgraded["schema"] == kEffectStateSchema && upgraded["Post 4 Black Hold"] == 19.);
  for (int p = uPedalStyle; p <= uPedalCharacter; ++p)
    assert(upgraded[GridSlotName(15) + " " + kEffectParameters[p].name] == kEffectParameters[p].initial);
  // Schema 3 had the original seven styles but no character field. Loading it
  // must retain its drive settings and use the centered character by default.
  auto previous = cleared;
  previous["schema"] = 3;
  previous["SlotEffects"][0] = static_cast<int>(SlotEffect::PedalDrive);
  previous["Pre 1 Pedal Style"] = 5.;
  previous["Pre 1 Pedal Drive"] = 17.;
  for (int slot = 0; slot < 16; ++slot)
    previous.erase(GridSlotName(slot) + " Pedal Character");
  load(previous);
  assert(save().config()["Pre 1 Pedal Character"] == 0.);
  assert(save().config()["Pre 1 Pedal Style"] == 5. && save().config()["Pre 1 Pedal Drive"] == 17.);
  auto restore = cleared;
  load(restore);
  std::cout << "Pedal Drive CLAP: example rigs, schema 2/3 upgrades, stereo isolation and latency passed\n";
}
