#pragma once
#include <clap/clap.h>
#include <json.hpp>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#ifdef _WIN32
  #include <windows.h>
#else
  #include <dlfcn.h>
#endif
using json = nlohmann::json;
struct Stream
{
  std::vector<uint8_t> bytes;
  size_t pos = 0;
  static int64_t write(const clap_ostream* stream, const void* data, uint64_t n)
  {
    auto& self = *static_cast<Stream*>(stream->ctx);
    auto* ptr = static_cast<const uint8_t*>(data);
    self.bytes.insert(self.bytes.end(), ptr, ptr + n);
    return n;
  }
  static int64_t read(const clap_istream* stream, void* data, uint64_t n)
  {
    auto& self = *static_cast<Stream*>(stream->ctx);
    n = std::min<uint64_t>(n, self.bytes.size() - self.pos);
    std::memcpy(data, self.bytes.data() + self.pos, n);
    self.pos += n;
    return n;
  }
  void string(const std::string& value)
  {
    int n = static_cast<int>(value.size());
    auto* p = reinterpret_cast<uint8_t*>(&n);
    bytes.insert(bytes.end(), p, p + 4);
    bytes.insert(bytes.end(), value.begin(), value.end());
  }
  json config() const
  {
    int headerSize;
    std::memcpy(&headerSize, bytes.data(), 4);
    int textSize;
    std::memcpy(&textSize, bytes.data() + 4 + headerSize, 4);
    return json::parse(std::string(reinterpret_cast<const char*>(bytes.data() + 8 + headerSize), textSize));
  }
};
struct Host
{
  bool restart = false, callback = false, dirty = false, flushRequested = false;
  static const void* extension(const clap_host*, const char* id)
  {
    static const clap_host_latency latency{[](const clap_host*) {}};
    static const clap_host_state state{[](const clap_host* h) { static_cast<Host*>(h->host_data)->dirty = true; }};
    static const clap_host_params params{
      [](const clap_host*, clap_param_rescan_flags) {}, [](const clap_host*, clap_id, clap_param_clear_flags) {},
      [](const clap_host* h) { static_cast<Host*>(h->host_data)->flushRequested = true; }};
    if (std::strcmp(id, CLAP_EXT_STATE) == 0)
      return &state;
    if (std::strcmp(id, CLAP_EXT_PARAMS) == 0)
      return &params;
    return std::strcmp(id, CLAP_EXT_LATENCY) == 0 ? &latency : nullptr;
  }
  clap_host host{CLAP_VERSION_INIT,
                 this,
                 "NAM smoke host",
                 "local",
                 "",
                 "1",
                 extension,
                 [](const clap_host* h) { static_cast<Host*>(h->host_data)->restart = true; },
                 [](const clap_host*) {},
                 [](const clap_host* h) { static_cast<Host*>(h->host_data)->callback = true; }};
};
void pump()
{
#ifdef _WIN32
  MSG msg;
  while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
#endif
}
