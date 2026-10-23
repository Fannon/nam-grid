#pragma once
// CLAP host-contract regressions for the patched iPlug2 adapter
// (patches/iplug2-clap-host-contract.patch): bounded value-to-text buffers,
// canonical stepped text, short writes/reads, state-load rescans and
// audio-port configuration agreement.
#include "ClapTestHost.h"
#include <clap/ext/audio-ports-config.h>

// Streams that move at most `chunk` bytes per call, as CLAP permits.
struct ChunkedStream
{
  std::vector<uint8_t> bytes;
  size_t pos = 0, chunk = 1;
  static int64_t write(const clap_ostream* stream, const void* data, uint64_t n)
  {
    auto& self = *static_cast<ChunkedStream*>(stream->ctx);
    const auto count = std::min<uint64_t>(n, self.chunk);
    auto* p = static_cast<const uint8_t*>(data);
    self.bytes.insert(self.bytes.end(), p, p + count);
    return static_cast<int64_t>(count);
  }
  static int64_t read(const clap_istream* stream, void* data, uint64_t n)
  {
    auto& self = *static_cast<ChunkedStream*>(stream->ctx);
    const auto count = std::min<uint64_t>({n, self.chunk, self.bytes.size() - self.pos});
    std::memcpy(data, self.bytes.data() + self.pos, count);
    self.pos += count;
    return static_cast<int64_t>(count);
  }
};

inline void TestClapHostContract(const clap_plugin* plugin, Host& host)
{
  const auto* params = static_cast<const clap_plugin_params*>(plugin->get_extension(plugin, CLAP_EXT_PARAMS));
  const auto* state = static_cast<const clap_plugin_state*>(plugin->get_extension(plugin, CLAP_EXT_STATE));
  const auto find = [&](const char* name) {
    for (uint32_t i = 0; i < params->count(plugin); ++i)
    {
      clap_param_info info{};
      assert(params->get_info(plugin, i, &info));
      if (std::strcmp(info.name, name) == 0)
        return info;
    }
    assert(false);
    return clap_param_info{};
  };

  // value_to_text writes the text and its NUL within the advertised size only.
  for (const char* name : {"Tuner", "Output", "Pre 1 Distortion Style"})
  {
    const auto info = find(name);
    char full[256]{};
    assert(params->value_to_text(plugin, info.id, info.default_value, full, sizeof(full)));
    const auto length = static_cast<uint32_t>(std::strlen(full));
    assert(length > 0);
    std::vector<char> buffer(length + 2, 'X');
    assert(!params->value_to_text(plugin, info.id, info.default_value, buffer.data(), length)); // No room for NUL
    assert(buffer[length] == 'X' && buffer[length + 1] == 'X');
    assert(!params->value_to_text(plugin, info.id, info.default_value, buffer.data(), 0));
    assert(params->value_to_text(plugin, info.id, info.default_value, buffer.data(), length + 1));
    assert(std::strcmp(buffer.data(), full) == 0 && buffer[length + 1] == 'X');
  }

  // Stepped values truncate fractional input, so text is canonical and round-trips.
  for (const char* name : {"Tuner", "Pre 1 Distortion Style", "LFO Target"})
  {
    const auto info = find(name);
    assert(info.flags & CLAP_PARAM_IS_STEPPED);
    for (double value = info.min_value; value <= info.max_value; value += .25)
    {
      char text[128]{}, canonical[128]{};
      assert(params->value_to_text(plugin, info.id, value, text, sizeof(text)));
      assert(params->value_to_text(plugin, info.id, std::trunc(value), canonical, sizeof(canonical)));
      assert(std::strcmp(text, canonical) == 0);
      double parsed = -1.;
      assert(params->text_to_value(plugin, info.id, text, &parsed) && parsed == std::trunc(value));
    }
  }

  // Saving must loop over short writes; loading over short reads.
  Stream reference;
  clap_ostream full{&reference, Stream::write};
  assert(state->save(plugin, &full));
  for (size_t chunk : {1, 23, 4096})
  {
    ChunkedStream partial;
    partial.chunk = chunk;
    clap_ostream writer{&partial, ChunkedStream::write};
    assert(state->save(plugin, &writer));
    assert(partial.bytes == reference.bytes);
    host.rescanned = 0;
    clap_istream reader{&partial, ChunkedStream::read};
    assert(state->load(plugin, &reader));
    // A host-side load refreshes the host's cached parameter values.
    assert(host.rescanned & CLAP_PARAM_RESCAN_VALUES);
  }
  clap_ostream refused{nullptr, [](const clap_ostream*, const void*, uint64_t) -> int64_t { return 0; }};
  assert(!state->save(plugin, &refused)); // No progress is an error, not an endless loop.

  // Each configuration's main-port summary agrees with the ports it selects.
  const auto* configs = static_cast<const clap_plugin_audio_ports_config*>(
    plugin->get_extension(plugin, CLAP_EXT_AUDIO_PORTS_CONFIG));
  const auto* ports = static_cast<const clap_plugin_audio_ports*>(plugin->get_extension(plugin, CLAP_EXT_AUDIO_PORTS));
  assert(configs && ports && configs->count(plugin) > 0);
  clap_audio_port_info initialIn{}, initialOut{};
  assert(ports->get(plugin, 0, true, &initialIn) && ports->get(plugin, 0, false, &initialOut));
  clap_id restore = CLAP_INVALID_ID;
  for (uint32_t i = 0; i < configs->count(plugin); ++i)
  {
    clap_audio_ports_config config{};
    assert(configs->get(plugin, i, &config));
    assert(config.has_main_input == (config.input_port_count > 0));
    assert(config.has_main_output == (config.output_port_count > 0));
    assert(config.has_main_input ? config.main_input_channel_count > 0 && config.main_input_port_type
                                 : config.main_input_channel_count == 0);
    assert(config.has_main_output ? config.main_output_channel_count > 0 && config.main_output_port_type
                                  : config.main_output_channel_count == 0);
    assert(configs->select(plugin, config.id));
    assert(ports->count(plugin, true) == config.input_port_count && ports->count(plugin, false) == config.output_port_count);
    clap_audio_port_info in{}, out{};
    assert(ports->get(plugin, 0, true, &in) && ports->get(plugin, 0, false, &out));
    assert((in.flags & CLAP_AUDIO_PORT_IS_MAIN) && (out.flags & CLAP_AUDIO_PORT_IS_MAIN));
    assert(in.channel_count == config.main_input_channel_count && out.channel_count == config.main_output_channel_count);
    assert(std::strcmp(in.port_type, config.main_input_port_type) == 0
           && std::strcmp(out.port_type, config.main_output_port_type) == 0);
    if (in.channel_count == initialIn.channel_count && out.channel_count == initialOut.channel_count)
      restore = config.id;
  }
  assert(restore != CLAP_INVALID_ID && configs->select(plugin, restore));
  std::cout << "CLAP host contract: bounded text, canonical stepped values, short writes/reads, rescans and port "
               "configurations passed\n";
}
