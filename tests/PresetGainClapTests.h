#pragma once
#include "../NeuralAmpModeler/RigPreset.h"

template <class Load>
void TestGlobalInputGain(const clap_plugin* plugin, json cleared, Load load)
{
  const auto render = [&](double gain, double output, bool tinyBlocks) {
    auto config = cleared; config["Global Input"] = gain; config["Output"] = output;
    load(config);
    assert(plugin->activate(plugin, 48000., 1, 512)); assert(plugin->start_processing(plugin));
    std::array<double, 512> left{}, right{}, outLeft{}, outRight{};
    double* in[]{left.data(), right.data()}, *out[]{outLeft.data(), outRight.data()};
    clap_audio_buffer input{nullptr, in, 2, 0, 0}, result{nullptr, out, 2, 0, 0};
    clap_input_events events{nullptr, [](const clap_input_events*) -> uint32_t { return 0; },
                            [](const clap_input_events*, uint32_t) -> const clap_event_header* { return nullptr; }};
    clap_output_events sent{nullptr, [](const clap_output_events*, const clap_event_header*) { return true; }};
    clap_process process{}; process.audio_inputs_count = process.audio_outputs_count = 1;
    process.audio_inputs = &input; process.audio_outputs = &result; process.in_events = &events; process.out_events = &sent;
    std::array<std::vector<double>, 2> rendered;
    for (int pos = 0, block = 0; pos < 4096;)
    {
      const int n = std::min(4096 - pos, tinyBlocks ? std::array<int, 4>{1, 3, 64, 127}[block++ % 4] : 512);
      for (int i = 0; i < n; ++i) { left[i] = .2 * std::sin((pos + i) * .13); right[i] = -.1 * std::cos((pos + i) * .07); }
      process.frames_count = n;
      assert(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR);
      rendered[0].insert(rendered[0].end(), outLeft.begin(), outLeft.begin() + n);
      rendered[1].insert(rendered[1].end(), outRight.begin(), outRight.begin() + n);
      pos += n;
    }
    plugin->stop_processing(plugin); plugin->deactivate(plugin);
    return rendered;
  };
  const auto reference = render(0., 0., false), quiet = render(-6., -12., true);
  const double attenuation = std::pow(10., -18. / 20.);
  for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 4096; ++i)
    assert(std::abs(quiet[ch][i] - reference[ch][i] * attenuation) < 1.e-10);
}
